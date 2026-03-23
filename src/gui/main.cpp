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
        novel::GuiAppRuntime runtime;
        runtime.initialize();

        webview::webview window(
#ifdef NDEBUG
            false,
#else
            true,
#endif
            nullptr);

        novel::GuiBridge bridge(window, runtime);

        window.set_title("Novel Downloader GUI");
        window.set_size(1320, 900, WEBVIEW_HINT_NONE);
        bridge.install();
        runtime.navigate_frontend(window);
        window.run();
    } catch (const webview::exception& e) {
        spdlog::error("Failed to start WebView host: {}", e.what());
        MessageBoxA(nullptr, e.what(), "Novel Downloader", MB_OK | MB_ICONERROR);
        return EXIT_FAILURE;
    } catch (const novel::SourceException& e) {
        spdlog::error("Failed to initialize GUI runtime: {}", novel::format_source_error_log(e.error()));
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
