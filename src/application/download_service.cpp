#include "application/download_service.h"

#include <algorithm>
#include <chrono>
#include <thread>

#include <spdlog/spdlog.h>

#include "application/library_service.h"
#include "source/domain/book_source.h"
#include "source/runtime/source_manager.h"

namespace novel {

namespace {

constexpr auto k_request_interval = std::chrono::milliseconds(200);

Chapter align_chapter_with_toc(const TocItem& item, Chapter chapter) {
    chapter.item_id = item.item_id;
    chapter.title = item.title;
    return chapter;
}

} // namespace

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
    auto* source = source_manager_->current_source().get();
    const auto& caps = source->capabilities();
    const bool can_use_batch = caps.supports_batch && (!caps.supports_login || source->is_logged_in());
    if (can_use_batch) {
        download_book_batch(source, book, toc, progress_cb);
        return;
    }

    download_book_chapter_by_chapter(source, book, toc, progress_cb);
}

void DownloadService::download_book_chapter_by_chapter(
    IBookSource* source,
    const Book& book,
    const std::vector<TocItem>& toc,
    std::function<void(int, int)> progress_cb) {
    const int total = static_cast<int>(toc.size());
    for (int i = 0; i < total; ++i) {
        const auto& item = toc[static_cast<std::size_t>(i)];

        if (!library_service_->chapter_cached(item.item_id)) {
            auto chapter = source->get_chapter(book.book_id, item.item_id);
            if (chapter) {
                *chapter = align_chapter_with_toc(item, std::move(*chapter));
                library_service_->save_chapter(book.book_id, *chapter);
            }
        }

        if (progress_cb) {
            progress_cb(i + 1, total);
        }
        std::this_thread::sleep_for(k_request_interval);
    }
}

