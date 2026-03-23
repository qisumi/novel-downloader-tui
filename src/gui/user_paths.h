#pragma once

#include <filesystem>

namespace novel {

struct GuiPaths {
    std::filesystem::path executable_dir;
    std::filesystem::path run_dir;
    std::filesystem::path app_root;
    std::filesystem::path logs_dir;
    std::filesystem::path data_dir;
    std::filesystem::path exports_dir;
    std::filesystem::path webview_dir;
    std::filesystem::path db_path;
    std::filesystem::path log_path;
    std::filesystem::path plugin_dir;
    std::filesystem::path frontend_dir;
};

GuiPaths resolve_gui_paths();

} // namespace novel
