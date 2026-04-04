#include "source/runtime/source_manager.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>

#include <spdlog/spdlog.h>

#include "source/domain/source_errors.h"
#include "source/js/js_book_source.h"
#include "source/js/js_plugin_runtime.h"

namespace novel {

namespace {

/// 读取文本文件的全部内容到字符串
std::string read_text_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("failed to read plugin file");
    }
    return std::string(
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>());
}

/// 从文件相对路径生成模块 ID（去除 .js 后缀，使用正斜杠分隔）
/// 例如 "sub/fanqie.js" → "sub/fanqie"
std::string make_module_id(const std::filesystem::path& relative_path) {
    auto module_id = relative_path.generic_string();
    if (module_id.ends_with(".js")) {
        module_id.resize(module_id.size() - 3);
    }
    return module_id;
}

/// 检查相对路径中是否包含以 _ 开头的路径段（私有模块标记）
/// 以 _ 开头的路径段表示该模块是内部共享模块，不作为书源候选
bool has_private_path_segment(const std::filesystem::path& relative_path) {
    for (const auto& segment : relative_path) {
        const auto name = segment.string();
        if (!name.empty() && name.front() == '_') {
            return true;
        }
    }
    return false;
}

} // namespace

SourceManager::SourceManager(std::shared_ptr<JsPluginRuntime> plugin_runtime)
    : plugin_runtime_(std::move(plugin_runtime)) {}

// ── 插件扫描与加载 ─────────────

/// 从指定目录递归扫描 .js 文件，区分为普通模块和书源候选插件
/// 扫描完成后将模块注册到 JS 运行时并触发引导流程
void SourceManager::load_from_directory(const std::string& plugin_dir) {
    namespace fs = std::filesystem;

    const fs::path plugin_path(plugin_dir);
    const fs::path cwd = fs::current_path();
    spdlog::info("Plugin load start. cwd='{}', plugin_dir='{}'", cwd.string(), plugin_path.string());
    if (plugin_path.is_relative()) {
        spdlog::info("Plugin dir is relative. resolved='{}'", fs::absolute(plugin_path).string());
    }

    // 校验插件目录是否存在且为目录
    if (!fs::exists(plugin_path)) {
        spdlog::error("Plugin directory does not exist: {}", plugin_path.string());
        throw SourceException({SourceErrorCode::PluginLoadFailed, "", plugin_dir,
                               "scan", "plugin directory does not exist"});
    }
    if (!fs::is_directory(plugin_path)) {
        spdlog::error("Plugin path is not a directory: {}", plugin_path.string());
        throw SourceException({SourceErrorCode::PluginLoadFailed, "", plugin_dir,
                               "scan", "plugin path is not a directory"});
    }

    // 递归遍历插件目录，收集所有 .js 模块
    std::vector<JsModule> modules;
    std::vector<JsModule> plugins;
    for (const auto& entry : fs::recursive_directory_iterator(plugin_path)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        if (entry.path().extension() != ".js") {
            continue;
        }

        const auto relative_path = fs::relative(entry.path(), plugin_path);
        JsModule module;
        module.module_id = make_module_id(relative_path);
        module.plugin_path = entry.path().string();
        module.source = read_text_file(entry.path());
        // 路径段以 _ 开头的模块视为私有共享模块，不作为书源候选
        module.is_plugin_candidate = !has_private_path_segment(relative_path);

        modules.push_back(module);
        if (module.is_plugin_candidate) {
            plugins.push_back(module);
        }
    }

    // 按模块 ID 排序，确保加载顺序稳定
    std::sort(modules.begin(), modules.end(), [](const JsModule& lhs, const JsModule& rhs) {
        return lhs.module_id < rhs.module_id;
    });
    std::sort(plugins.begin(), plugins.end(), [](const JsModule& lhs, const JsModule& rhs) {
        return lhs.module_id < rhs.module_id;
    });

    // 至少需要一个书源候选插件
    if (plugins.empty()) {
        spdlog::error("No valid source plugins found in {}", plugin_path.string());
        throw SourceException({SourceErrorCode::PluginLoadFailed, "", plugin_dir,
                               "scan", "no valid source plugins found"});
    }

    plugin_dir_ = plugin_dir;
    {
        std::lock_guard lock(mutex_);
        initialized_ = false;
        sources_.clear();
        current_source_.reset();
    }

    // 将模块注册到 JS 运行时并触发引导
    plugin_runtime_->queue_modules(modules);
    plugin_runtime_->queue_bootstrap(plugins);
}

// ── 书源选择与管理 ─────────────

/// 设置首选书源 ID（若已完成初始化则立即切换）
void SourceManager::set_preferred_source(const std::string& source_id) {
    std::scoped_lock lock(mutex_);
    preferred_source_id_ = source_id;
    if (initialized_ && !preferred_source_id_.empty()) {
        select_source_unlocked(preferred_source_id_);
    }
}

