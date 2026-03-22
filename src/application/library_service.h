#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "db/database.h"
#include "models/book.h"

namespace novel {

class SourceManager;

class LibraryService {
public:
    LibraryService(std::shared_ptr<SourceManager> source_manager, std::shared_ptr<Database> db);

    std::vector<Book> search_books(const std::string& keywords, int page = 0);

    void save_to_bookshelf(const Book& book);
    bool remove_from_bookshelf(const std::string& book_id);
    std::vector<Book> list_bookshelf();

    std::vector<TocItem> load_toc(const Book& book, bool force_remote);
    int toc_count(const std::string& book_id);
    int cached_chapter_count(const std::string& book_id);
    bool chapter_cached(const std::string& item_id);

    std::optional<Chapter> get_cached_chapter(const std::string& item_id);
    void save_chapter(const std::string& book_id, const Chapter& chapter);

    std::string current_source_id() const;
    std::string current_source_name() const;

private:
    std::shared_ptr<SourceManager> source_manager_;
    std::shared_ptr<Database>      db_;
};

} // namespace novel
