#include "gui/backend/backend_bridge.h"

#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

#include "application/download_service.h"
#include "application/export_service.h"
#include "application/library_service.h"
#include "db/database.h"
#include "dotenv.h"
#include "logger.h"
#include "source/domain/source_errors.h"
#include "source/domain/source_types.h"
#include "source/host/host_api.h"
#include "source/host/http_service.h"
#include "source/runtime/source_manager.h"

namespace novel {

using nlohmann::json;

void to_json(json& j, const Book& book) {
    j = json{
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

void from_json(const json& j, Book& book) {
    book.book_id = j.value("book_id", "");
    book.title = j.value("title", "");
    book.author = j.value("author", "");
    book.cover_url = j.value("cover_url", "");
    book.abstract = j.value("abstract", "");
    book.category = j.value("category", "");
    book.word_count = j.value("word_count", "");
    book.score = j.value("score", 0.0);
    book.gender = j.value("gender", 0);
    book.creation_status = j.value("creation_status", 0);
    book.last_chapter_title = j.value("last_chapter_title", "");
    book.last_update_time = j.value("last_update_time", 0LL);
}

void to_json(json& j, const TocItem& item) {
    j = json{
        {"item_id", item.item_id},
        {"title", item.title},
        {"volume_name", item.volume_name},
        {"word_count", item.word_count},
        {"update_time", item.update_time},
    };
}

void from_json(const json& j, TocItem& item) {
    item.item_id = j.value("item_id", "");
    item.title = j.value("title", "");
    item.volume_name = j.value("volume_name", "");
    item.word_count = j.value("word_count", 0);
    item.update_time = j.value("update_time", 0LL);
}

void to_json(json& j, const SourceInfo& info) {
    j = json{
        {"id", info.id},
        {"name", info.name},
        {"version", info.version},
        {"author", info.author},
        {"description", info.description},
        {"required_envs", info.required_envs},
        {"optional_envs", info.optional_envs},
    };
}

} // namespace novel

namespace {

using nlohmann::json;

struct BackendContext {
    std::shared_ptr<novel::Database>        db;
    std::shared_ptr<novel::SourceManager>   source_manager;
    std::shared_ptr<novel::LibraryService>  library_service;
    std::shared_ptr<novel::DownloadService> download_service;
    std::shared_ptr<novel::ExportService>   export_service;
};

json success_response(json data) {
    return json{
        {"success", true},
        {"data", std::move(data)},
    };
}

json error_response(const std::string& message, std::string kind = "runtime") {
    return json{
        {"success", false},
        {"error", {
            {"kind", std::move(kind)},
            {"message", message},
        }},
    };
}

json error_response(const novel::SourceException& e) {
    const auto& error = e.error();
    return json{
        {"success", false},
        {"error", {
            {"kind", "source"},
            {"message", e.what()},
            {"code", std::string(novel::source_error_code_name(error.code))},
            {"source_id", error.source_id},
            {"operation", error.operation},
            {"plugin_path", error.plugin_path},
        }},
    };
}

BackendContext build_context(
    const std::string& db_path,
    const std::string& plugin_dir,
    const std::string& source_id,
    bool configure_source) {
    auto db = std::make_shared<novel::Database>(db_path);
    auto http_service = std::make_shared<novel::HttpService>();
    auto host_api = std::make_shared<novel::HostApi>(http_service);
    auto source_manager = std::make_shared<novel::SourceManager>(host_api);
    source_manager->load_from_directory(plugin_dir);

    if (!source_id.empty() && !source_manager->select_source(source_id)) {
        throw std::runtime_error("未找到书源: " + source_id);
    }

    if (configure_source) {
        source_manager->configure_current();
    }

    auto library_service = std::make_shared<novel::LibraryService>(source_manager, db);
    auto download_service =
        std::make_shared<novel::DownloadService>(source_manager, library_service);
    auto export_service = std::make_shared<novel::ExportService>(download_service);

    return BackendContext{
        db,
        source_manager,
        library_service,
        download_service,
        export_service,
    };
}

novel::Book parse_book_json(const std::string& text) {
    return json::parse(text).get<novel::Book>();
}

std::once_flag g_runtime_initialized;

} // namespace

namespace novel::gui {

void initialize_backend_runtime() {
    std::call_once(g_runtime_initialized, [] {
#ifdef _WIN32
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
#endif
        novel::init_logger("novel.log");
        dotenv::load(".env");
    });
}

BackendRequest request_from_json(const json& j) {
    BackendRequest request;
    request.db_path = j.value("db_path", request.db_path);
    request.output_dir = j.value("output_dir", request.output_dir);
    request.plugin_dir = j.value("plugin_dir", request.plugin_dir);
    request.source_id = j.value("source_id", "");
    request.command = j.value("command", "");
    request.keywords = j.value("keywords", "");
    request.page = j.value("page", 0);
    request.book_json = j.value("book_json", "");
    request.book_id = j.value("book_id", "");
    request.force_remote = j.value("force_remote", false);
    request.start = j.value("start", 0);
    request.end = j.value("end", -1);
    request.format = j.value("format", "epub");
    return request;
}

json request_to_json(const BackendRequest& request) {
    return json{
        {"db_path", request.db_path},
        {"output_dir", request.output_dir},
        {"plugin_dir", request.plugin_dir},
        {"source_id", request.source_id},
        {"command", request.command},
        {"keywords", request.keywords},
        {"page", request.page},
        {"book_json", request.book_json},
        {"book_id", request.book_id},
        {"force_remote", request.force_remote},
        {"start", request.start},
        {"end", request.end},
        {"format", request.format},
    };
}

json execute_backend_request(const BackendRequest& request) {
    initialize_backend_runtime();

    try {
        if (request.command == "sources") {
            auto ctx = build_context(request.db_path, request.plugin_dir, request.source_id, false);
            auto current = ctx.source_manager->current_info();
            return success_response({
                {"current_source_id", current ? current->id : ""},
                {"sources", ctx.source_manager->list_sources()},
            });
        }

        auto ctx = build_context(request.db_path, request.plugin_dir, request.source_id, true);
        auto current = ctx.source_manager->current_info();

        if (request.command == "search") {
            auto books = ctx.library_service->search_books(request.keywords, request.page);
            return success_response({
                {"source_id", current ? current->id : ""},
                {"source_name", current ? current->name : ""},
                {"books", books},
            });
        }

        if (request.command == "toc") {
            auto book = parse_book_json(request.book_json);
            auto toc = ctx.library_service->load_toc(book, request.force_remote);
            return success_response({
                {"book", book},
                {"toc", toc},
                {"toc_count", static_cast<int>(toc.size())},
                {"cached_chapter_count",
                 ctx.library_service->cached_chapter_count(book.book_id)},
            });
        }

        if (request.command == "download") {
            auto book = parse_book_json(request.book_json);
            auto toc = ctx.library_service->load_toc(book, request.force_remote);
            ctx.download_service->download_book(book, toc);
            return success_response({
                {"book", book},
                {"downloaded_count", static_cast<int>(toc.size())},
                {"total_count", static_cast<int>(toc.size())},
                {"cached_chapter_count",
                 ctx.library_service->cached_chapter_count(book.book_id)},
            });
        }

        if (request.command == "export") {
            auto book = parse_book_json(request.book_json);
            auto toc = ctx.library_service->load_toc(book, request.force_remote);
            if (toc.empty()) {
                throw std::runtime_error("目录为空，无法导出");
            }

            int resolved_end = request.end < 0 ? static_cast<int>(toc.size()) - 1 : request.end;
            auto exported_path = ctx.export_service->export_book(
                book,
                toc,
                request.start,
                resolved_end,
                request.format == "epub",
                request.output_dir);

            return success_response({
                {"book", book},
                {"format", request.format},
                {"output_path", exported_path},
                {"toc_count", static_cast<int>(toc.size())},
            });
        }

        if (request.command == "bookshelf.list") {
            auto books = ctx.library_service->list_bookshelf();
            return success_response({
                {"source_id", current ? current->id : ""},
                {"books", books},
            });
        }

        if (request.command == "bookshelf.save") {
            auto book = parse_book_json(request.book_json);
            ctx.library_service->save_to_bookshelf(book);
            return success_response({
                {"saved", true},
                {"book", book},
            });
        }

        if (request.command == "bookshelf.remove") {
            auto removed = ctx.library_service->remove_from_bookshelf(request.book_id);
            return success_response({
                {"removed", removed},
                {"book_id", request.book_id},
            });
        }

        return error_response("未识别的命令");
    } catch (const novel::SourceException& e) {
        return error_response(e);
    } catch (const std::exception& e) {
        return error_response(e.what());
    }
}

std::string execute_backend_request_json(const std::string& request_json) {
    try {
        auto request = request_from_json(json::parse(request_json));
        auto response = execute_backend_request(request);
        return response.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
    } catch (const std::exception& e) {
        return error_response(e.what(), "bridge")
            .dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
    }
}

} // namespace novel::gui
