#include "gui/user_paths.h"

#include <windows.h>
#include <shlobj.h>

#include <stdexcept>
#include <string>
#include <vector>

namespace novel {

namespace {

std::filesystem::path executable_directory()
{
    std::vector<wchar_t> buffer(MAX_PATH, L'\0');
    for (;;) {
        DWORD copied = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (copied == 0) {
            throw std::runtime_error("GetModuleFileNameW failed");
        }
        if (copied < buffer.size() - 1) {
            return std::filesystem::path(std::wstring(buffer.data(), copied)).parent_path();
        }
        buffer.resize(buffer.size() * 2, L'\0');
    }
}

std::filesystem::path local_app_data()
{
    PWSTR raw = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &raw))) {
        throw std::runtime_error("Failed to resolve LocalAppData");
    }

    std::filesystem::path path(raw);
    CoTaskMemFree(raw);
    return path;
}

std::filesystem::path first_existing_path(const std::vector<std::filesystem::path>& candidates)
{
    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
    }
    return {};
}

} // namespace

GuiPaths resolve_gui_paths()
{
    namespace fs = std::filesystem;

    GuiPaths paths;
    paths.executable_dir = executable_directory();
    paths.run_dir = fs::current_path();
    paths.app_root = local_app_data() / "NovelDownloader";
    paths.logs_dir = paths.app_root / "logs";
    paths.data_dir = paths.app_root / "data";
    paths.exports_dir = paths.app_root / "exports";
    paths.webview_dir = paths.app_root / "webview";
    paths.db_path = paths.executable_dir / "novel.db";
    paths.log_path = paths.run_dir / "novel-gui.log";

    paths.plugin_dir = first_existing_path({
        paths.executable_dir / "plugins",
        paths.run_dir / "plugins",
    });
    paths.frontend_dir = first_existing_path({
        paths.run_dir / "src" / "gui" / "frontend",
        paths.run_dir / "gui",
        paths.executable_dir / "gui",
    });

    if (paths.plugin_dir.empty()) {
        paths.plugin_dir = paths.executable_dir / "plugins";
    }
    if (paths.frontend_dir.empty()) {
        paths.frontend_dir = paths.run_dir / "src" / "gui" / "frontend";
    }

    fs::create_directories(paths.logs_dir);
    fs::create_directories(paths.data_dir);
    fs::create_directories(paths.exports_dir);
    fs::create_directories(paths.webview_dir);

    return paths;
}

} // namespace novel
