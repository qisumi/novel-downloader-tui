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

#include "api/fanqie_client.h"
#include "db/database.h"
#include "dotenv.h"
#include "logger.h"
#include "ui/app.h"

// ── API Key 输入对话框 ────────────────────────────────────────────────────────
// 当未通过任何外部方式提供 API Key 时，通过 TUI 交互让用户输入。
// 返回用户输入的字符串；若用户取消（按 ESC / Ctrl+C）则返回空字符串。
static std::string prompt_apikey() {
    using namespace ftxui;

    std::string input_val;
    bool        confirmed = false;

    auto screen = ScreenInteractive::Fullscreen();

    InputOption opt;
    opt.password = false;
    opt.on_enter = [&] { confirmed = true; screen.ExitLoopClosure()(); };

    auto input_box = Input(&input_val, "在此粘贴或输入 API Key …", opt);

    auto btn_ok = Button("  确认  ", [&] {
        confirmed = true;
        screen.ExitLoopClosure()();
    }, ButtonOption::Ascii());

    auto btn_exit = Button("  退出  ", [&] {
        screen.ExitLoopClosure()();
    }, ButtonOption::Ascii());

    auto buttons = Container::Horizontal({btn_ok, btn_exit});
    auto layout  = Container::Vertical({input_box, buttons});

    auto renderer = Renderer(layout, [&] {
        return vbox({
            text(" 🍅 番茄小说 TUI 下载器") | bold | color(Color::Red),
            separator(),
            text(""),
            text(" 未检测到 API Key，请通过以下任意方式提供：") | color(Color::Yellow),
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


int main(int argc, char *argv[]) {
#ifdef _WIN32
  // 启用 UTF-8 控制台输出
  SetConsoleOutputCP(CP_UTF8);
  SetConsoleCP(CP_UTF8);
  // 启用虚拟终端（ANSI 序列）
  HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
  DWORD mode = 0;
  GetConsoleMode(hOut, &mode);
  SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#endif

  // ── 初始化日志（最先执行，方便后续所有代码都能记录日志）─────
  fanqie::init_logger("fanqie.log");
  spdlog::info("Application starting");

  // ── 加载 .env 文件（最低优先级）────────────────────────────
  // 仅对系统环境变量中不存在的 key 生效，保证：
  //   命令行参数 > 系统环境变量 > .env 文件
  dotenv::load(".env"); // 文件不存在时静默忽略

  // ── 命令行参数解析（CLI11）────────────────────────────────
  std::string api_key;
  std::string db_path  = "fanqie.db";
  std::string epub_dir = ".";

  CLI::App app{"番茄小说 TUI 下载器"};
  app.add_option("-k,--apikey", api_key,
                 "API 密钥 [必填，可通过 FANQIE_APIKEY 环境变量或 .env 文件设置]")
      ->envname("FANQIE_APIKEY");
  app.add_option("--db", db_path,
                 "SQLite 数据库路径 (可通过 FANQIE_DB 环境变量设置)")
      ->default_str(db_path)
      ->envname("FANQIE_DB");
  app.add_option("-o,--epub-dir", epub_dir,
                 "EPUB 输出目录 (可通过 FANQIE_EPUB_DIR 环境变量设置)")
      ->default_str(epub_dir)
      ->envname("FANQIE_EPUB_DIR");

  CLI11_PARSE(app, argc, argv);

  if (api_key.empty()) {
    api_key = prompt_apikey();
    if (api_key.empty()) {
      std::cerr << "未提供 API 密钥，程序退出。\n";
      return 1;
    }
  }

  // ── 初始化上下文 ──────────────────────────────────────────
  auto ctx = std::make_shared<fanqie::AppContext>();
  ctx->api_key = api_key;
  ctx->epub_output_dir = epub_dir;

  try {
    ctx->client = std::make_shared<fanqie::FanqieClient>(api_key);
    ctx->db = std::make_shared<fanqie::Database>(db_path);
  } catch (const std::exception &e) {
    std::cerr << "初始化失败: " << e.what() << "\n";
    return 1;
  }

  // ── 启动 TUI ──────────────────────────────────────────────
  return fanqie::run_app(ctx);
}
