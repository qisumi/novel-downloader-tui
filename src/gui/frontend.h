/// @file frontend.h
/// @brief 前端页面导航
///
/// 提供将本地文件路径转换为 file:// URL 的工具函数，
/// 以及将 WebView 导航到前端入口页面的函数。
/// 支持开发模式（通过环境变量指向 dev server）和本地 file:// 模式。

#pragma once

#include <filesystem>
#include <string>

#include <webview/webview.h>

namespace novel {

/// 将本地文件路径转换为 file:// URL
///
/// 处理 Windows 盘符路径，对非 ASCII 字符进行百分号编码
std::string to_file_url(const std::filesystem::path& path);

/// 导航 WebView 到前端入口页面
///
/// 优先检查 NOVEL_GUI_DEV_SERVER 环境变量：
///   - 若已设置，导航到指定的 dev server URL
///   - 否则导航到 frontend_dir/index.html 的 file:// URL
void navigate_frontend(
    webview::webview& window,
    const std::filesystem::path& frontend_dir);

} // namespace novel
