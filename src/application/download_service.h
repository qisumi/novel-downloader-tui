#pragma once

#include <functional>
#include <memory>
#include <vector>

#include "models/book.h"

namespace novel {

class LibraryService;
class IBookSource;
class SourceManager;

/// 下载服务 —— 负责从书源拉取章节内容并缓存到本地数据库。
///
/// 核心能力：
/// - download_book     按目录顺序逐章下载并持久化
/// - collect_chapters  收集指定范围的章节（优先命中缓存，缺失时在线拉取）
class DownloadService {
public:
    /// 构造下载服务
    /// \param source_manager  书源管理器，用于获取当前书源
    /// \param library_service 书库服务，用于章节缓存读写
    DownloadService(
        std::shared_ptr<SourceManager> source_manager,
        std::shared_ptr<LibraryService> library_service);

    /// 下载整本书：遍历目录，跳过已缓存章节，逐章拉取并存储。
    /// \param book        书籍信息
    /// \param toc         目录列表
    /// \param progress_cb 进度回调 (已完成数, 总数)
    void download_book(
        const Book& book,
        const std::vector<TocItem>& toc,
        std::function<void(int, int)> progress_cb = nullptr);

    /// 收集指定范围的章节内容。
    /// 优先从本地缓存读取，缓存未命中则在线拉取并写入缓存。
    /// \param book        书籍信息
    /// \param toc         目录列表
    /// \param start       起始索引（含，0-based）
    /// \param end         结束索引（含，0-based）
    /// \param progress_cb 进度回调 (已完成数, 总数)
    /// \return 范围内所有成功获取的章节列表
    std::vector<Chapter> collect_chapters(
        const Book& book,
        const std::vector<TocItem>& toc,
        int start,
        int end,
        std::function<void(int, int)> progress_cb = nullptr);

private:
    void download_book_chapter_by_chapter(
        IBookSource* source,
        const Book& book,
        const std::vector<TocItem>& toc,
        std::function<void(int, int)> progress_cb);

    void download_book_batch(
        IBookSource* source,
        const Book& book,
        const std::vector<TocItem>& toc,
        std::function<void(int, int)> progress_cb);

    std::shared_ptr<SourceManager>  source_manager_;  ///< 书源管理器
    std::shared_ptr<LibraryService> library_service_;  ///< 书库服务（缓存读写）
};

} // namespace novel
