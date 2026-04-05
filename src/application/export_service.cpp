#include "application/export_service.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string_view>

#include "application/download_service.h"
#include "export/epub_exporter.h"
#include "export/txt_exporter.h"
#include "source/host/http_service.h"

namespace novel {

namespace {

struct CoverAsset {
    std::string filename;
    std::string media_type;
    std::string data;
};

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

std::string to_lower_ascii(std::string value) {
    for (char& ch : value) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return value;
}

std::string trim_ascii(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::string normalize_cover_url(std::string url) {
    url = trim_ascii(std::move(url));
    if (url.rfind("//", 0) == 0) {
        return "https:" + url;
    }
    return url;
}

std::string origin_from_url(std::string_view url) {
    const auto scheme_pos = url.find("://");
    if (scheme_pos == std::string::npos) {
        return {};
    }

    const auto host_start = scheme_pos + 3;
    const auto host_end = url.find('/', host_start);
    if (host_end == std::string::npos) {
        return std::string(url);
    }
    return std::string(url.substr(0, host_end));
}

std::string media_type_from_url(std::string_view url) {
    auto trimmed = std::string(url);
    const auto fragment_pos = trimmed.find('#');
    if (fragment_pos != std::string::npos) {
        trimmed.erase(fragment_pos);
    }
    const auto query_pos = trimmed.find('?');
    if (query_pos != std::string::npos) {
        trimmed.erase(query_pos);
    }

    const auto dot_pos = trimmed.find_last_of('.');
    if (dot_pos == std::string::npos) {
        return {};
    }

    const auto ext = to_lower_ascii(trimmed.substr(dot_pos + 1));
    if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
    if (ext == "png") return "image/png";
    if (ext == "gif") return "image/gif";
    if (ext == "webp") return "image/webp";
    if (ext == "svg") return "image/svg+xml";
    return {};
}

std::string media_type_from_signature(const std::string& body) {
    if (body.size() >= 3
        && static_cast<unsigned char>(body[0]) == 0xFF
        && static_cast<unsigned char>(body[1]) == 0xD8
        && static_cast<unsigned char>(body[2]) == 0xFF) {
        return "image/jpeg";
    }
    if (body.size() >= 8
        && body.compare(0, 8, "\x89PNG\r\n\x1A\n", 8) == 0) {
        return "image/png";
    }
    if (body.size() >= 6
        && (body.compare(0, 6, "GIF87a", 6) == 0 || body.compare(0, 6, "GIF89a", 6) == 0)) {
        return "image/gif";
    }
    if (body.size() >= 12
        && body.compare(0, 4, "RIFF", 4) == 0
        && body.compare(8, 4, "WEBP", 4) == 0) {
        return "image/webp";
    }
    if (body.find("<svg") != std::string::npos || body.find("<?xml") == 0) {
        return "image/svg+xml";
    }
    return {};
}

std::string cover_extension_for_media_type(std::string_view media_type) {
    const auto normalized = to_lower_ascii(trim_ascii(std::string(media_type)));
    if (normalized == "image/jpeg" || normalized == "image/jpg") return ".jpg";
    if (normalized == "image/png") return ".png";
    if (normalized == "image/gif") return ".gif";
    if (normalized == "image/webp") return ".webp";
    if (normalized == "image/svg+xml") return ".svg";
    return {};
}

std::string media_type_from_response(const HttpResponse& response, std::string_view cover_url) {
    std::string header_media_type;
    for (const auto& [name, value] : response.headers) {
        if (to_lower_ascii(name) != "content-type") {
            continue;
        }

        header_media_type = value;
        auto media_type = header_media_type;
        const auto semicolon_pos = media_type.find(';');
        if (semicolon_pos != std::string::npos) {
            media_type.erase(semicolon_pos);
        }
        media_type = to_lower_ascii(trim_ascii(media_type));
        if (media_type.rfind("image/", 0) == 0) {
            return media_type;
        }
        break;
    }

    auto media_type = media_type_from_signature(response.body);
    if (!media_type.empty()) {
        return media_type;
    }

    media_type = media_type_from_url(cover_url);
    if (!media_type.empty()) {
        return media_type;
    }

    return to_lower_ascii(trim_ascii(std::move(header_media_type)));
}

std::optional<CoverAsset> fetch_cover_asset(
    const std::shared_ptr<HttpService>& http_service,
    const std::string& cover_url) {
    const auto normalized_cover_url = normalize_cover_url(cover_url);
    if (!http_service || normalized_cover_url.empty()) {
        return std::nullopt;
    }

    std::vector<std::pair<std::string, std::string>> headers{
        {"Accept", "image/jpeg,image/png,image/gif,image/*;q=0.9,*/*;q=0.8"},
        {"User-Agent", "novel-downloader/0.2"},
    };
    if (const auto origin = origin_from_url(normalized_cover_url); !origin.empty()) {
        headers.emplace_back("Referer", origin + "/");
    }

    const auto response = http_service->get(normalized_cover_url, headers, 30);
    if (!response || response->status < 200 || response->status >= 300 || response->body.empty()) {
        return std::nullopt;
    }

    const auto media_type = media_type_from_response(*response, normalized_cover_url);
    if (media_type.rfind("image/", 0) != 0) {
        return std::nullopt;
    }

    const auto extension = cover_extension_for_media_type(media_type);
    if (extension.empty()) {
        return std::nullopt;
    }

    CoverAsset asset;
    asset.filename = "cover" + extension;
    asset.media_type = media_type;
    asset.data = response->body;
    return asset;
}

} // namespace

ExportService::ExportService(
    std::shared_ptr<DownloadService> download_service,
    std::shared_ptr<HttpService> http_service)
    : download_service_(std::move(download_service)),
      http_service_(std::move(http_service)) {}

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
        opts.include_toc_page = false;
        opts.output_dir = output_dir;
        opts.filename_suffix = suffix;
        if (auto cover_asset = fetch_cover_asset(http_service_, book.cover_url)) {
            opts.cover_image_filename = std::move(cover_asset->filename);
            opts.cover_image_media_type = std::move(cover_asset->media_type);
            opts.cover_image_data = std::move(cover_asset->data);
        }
        return EpubExporter::export_book(book, chapters, opts, std::move(export_progress_cb));
    }

    // 导出为 TXT 格式
    TxtOptions opts;
    opts.output_dir = output_dir;
    opts.filename_suffix = suffix;
    return TxtExporter::export_book(book, chapters, opts, std::move(export_progress_cb));
}

} // namespace novel
