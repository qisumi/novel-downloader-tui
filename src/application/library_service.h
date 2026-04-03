#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "db/database.h"
#include "models/book.h"

namespace novel {

class SourceManager;

/// 书库服务 —— 封装搜索、书架管理、目录加载、章节缓存等核心业务逻辑。
///
/// 所有数据库操作均附带当前书源 ID，实现多书源数据隔离。
class LibraryService {
public:
    /// 构造书库服务
    /// \param source_manager 书源管理器，用于获取当前书源及执行搜索/目录查询
    /// \param db             数据库实例，用于持久化书籍、目录、章节
    LibraryService(std::shared_ptr<SourceManager> source_manager, std::shared_ptr<Database> db);

    /// 搜索书籍（委托给当前书源）
    /// \param keywords 搜索关键词
    /// \param page     页码（从 0 开始）
    std::vector<Book> search_books(const std::string& keywords, int page = 0);

    /// 将书籍添加到书架（持久化到数据库）
    void save_to_bookshelf(const Book& book);

    /// 从书架移除书籍
    /// \return 是否成功移除
    bool remove_from_bookshelf(const std::string& book_id);

    /// 列出当前书源下书架中的所有书籍
    std::vector<Book> list_bookshelf();

    /// 加载目录。优先从本地数据库读取；若 force_remote 为 true 或本地无缓存则在线拉取。
    /// 在线拉取成功后会自动持久化书籍信息和目录。
    /// \param book          书籍信息
    /// \param force_remote  是否强制从远程拉取
    std::vector<TocItem> load_toc(const Book& book, bool force_remote);

    /// 获取指定书籍在数据库中的目录条数
    int toc_count(const std::string& book_id);

    /// 获取指定书籍已缓存的章节数量
    int cached_chapter_count(const std::string& book_id);

    /// 判断指定章节是否已缓存
    bool chapter_cached(const std::string& item_id);

    /// 从缓存中读取指定章节内容
    /// \return 章节内容，未命中缓存时返回 std::nullopt
    std::optional<Chapter> get_cached_chapter(const std::string& item_id);

    /// 将章节内容写入缓存
    void save_chapter(const std::string& book_id, const Chapter& chapter);

    /// 获取当前书源 ID
    std::string current_source_id() const;

    /// 获取当前书源名称
    std::string current_source_name() const;

private:
    std::shared_ptr<SourceManager> source_manager_;  ///< 书源管理器
    std::shared_ptr<Database>      db_;              ///< 数据库实例
};

} // namespace novel
