/// @file main.cpp
/// @brief GUI 应用程序入口
///
/// 创建 WebView 窗口，初始化运行时（加载插件、数据库、服务），
/// 安装 JS 桥接 API，导航到前端页面后进入消息循环。
/// 异常通过 MessageBox 弹窗提示用户。

#include <windows.h>

#include <cstdlib>
#include <exception>

#include <spdlog/spdlog.h>
#include <webview/webview.h>

#include "gui/app_runtime.h"
#include "gui/bridge.h"
#include "source/domain/source_errors.h"

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    try {
        // 创建 WebView 实例，Debug 模式启用开发者工具
        webview::webview window(
#ifdef NDEBUG
            false,
#else
            true,
#endif
            nullptr);

        // 初始化应用运行时（路径、日志、数据库、书源、服务）
        novel::GuiAppRuntime runtime;
        runtime.initialize(window);

        // 创建并安装 C++ <-> JS 桥接 API
        novel::GuiBridge bridge(window, runtime);

        window.set_title("Novel Downloader GUI");
        window.set_size(1280, 720, WEBVIEW_HINT_NONE);
        bridge.install();
        runtime.navigate_frontend(window);
        window.run();
    } catch (const webview::exception& e) {
        // WebView 初始化失败
        spdlog::error("Failed to start WebView host: {}", e.what());
        MessageBoxA(nullptr, e.what(), "Novel Downloader", MB_OK | MB_ICONERROR);
        return EXIT_FAILURE;
    } catch (const novel::SourceException& e) {
        // 书源初始化失败（如插件加载异常）
        spdlog::error("Failed to initialize GUI runtime: {}", novel::format_source_error_log(e.error()));
        MessageBoxA(nullptr, e.what(), "Novel Downloader", MB_OK | MB_ICONERROR);
        return EXIT_FAILURE;
    } catch (const std::exception& e) {
        // 其他标准异常
        spdlog::error("Unhandled GUI exception: {}", e.what());
        MessageBoxA(nullptr, e.what(), "Novel Downloader", MB_OK | MB_ICONERROR);
        return EXIT_FAILURE;
    } catch (...) {
        // 未知异常
        spdlog::error("Unhandled non-standard GUI exception");
        MessageBoxA(nullptr, "Unknown startup error.", "Novel Downloader", MB_OK | MB_ICONERROR);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
