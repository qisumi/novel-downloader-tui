#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <CLI/CLI.hpp>

#ifdef _WIN32
#include <windows.h>
#endif

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include "application/download_service.h"
#include "application/export_service.h"
#include "application/library_service.h"
#include "db/database.h"
#include "dotenv.h"
#include "logger.h"
#include "source/domain/source_errors.h"
#include "source/host/host_api.h"
#include "source/host/http_service.h"
#include "source/runtime/source_manager.h"
#include "tui/app.h"

namespace {

std::string join_list(const std::vector<std::string>& items) {
    std::string result;
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (i > 0) {
            result += ", ";
        }
        result += items[i];
    }
    return result;
}

} // namespace

int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD outMode = 0;
    GetConsoleMode(hOut, &outMode);
    SetConsoleMode(hOut, outMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    DWORD inMode = 0;
    GetConsoleMode(hIn, &inMode);
    DWORD newInMode = inMode;
    newInMode |= ENABLE_EXTENDED_FLAGS;
    newInMode |= ENABLE_MOUSE_INPUT;
#ifdef ENABLE_VIRTUAL_TERMINAL_INPUT
    newInMode |= ENABLE_VIRTUAL_TERMINAL_INPUT;
#endif
#ifdef ENABLE_QUICK_EDIT_MODE
    newInMode &= ~ENABLE_QUICK_EDIT_MODE;
#endif
    SetConsoleMode(hIn, newInMode);
#endif

    novel::init_logger("novel.log");
    spdlog::info("Application starting");

    dotenv::load(".env");

    std::string db_path = "novel.db";
    std::string epub_dir = ".";
    std::string plugin_dir = "plugins";
    std::string source_id;
    bool list_sources = false;

    CLI::App app{"小说下载 TUI 工具"};
    app.add_option("--db", db_path,
                   "SQLite 数据库路径 (可通过 NOVEL_DB 环境变量设置)")
        ->default_str(db_path)
        ->envname("NOVEL_DB");
    app.add_option("-o,--epub-dir", epub_dir,
                   "EPUB 输出目录 (可通过 NOVEL_EPUB_DIR 环境变量设置)")
        ->default_str(epub_dir)
        ->envname("NOVEL_EPUB_DIR");
    app.add_option("--plugin-dir", plugin_dir,
                   "Lua 插件目录 (可通过 NOVEL_PLUGIN_DIR 环境变量设置)")
        ->default_str(plugin_dir)
        ->envname("NOVEL_PLUGIN_DIR");
    app.add_option("--source", source_id,
                   "默认书源 ID (可通过 NOVEL_SOURCE 环境变量设置)")
        ->envname("NOVEL_SOURCE");
    app.add_flag("--list-sources", list_sources, "列出可用书源并退出");

    CLI11_PARSE(app, argc, argv);

    try {
        auto db = std::make_shared<novel::Database>(db_path);
        auto http_service = std::make_shared<novel::HttpService>();
        auto host_api = std::make_shared<novel::HostApi>(http_service);
        auto source_manager = std::make_shared<novel::SourceManager>(host_api);
        source_manager->load_from_directory(plugin_dir);

        if (!source_id.empty() && !source_manager->select_source(source_id)) {
            std::cerr << "未找到书源: " << source_id << "\n";
            return 1;
        }

        if (list_sources) {
            for (const auto& info : source_manager->list_sources()) {
                std::cout << info.id << " - " << info.name;
                if (!info.version.empty()) {
                    std::cout << " (" << info.version << ")";
                }
                std::cout << "\n";
                if (!info.required_envs.empty()) {
                    std::cout << "  required_envs: " << join_list(info.required_envs) << "\n";
                }
                if (!info.optional_envs.empty()) {
                    std::cout << "  optional_envs: " << join_list(info.optional_envs) << "\n";
                }
            }
            return 0;
        }

        source_manager->configure_current();

        auto library_service = std::make_shared<novel::LibraryService>(source_manager, db);
        auto download_service =
            std::make_shared<novel::DownloadService>(source_manager, library_service);
        auto export_service = std::make_shared<novel::ExportService>(download_service);

        auto ctx = std::make_shared<novel::AppContext>();
        ctx->source_manager = source_manager;
        ctx->library_service = library_service;
        ctx->download_service = download_service;
        ctx->export_service = export_service;
        ctx->db = db;
        ctx->plugin_dir = plugin_dir;
        ctx->epub_output_dir = epub_dir;
        auto current_info = source_manager->current_info();
        if (current_info) {
            ctx->current_source_id = current_info->id;
            ctx->current_source_name = current_info->name;
        }

        return novel::run_app(ctx);
    } catch (const novel::SourceException& e) {
        spdlog::error("Initialization failed: {}", novel::format_source_error_log(e.error()));
        std::cerr << "初始化失败: " << e.what() << "\n";
        return 1;
    } catch (const std::exception& e) {
        spdlog::error("Initialization failed: {}", e.what());
        std::cerr << "初始化失败: " << e.what() << "\n";
        return 1;
    }
}
