#include "application/library_service.h"

#include "source/runtime/source_manager.h"

namespace novel {

LibraryService::LibraryService(
    std::shared_ptr<SourceManager> source_manager,
    std::shared_ptr<Database> db)
    : source_manager_(std::move(source_manager)), db_(std::move(db)) {}

// ── 搜索 ─────────────

std::vector<Book> LibraryService::search_books(const std::string& keywords, int page) {
    return source_manager_->current_source()->search(keywords, page);
}

// ── 书架管理 ─────────────

void LibraryService::save_to_bookshelf(const Book& book) {
    db_->save_book(current_source_id(), book);
}

bool LibraryService::remove_from_bookshelf(const std::string& book_id) {
    return db_->remove_book(current_source_id(), book_id);
}

std::vector<Book> LibraryService::list_bookshelf() {
    return db_->list_bookshelf(current_source_id());
}

// ── 目录加载 ─────────────

std::vector<TocItem> LibraryService::load_toc(const Book& book, bool force_remote) {
    std::vector<TocItem> toc;

    // 非强制刷新时，先尝试从本地数据库读取
    if (!force_remote) {
        toc = db_->get_toc(current_source_id(), book.book_id);
    }

    // 本地无缓存或强制刷新时，从远程书源拉取
    if (toc.empty()) {
        toc = source_manager_->current_source()->get_toc(book.book_id);
        if (!toc.empty()) {
            // 拉取成功后同时持久化书籍信息和目录
            db_->save_book(current_source_id(), book);
            db_->save_toc(current_source_id(), book.book_id, toc);
        }
    }
    return toc;
}

// ── 目录与章节统计 ─────────────

int LibraryService::toc_count(const std::string& book_id) {
    return db_->toc_count(current_source_id(), book_id);
}

int LibraryService::cached_chapter_count(const std::string& book_id) {
    return db_->cached_chapter_count(current_source_id(), book_id);
}

bool LibraryService::chapter_cached(const std::string& item_id) {
    return db_->chapter_cached(current_source_id(), item_id);
}

// ── 章节缓存读写 ─────────────

std::optional<Chapter> LibraryService::get_cached_chapter(const std::string& item_id) {
    return db_->get_chapter(current_source_id(), item_id);
}

void LibraryService::save_chapter(const std::string& book_id, const Chapter& chapter) {
    db_->save_chapter(current_source_id(), book_id, chapter);
}

// ── 当前书源信息 ─────────────

std::string LibraryService::current_source_id() const {
    return source_manager_->current_info()->id;
}

std::string LibraryService::current_source_name() const {
    return source_manager_->current_info()->name;
}

} // namespace novel
