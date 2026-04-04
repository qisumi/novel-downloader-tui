/// @file bridge.cpp
/// @brief C++ <-> JavaScript 桥接层实现
///
/// 实现所有 window.app.* API 的 C++ 端处理逻辑，
/// 包括参数解析、服务调用、进度事件推送和错误处理。

#include "gui/bridge.h"

#include <filesystem>
#include <sstream>
#include <string_view>
#include <thread>

#include <spdlog/spdlog.h>

namespace novel {

namespace {

using json = nlohmann::json;

std::string path_to_utf8(const std::filesystem::path& path)
{
    const auto utf8 = path.u8string();
    return std::string(reinterpret_cast<const char*>(utf8.data()), utf8.size());
}

/// 将 SourceInfo 转换为 JSON 对象
json source_to_json(const SourceInfo& source, bool selected)
{
    return {
        {"id", source.id},
        {"name", source.name},
        {"version", source.version},
        {"author", source.author},
        {"description", source.description},
        {"required_envs", source.required_envs},
        {"optional_envs", source.optional_envs},
        {"selected", selected},
    };
}

/// 将当前书源能力和登录态转换为 JSON 对象
json capabilities_to_json(const IBookSource& source)
{
    const auto& capabilities = source.capabilities();
    const bool logged_in = source.is_logged_in();
    return {
        {"supports_search", capabilities.supports_search},
        {"supports_book_info", capabilities.supports_book_info},
        {"supports_toc", capabilities.supports_toc},
        {"supports_chapter", capabilities.supports_chapter},
        {"supports_batch", capabilities.supports_batch},
        {"supports_login", capabilities.supports_login},
        {"logged_in", logged_in},
        {"batch_enabled", capabilities.supports_batch && (!capabilities.supports_login || logged_in)},
    };
}

/// 将 Book 转换为 JSON 对象
json book_to_json(const Book& book)
{
    json j;
    to_json(j, book);
    return j;
}

/// 将 TocItem 转换为 JSON 对象
json toc_to_json(const TocItem& item, bool cached)
{
    json j;
    to_json(j, item);
    j["cached"] = cached;
    return j;
}

/// 将 JSON 对象转换为 Book 结构体
Book json_to_book(const json& payload)
{
    Book book;
    from_json(payload, book);
    return book;
}

/// 生成下一个任务标签（如 "download-1"、"export-2"）
std::string next_task_label(std::atomic_uint64_t& counter, std::string_view prefix)
{
    std::ostringstream oss;
    oss << prefix << "-" << counter.fetch_add(1);
    return oss.str();
}

json make_task_event(
    const std::string& task_id,
    const std::string& kind,
    const std::string& stage,
    const Book& book,
    std::initializer_list<std::pair<std::string, json>> extra = {})
{
    json payload{
        {"task_id", task_id},
        {"kind", kind},
        {"stage", stage},
        {"book_id", book.book_id},
        {"title", book.title},
        {"cover_url", book.cover_url},
    };
    for (const auto& [k, v] : extra) {
        payload[k] = v;
    }
    return payload;
}

} // namespace

GuiBridge::GuiBridge(webview::webview& window, GuiAppRuntime& runtime)
    : window_(window), runtime_(runtime) {}

void GuiBridge::install()
{
    // 注入前端桥接层初始化脚本
    // window.__NOVEL_BRIDGE__.wrap 用于将 native 绑定包装为 Promise 风格调用
    window_.init(R"JS(
window.__NOVEL_BRIDGE__ = {
  wrap(name) {
    return (...args) => window[name](...args);
  }
};
window.app = {
  get_sources: (...args) => window.__NOVEL_BRIDGE__.wrap("native_get_sources")(...args),
  select_source: (...args) => window.__NOVEL_BRIDGE__.wrap("native_select_source")(...args),
  search_books: (...args) => window.__NOVEL_BRIDGE__.wrap("native_search_books")(...args),
  get_book_detail: (...args) => window.__NOVEL_BRIDGE__.wrap("native_get_book_detail")(...args),
  get_toc: (...args) => window.__NOVEL_BRIDGE__.wrap("native_get_toc")(...args),
  login: (...args) => window.__NOVEL_BRIDGE__.wrap("native_login")(...args),
  getSourceCapabilities: (...args) => window.__NOVEL_BRIDGE__.wrap("native_get_source_capabilities")(...args),
  download_book: (...args) => window.__NOVEL_BRIDGE__.wrap("native_download_book")(...args),
  export_book: (...args) => window.__NOVEL_BRIDGE__.wrap("native_export_book")(...args),
  list_bookshelf: (...args) => window.__NOVEL_BRIDGE__.wrap("native_list_bookshelf")(...args),
  save_bookshelf: (...args) => window.__NOVEL_BRIDGE__.wrap("native_save_bookshelf")(...args),
  remove_bookshelf: (...args) => window.__NOVEL_BRIDGE__.wrap("native_remove_bookshelf")(...args),
};
)JS");

    // ── 获取所有书源列表 ────────────────────────────────────────
    bind_async("native_get_sources", [&](const json&) {
        std::scoped_lock lock(core_mutex_);
        const auto current = runtime_.source_manager()->current_info();
        const auto sources = runtime_.source_manager()->list_sources();
        json items = json::array();
        for (const auto& source : sources) {
            items.push_back(source_to_json(source, current && current->id == source.id));
        }
        return make_success({
            {"sources", items},
            {"current_source_id", current ? current->id : ""},
        });
    });

    // ── 切换当前书源 ────────────────────────────────────────────
    bind_async("native_select_source", [&](const json& args) {
        const auto source_id = require_string_arg(args, 0, "source_id");

        std::scoped_lock lock(core_mutex_);
        if (!runtime_.source_manager()->select_source(source_id)) {
            return make_error("validation_error", "source_not_found", "未找到指定书源", {
                {"source_id", source_id},
            });
        }
        const auto current = runtime_.source_manager()->current_info();
        return make_success({
            {"source", source_to_json(*current, true)},
        });
    });

    // ── 搜索书籍 ────────────────────────────────────────────────
    bind_async("native_search_books", [&](const json& args) {
        const auto keywords = require_string_arg(args, 0, "keywords");
        const int page = optional_int_arg(args, 1, 0);

        std::scoped_lock lock(core_mutex_);
        json items = json::array();
        for (const auto& book : runtime_.library_service()->search_books(keywords, page)) {
            items.push_back(book_to_json(book));
        }
        return make_success({
            {"items", items},
            {"keywords", keywords},
            {"page", page},
        });
    });

    // ── 获取书籍详情 ────────────────────────────────────────────
    // 优先从书源获取，失败后回退到本地数据库缓存
    bind_async("native_get_book_detail", [&](const json& args) {
        const auto book_id = require_string_arg(args, 0, "book_id");

        std::scoped_lock lock(core_mutex_);
        auto book = runtime_.source_manager()->current_source()->get_book_info(book_id);
        if (!book) {
            book = runtime_.database()->get_book(
                runtime_.library_service()->current_source_id(), book_id);
        }
        if (!book) {
            return make_error("runtime_error", "book_not_found", "未获取到书籍详情", {
                {"book_id", book_id},
            });
        }
        return make_success({{"book", book_to_json(*book)}});
    });

    // ── 获取目录 ────────────────────────────────────────────────
    // 返回目录列表及各章节的缓存状态
    bind_async("native_get_toc", [&](const json& args) {
        const auto book_id = require_string_arg(args, 0, "book_id");
        const bool force_remote = optional_bool_arg(args, 1, false);

        std::scoped_lock lock(core_mutex_);
        auto book = runtime_.database()->get_book(runtime_.library_service()->current_source_id(), book_id);
        if (!book) {
            auto remote_book = runtime_.source_manager()->current_source()->get_book_info(book_id);
            if (remote_book) {
                book = *remote_book;
            } else {
                Book fallback;
                fallback.book_id = book_id;
                book = fallback;
            }
        }

        const auto toc = runtime_.library_service()->load_toc(*book, force_remote);
        json items = json::array();
        for (const auto& item : toc) {
            items.push_back(toc_to_json(item, runtime_.library_service()->chapter_cached(item.item_id)));
        }
        return make_success({
            {"items", items},
            {"cached_chapter_count", runtime_.library_service()->cached_chapter_count(book_id)},
            {"toc_count", runtime_.library_service()->toc_count(book_id)},
        });
    });

    // ── 当前书源登录（可选） ────────────────────────────────────
    bind_async("native_login", [&](const json&) {
        std::scoped_lock lock(core_mutex_);
        auto source = runtime_.source_manager()->current_source();
        const bool logged_in = source->login();
        return make_success({
            {"source_id", source->info().id},
            {"logged_in", logged_in},
            {"batch_enabled", source->capabilities().supports_batch
                                  && (!source->capabilities().supports_login || logged_in)},
        });
    });

    // ── 获取当前书源能力 ────────────────────────────────────────
    bind_async("native_get_source_capabilities", [&](const json&) {
        std::scoped_lock lock(core_mutex_);
        auto source = runtime_.source_manager()->current_source();
        auto payload = capabilities_to_json(*source);
        payload["source_id"] = source->info().id;
        return make_success(std::move(payload));
    });

    // ── 下载书籍 ────────────────────────────────────────────────
    // 通过事件推送下载进度（started / progress / finished）
    bind_async("native_download_book", [&](const json& args) {
        const auto payload = optional_object_arg(args, 0);
        if (!payload.is_object()) {
            throw ValidationError("book 参数必须是对象");
        }

        auto book = json_to_book(payload);
        if (book.book_id.empty()) {
            throw ValidationError("book.book_id 不能为空");
        }

        const std::string task_id = next_task_label(next_task_id_, "download");
        emit_event("task", make_task_event(task_id, "download", "started", book));

        std::scoped_lock lock(core_mutex_);
        const auto source = runtime_.source_manager()->current_source();
        const auto progress_stage = (source->capabilities().supports_batch
                                     && (!source->capabilities().supports_login || source->is_logged_in()))
                                         ? "batch_progress"
                                         : "chapter_progress";
        const auto toc = runtime_.library_service()->load_toc(book, false);
        runtime_.download_service()->download_book(
            book, toc,
            [&](int current, int total) {
                emit_event("task", make_task_event(task_id, "download", progress_stage, book, {
                    {"current", current},
                    {"total", total},
                }));
            });

        emit_event("task", make_task_event(task_id, "download", "finished", book, {
            {"total", static_cast<int>(toc.size())},
        }));

        return make_success({
            {"task_id", task_id},
            {"downloaded", static_cast<int>(toc.size())},
            {"cached_chapter_count", runtime_.library_service()->cached_chapter_count(book.book_id)},
        });
    });

    // ── 导出书籍 ────────────────────────────────────────────────
    // 支持 EPUB 和 TXT 两种格式，通过事件推送导出进度
    bind_async("native_export_book", [&](const json& args) {
        const auto payload = optional_object_arg(args, 0);
        if (!payload.is_object()) {
            throw ValidationError("book 参数必须是对象");
        }

        auto book = json_to_book(payload);
        if (book.book_id.empty()) {
            throw ValidationError("book.book_id 不能为空");
        }

        const int start = optional_int_arg(args, 1, 0);
        const int end = optional_int_arg(args, 2, start);
        const auto format = require_string_arg(args, 3, "format");
        const bool as_epub = format == "epub";
        if (!as_epub && format != "txt") {
            throw ValidationError("format 仅支持 epub 或 txt");
        }

        const std::string task_id = next_task_label(next_task_id_, "export");
        emit_event("task", make_task_event(task_id, "export", "started", book, {
            {"format", format},
        }));

        std::scoped_lock lock(core_mutex_);
        const auto toc = runtime_.library_service()->load_toc(book, false);
        const auto exports_dir = runtime_.paths().exports_dir;
        const auto output_path = runtime_.export_service()->export_book(
            book,
            toc,
            start,
            end,
            as_epub,
            path_to_utf8(exports_dir),
            [&](int current, int total) {
                emit_event("task", make_task_event(task_id, "export", "prepare", book, {
                    {"format", format},
                    {"current", current},
                    {"total", total},
                }));
            },
            [&](int current, int total) {
                emit_event("task", make_task_event(task_id, "export", "write", book, {
                    {"format", format},
                    {"current", current},
                    {"total", total},
                }));
            });

        emit_event("task", make_task_event(task_id, "export", "finished", book, {
            {"path", output_path},
            {"format", format},
        }));

        return make_success({
            {"task_id", task_id},
            {"path", output_path},
            {"exports_dir", path_to_utf8(exports_dir)},
        });
    });

    // ── 列出书架 ────────────────────────────────────────────────
    bind_async("native_list_bookshelf", [&](const json&) {
        std::scoped_lock lock(core_mutex_);
        json items = json::array();
        for (const auto& book : runtime_.library_service()->list_bookshelf()) {
            items.push_back(book_to_json(book));
        }
        return make_success({{"items", items}});
    });

    // ── 保存到书架 ──────────────────────────────────────────────
    bind_async("native_save_bookshelf", [&](const json& args) {
        const auto payload = optional_object_arg(args, 0);
        if (!payload.is_object()) {
            throw ValidationError("book 参数必须是对象");
        }

        std::scoped_lock lock(core_mutex_);
        runtime_.library_service()->save_to_bookshelf(json_to_book(payload));
        return make_success({{"saved", true}});
    });

    // ── 从书架移除 ──────────────────────────────────────────────
    bind_async("native_remove_bookshelf", [&](const json& args) {
        const auto book_id = require_string_arg(args, 0, "book_id");

        std::scoped_lock lock(core_mutex_);
        return make_success({
            {"removed", runtime_.library_service()->remove_from_bookshelf(book_id)},
        });
    });
}

/// 注册异步 JS 绑定
///
/// 将 WebView bind 回调转发到后台线程执行，
/// 自动捕获 ValidationError / SourceException / std::exception 并返回对应错误格式
void GuiBridge::bind_async(
    const std::string& name,
    std::function<json(const json&)> handler)
{
    window_.bind(name, [this, handler = std::move(handler), name](std::string call_id, std::string raw_args, void*) {
        std::thread([this, call_id = std::move(call_id), raw_args = std::move(raw_args), handler, name]() mutable {
            try {
                resolve_success(call_id, handler(parse_args(raw_args)));
            } catch (const ValidationError& e) {
                // 参数验证错误
                spdlog::warn("GUI validation error [{}]: {}", name, e.what());
                resolve_error(call_id, make_error("validation_error", "invalid_arguments", e.what()));
            } catch (const SourceException& e) {
                // 书源操作错误
                spdlog::error("GUI source error [{}]: {}", name, format_source_error_log(e.error()));
                resolve_error(call_id, make_error(
                    "source_error",
                    std::string(source_error_code_name(e.error().code)),
                    e.what(),
                    {
                        {"source_id", e.error().source_id},
                        {"operation", e.error().operation},
                        {"plugin_path", e.error().plugin_path},
                    }));
            } catch (const std::exception& e) {
                // 其他运行时错误
                spdlog::error("GUI runtime error [{}]: {}", name, e.what());
                resolve_error(call_id, make_error("runtime_error", "internal_error", e.what()));
            }
        }).detach();
    }, nullptr);
}

/// 向 JS 端返回成功结果（resolve code = 0）
void GuiBridge::resolve_success(const std::string& call_id, json payload)
{
    const auto raw_payload = payload.dump();
    window_.dispatch([this, call_id, raw_payload] {
        window_.resolve(call_id, 0, raw_payload);
    });
}

/// 向 JS 端返回错误结果（resolve code = 1）
void GuiBridge::resolve_error(const std::string& call_id, const json& payload)
{
    const auto raw_payload = payload.dump();
    window_.dispatch([this, call_id, raw_payload] {
        window_.resolve(call_id, 1, raw_payload);
    });
}

/// 向前端派发自定义事件
///
/// 等效于 JS：window.dispatchEvent(new CustomEvent("novel:name", { detail: payload }))
void GuiBridge::emit_event(const std::string& name, const json& payload)
{
    std::string script =
        "window.dispatchEvent(new CustomEvent(" + json("novel:" + name).dump() +
        ", { detail: " + payload.dump() + " }));";
    window_.dispatch([this, script = std::move(script)] {
        window_.eval(script);
    });
}

/// 解析 JS 端传入的参数字符串为 JSON 数组
GuiBridge::json GuiBridge::parse_args(const std::string& raw_args)
{
    auto parsed = json::parse(raw_args.empty() ? "[]" : raw_args);
    if (!parsed.is_array()) {
        throw ValidationError("参数必须是数组");
    }
    return parsed;
}

/// 构造标准成功响应
GuiBridge::json GuiBridge::make_success(json data)
{
    return {
        {"ok", true},
        {"data", std::move(data)},
    };
}

/// 构造标准错误响应
GuiBridge::json GuiBridge::make_error(
    std::string type,
    std::string code,
    std::string message,
    json details)
{
    return {
        {"ok", false},
        {"error", {
            {"type", std::move(type)},
            {"code", std::move(code)},
            {"message", std::move(message)},
            {"details", std::move(details)},
        }},
    };
}

/// 获取必需的字符串参数，缺失或为空时抛出 ValidationError
std::string GuiBridge::require_string_arg(
    const json& args,
    std::size_t index,
    const char* field_name)
{
    if (args.size() <= index || !args[index].is_string()) {
        throw ValidationError(std::string(field_name) + " 必须是字符串");
    }
    const auto value = args[index].get<std::string>();
    if (value.empty()) {
        throw ValidationError(std::string(field_name) + " 不能为空");
    }
    return value;
}

/// 获取可选整数参数，缺失或为 null 时返回默认值
int GuiBridge::optional_int_arg(
    const json& args,
    std::size_t index,
    int default_value)
{
    if (args.size() <= index || args[index].is_null()) {
        return default_value;
    }
    if (!args[index].is_number_integer()) {
        throw ValidationError("整数参数类型不正确");
    }
    return args[index].get<int>();
}

/// 获取可选布尔参数，缺失或为 null 时返回默认值
bool GuiBridge::optional_bool_arg(
    const json& args,
    std::size_t index,
    bool default_value)
{
    if (args.size() <= index || args[index].is_null()) {
        return default_value;
    }
    if (!args[index].is_boolean()) {
        throw ValidationError("布尔参数类型不正确");
    }
    return args[index].get<bool>();
}

/// 获取可选对象参数，缺失或为 null 时返回 nullptr
GuiBridge::json GuiBridge::optional_object_arg(const json& args, std::size_t index)
{
    if (args.size() <= index || args[index].is_null()) {
        return nullptr;
    }
    if (!args[index].is_object()) {
        throw ValidationError("对象参数类型不正确");
    }
    return args[index];
}

} // namespace novel
