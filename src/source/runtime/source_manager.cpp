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

std::string read_text_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("failed to read plugin file");
    }
    return std::string(
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>());
}

std::string make_module_id(const std::filesystem::path& relative_path) {
    auto module_id = relative_path.generic_string();
    if (module_id.ends_with(".js")) {
        module_id.resize(module_id.size() - 3);
    }
    return module_id;
}

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

void SourceManager::load_from_directory(const std::string& plugin_dir) {
    namespace fs = std::filesystem;

    const fs::path plugin_path(plugin_dir);
    const fs::path cwd = fs::current_path();
    spdlog::info("Plugin load start. cwd='{}', plugin_dir='{}'", cwd.string(), plugin_path.string());
    if (plugin_path.is_relative()) {
        spdlog::info("Plugin dir is relative. resolved='{}'", fs::absolute(plugin_path).string());
    }

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

    std::vector<JsModule> modules;
    std::vector<JsModule> plugins;
    for (const auto& entry : fs::recursive_directory_iterator(plugin_path)) {
        if (!entry.is_regular_file()) {
            spdlog::debug("Skip non-file entry: {}", entry.path().string());
            continue;
        }
        if (entry.path().extension() != ".js") {
            spdlog::debug("Skip non-js file: {}", entry.path().string());
            continue;
        }

        const auto relative_path = fs::relative(entry.path(), plugin_path);
        JsModule module;
        module.module_id = make_module_id(relative_path);
        module.plugin_path = entry.path().string();
        module.source = read_text_file(entry.path());
        module.is_plugin_candidate = !has_private_path_segment(relative_path);

        spdlog::info("Discovered JS module: {} -> {}", module.module_id, module.plugin_path);
        modules.push_back(module);
        if (module.is_plugin_candidate) {
            plugins.push_back(module);
        }
    }

    std::sort(modules.begin(), modules.end(), [](const JsModule& lhs, const JsModule& rhs) {
        return lhs.module_id < rhs.module_id;
    });
    std::sort(plugins.begin(), plugins.end(), [](const JsModule& lhs, const JsModule& rhs) {
        return lhs.module_id < rhs.module_id;
    });

    spdlog::info("Plugin scan finished. module_count={}, candidate_count={}",
                 modules.size(), plugins.size());

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

    plugin_runtime_->queue_modules(modules);
    plugin_runtime_->queue_bootstrap(plugins);
}

void SourceManager::set_preferred_source(const std::string& source_id) {
    std::scoped_lock lock(mutex_);
    preferred_source_id_ = source_id;
    if (initialized_ && !preferred_source_id_.empty()) {
        select_source_unlocked(preferred_source_id_);
    }
}

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

bool SourceManager::select_source(const std::string& source_id) {
    ensure_ready();

    std::scoped_lock lock(mutex_);
    return select_source_unlocked(source_id);
}

std::shared_ptr<IBookSource> SourceManager::current_source() const {
    ensure_ready();

    std::scoped_lock lock(mutex_);
    if (!current_source_) {
        throw SourceException({SourceErrorCode::SourceNotSelected, "", "", "current_source",
                               "no source selected"});
    }
    return current_source_;
}

std::optional<SourceInfo> SourceManager::current_info() const {
    ensure_ready();

    std::scoped_lock lock(mutex_);
    if (!current_source_) {
        return std::nullopt;
    }
    return current_source_->info();
}

void SourceManager::configure_current() {
    current_source()->configure();
}

void SourceManager::ensure_ready() const {
    {
        std::scoped_lock lock(mutex_);
        if (initialized_) {
            return;
        }
    }

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

    std::vector<std::shared_ptr<IBookSource>> loaded_sources;
    loaded_sources.reserve(plugins.size());
    for (const auto& plugin : plugins) {
        try {
            auto source = std::make_shared<JsBookSource>(plugin_runtime_, plugin);
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

            spdlog::info("Loaded source '{}' from {}", source->info().id, plugin.plugin_path);
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

    std::shared_ptr<IBookSource> current;
    {
        std::scoped_lock lock(mutex_);
        if (initialized_) {
            return;
        }

        sources_ = std::move(loaded_sources);
        current_source_ = sources_.front();
        if (!preferred_source_id_.empty()) {
            select_source_unlocked(preferred_source_id_);
        }
        current = current_source_;
        initialized_ = true;
    }

    try {
        current->configure();
    } catch (const SourceException& e) {
        spdlog::warn("Current source is not fully configured yet: {}",
                     format_source_error_log(e.error()));
    }
}

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
