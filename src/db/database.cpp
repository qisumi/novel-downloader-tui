#include "db/database.h"

#include <spdlog/spdlog.h>

#include <memory>
#include <stdexcept>

namespace novel {

namespace {

Book read_book(SQLite::Statement& stmt) {
    Book book;
    book.book_id = stmt.getColumn(0).getString();
    book.title = stmt.getColumn(1).getString();
    book.author = stmt.getColumn(2).getString();
    book.cover_url = stmt.getColumn(3).getString();
    book.abstract = stmt.getColumn(4).getString();
    book.category = stmt.getColumn(5).getString();
    book.word_count = stmt.getColumn(6).getString();
    book.score = stmt.getColumn(7).getDouble();
    book.gender = stmt.getColumn(8).getInt();
    book.creation_status = stmt.getColumn(9).getInt();
    book.last_chapter_title = stmt.getColumn(10).getString();
    book.last_update_time = stmt.getColumn(11).getInt64();
    return book;
}

bool table_exists(SQLite::Database& db, const char* table_name) {
    SQLite::Statement stmt(
        db, "SELECT 1 FROM sqlite_master WHERE type='table' AND name=? LIMIT 1");
    stmt.bind(1, table_name);
    return stmt.executeStep();
}

bool table_has_column(SQLite::Database& db, const char* table_name, const char* column_name) {
    if (!table_exists(db, table_name)) {
        return false;
    }

    SQLite::Statement stmt(db, std::string("PRAGMA table_info(") + table_name + ")");
    while (stmt.executeStep()) {
        if (stmt.getColumn(1).getString() == column_name) {
            return true;
        }
    }
    return false;
}

} // namespace

Database::Database(const std::string& db_path)
    : db_(std::make_unique<SQLite::Database>(
          db_path, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE)) {
    db_->exec("PRAGMA journal_mode=WAL;");
    db_->exec("PRAGMA synchronous=NORMAL;");
    db_->exec("PRAGMA foreign_keys=ON;");
    init_schema();
}

void Database::init_schema() {
    bool legacy_schema =
        (table_exists(*db_, "books") && !table_has_column(*db_, "books", "source_id")) ||
        (table_exists(*db_, "toc") && !table_has_column(*db_, "toc", "source_id")) ||
        (table_exists(*db_, "chapters") && !table_has_column(*db_, "chapters", "source_id"));

    if (legacy_schema) {
        spdlog::warn("Detected legacy database schema, rebuilding tables with source_id support");
        db_->exec(R"(
            DROP TABLE IF EXISTS chapters;
            DROP TABLE IF EXISTS toc;
            DROP TABLE IF EXISTS books;
        )");
    }

    db_->exec(R"(
        CREATE TABLE IF NOT EXISTS books (
            source_id        TEXT NOT NULL,
            book_id          TEXT NOT NULL,
            title            TEXT NOT NULL,
            author           TEXT,
            cover_url        TEXT,
            abstract         TEXT,
            category         TEXT,
            word_count       TEXT,
            score            REAL,
            gender           INTEGER,
            creation_status  INTEGER,
            last_chapter     TEXT,
            last_update_time INTEGER,
            added_at         INTEGER DEFAULT (unixepoch()),
            PRIMARY KEY (source_id, book_id)
        );

        CREATE TABLE IF NOT EXISTS toc (
            source_id   TEXT NOT NULL,
            item_id     TEXT NOT NULL,
            book_id     TEXT NOT NULL,
            title       TEXT,
            volume_name TEXT,
            word_count  INTEGER,
            update_time INTEGER,
            sort_order  INTEGER,
            PRIMARY KEY (source_id, item_id),
            FOREIGN KEY (source_id, book_id)
                REFERENCES books(source_id, book_id) ON DELETE CASCADE
        );

        CREATE TABLE IF NOT EXISTS chapters (
            source_id TEXT NOT NULL,
            item_id   TEXT NOT NULL,
            book_id   TEXT NOT NULL,
            title     TEXT,
            content   TEXT,
            cached_at INTEGER DEFAULT (unixepoch()),
            PRIMARY KEY (source_id, item_id),
            FOREIGN KEY (source_id, book_id)
                REFERENCES books(source_id, book_id) ON DELETE CASCADE
        );
    )");
}

void Database::save_book(const std::string& source_id, const Book& book) {
    SQLite::Statement stmt(*db_,
        R"(INSERT INTO books
               (source_id,book_id,title,author,cover_url,abstract,category,
                word_count,score,gender,creation_status,last_chapter,last_update_time)
           VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?)
           ON CONFLICT(source_id,book_id) DO UPDATE SET
               title            = excluded.title,
               author           = excluded.author,
               cover_url        = excluded.cover_url,
               abstract         = excluded.abstract,
               category         = excluded.category,
               word_count       = excluded.word_count,
               score            = excluded.score,
               gender           = excluded.gender,
               creation_status  = excluded.creation_status,
               last_chapter     = excluded.last_chapter,
               last_update_time = excluded.last_update_time)");
    stmt.bind(1, source_id);
    stmt.bind(2, book.book_id);
    stmt.bind(3, book.title);
    stmt.bind(4, book.author);
    stmt.bind(5, book.cover_url);
    stmt.bind(6, book.abstract);
    stmt.bind(7, book.category);
    stmt.bind(8, book.word_count);
    stmt.bind(9, book.score);
    stmt.bind(10, book.gender);
    stmt.bind(11, book.creation_status);
    stmt.bind(12, book.last_chapter_title);
    stmt.bind(13, static_cast<std::int64_t>(book.last_update_time));
    stmt.exec();
}

