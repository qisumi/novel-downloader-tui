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
    paths_ = resolve_gui_paths();

    dotenv::load((paths_.executable_dir / ".env").string());
    init_logger(paths_.log_path.string());

    spdlog::info("GUI run dir: {}", paths_.run_dir.string());
    spdlog::info("GUI app root: {}", paths_.app_root.string());
    spdlog::info("GUI plugin dir: {}", paths_.plugin_dir.string());
    spdlog::info("GUI db path: {}", paths_.db_path.string());
    spdlog::info("GUI frontend dir: {}", paths_.frontend_dir.string());

    http_service_ = std::make_shared<HttpService>();
    host_api_ = std::make_shared<HostApi>(http_service_);
    plugin_runtime_ = std::make_shared<JsPluginRuntime>(window, host_api_);
    plugin_runtime_->install();
    source_manager_ = std::make_shared<SourceManager>(plugin_runtime_);
    database_ = std::make_shared<Database>(paths_.db_path.string());
    library_service_ = std::make_shared<LibraryService>(source_manager_, database_);
    download_service_ = std::make_shared<DownloadService>(source_manager_, library_service_);
    export_service_ = std::make_shared<ExportService>(download_service_);

    source_manager_->load_from_directory(paths_.plugin_dir.string());

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
