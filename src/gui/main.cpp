#include <windows.h>

#include <cstdlib>
#include <exception>
#include <string>

#include <spdlog/spdlog.h>
#include <webview/webview.h>

#include "dotenv.h"
#include "logger.h"

namespace novel {

namespace {

std::string build_shell_html()
{
    return R"HTML(
<!doctype html>
<html lang="zh-CN">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Novel Downloader</title>
  <style>
    :root {
      color-scheme: light;
      font-family: "Segoe UI", "Microsoft YaHei UI", sans-serif;
      background: #f6f1e8;
      color: #1f2937;
    }
    * {
      box-sizing: border-box;
    }
    body {
      margin: 0;
      min-height: 100vh;
      background:
        radial-gradient(circle at top left, #fff7e8 0%, transparent 35%),
        linear-gradient(135deg, #f7f3ea 0%, #efe7d8 100%);
      display: grid;
      place-items: center;
    }
    main {
      width: min(760px, calc(100vw - 48px));
      padding: 32px;
      border-radius: 24px;
      background: rgba(255, 255, 255, 0.82);
      border: 1px solid rgba(148, 163, 184, 0.22);
      box-shadow: 0 18px 60px rgba(15, 23, 42, 0.12);
      backdrop-filter: blur(12px);
    }
    h1 {
      margin: 0 0 12px;
      font-size: clamp(32px, 5vw, 48px);
      line-height: 1.05;
    }
    p {
      margin: 0;
      font-size: 16px;
      line-height: 1.7;
      color: #475569;
    }
    .tag {
      display: inline-flex;
      margin-bottom: 16px;
      padding: 6px 12px;
      border-radius: 999px;
      background: #111827;
      color: #fff;
      font-size: 12px;
      letter-spacing: 0.08em;
      text-transform: uppercase;
    }
  </style>
</head>
<body>
  <main>
    <div class="tag">WebView Host Ready</div>
    <h1>小说下载器 GUI 壳已接入</h1>
    <p>
      CMake 和 vcpkg 已经调整为可构建 Windows WebView GUI 宿主。
      下一步可以把搜索、书架、下载和导出桥接到这里。
    </p>
  </main>
</body>
</html>
)HTML";
}

} // namespace

} // namespace novel

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    dotenv::load(".env");
    novel::init_logger("novel.log");

    try {
        webview::webview window(
#ifdef NDEBUG
            false,
#else
            true,
#endif
            nullptr);

        window.set_title("Novel Downloader");
        window.set_size(1200, 820, WEBVIEW_HINT_NONE);
        window.set_html(novel::build_shell_html());
        window.run();
    } catch (const webview::exception& e) {
        spdlog::error("Failed to start WebView host: {}", e.what());
        MessageBoxA(nullptr, e.what(), "Novel Downloader", MB_OK | MB_ICONERROR);
        return EXIT_FAILURE;
    } catch (const std::exception& e) {
        spdlog::error("Unhandled GUI exception: {}", e.what());
        MessageBoxA(nullptr, e.what(), "Novel Downloader", MB_OK | MB_ICONERROR);
        return EXIT_FAILURE;
    } catch (...) {
        spdlog::error("Unhandled non-standard GUI exception");
        MessageBoxA(nullptr, "Unknown startup error.", "Novel Downloader", MB_OK | MB_ICONERROR);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
