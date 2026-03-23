#include "gui/bridge.h"

#include <sstream>
#include <string_view>
#include <thread>

#include <spdlog/spdlog.h>

namespace novel {

namespace {

using json = nlohmann::json;

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

json book_to_json(const Book& book)
{
    return {
        {"book_id", book.book_id},
        {"title", book.title},
        {"author", book.author},
        {"cover_url", book.cover_url},
        {"abstract", book.abstract},
        {"category", book.category},
        {"word_count", book.word_count},
        {"score", book.score},
        {"gender", book.gender},
        {"creation_status", book.creation_status},
        {"last_chapter_title", book.last_chapter_title},
        {"last_update_time", book.last_update_time},
    };
}

json toc_to_json(const TocItem& item, bool cached)
{
    return {
        {"item_id", item.item_id},
        {"title", item.title},
        {"volume_name", item.volume_name},
        {"word_count", item.word_count},
        {"update_time", item.update_time},
        {"cached", cached},
    };
}

Book json_to_book(const json& payload)
{
    Book book;
    book.book_id = payload.value("book_id", "");
    book.title = payload.value("title", "");
    book.author = payload.value("author", "");
    book.cover_url = payload.value("cover_url", "");
    book.abstract = payload.value("abstract", "");
    book.category = payload.value("category", "");
    book.word_count = payload.value("word_count", "");
    book.score = payload.value("score", 0.0);
    book.gender = payload.value("gender", 0);
    book.creation_status = payload.value("creation_status", 0);
    book.last_chapter_title = payload.value("last_chapter_title", "");
    book.last_update_time = payload.value("last_update_time", static_cast<std::int64_t>(0));
    return book;
}

std::string next_task_label(std::atomic_uint64_t& counter, std::string_view prefix)
{
    std::ostringstream oss;
    oss << prefix << "-" << counter.fetch_add(1);
    return oss.str();
}

} // namespace

GuiBridge::GuiBridge(webview::webview& window, GuiAppRuntime& runtime)
    : window_(window), runtime_(runtime) {}

void GuiBridge::install()
{
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
  download_book: (...args) => window.__NOVEL_BRIDGE__.wrap("native_download_book")(...args),
  export_book: (...args) => window.__NOVEL_BRIDGE__.wrap("native_export_book")(...args),
  list_bookshelf: (...args) => window.__NOVEL_BRIDGE__.wrap("native_list_bookshelf")(...args),
  save_bookshelf: (...args) => window.__NOVEL_BRIDGE__.wrap("native_save_bookshelf")(...args),
  remove_bookshelf: (...args) => window.__NOVEL_BRIDGE__.wrap("native_remove_bookshelf")(...args),
};
)JS");

    bind_async("native_get_sources", [&](const json&) {
        std::scoped_lock lock(core_mutex_);
        const auto current = runtime_.source_manager()->current_info();
        const auto sources = runtime_.source_manager()->list_sources();
        spdlog::info("native_get_sources called. source_count={}, current_source='{}'",
                     sources.size(), current ? current->id : "");
        json items = json::array();
        for (const auto& source : sources) {
            items.push_back(source_to_json(source, current && current->id == source.id));
        }
        return make_success({
            {"sources", items},
            {"current_source_id", current ? current->id : ""},
        });
    });

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
        emit_event("task", {
            {"task_id", task_id},
            {"kind", "download"},
            {"stage", "started"},
            {"book_id", book.book_id},
            {"title", book.title},
        });

        std::scoped_lock lock(core_mutex_);
        const auto toc = runtime_.library_service()->load_toc(book, false);
        runtime_.download_service()->download_book(
            book, toc,
            [&](int current, int total) {
                emit_event("task", {
                    {"task_id", task_id},
                    {"kind", "download"},
                    {"stage", "progress"},
                    {"book_id", book.book_id},
                    {"current", current},
                    {"total", total},
                });
            });

        emit_event("task", {
            {"task_id", task_id},
            {"kind", "download"},
            {"stage", "finished"},
            {"book_id", book.book_id},
            {"total", static_cast<int>(toc.size())},
        });

        return make_success({
            {"task_id", task_id},
            {"downloaded", static_cast<int>(toc.size())},
            {"cached_chapter_count", runtime_.library_service()->cached_chapter_count(book.book_id)},
        });
    });

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
        emit_event("task", {
            {"task_id", task_id},
            {"kind", "export"},
            {"stage", "started"},
            {"book_id", book.book_id},
            {"format", format},
        });

        std::scoped_lock lock(core_mutex_);
        const auto toc = runtime_.library_service()->load_toc(book, false);
        const auto output_path = runtime_.export_service()->export_book(
            book,
            toc,
            start,
            end,
            as_epub,
            runtime_.paths().exports_dir.string(),
            [&](int current, int total) {
                emit_event("task", {
                    {"task_id", task_id},
                    {"kind", "export"},
                    {"stage", "prepare"},
                    {"current", current},
                    {"total", total},
                });
            },
            [&](int current, int total) {
                emit_event("task", {
                    {"task_id", task_id},
                    {"kind", "export"},
                    {"stage", "write"},
                    {"current", current},
                    {"total", total},
                });
            });

        emit_event("task", {
            {"task_id", task_id},
            {"kind", "export"},
            {"stage", "finished"},
            {"path", output_path},
            {"format", format},
        });

        return make_success({
            {"task_id", task_id},
            {"path", output_path},
            {"exports_dir", runtime_.paths().exports_dir.string()},
        });
    });

    bind_async("native_list_bookshelf", [&](const json&) {
        std::scoped_lock lock(core_mutex_);
        json items = json::array();
        for (const auto& book : runtime_.library_service()->list_bookshelf()) {
            items.push_back(book_to_json(book));
        }
        return make_success({{"items", items}});
    });

    bind_async("native_save_bookshelf", [&](const json& args) {
        const auto payload = optional_object_arg(args, 0);
        if (!payload.is_object()) {
            throw ValidationError("book 参数必须是对象");
        }

        std::scoped_lock lock(core_mutex_);
        runtime_.library_service()->save_to_bookshelf(json_to_book(payload));
        return make_success({{"saved", true}});
    });

    bind_async("native_remove_bookshelf", [&](const json& args) {
        const auto book_id = require_string_arg(args, 0, "book_id");

        std::scoped_lock lock(core_mutex_);
        return make_success({
            {"removed", runtime_.library_service()->remove_from_bookshelf(book_id)},
        });
    });
}

