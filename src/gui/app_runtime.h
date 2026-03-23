#pragma once

#include <memory>

#include "application/download_service.h"
#include "application/export_service.h"
#include "application/library_service.h"
#include "db/database.h"
#include "gui/user_paths.h"
#include "source/host/host_api.h"
#include "source/host/http_service.h"
#include "source/runtime/source_manager.h"
#include <webview/webview.h>

namespace novel {

class GuiAppRuntime {
public:
    void initialize();
    void navigate_frontend(webview::webview& window) const;

    const GuiPaths& paths() const { return paths_; }
    const std::shared_ptr<SourceManager>& source_manager() const { return source_manager_; }
    const std::shared_ptr<LibraryService>& library_service() const { return library_service_; }
    const std::shared_ptr<DownloadService>& download_service() const { return download_service_; }
    const std::shared_ptr<ExportService>& export_service() const { return export_service_; }
    const std::shared_ptr<Database>& database() const { return database_; }

private:
    GuiPaths                       paths_;
    std::shared_ptr<HttpService>   http_service_;
    std::shared_ptr<HostApi>       host_api_;
    std::shared_ptr<SourceManager> source_manager_;
    std::shared_ptr<Database>      database_;
    std::shared_ptr<LibraryService> library_service_;
    std::shared_ptr<DownloadService> download_service_;
    std::shared_ptr<ExportService>   export_service_;
};

} // namespace novel