bool Database::remove_book(const std::string& source_id, const std::string& book_id) {
    SQLite::Statement stmt(*db_, "DELETE FROM books WHERE source_id=? AND book_id=?");
    stmt.bind(1, source_id);
    stmt.bind(2, book_id);
    return stmt.exec() > 0;
}

std::vector<Book> Database::list_bookshelf(const std::string& source_id) {
    std::vector<Book> books;
    SQLite::Statement stmt(*db_,
        "SELECT book_id,title,author,cover_url,abstract,category,"
        "word_count,score,gender,creation_status,last_chapter,last_update_time "
        "FROM books WHERE source_id=? ORDER BY added_at DESC");
    stmt.bind(1, source_id);
    while (stmt.executeStep()) {
        books.push_back(read_book(stmt));
    }
    return books;
}

std::optional<Book> Database::get_book(const std::string& source_id, const std::string& book_id) {
    SQLite::Statement stmt(*db_,
        "SELECT book_id,title,author,cover_url,abstract,category,"
        "word_count,score,gender,creation_status,last_chapter,last_update_time "
        "FROM books WHERE source_id=? AND book_id=?");
    stmt.bind(1, source_id);
    stmt.bind(2, book_id);
    if (!stmt.executeStep()) {
        return std::nullopt;
    }
    return read_book(stmt);
}

bool Database::is_in_bookshelf(const std::string& source_id, const std::string& book_id) {
    SQLite::Statement stmt(*db_,
        "SELECT 1 FROM books WHERE source_id=? AND book_id=? LIMIT 1");
    stmt.bind(1, source_id);
    stmt.bind(2, book_id);
    return stmt.executeStep();
}

