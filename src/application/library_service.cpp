#include "application/library_service.h"

#include "source/runtime/source_manager.h"

namespace fanqie {

LibraryService::LibraryService(
    std::shared_ptr<SourceManager> source_manager,
    std::shared_ptr<Database> db)
    : source_manager_(std::move(source_manager)), db_(std::move(db)) {}

std::vector<Book> LibraryService::search_books(const std::string& keywords, int page) {
    return source_manager_->current_source()->search(keywords, page);
}

void LibraryService::save_to_bookshelf(const Book& book) {
    db_->save_book(current_source_id(), book);
}

bool LibraryService::remove_from_bookshelf(const std::string& book_id) {
    return db_->remove_book(current_source_id(), book_id);
}

std::vector<Book> LibraryService::list_bookshelf() {
    return db_->list_bookshelf(current_source_id());
}

std::vector<TocItem> LibraryService::load_toc(const Book& book, bool force_remote) {
    std::vector<TocItem> toc;
    if (!force_remote) {
        toc = db_->get_toc(current_source_id(), book.book_id);
    }
    if (toc.empty()) {
        toc = source_manager_->current_source()->get_toc(book.book_id);
        if (!toc.empty()) {
            db_->save_book(current_source_id(), book);
            db_->save_toc(current_source_id(), book.book_id, toc);
        }
    }
    return toc;
}

int LibraryService::toc_count(const std::string& book_id) {
    return db_->toc_count(current_source_id(), book_id);
}

int LibraryService::cached_chapter_count(const std::string& book_id) {
    return db_->cached_chapter_count(current_source_id(), book_id);
}

bool LibraryService::chapter_cached(const std::string& item_id) {
    return db_->chapter_cached(current_source_id(), item_id);
}

std::optional<Chapter> LibraryService::get_cached_chapter(const std::string& item_id) {
    return db_->get_chapter(current_source_id(), item_id);
}

void LibraryService::save_chapter(const std::string& book_id, const Chapter& chapter) {
    db_->save_chapter(current_source_id(), book_id, chapter);
}

std::string LibraryService::current_source_id() const {
    return source_manager_->current_info()->id;
}

std::string LibraryService::current_source_name() const {
    return source_manager_->current_info()->name;
}

} // namespace fanqie
