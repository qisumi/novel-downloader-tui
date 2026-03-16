#pragma once

#include <functional>
#include <memory>
#include <vector>

#include "models/book.h"

namespace fanqie {

class LibraryService;
class SourceManager;

class DownloadService {
public:
    DownloadService(
        std::shared_ptr<SourceManager> source_manager,
        std::shared_ptr<LibraryService> library_service);

    void download_book(
        const Book& book,
        const std::vector<TocItem>& toc,
        std::function<void(int, int)> progress_cb = nullptr);

    std::vector<Chapter> collect_chapters(
        const Book& book,
        const std::vector<TocItem>& toc,
        int start,
        int end,
        std::function<void(int, int)> progress_cb = nullptr);

private:
    std::shared_ptr<SourceManager>  source_manager_;
    std::shared_ptr<LibraryService> library_service_;
};

} // namespace fanqie