void DownloadService::download_book_batch(
    IBookSource* source,
    const Book& book,
    const std::vector<TocItem>& toc,
    std::function<void(int, int)> progress_cb) {
    if (toc.empty()) {
        return;
    }

    const int total_batches = source->get_batch_count(book.book_id);
    if (total_batches <= 0) {
        spdlog::warn(
            "Batch download unavailable for source='{}' book_id='{}', fallback to chapter mode",
            source->info().id,
            book.book_id);
        download_book_chapter_by_chapter(source, book, toc, std::move(progress_cb));
        return;
    }

    std::size_t toc_index = 0;
    for (int batch_no = 1; batch_no <= total_batches; ++batch_no) {
        const auto batch_items = source->get_batch(book.book_id, batch_no);
        const auto remaining = toc.size() > toc_index ? toc.size() - toc_index : 0;
        const auto mapped_count = std::min<std::size_t>(batch_items.size(), remaining);

        if (batch_items.size() > remaining) {
            spdlog::warn(
                "Batch {} returned more chapters than remaining toc items. source='{}' book_id='{}' batch_size={} remaining={}",
                batch_no,
                source->info().id,
                book.book_id,
                batch_items.size(),
                remaining);
        }

        for (std::size_t index = 0; index < mapped_count; ++index) {
            auto chapter = align_chapter_with_toc(toc[toc_index + index], batch_items[index]);
            library_service_->save_chapter(book.book_id, chapter);
        }
        toc_index += mapped_count;

        if (progress_cb) {
            progress_cb(batch_no, total_batches);
        }
        std::this_thread::sleep_for(k_request_interval);
    }

    if (toc_index >= toc.size()) {
        return;
    }

    spdlog::warn(
        "Batch download incomplete. source='{}' book_id='{}' downloaded={} expected={}. Backfilling missing chapters.",
        source->info().id,
        book.book_id,
        toc_index,
        toc.size());

    for (const auto& item : toc) {
        if (library_service_->chapter_cached(item.item_id)) {
            continue;
        }

        auto chapter = source->get_chapter(book.book_id, item.item_id);
        if (!chapter) {
            continue;
        }

        *chapter = align_chapter_with_toc(item, std::move(*chapter));
        library_service_->save_chapter(book.book_id, *chapter);
        std::this_thread::sleep_for(k_request_interval);
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

    start = std::clamp(start, 0, static_cast<int>(toc.size()) - 1);
    end = std::clamp(end, 0, static_cast<int>(toc.size()) - 1);
    if (start > end) {
        std::swap(start, end);
    }

    chapters.reserve(static_cast<std::size_t>(end - start + 1));
    const int total = end - start + 1;
    auto* source = source_manager_->current_source().get();
    const auto& caps = source->capabilities();
    const bool can_use_batch = caps.supports_batch && (!caps.supports_login || source->is_logged_in());

    if (!can_use_batch) {
        for (int index = start; index <= end; ++index) {
            const auto& item = toc[static_cast<std::size_t>(index)];
            auto chapter = library_service_->get_cached_chapter(item.item_id);
            if (!chapter) {
                chapter = source->get_chapter(book.book_id, item.item_id);
                if (chapter) {
                    *chapter = align_chapter_with_toc(item, std::move(*chapter));
                    library_service_->save_chapter(book.book_id, *chapter);
                }
            }
            if (chapter) {
                *chapter = align_chapter_with_toc(item, std::move(*chapter));
                chapters.push_back(*chapter);
            }
            if (progress_cb) {
                progress_cb(index - start + 1, total);
            }
        }
        return chapters;
    }

    std::vector<Chapter> resolved(static_cast<std::size_t>(total));
    std::vector<bool> ready(static_cast<std::size_t>(total), false);
    int completed = 0;

    for (int index = start; index <= end; ++index) {
        const auto& item = toc[static_cast<std::size_t>(index)];
        auto chapter = library_service_->get_cached_chapter(item.item_id);
        if (!chapter) {
            continue;
        }

        const auto offset = static_cast<std::size_t>(index - start);
        ready[offset] = true;
        resolved[offset] = align_chapter_with_toc(item, std::move(*chapter));
        ++completed;
        if (progress_cb) {
            progress_cb(completed, total);
        }
    }

    if (completed == total) {
        return resolved;
    }

    const int total_batches = source->get_batch_count(book.book_id);
    if (total_batches <= 0) {
        spdlog::warn(
            "Batch collect unavailable for source='{}' book_id='{}', fallback to chapter mode",
            source->info().id,
            book.book_id);

        for (int index = start; index <= end; ++index) {
            const auto offset = static_cast<std::size_t>(index - start);
            if (ready[offset]) {
                continue;
            }

            const auto& item = toc[static_cast<std::size_t>(index)];
            auto chapter = source->get_chapter(book.book_id, item.item_id);
            if (!chapter) {
                continue;
            }

            resolved[offset] = align_chapter_with_toc(item, std::move(*chapter));
            ready[offset] = true;
            library_service_->save_chapter(book.book_id, resolved[offset]);
            ++completed;
            if (progress_cb) {
                progress_cb(completed, total);
            }
        }

        for (std::size_t index = 0; index < resolved.size(); ++index) {
            if (ready[index]) {
                chapters.push_back(resolved[index]);
            }
        }
        return chapters;
    }

    std::size_t toc_index = 0;
    for (int batch_no = 1; batch_no <= total_batches && completed < total; ++batch_no) {
        const auto batch_items = source->get_batch(book.book_id, batch_no);
        const auto remaining = toc.size() > toc_index ? toc.size() - toc_index : 0;
        const auto mapped_count = std::min<std::size_t>(batch_items.size(), remaining);

        for (std::size_t index = 0; index < mapped_count; ++index) {
            const auto absolute_index = static_cast<int>(toc_index + index);
            const auto& item = toc[static_cast<std::size_t>(absolute_index)];
            auto chapter = align_chapter_with_toc(item, batch_items[index]);
            library_service_->save_chapter(book.book_id, chapter);

            if (absolute_index < start || absolute_index > end) {
                continue;
            }

            const auto offset = static_cast<std::size_t>(absolute_index - start);
            if (ready[offset]) {
                continue;
            }

            ready[offset] = true;
            resolved[offset] = std::move(chapter);
            ++completed;
            if (progress_cb) {
                progress_cb(completed, total);
            }
        }

        toc_index += mapped_count;
        std::this_thread::sleep_for(k_request_interval);
    }

    for (int index = start; index <= end; ++index) {
        const auto offset = static_cast<std::size_t>(index - start);
        if (ready[offset]) {
            chapters.push_back(resolved[offset]);
            continue;
        }

        const auto& item = toc[static_cast<std::size_t>(index)];
        auto chapter = source->get_chapter(book.book_id, item.item_id);
        if (!chapter) {
            continue;
        }

        *chapter = align_chapter_with_toc(item, std::move(*chapter));
        library_service_->save_chapter(book.book_id, *chapter);
        chapters.push_back(*chapter);
        ++completed;
        if (progress_cb) {
            progress_cb(completed, total);
        }
    }

    return chapters;
}

} // namespace novel