void Database::save_toc(
    const std::string& source_id,
    const std::string& book_id,
    const std::vector<TocItem>& toc) {
    try {
        SQLite::Transaction tx(*db_);
        SQLite::Statement del_stmt(*db_, "DELETE FROM toc WHERE source_id=? AND book_id=?");
        del_stmt.bind(1, source_id);
        del_stmt.bind(2, book_id);
        del_stmt.exec();

        SQLite::Statement stmt(*db_,
            "INSERT INTO toc(source_id,item_id,book_id,title,volume_name,word_count,update_time,sort_order) "
            "VALUES(?,?,?,?,?,?,?,?)");
        for (int i = 0; i < static_cast<int>(toc.size()); ++i) {
            const auto& item = toc[i];
            stmt.bind(1, source_id);
            stmt.bind(2, item.item_id);
            stmt.bind(3, book_id);
            stmt.bind(4, item.title);
            stmt.bind(5, item.volume_name);
            stmt.bind(6, item.word_count);
            stmt.bind(7, static_cast<std::int64_t>(item.update_time));
            stmt.bind(8, i);
            stmt.exec();
            stmt.reset();
            stmt.clearBindings();
        }
        tx.commit();
    } catch (const std::exception& e) {
        spdlog::error("save_toc() exception: source_id={} book_id={} error={}",
                      source_id, book_id, e.what());
        throw;
    }
}

std::vector<TocItem> Database::get_toc(const std::string& source_id, const std::string& book_id) {
    std::vector<TocItem> toc;
    SQLite::Statement stmt(*db_,
        "SELECT item_id,title,volume_name,word_count,update_time "
        "FROM toc WHERE source_id=? AND book_id=? ORDER BY sort_order");
    stmt.bind(1, source_id);
    stmt.bind(2, book_id);
    while (stmt.executeStep()) {
        TocItem item;
        item.item_id = stmt.getColumn(0).getString();
        item.title = stmt.getColumn(1).getString();
        item.volume_name = stmt.getColumn(2).getString();
        item.word_count = stmt.getColumn(3).getInt();
        item.update_time = stmt.getColumn(4).getInt64();
        toc.push_back(std::move(item));
    }
    return toc;
}

int Database::toc_count(const std::string& source_id, const std::string& book_id) {
    SQLite::Statement stmt(*db_,
        "SELECT COUNT(*) FROM toc WHERE source_id=? AND book_id=?");
    stmt.bind(1, source_id);
    stmt.bind(2, book_id);
    if (stmt.executeStep()) {
        return stmt.getColumn(0).getInt();
    }
    return 0;
}

void Database::save_chapter(
    const std::string& source_id,
    const std::string& book_id,
    const Chapter& ch) {
    SQLite::Statement stmt(*db_,
        "INSERT OR REPLACE INTO chapters(source_id,item_id,book_id,title,content) "
        "VALUES(?,?,?,?,?)");
    stmt.bind(1, source_id);
    stmt.bind(2, ch.item_id);
    stmt.bind(3, book_id);
    stmt.bind(4, ch.title);
    stmt.bind(5, ch.content);
    stmt.exec();
}

std::optional<Chapter> Database::get_chapter(const std::string& source_id, const std::string& item_id) {
    SQLite::Statement stmt(*db_,
        "SELECT item_id,title,content FROM chapters WHERE source_id=? AND item_id=?");
    stmt.bind(1, source_id);
    stmt.bind(2, item_id);
    if (!stmt.executeStep()) {
        return std::nullopt;
    }
    Chapter chapter;
    chapter.item_id = stmt.getColumn(0).getString();
    chapter.title = stmt.getColumn(1).getString();
    chapter.content = stmt.getColumn(2).getString();
    return chapter;
}

bool Database::chapter_cached(const std::string& source_id, const std::string& item_id) {
    SQLite::Statement stmt(*db_,
        "SELECT 1 FROM chapters WHERE source_id=? AND item_id=? LIMIT 1");
    stmt.bind(1, source_id);
    stmt.bind(2, item_id);
    return stmt.executeStep();
}

int Database::cached_chapter_count(const std::string& source_id, const std::string& book_id) {
    SQLite::Statement stmt(*db_,
        "SELECT COUNT(*) FROM chapters WHERE source_id=? AND book_id=?");
    stmt.bind(1, source_id);
    stmt.bind(2, book_id);
    if (stmt.executeStep()) {
        return stmt.getColumn(0).getInt();
    }
    return 0;
}

} // namespace novel
