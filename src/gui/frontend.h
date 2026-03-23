#pragma once

#include <filesystem>
#include <string>

#include <webview/webview.h>

namespace novel {

std::string to_file_url(const std::filesystem::path& path);
void navigate_frontend(
    webview::webview& window,
    const std::filesystem::path& frontend_dir);

} // namespace novel
