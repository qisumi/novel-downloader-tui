#include "application/download_service.h"

#include <algorithm>
#include <chrono>
#include <thread>

#include "application/library_service.h"
#include "source/runtime/source_manager.h"

namespace novel {

DownloadService::DownloadService(
    std::shared_ptr<SourceManager> source_manager,
    std::shared_ptr<LibraryService> library_service)
    : source_manager_(std::move(source_manager)),
      library_service_(std::move(library_service)) {}

// ── 整本下载 ─────────────

void DownloadService::download_book(
    const Book& book,
    const std::vector<TocItem>& toc,
    std::function<void(int, int)> progress_cb) {
    int total = static_cast<int>(toc.size());
    for (int i = 0; i < total; ++i) {
        const auto& item = toc[i];

        // 跳过已缓存章节，避免重复请求
        if (!library_service_->chapter_cached(item.item_id)) {
            // 从当前书源拉取章节内容
            auto chapter = source_manager_->current_source()->get_chapter(book.book_id, item.item_id);
            if (chapter) {
                // 用目录条目的标题覆盖，确保一致性
                chapter->title = item.title;
                // 若书源未返回 item_id，则用目录中的补全
                if (chapter->item_id.empty()) {
                    chapter->item_id = item.item_id;
                }
                // 持久化到本地缓存
                library_service_->save_chapter(book.book_id, *chapter);
            }
        }
        if (progress_cb) {
            progress_cb(i + 1, total);
        }
        // 请求间隔 200ms，降低被书源限频的风险
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}

// ── 按范围收集章节 ─────────────

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

    // 将起止索引钳位到合法范围，并保证 start <= end
    start = std::clamp(start, 0, static_cast<int>(toc.size()) - 1);
    end = std::clamp(end, 0, static_cast<int>(toc.size()) - 1);
    if (start > end) {
        std::swap(start, end);
    }

    chapters.reserve(end - start + 1);
    int total = end - start + 1;
    for (int index = start; index <= end; ++index) {
        const auto& item = toc[index];

        // 优先命中本地缓存
        auto chapter = library_service_->get_cached_chapter(item.item_id);
        if (!chapter) {
            // 缓存未命中，在线拉取
            chapter = source_manager_->current_source()->get_chapter(book.book_id, item.item_id);
            if (chapter) {
                chapter->title = item.title;
                if (chapter->item_id.empty()) {
                    chapter->item_id = item.item_id;
                }
                // 拉取后写入缓存，供后续复用
                library_service_->save_chapter(book.book_id, *chapter);
            }
        }
        if (chapter) {
            // 统一设置目录标题后加入结果集
            chapter->title = item.title;
            chapters.push_back(*chapter);
        }
        if (progress_cb) {
            progress_cb(index - start + 1, total);
        }
    }
    return chapters;
}

} // namespace novel
