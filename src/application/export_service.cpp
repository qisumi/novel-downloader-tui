#include "application/export_service.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

#include "application/download_service.h"
#include "export/epub_exporter.h"
#include "export/txt_exporter.h"

namespace novel {

namespace {

/// 根据章节范围生成文件名后缀。
/// 当范围为全书（start==0 && end==total-1）时返回空字符串；
/// 否则返回形如 "_ch0001-0050" 的后缀。
std::string make_range_suffix(int start, int end, int total) {
    if (total <= 0 || (start == 0 && end == total - 1)) {
        return {};
    }
    std::ostringstream os;
    os << "_ch" << std::setw(4) << std::setfill('0') << (start + 1)
       << "-" << std::setw(4) << std::setfill('0') << (end + 1);
    return os.str();
}

} // namespace

ExportService::ExportService(std::shared_ptr<DownloadService> download_service)
    : download_service_(std::move(download_service)) {}

// ── 导出书籍 ─────────────

std::string ExportService::export_book(
    const Book& book,
    const std::vector<TocItem>& toc,
    int start,
    int end,
    bool as_epub,
    const std::string& output_dir,
    std::function<void(int, int)> prepare_progress_cb,
    std::function<void(int, int)> export_progress_cb) {
    if (toc.empty()) {
        return {};
    }

    // 钳位起止索引到合法范围，并保证 start <= end
    start = std::clamp(start, 0, static_cast<int>(toc.size()) - 1);
    end = std::clamp(end, 0, static_cast<int>(toc.size()) - 1);
    if (start > end) {
        std::swap(start, end);
    }

    // 收集指定范围的章节内容（优先命中缓存）
    auto chapters = download_service_->collect_chapters(
        book, toc, start, end, std::move(prepare_progress_cb));
    auto suffix = make_range_suffix(start, end, static_cast<int>(toc.size()));

    if (as_epub) {
        // 导出为 EPUB 格式
        EpubOptions opts;
        opts.output_dir = output_dir;
        opts.filename_suffix = suffix;
        return EpubExporter::export_book(book, chapters, opts, std::move(export_progress_cb));
    }

    // 导出为 TXT 格式
    TxtOptions opts;
    opts.output_dir = output_dir;
    opts.filename_suffix = suffix;
    return TxtExporter::export_book(book, chapters, opts, std::move(export_progress_cb));
}

} // namespace novel