/// 列出所有已加载书源的基本信息
std::vector<SourceInfo> SourceManager::list_sources() const {
    ensure_ready();

    std::scoped_lock lock(mutex_);
    std::vector<SourceInfo> infos;
    infos.reserve(sources_.size());
    for (const auto& source : sources_) {
        infos.push_back(source->info());
    }
    return infos;
}

/// 切换当前书源（若指定 ID 不存在则返回 false，不改变当前书源）
bool SourceManager::select_source(const std::string& source_id) {
    ensure_ready();

    std::scoped_lock lock(mutex_);
    return select_source_unlocked(source_id);
}

/// 获取当前书源实例，若未选择书源则抛出 SourceNotSelected 异常
std::shared_ptr<IBookSource> SourceManager::current_source() const {
    ensure_ready();

    std::scoped_lock lock(mutex_);
    if (!current_source_) {
        throw SourceException({SourceErrorCode::SourceNotSelected, "", "", "current_source",
                               "no source selected"});
    }
    return current_source_;
}

/// 获取当前书源的基本信息，若未选择则返回 nullopt
std::optional<SourceInfo> SourceManager::current_info() const {
    ensure_ready();

    std::scoped_lock lock(mutex_);
    if (!current_source_) {
        return std::nullopt;
    }
    return current_source_->info();
}

/// 对当前书源执行 configure 操作
void SourceManager::configure_current() {
    current_source()->configure();
}

// ── 懒初始化与引导等待 ─────────────

/// 确保插件引导流程已完成（懒初始化入口）
/// 首次调用时会阻塞等待 JS 引导完成，之后直接返回
void SourceManager::ensure_ready() const {
    {
        std::scoped_lock lock(mutex_);
        if (initialized_) {
            return;
        }
    }

    // 阻塞等待 JS 插件引导完成
    std::vector<JsBootstrapPlugin> plugins;
    try {
        plugins = plugin_runtime_->wait_for_bootstrap();
    } catch (const std::exception& e) {
        throw SourceException({
            SourceErrorCode::PluginLoadFailed,
            "",
            plugin_dir_,
            "bootstrap",
            e.what(),
        });
    }

    // 将引导成功的插件转换为 JsBookSource 实例
    std::vector<std::shared_ptr<IBookSource>> loaded_sources;
    loaded_sources.reserve(plugins.size());
    for (const auto& plugin : plugins) {
        try {
            auto source = std::make_shared<JsBookSource>(plugin_runtime_, plugin);
            // 检查是否存在重复的书源 ID
            auto duplicate = std::find_if(
                loaded_sources.begin(),
                loaded_sources.end(),
                [&](const std::shared_ptr<IBookSource>& current) {
                    return current->info().id == source->info().id;
                });
            if (duplicate != loaded_sources.end()) {
                spdlog::error("duplicate source id '{}', ignoring {}",
                              source->info().id,
                              plugin.plugin_path);
                continue;
            }

            loaded_sources.push_back(std::move(source));
        } catch (const SourceException& e) {
            spdlog::error("Failed to load plugin {}: {}",
                          plugin.plugin_path,
                          format_source_error_log(e.error()));
        } catch (const std::exception& e) {
            spdlog::error("Failed to load plugin {}: {}", plugin.plugin_path, e.what());
        }
    }

    if (loaded_sources.empty()) {
        throw SourceException({
            SourceErrorCode::PluginLoadFailed,
            "",
            plugin_dir_,
            "bootstrap",
            "no valid source plugins found",
        });
    }

    // 设置已加载的书源列表，默认选择第一个书源
    std::shared_ptr<IBookSource> current;
    {
        std::scoped_lock lock(mutex_);
        // 双重检查：防止并发 ensure_ready 重复初始化
        if (initialized_) {
            return;
        }

        sources_ = std::move(loaded_sources);
        current_source_ = sources_.front();
        // 若设置了首选书源，则切换到首选
        if (!preferred_source_id_.empty()) {
            select_source_unlocked(preferred_source_id_);
        }
        current = current_source_;
        initialized_ = true;
    }

    // 对当前书源执行 configure（初始化环境变量等）
    try {
        current->configure();
    } catch (const SourceException& e) {
        spdlog::warn("Current source is not fully configured yet: {}",
                     format_source_error_log(e.error()));
    }
}

/// 不加锁版本的书源切换实现
bool SourceManager::select_source_unlocked(const std::string& source_id) const {
    auto it = std::find_if(
        sources_.begin(), sources_.end(),
        [&](const std::shared_ptr<IBookSource>& source) {
            return source->info().id == source_id;
        });
    if (it == sources_.end()) {
        return false;
    }
    current_source_ = *it;
    return true;
}

} // namespace novel
