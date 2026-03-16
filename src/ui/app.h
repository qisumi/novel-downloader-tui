#pragma once

#include <atomic>
#include <memory>
#include <string>

#include "application/download_service.h"
#include "application/export_service.h"
#include "application/library_service.h"
#include "db/database.h"
#include "models/book.h"
#include "source/runtime/source_manager.h"

namespace fanqie {

struct AppContext {
    std::shared_ptr<SourceManager>  source_manager;
    std::shared_ptr<LibraryService> library_service;
    std::shared_ptr<DownloadService> download_service;
    std::shared_ptr<ExportService>  export_service;
    std::shared_ptr<Database>       db;

    Book        current_book;

    std::string plugin_dir = "plugins";
    std::string current_source_id;
    std::string current_source_name;
    std::string epub_output_dir = ".";

    std::atomic<bool> bookshelf_dirty{false};
    std::atomic<bool> bookshelf_needs_refresh{false};
    std::atomic<bool> app_exit_requested{false};
};

int run_app(std::shared_ptr<AppContext> ctx);

} // namespace fanqie
