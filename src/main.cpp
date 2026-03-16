#include <iostream>
#include <memory>
#include <string>

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
#include "source/host/host_api.h"
#include "source/host/http_service.h"
#include "source/runtime/source_manager.h"
#include "ui/app.h"

static std::string prompt_apikey() {
    using namespace ftxui;

    std::string input_val;
    bool confirmed = false;

    auto screen = ScreenInteractive::Fullscreen();

    InputOption opt;
    opt.password = false;
    opt.on_enter = [&] {
        confirmed = true;
        screen.ExitLoopClosure()();
    };

    auto input_box = Input(&input_val, "在此粘贴或输入 API Key …", opt);
    auto btn_ok = Button("  确认  ", [&] {
        confirmed = true;
        screen.ExitLoopClosure()();
    }, ButtonOption::Ascii());
    auto btn_exit = Button("  退出  ", [&] {
        screen.ExitLoopClosure()();
    }, ButtonOption::Ascii());

    auto buttons = Container::Horizontal({btn_ok, btn_exit});
    auto layout = Container::Vertical({input_box, buttons});

    auto renderer = Renderer(layout, [&] {
        return vbox({
            text(" 🍅 番茄小说 TUI 下载器") | bold | color(Color::Red),
            separator(),
            text(""),
            text(" 当前书源需要 API Key，请通过以下任意方式提供：") | color(Color::Yellow),
            text("   • 命令行：-k <key>  或  --apikey <key>"),
            text("   • 系统环境变量：FANQIE_APIKEY=<key>"),
            text("   • .env 文件：FANQIE_APIKEY=<key>"),
            text("   • 在下方直接输入（仅本次运行有效）"),
            text(""),
            separator(),
            hbox({text(" API Key: "), input_box->Render() | flex}) | border,
            separator(),
            hbox({filler(), buttons->Render(), filler()}),
            text(""),
        }) | size(WIDTH, EQUAL, 72);
    });

    auto event_handler = CatchEvent(renderer, [&](Event ev) {
        if (ev == Event::Escape) {
            screen.ExitLoopClosure()();
            return true;
        }
        return false;
    });

    screen.Loop(event_handler);
    return confirmed ? input_val : "";
}

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

    fanqie::init_logger("fanqie.log");
    spdlog::info("Application starting");

    dotenv::load(".env");

    std::string api_key;
    std::string db_path = "fanqie.db";
    std::string epub_dir = ".";
    std::string plugin_dir = "plugins";
    std::string source_id;
    bool list_sources = false;

    CLI::App app{"番茄小说 TUI 下载器"};
    app.add_option("-k,--apikey", api_key,
                   "API 密钥 [可通过 FANQIE_APIKEY 环境变量或 .env 文件设置]")
        ->envname("FANQIE_APIKEY");
    app.add_option("--db", db_path,
                   "SQLite 数据库路径 (可通过 FANQIE_DB 环境变量设置)")
        ->default_str(db_path)
        ->envname("FANQIE_DB");
    app.add_option("-o,--epub-dir", epub_dir,
                   "EPUB 输出目录 (可通过 FANQIE_EPUB_DIR 环境变量设置)")
        ->default_str(epub_dir)
        ->envname("FANQIE_EPUB_DIR");
    app.add_option("--plugin-dir", plugin_dir,
                   "Lua 插件目录 (可通过 FANQIE_PLUGIN_DIR 环境变量设置)")
        ->default_str(plugin_dir)
        ->envname("FANQIE_PLUGIN_DIR");
    app.add_option("--source", source_id,
                   "默认书源 ID (可通过 FANQIE_SOURCE 环境变量设置)")
        ->envname("FANQIE_SOURCE");
    app.add_flag("--list-sources", list_sources, "列出可用书源并退出");

    CLI11_PARSE(app, argc, argv);

    try {
        auto db = std::make_shared<fanqie::Database>(db_path);
        auto http_service = std::make_shared<fanqie::HttpService>();
        auto host_api = std::make_shared<fanqie::HostApi>(http_service);
        auto source_manager = std::make_shared<fanqie::SourceManager>(host_api);
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
            }
            return 0;
        }

        auto current_info = source_manager->current_info();
        if (current_info && current_info->requires_api_key && api_key.empty()) {
            api_key = prompt_apikey();
            if (api_key.empty()) {
                std::cerr << "未提供 API 密钥，程序退出。\n";
                return 1;
            }
        }

        source_manager->configure_current({api_key});

        auto library_service = std::make_shared<fanqie::LibraryService>(source_manager, db);
        auto download_service =
            std::make_shared<fanqie::DownloadService>(source_manager, library_service);
        auto export_service = std::make_shared<fanqie::ExportService>(download_service);

        auto ctx = std::make_shared<fanqie::AppContext>();
        ctx->source_manager = source_manager;
        ctx->library_service = library_service;
        ctx->download_service = download_service;
        ctx->export_service = export_service;
        ctx->db = db;
        ctx->api_key = api_key;
        ctx->plugin_dir = plugin_dir;
        ctx->epub_output_dir = epub_dir;
        if (current_info) {
            ctx->current_source_id = current_info->id;
            ctx->current_source_name = current_info->name;
        }

        return fanqie::run_app(ctx);
    } catch (const std::exception& e) {
        std::cerr << "初始化失败: " << e.what() << "\n";
        return 1;
    }
}
