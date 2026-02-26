#include "db/database.h"
#include <stdexcept>

namespace fanqie {

// ──────────────────────────────────────────────────────────────────────────────
// 构造 / Schema 初始化
// ──────────────────────────────────────────────────────────────────────────────

Database::Database(const std::string& db_path)
    : db_(std::make_unique<SQLite::Database>(db_path,
          SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE))
{
    db_->exec("PRAGMA journal_mode=WAL;");
    db_->exec("PRAGMA synchronous=NORMAL;");
    db_->exec("PRAGMA foreign_keys=ON;");
    init_schema();
}

void Database::init_schema() {
    db_->exec(R"(
        CREATE TABLE IF NOT EXISTS books (
            book_id          TEXT PRIMARY KEY,
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
            added_at         INTEGER DEFAULT (unixepoch())
        );

        CREATE TABLE IF NOT EXISTS toc (
            item_id     TEXT PRIMARY KEY,
            book_id     TEXT NOT NULL,
            title       TEXT,
            volume_name TEXT,
            word_count  INTEGER,
            update_time INTEGER,
            sort_order  INTEGER,
            FOREIGN KEY (book_id) REFERENCES books(book_id) ON DELETE CASCADE
        );

        CREATE TABLE IF NOT EXISTS chapters (
            item_id  TEXT PRIMARY KEY,
            book_id  TEXT NOT NULL,
            title    TEXT,
            content  TEXT,
            cached_at INTEGER DEFAULT (unixepoch()),
            FOREIGN KEY (book_id) REFERENCES books(book_id) ON DELETE CASCADE
        );
    )");
}

// ──────────────────────────────────────────────────────────────────────────────
// 书架
// ──────────────────────────────────────────────────────────────────────────────

void Database::save_book(const Book& b) {
    // 使用 upsert（INSERT … ON CONFLICT DO UPDATE）而非 INSERT OR REPLACE：
    // INSERT OR REPLACE 在内部执行 DELETE + INSERT，会通过 ON DELETE CASCADE
    // 级联删除已缓存的目录和章节数据，且会重置 added_at 时间戳。
    SQLite::Statement stmt(*db_,
        R"(INSERT INTO books
               (book_id,title,author,cover_url,abstract,category,
                word_count,score,gender,creation_status,last_chapter,last_update_time)
           VALUES(?,?,?,?,?,?,?,?,?,?,?,?)
           ON CONFLICT(book_id) DO UPDATE SET
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
    stmt.bind(1,  b.book_id);
    stmt.bind(2,  b.title);
    stmt.bind(3,  b.author);
    stmt.bind(4,  b.cover_url);
    stmt.bind(5,  b.abstract);
    stmt.bind(6,  b.category);
    stmt.bind(7,  b.word_count);
    stmt.bind(8,  b.score);
    stmt.bind(9,  b.gender);
    stmt.bind(10, b.creation_status);
    stmt.bind(11, b.last_chapter_title);
    stmt.bind(12, static_cast<int64_t>(b.last_update_time));
    stmt.exec();
}

bool Database::remove_book(const std::string& book_id) {
    SQLite::Statement stmt(*db_, "DELETE FROM books WHERE book_id=?");
    stmt.bind(1, book_id);
    return stmt.exec() > 0;
}

std::vector<Book> Database::list_bookshelf() {
    std::vector<Book> list;
    SQLite::Statement stmt(*db_,
        "SELECT book_id,title,author,cover_url,abstract,category,"
        "word_count,score,gender,creation_status,last_chapter,last_update_time "
        "FROM books ORDER BY added_at DESC");
    while (stmt.executeStep()) {
        Book b;
        b.book_id           = stmt.getColumn(0).getString();
        b.title             = stmt.getColumn(1).getString();
        b.author            = stmt.getColumn(2).getString();
        b.cover_url         = stmt.getColumn(3).getString();
        b.abstract          = stmt.getColumn(4).getString();
        b.category          = stmt.getColumn(5).getString();
        b.word_count        = stmt.getColumn(6).getString();
        b.score             = stmt.getColumn(7).getDouble();
        b.gender            = stmt.getColumn(8).getInt();
        b.creation_status   = stmt.getColumn(9).getInt();
        b.last_chapter_title= stmt.getColumn(10).getString();
        b.last_update_time  = stmt.getColumn(11).getInt64();
        list.push_back(std::move(b));
    }
    return list;
}

std::optional<Book> Database::get_book(const std::string& book_id) {
    SQLite::Statement stmt(*db_,
        "SELECT book_id,title,author,cover_url,abstract,category,"
        "word_count,score,gender,creation_status,last_chapter,last_update_time "
        "FROM books WHERE book_id=?");
    stmt.bind(1, book_id);
    if (!stmt.executeStep()) return std::nullopt;
    Book b;
    b.book_id           = stmt.getColumn(0).getString();
    b.title             = stmt.getColumn(1).getString();
    b.author            = stmt.getColumn(2).getString();
    b.cover_url         = stmt.getColumn(3).getString();
    b.abstract          = stmt.getColumn(4).getString();
    b.category          = stmt.getColumn(5).getString();
    b.word_count        = stmt.getColumn(6).getString();
    b.score             = stmt.getColumn(7).getDouble();
    b.gender            = stmt.getColumn(8).getInt();
    b.creation_status   = stmt.getColumn(9).getInt();
    b.last_chapter_title= stmt.getColumn(10).getString();
    b.last_update_time  = stmt.getColumn(11).getInt64();
    return b;
}

bool Database::is_in_bookshelf(const std::string& book_id) {
    SQLite::Statement stmt(*db_,
        "SELECT 1 FROM books WHERE book_id=? LIMIT 1");
    stmt.bind(1, book_id);
    return stmt.executeStep();
}

// ──────────────────────────────────────────────────────────────────────────────
// 目录
// ──────────────────────────────────────────────────────────────────────────────

void Database::save_toc(const std::string& book_id,
                        const std::vector<TocItem>& toc) {
    SQLite::Transaction tx(*db_);
    db_->exec("DELETE FROM toc WHERE book_id='" + book_id + "'");
    SQLite::Statement stmt(*db_,
        "INSERT INTO toc(item_id,book_id,title,volume_name,word_count,update_time,sort_order)"
        " VALUES(?,?,?,?,?,?,?)");
    for (int i = 0; i < static_cast<int>(toc.size()); ++i) {
        const auto& t = toc[i];
        stmt.bind(1, t.item_id);
        stmt.bind(2, book_id);
        stmt.bind(3, t.title);
        stmt.bind(4, t.volume_name);
        stmt.bind(5, t.word_count);
        stmt.bind(6, static_cast<int64_t>(t.update_time));
        stmt.bind(7, i);
        stmt.exec();
        stmt.reset();
    }
    tx.commit();
}

std::vector<TocItem> Database::get_toc(const std::string& book_id) {
    std::vector<TocItem> toc;
    SQLite::Statement stmt(*db_,
        "SELECT item_id,title,volume_name,word_count,update_time "
        "FROM toc WHERE book_id=? ORDER BY sort_order");
    stmt.bind(1, book_id);
    while (stmt.executeStep()) {
        TocItem t;
        t.item_id    = stmt.getColumn(0).getString();
        t.title      = stmt.getColumn(1).getString();
        t.volume_name= stmt.getColumn(2).getString();
        t.word_count = stmt.getColumn(3).getInt();
        t.update_time= stmt.getColumn(4).getInt64();
        toc.push_back(std::move(t));
    }
    return toc;
}

// ──────────────────────────────────────────────────────────────────────────────
// 章节
// ──────────────────────────────────────────────────────────────────────────────

void Database::save_chapter(const std::string& book_id, const Chapter& ch) {
    SQLite::Statement stmt(*db_,
        "INSERT OR REPLACE INTO chapters(item_id,book_id,title,content)"
        " VALUES(?,?,?,?)");
    stmt.bind(1, ch.item_id);
    stmt.bind(2, book_id);
    stmt.bind(3, ch.title);
    stmt.bind(4, ch.content);
    stmt.exec();
}

std::optional<Chapter> Database::get_chapter(const std::string& item_id) {
    SQLite::Statement stmt(*db_,
        "SELECT item_id,title,content FROM chapters WHERE item_id=?");
    stmt.bind(1, item_id);
    if (!stmt.executeStep()) return std::nullopt;
    Chapter ch;
    ch.item_id  = stmt.getColumn(0).getString();
    ch.title    = stmt.getColumn(1).getString();
    ch.content  = stmt.getColumn(2).getString();
    return ch;
}

bool Database::chapter_cached(const std::string& item_id) {
    SQLite::Statement stmt(*db_,
        "SELECT 1 FROM chapters WHERE item_id=? LIMIT 1");
    stmt.bind(1, item_id);
    return stmt.executeStep();
}

int Database::cached_chapter_count(const std::string& book_id) {
    SQLite::Statement stmt(*db_,
        "SELECT COUNT(*) FROM chapters WHERE book_id=?");
    stmt.bind(1, book_id);
    if (stmt.executeStep()) return stmt.getColumn(0).getInt();
    return 0;
}

} // namespace fanqie
