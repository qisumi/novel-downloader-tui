#include "application/download_service.h"

#include <algorithm>
#include <chrono>
#include <thread>

#include "application/library_service.h"
#include "source/runtime/source_manager.h"

namespace fanqie {

DownloadService::DownloadService(
    std::shared_ptr<SourceManager> source_manager,
    std::shared_ptr<LibraryService> library_service)
    : source_manager_(std::move(source_manager)),
      library_service_(std::move(library_service)) {}

void DownloadService::download_book(
    const Book& book,
    const std::vector<TocItem>& toc,
    std::function<void(int, int)> progress_cb) {
    int total = static_cast<int>(toc.size());
    for (int i = 0; i < total; ++i) {
        const auto& item = toc[i];
        if (!library_service_->chapter_cached(item.item_id)) {
            auto chapter = source_manager_->current_source()->get_chapter(book.book_id, item.item_id);
            if (chapter) {
                chapter->title = item.title;
                if (chapter->item_id.empty()) {
                    chapter->item_id = item.item_id;
                }
                library_service_->save_chapter(book.book_id, *chapter);
            }
        }
        if (progress_cb) {
            progress_cb(i + 1, total);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}

std::vector<Chapter> DownloadService::collect_chapters(
    const Book& book,
    const std::vector<TocItem>& toc,
    int start,
    int end,
    std::function<void(int, int)> progress_cb) {
    std::vector<Chapter> chapters;
    if (toc.empty()) {
        return chapters;
    }

    start = std::clamp(start, 0, static_cast<int>(toc.size()) - 1);
    end = std::clamp(end, 0, static_cast<int>(toc.size()) - 1);
    if (start > end) {
        std::swap(start, end);
    }

    chapters.reserve(end - start + 1);
    int total = end - start + 1;
    for (int index = start; index <= end; ++index) {
        const auto& item = toc[index];
        auto chapter = library_service_->get_cached_chapter(item.item_id);
        if (!chapter) {
            chapter = source_manager_->current_source()->get_chapter(book.book_id, item.item_id);
            if (chapter) {
                chapter->title = item.title;
                if (chapter->item_id.empty()) {
                    chapter->item_id = item.item_id;
                }
                library_service_->save_chapter(book.book_id, *chapter);
            }
        }
        if (chapter) {
            chapter->title = item.title;
            chapters.push_back(*chapter);
        }
        if (progress_cb) {
            progress_cb(index - start + 1, total);
        }
    }
    return chapters;
}

} // namespace fanqie
