/// @file app_runtime.cpp
/// @brief GUI 应用运行时初始化实现
///
/// 按顺序完成：路径解析 -> .env 加载 -> 日志初始化 -> 组件创建 -> 插件扫描 -> 首选书源配置

#include "gui/app_runtime.h"

#include <cstdlib>
#include <string>

#include <spdlog/spdlog.h>
#include <webview/webview.h>

#include "dotenv.h"
#include "gui/frontend.h"
#include "logger.h"
#include "source/domain/source_errors.h"

namespace novel {

void GuiAppRuntime::initialize(webview::webview& window)
{
    // 解析运行时路径（可执行文件目录、插件目录、前端目录等）
    paths_ = resolve_gui_paths();

    // 加载 .env 环境变量（如 API 密钥）
    dotenv::load((paths_.executable_dir / ".env").string());

    // 初始化日志系统，输出到文件
    init_logger(paths_.log_path.string());

    // 记录关键路径信息，便于调试
    spdlog::info("GUI run dir: {}", paths_.run_dir.string());
    spdlog::info("GUI app root: {}", paths_.app_root.string());
    spdlog::info("GUI plugin dir: {}", paths_.plugin_dir.string());
    spdlog::info("GUI db path: {}", paths_.db_path.string());
    spdlog::info("GUI frontend dir: {}", paths_.frontend_dir.string());

    // 创建基础服务层：HTTP -> 宿主 API -> JS 插件运行时
    http_service_ = std::make_shared<HttpService>();
    host_api_ = std::make_shared<HostApi>(http_service_);
    plugin_runtime_ = std::make_shared<JsPluginRuntime>(window, host_api_);
    plugin_runtime_->install();

    // 创建书源管理器并加载插件
    source_manager_ = std::make_shared<SourceManager>(plugin_runtime_);

    // 创建数据库和应用服务
    database_ = std::make_shared<Database>(paths_.db_path.string());
    library_service_ = std::make_shared<LibraryService>(source_manager_, database_);
    download_service_ = std::make_shared<DownloadService>(source_manager_, library_service_);
    export_service_ = std::make_shared<ExportService>(download_service_);

    // 扫描插件目录，注册所有书源
    source_manager_->load_from_directory(paths_.plugin_dir.string());

    // 如果设置了 NOVEL_SOURCE 环境变量，自动切换到指定书源
    if (const char* preferred_source = std::getenv("NOVEL_SOURCE")) {
        std::string source_id(preferred_source);
        if (!source_id.empty()) {
            source_manager_->set_preferred_source(source_id);
            spdlog::info("Preferred source configured: {}", source_id);
        }
    }
}

void GuiAppRuntime::navigate_frontend(webview::webview& window) const
{
    novel::navigate_frontend(window, paths_.frontend_dir);
}

} // namespace novel
