#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <SQLiteCpp/SQLiteCpp.h>

#include "models/book.h"

namespace fanqie {

class Database {
public:
    explicit Database(const std::string& db_path = "fanqie.db");

    void save_book(const std::string& source_id, const Book& book);
    bool remove_book(const std::string& source_id, const std::string& book_id);
    std::vector<Book> list_bookshelf(const std::string& source_id);
    std::optional<Book> get_book(const std::string& source_id, const std::string& book_id);
    bool is_in_bookshelf(const std::string& source_id, const std::string& book_id);

    void save_toc(
        const std::string& source_id,
        const std::string& book_id,
        const std::vector<TocItem>& toc);
    std::vector<TocItem> get_toc(const std::string& source_id, const std::string& book_id);
    int toc_count(const std::string& source_id, const std::string& book_id);

    void save_chapter(
        const std::string& source_id,
        const std::string& book_id,
        const Chapter& ch);
    std::optional<Chapter> get_chapter(const std::string& source_id, const std::string& item_id);
    bool chapter_cached(const std::string& source_id, const std::string& item_id);
    int cached_chapter_count(const std::string& source_id, const std::string& book_id);

private:
    std::unique_ptr<SQLite::Database> db_;

    void init_schema();
};

} // namespace fanqie
