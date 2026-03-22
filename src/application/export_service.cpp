#include "application/export_service.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

#include "application/download_service.h"
#include "export/epub_exporter.h"
#include "export/txt_exporter.h"

namespace novel {

namespace {

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

    start = std::clamp(start, 0, static_cast<int>(toc.size()) - 1);
    end = std::clamp(end, 0, static_cast<int>(toc.size()) - 1);
    if (start > end) {
        std::swap(start, end);
    }

    auto chapters = download_service_->collect_chapters(
        book, toc, start, end, std::move(prepare_progress_cb));
    auto suffix = make_range_suffix(start, end, static_cast<int>(toc.size()));

    if (as_epub) {
        EpubOptions opts;
        opts.output_dir = output_dir;
        opts.filename_suffix = suffix;
        return EpubExporter::export_book(book, chapters, opts, std::move(export_progress_cb));
    }

    TxtOptions opts;
    opts.output_dir = output_dir;
    opts.filename_suffix = suffix;
    return TxtExporter::export_book(book, chapters, opts, std::move(export_progress_cb));
}

} // namespace novel
