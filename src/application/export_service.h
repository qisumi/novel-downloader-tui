#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "models/book.h"

namespace novel {

class DownloadService;
class HttpService;

/// 导出服务 —— 负责将书籍导出为 EPUB 或 TXT 格式。
///
/// 工作流程：
/// 1. 通过 DownloadService 收集指定范围的章节内容
/// 2. 调用对应的导出器（EpubExporter / TxtExporter）生成文件
class ExportService {
public:
    /// 构造导出服务
    /// \param download_service 下载服务，用于收集章节内容
    ExportService(
        std::shared_ptr<DownloadService> download_service,
        std::shared_ptr<HttpService> http_service);

    /// 导出书籍为指定格式文件。
    /// \param book                书籍信息
    /// \param toc                 目录列表
    /// \param start               起始章节索引（含，0-based）
    /// \param end                 结束章节索引（含，0-based）
    /// \param as_epub             true 导出 EPUB，false 导出 TXT
    /// \param output_dir          输出目录
    /// \param prepare_progress_cb 章节收集进度回调 (已完成数, 总数)
    /// \param export_progress_cb  文件导出进度回调 (已完成数, 总数)
    /// \return 导出文件的完整路径，目录为空时返回空字符串
    std::string export_book(
        const Book& book,
        const std::vector<TocItem>& toc,
        int start,
        int end,
        bool as_epub,
        const std::string& output_dir,
        std::function<void(int, int)> prepare_progress_cb = nullptr,
        std::function<void(int, int)> export_progress_cb = nullptr);

private:
    std::shared_ptr<DownloadService> download_service_;  ///< 下载服务实例
    std::shared_ptr<HttpService>     http_service_;      ///< HTTP 服务实例
};

} // namespace novel
