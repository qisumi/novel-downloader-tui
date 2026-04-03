/// @file user_paths.h
/// @brief GUI 运行时路径解析
///
/// 定义所有运行时路径的结构体（GuiPaths），并提供路径解析函数。
/// 路径包括：可执行文件目录、应用数据根目录、日志/数据/导出目录、
/// 数据库路径、插件目录和前端资源目录。

#pragma once

#include <filesystem>

namespace novel {

/// GUI 运行时所有关键路径
struct GuiPaths {
    std::filesystem::path executable_dir;   ///< 可执行文件所在目录
    std::filesystem::path run_dir;          ///< 当前工作目录
    std::filesystem::path app_root;         ///< 应用数据根目录（%LocalAppData%/NovelDownloader）
    std::filesystem::path logs_dir;         ///< 日志文件目录
    std::filesystem::path data_dir;         ///< 应用数据目录
    std::filesystem::path exports_dir;      ///< 导出文件输出目录
    std::filesystem::path webview_dir;      ///< WebView 缓存目录
    std::filesystem::path db_path;          ///< SQLite 数据库文件路径
    std::filesystem::path log_path;         ///< 日志文件路径
    std::filesystem::path plugin_dir;       ///< JS 插件目录
    std::filesystem::path frontend_dir;     ///< 前端静态资源目录
};

/// 解析并返回所有 GUI 运行时路径
///
/// 自动在多个候选位置中查找插件和前端资源目录，
/// 兼容源码运行、输出目录运行和打包运行三种场景。
GuiPaths resolve_gui_paths();

} // namespace novel
