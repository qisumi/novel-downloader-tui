#pragma once
#include <string>
#include <vector>
#include <optional>
#include <memory>
#include <SQLiteCpp/SQLiteCpp.h>
#include "models/book.h"

namespace fanqie {

/// SQLite 持久化层
/// 负责书架书籍、目录条目、章节正文的本地缓存
class Database {
public:
    explicit Database(const std::string& db_path = "fanqie.db");

    // ── 书架 ──────────────────────────────────────────────────
    void        save_book(const Book& book);
    bool        remove_book(const std::string& book_id);
    std::vector<Book> list_bookshelf();
    std::optional<Book> get_book(const std::string& book_id);
    bool        is_in_bookshelf(const std::string& book_id);

    // ── 目录缓存 ───────────────────────────────────────────────
    void        save_toc(const std::string& book_id, const std::vector<TocItem>& toc);
    std::vector<TocItem> get_toc(const std::string& book_id);

    // ── 章节缓存 ───────────────────────────────────────────────
    void        save_chapter(const std::string& book_id, const Chapter& ch);
    std::optional<Chapter> get_chapter(const std::string& item_id);
    bool        chapter_cached(const std::string& item_id);
    int         cached_chapter_count(const std::string& book_id);

private:
    std::unique_ptr<SQLite::Database> db_;

    void init_schema();
};

} // namespace fanqie
