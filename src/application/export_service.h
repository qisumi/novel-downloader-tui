#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "models/book.h"

namespace novel {

class DownloadService;

class ExportService {
public:
    explicit ExportService(std::shared_ptr<DownloadService> download_service);

    std::string export_book(
        const Book& book,
        const std::vector<TocItem>& toc,
        int start,
        int end,
        bool as_epub,
        const std::string& output_dir,
        std::function<void(int, int)> prepare_progress_cb = nullptr,
        std::function<void(int, int)> export_progress_cb = nullptr);

private:
    std::shared_ptr<DownloadService> download_service_;
};

} // namespace novel
