/// @file user_paths.cpp
/// @brief GUI 运行时路径解析实现
///
/// 通过 Windows API 获取可执行文件路径和 LocalAppData 路径，
/// 并在多个候选位置中查找插件和前端资源目录。

#include "gui/user_paths.h"

#include <windows.h>
#include <shlobj.h>

#include <stdexcept>
#include <string>
#include <vector>

namespace novel {

namespace {

/// 获取当前可执行文件所在目录
static std::filesystem::path executable_directory()
{
    std::vector<wchar_t> buffer(MAX_PATH, L'\0');
    for (;;) {
        DWORD copied = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (copied == 0) {
            throw std::runtime_error("GetModuleFileNameW failed");
        }
        // 缓冲区足够大时 copied 不包含末尾 null，直接返回父路径
        if (copied < buffer.size() - 1) {
            return std::filesystem::path(std::wstring(buffer.data(), copied)).parent_path();
        }
        // 缓冲区不足，倍增后重试
        buffer.resize(buffer.size() * 2, L'\0');
    }
}

/// 获取 Windows LocalAppData 目录（如 C:\Users\xxx\AppData\Local）
static std::filesystem::path local_app_data()
{
    PWSTR raw = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &raw))) {
        throw std::runtime_error("Failed to resolve LocalAppData");
    }

    std::filesystem::path path(raw);
    CoTaskMemFree(raw);
    return path;
}

/// 从候选路径列表中返回第一个存在的路径，若都不存在则返回空路径
static std::filesystem::path first_existing_path(const std::vector<std::filesystem::path>& candidates)
{
    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
    }
    return {};
}

} // namespace

/// 解析 GUI 运行时所有路径
///
/// 路径解析策略：
///   - 可执行文件目录：通过 GetModuleFileNameW 获取
///   - 应用数据根目录：%LocalAppData%/NovelDownloader
///   - 插件目录：优先查找 exe 同级 plugins/，其次查找工作目录 plugins/
///   - 前端目录：依次查找工作目录 src/gui/frontend/、gui/、exe 同级 gui/
///   - 数据库文件：exe 同级 novel.db
///   - 日志文件：工作目录下 novel-gui.log
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

    // 按优先级查找插件目录
    paths.plugin_dir = first_existing_path({
        paths.executable_dir / "plugins",
        paths.run_dir / "plugins",
    });

    // 按优先级查找前端资源目录
    paths.frontend_dir = first_existing_path({
        paths.run_dir / "src" / "gui" / "frontend",
        paths.run_dir / "gui",
        paths.executable_dir / "gui",
    });

    // 若未找到，使用默认路径（后续逻辑会报错提示）
    if (paths.plugin_dir.empty()) {
        paths.plugin_dir = paths.executable_dir / "plugins";
    }
    if (paths.frontend_dir.empty()) {
        paths.frontend_dir = paths.run_dir / "src" / "gui" / "frontend";
    }

    // 确保必要目录存在
    fs::create_directories(paths.logs_dir);
    fs::create_directories(paths.data_dir);
    fs::create_directories(paths.exports_dir);
    fs::create_directories(paths.webview_dir);

    return paths;
}

} // namespace novel
