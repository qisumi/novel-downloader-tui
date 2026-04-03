#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <SQLiteCpp/SQLiteCpp.h>

#include "models/book.h"

namespace novel {

/// 数据库访问层 —— 负责书架、目录、章节内容的持久化存储。
///
/// 所有数据均以 (source_id, book_id/item_id) 复合主键隔离，
/// 支持多书源并存，删除书籍时通过外键级联自动清理关联的目录和章节。
class Database {
public:
    /// 构造并打开数据库。若文件不存在则自动创建。
    /// @param db_path 数据库文件路径，默认为 "novel.db"
    explicit Database(const std::string& db_path = "novel.db");

    // ── 书架操作 ─────────────

    /// 保存或更新一本书到书架（UPSERT 语义）
    void save_book(const std::string& source_id, const Book& book);

    /// 从书架移除一本书，同时级联删除其目录与章节缓存
    /// @return 是否确实删除了一行
    bool remove_book(const std::string& source_id, const std::string& book_id);

    /// 列出指定书源下的全部书架书籍，按添加时间倒序排列
    std::vector<Book> list_bookshelf(const std::string& source_id);

    /// 按 source_id + book_id 查询单本书，不存在则返回 std::nullopt
    std::optional<Book> get_book(const std::string& source_id, const std::string& book_id);

    /// 判断指定书籍是否已在书架中
    bool is_in_bookshelf(const std::string& source_id, const std::string& book_id);

    // ── 目录操作 ─────────────

    /// 保存书籍目录（事务内先删除旧目录再批量插入）
    void save_toc(
        const std::string& source_id,
        const std::string& book_id,
        const std::vector<TocItem>& toc);

    /// 获取书籍目录，按 sort_order 升序返回
    std::vector<TocItem> get_toc(const std::string& source_id, const std::string& book_id);

    /// 获取指定书籍的目录条目总数
    int toc_count(const std::string& source_id, const std::string& book_id);

    // ── 章节缓存操作 ─────────────

    /// 保存单章内容（INSERT OR REPLACE 语义）
    void save_chapter(
        const std::string& source_id,
        const std::string& book_id,
        const Chapter& ch);

    /// 按 source_id + item_id 获取已缓存的章节，不存在则返回 std::nullopt
    std::optional<Chapter> get_chapter(const std::string& source_id, const std::string& item_id);

    /// 判断指定章节是否已缓存
    bool chapter_cached(const std::string& source_id, const std::string& item_id);

    /// 获取指定书籍已缓存的章节数量
    int cached_chapter_count(const std::string& source_id, const std::string& book_id);

private:
    std::unique_ptr<SQLite::Database> db_;

    /// 初始化数据库表结构；若检测到旧版无 source_id 的表则自动重建
    void init_schema();
};

} // namespace novel