void GuiBridge::bind_async(
    const std::string& name,
    std::function<json(const json&)> handler)
{
    window_.bind(name, [this, handler = std::move(handler), name](std::string call_id, std::string raw_args, void*) {
        std::thread([this, call_id = std::move(call_id), raw_args = std::move(raw_args), handler, name]() mutable {
            try {
                resolve_success(call_id, handler(parse_args(raw_args)));
            } catch (const ValidationError& e) {
                spdlog::warn("GUI validation error [{}]: {}", name, e.what());
                resolve_error(call_id, make_error("validation_error", "invalid_arguments", e.what()));
            } catch (const SourceException& e) {
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
                spdlog::error("GUI runtime error [{}]: {}", name, e.what());
                resolve_error(call_id, make_error("runtime_error", "internal_error", e.what()));
            }
        }).detach();
    }, nullptr);
}

void GuiBridge::resolve_success(const std::string& call_id, json payload)
{
    window_.resolve(call_id, 0, payload.dump());
}

void GuiBridge::resolve_error(const std::string& call_id, const json& payload)
{
    window_.resolve(call_id, 1, payload.dump());
}

void GuiBridge::emit_event(const std::string& name, const json& payload)
{
    std::string script =
        "window.dispatchEvent(new CustomEvent(" + json("novel:" + name).dump() +
        ", { detail: " + payload.dump() + " }));";
    window_.eval(script);
}

GuiBridge::json GuiBridge::parse_args(const std::string& raw_args)
{
    auto parsed = json::parse(raw_args.empty() ? "[]" : raw_args);
    if (!parsed.is_array()) {
        throw ValidationError("参数必须是数组");
    }
    return parsed;
}

GuiBridge::json GuiBridge::make_success(json data)
{
    return {
        {"ok", true},
        {"data", std::move(data)},
    };
}

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
