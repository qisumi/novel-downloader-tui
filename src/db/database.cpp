#include "db/database.h"

#include <spdlog/spdlog.h>

#include <memory>
#include <stdexcept>

namespace novel {

// ── 匿名命名空间：内部辅助函数 ─────────────

namespace {

/// 从查询结果的固定列顺序中读取一本书的完整字段
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

/// 检查数据库中是否存在指定名称的表
bool table_exists(SQLite::Database& db, const char* table_name) {
    SQLite::Statement stmt(
        db, "SELECT 1 FROM sqlite_master WHERE type='table' AND name=? LIMIT 1");
    stmt.bind(1, table_name);
    return stmt.executeStep();
}

/// 检查指定表中是否包含某个列（用于判断旧版表结构）
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

// ── 构造函数 ─────────────

/// 打开（或创建）数据库文件，设置 WAL 模式与外键约束，然后初始化表结构。
Database::Database(const std::string& db_path)
    : db_(std::make_unique<SQLite::Database>(
          db_path, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE)) {
    db_->exec("PRAGMA journal_mode=WAL;");      // 使用 WAL 模式提升并发读写性能
    db_->exec("PRAGMA synchronous=NORMAL;");     // 在 WAL 模式下 NORMAL 已足够安全
    db_->exec("PRAGMA foreign_keys=ON;");        // 启用外键约束，支持级联删除
    init_schema();
}

// ── 表结构初始化 ─────────────

/// 创建 books / toc / chapters 三张表。
/// 若检测到旧版（无 source_id 列）的表，则先删除旧表再重建。
void Database::init_schema() {
    // 判断是否存在缺少 source_id 列的旧版表
    bool legacy_schema =
        (table_exists(*db_, "books") && !table_has_column(*db_, "books", "source_id")) ||
        (table_exists(*db_, "toc") && !table_has_column(*db_, "toc", "source_id")) ||
        (table_exists(*db_, "chapters") && !table_has_column(*db_, "chapters", "source_id"));

    if (legacy_schema) {
        spdlog::warn("Detected legacy database schema, rebuilding tables with source_id support");
        // 旧表数据不兼容，直接清除重建
        db_->exec(R"(
            DROP TABLE IF EXISTS chapters;
            DROP TABLE IF EXISTS toc;
            DROP TABLE IF EXISTS books;
        )");
    }

    // books：书架主表，以 (source_id, book_id) 为主键
    // toc：目录表，外键关联 books，删除书籍时级联删除目录
    // chapters：章节缓存表，外键关联 books，删除书籍时级联删除章节
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

// ── 书架操作 ─────────────

/// 将书籍信息写入 books 表；若已存在则更新除 added_at 之外的全部字段。
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

/// 从书架删除指定书籍（外键级联会自动删除对应的 toc 和 chapters）。
/// @return 是否删除了一行
bool Database::remove_book(const std::string& source_id, const std::string& book_id) {
    SQLite::Statement stmt(*db_, "DELETE FROM books WHERE source_id=? AND book_id=?");
    stmt.bind(1, source_id);
    stmt.bind(2, book_id);
    return stmt.exec() > 0;
}

/// 列出指定书源的书架，按添加时间倒序。
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

/// 按 source_id + book_id 精确查询单本书。
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

/// 判断指定书籍是否已存在于书架。
bool Database::is_in_bookshelf(const std::string& source_id, const std::string& book_id) {
    SQLite::Statement stmt(*db_,
        "SELECT 1 FROM books WHERE source_id=? AND book_id=? LIMIT 1");
    stmt.bind(1, source_id);
    stmt.bind(2, book_id);
    return stmt.executeStep();
}

// ── 目录操作 ─────────────

/// 保存书籍目录：在事务内先清除旧目录，再批量插入新条目。
/// sort_order 字段用于保持原始目录顺序。
void Database::save_toc(
    const std::string& source_id,
    const std::string& book_id,
    const std::vector<TocItem>& toc) {
    try {
        SQLite::Transaction tx(*db_);
        // 先删除该书的旧目录
        SQLite::Statement del_stmt(*db_, "DELETE FROM toc WHERE source_id=? AND book_id=?");
        del_stmt.bind(1, source_id);
        del_stmt.bind(2, book_id);
        del_stmt.exec();

        // 批量插入新目录条目
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
            stmt.bind(8, i);            // sort_order：数组下标即顺序
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

/// 获取书籍目录，按 sort_order 升序返回。
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

/// 返回指定书籍的目录条目总数。
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

// ── 章节缓存操作 ─────────────

/// 保存单章内容到 chapters 表（INSERT OR REPLACE：已存在则覆盖）。
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

/// 按 source_id + item_id 获取已缓存的章节内容。
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

/// 判断指定章节是否已在本地缓存。
bool Database::chapter_cached(const std::string& source_id, const std::string& item_id) {
    SQLite::Statement stmt(*db_,
        "SELECT 1 FROM chapters WHERE source_id=? AND item_id=? LIMIT 1");
    stmt.bind(1, source_id);
    stmt.bind(2, item_id);
    return stmt.executeStep();
}

/// 获取指定书籍已缓存的章节数量，用于计算下载进度。
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
