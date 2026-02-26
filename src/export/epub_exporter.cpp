#include "export/epub_exporter.h"
#include "export/text_sanitizer.h"
#include <tinyxml2.h>
#include <zip.h>
#include <filesystem>
#include <sstream>
#include <fstream>
#include <cstring>
#include <cstdlib>
#include <stdexcept>
#include <chrono>
#include <iomanip>
#include <cctype>

namespace fs = std::filesystem;
using namespace tinyxml2;

namespace fanqie {

// ──────────────────────────────────────────────────────────────────────────────
// 辅助
// ──────────────────────────────────────────────────────────────────────────────

static std::string xml_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '&':  out += "&amp;";  break;
            case '<':  out += "&lt;";   break;
            case '>':  out += "&gt;";   break;
            case '"':  out += "&quot;"; break;
            case '\'': out += "&apos;"; break;
            default:   out += c;
        }
    }
    return out;
}

static std::string trim_ascii(std::string s) {
    auto is_space = [](unsigned char c) { return std::isspace(c) != 0; };
    size_t start = 0;
    while (start < s.size() && is_space(static_cast<unsigned char>(s[start]))) {
        ++start;
    }
    size_t end = s.size();
    while (end > start && is_space(static_cast<unsigned char>(s[end - 1]))) {
        --end;
    }
    return s.substr(start, end - start);
}

static std::string clean_inline_text(const std::string& raw) {
    std::string cleaned = text_sanitizer::html_to_plain_text(raw);
    for (char& c : cleaned) {
        if (c == '\n' || c == '\r') c = ' ';
    }
    return trim_ascii(cleaned);
}

struct EpubChapterView {
    std::string title;
    std::string content;
};

static EpubChapterView make_epub_chapter_view(const Chapter& ch) {
    EpubChapterView view;
    view.title = clean_inline_text(ch.title);

    std::string content = text_sanitizer::html_to_plain_text(ch.content);
    std::istringstream iss(content);
    std::string first_line;
    if (!std::getline(iss, first_line)) {
        view.content = content;
        if (view.title.empty()) view.title = "未命名章节";
        return view;
    }

    const std::string normalized_first_line = clean_inline_text(first_line);
    if (view.title.empty() && !normalized_first_line.empty()) {
        view.title = normalized_first_line;
    }

    const bool should_drop_first_line =
        !normalized_first_line.empty() && normalized_first_line == view.title;

    if (!should_drop_first_line) {
        view.content = content;
    } else {
        std::string body;
        std::string line;
        while (std::getline(iss, line)) {
            if (line.empty()) continue;
            body += line;
            body += "\n";
        }
        view.content = body;
    }

    if (view.title.empty()) view.title = "未命名章节";
    return view;
}

static std::string today_iso() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%d");
    return ss.str();
}

static zip_source_t* zip_source_from_string_copy(zip_t* za,
                                                 const std::string& content) {
    void* data = nullptr;
    if (!content.empty()) {
        data = std::malloc(content.size());
        if (!data) return nullptr;
        std::memcpy(data, content.data(), content.size());
    }
    return zip_source_buffer(za, data, content.size(), 1);
}

static bool zip_add_str(zip_t* za,
                               const std::string& path,
                               const std::string& content) {
    zip_source_t* src = zip_source_from_string_copy(za, content);
    if (!src) return false;
    if (zip_file_add(za, path.c_str(), src, ZIP_FL_OVERWRITE | ZIP_FL_ENC_UTF_8) < 0) {
        zip_source_free(src);
        return false;
    }
    return true;
}

// ──────────────────────────────────────────────────────────────────────────────
// mimetype（必须是 EPUB 首个文件，且不压缩）
// ──────────────────────────────────────────────────────────────────────────────
std::string EpubExporter::make_mimetype() {
    return "application/epub+zip";
}

// ──────────────────────────────────────────────────────────────────────────────
// META-INF/container.xml
// ──────────────────────────────────────────────────────────────────────────────
std::string EpubExporter::make_container_xml() {
    return R"(<?xml version="1.0" encoding="UTF-8"?>
<container version="1.0"
    xmlns="urn:oasis:names:tc:opendocument:xmlns:container">
  <rootfiles>
    <rootfile full-path="OEBPS/content.opf"
              media-type="application/oebps-package+xml"/>
  </rootfiles>
</container>)";
}

// ──────────────────────────────────────────────────────────────────────────────
// OEBPS/content.opf — OPF 包文档（EPUB 3 with EPUB 2 fallbacks）
// ──────────────────────────────────────────────────────────────────────────────
std::string EpubExporter::make_opf(const Book& book,
                                   const std::vector<Chapter>& chapters) {
    const auto clean_title = clean_inline_text(book.title);
    const auto clean_author = clean_inline_text(book.author);
    const auto clean_abstract = clean_inline_text(book.abstract);
    std::ostringstream s;
    s << R"(<?xml version="1.0" encoding="UTF-8"?>
<package version="3.0" xmlns="http://www.idpf.org/2007/opf"
         unique-identifier="book-id" xml:lang="zh-CN">
  <metadata xmlns:dc="http://purl.org/dc/elements/1.1/"
            xmlns:opf="http://www.idpf.org/2007/opf">
    <dc:identifier id="book-id">fanqie-)" << xml_escape(book.book_id) << R"(</dc:identifier>
    <dc:title>)" << xml_escape(clean_title) << R"(</dc:title>
    <dc:creator>)" << xml_escape(clean_author) << R"(</dc:creator>
    <dc:language>zh-CN</dc:language>
    <dc:date>)" << today_iso() << R"(</dc:date>
    <dc:description>)" << xml_escape(clean_abstract) << R"(</dc:description>
    <meta property="dcterms:modified">)" << today_iso() << R"(T00:00:00Z</meta>
  </metadata>
  <manifest>
    <item id="nav" href="nav.xhtml" media-type="application/xhtml+xml" properties="nav"/>
    <item id="ncx" href="toc.ncx"   media-type="application/x-dtbncx+xml"/>
    <item id="css" href="style.css"  media-type="text/css"/>
    <item id="cover" href="cover.xhtml" media-type="application/xhtml+xml"/>
)";
    for (int i = 0; i < static_cast<int>(chapters.size()); ++i) {
        s << "    <item id=\"ch" << i << "\" href=\"chapter_" << i
          << ".xhtml\" media-type=\"application/xhtml+xml\"/>\n";
    }
    s << R"(  </manifest>
  <spine toc="ncx">
    <itemref idref="cover"/>
    <itemref idref="nav"/>
)";
    for (int i = 0; i < static_cast<int>(chapters.size()); ++i) {
        s << "    <itemref idref=\"ch" << i << "\"/>\n";
    }
    s << "  </spine>\n</package>\n";
    return s.str();
}

// ──────────────────────────────────────────────────────────────────────────────
// OEBPS/toc.ncx — EPUB 2 兼容目录
// ──────────────────────────────────────────────────────────────────────────────
std::string EpubExporter::make_ncx(const Book& book,
                                   const std::vector<Chapter>& chapters) {
    const auto clean_title = clean_inline_text(book.title);
    std::ostringstream s;
    s << R"(<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE ncx PUBLIC "-//NISO//DTD ncx 2005-1//EN"
   "http://www.daisy.org/z3986/2005/ncx-2005-1.dtd">
<ncx xmlns="http://www.daisy.org/z3986/2005/ncx/" version="2005-1">
  <head>
    <meta name="dtb:uid" content="fanqie-)" << xml_escape(book.book_id) << R"("/>
    <meta name="dtb:depth" content="1"/>
    <meta name="dtb:totalPageCount" content="0"/>
    <meta name="dtb:maxPageNumber" content="0"/>
  </head>
    <docTitle><text>)" << xml_escape(clean_title) << R"(</text></docTitle>
  <navMap>
)";
    for (int i = 0; i < static_cast<int>(chapters.size()); ++i) {
                const auto view = make_epub_chapter_view(chapters[i]);
        s << "    <navPoint id=\"nav" << i << "\" playOrder=\"" << (i + 1) << "\">\n"
                    << "      <navLabel><text>" << xml_escape(view.title) << "</text></navLabel>\n"
          << "      <content src=\"chapter_" << i << ".xhtml\"/>\n"
          << "    </navPoint>\n";
    }
    s << "  </navMap>\n</ncx>\n";
    return s.str();
}

// ──────────────────────────────────────────────────────────────────────────────
// OEBPS/nav.xhtml — EPUB 3 导航文件
// ──────────────────────────────────────────────────────────────────────────────
std::string EpubExporter::make_nav_xhtml(const Book& book,
                                         const std::vector<Chapter>& chapters) {
    const auto clean_title = clean_inline_text(book.title);
    std::ostringstream s;
    s << R"(<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE html>
<html xmlns="http://www.w3.org/1999/xhtml"
      xmlns:epub="http://www.idpf.org/2007/ops" lang="zh-CN">
<head><meta charset="UTF-8"/>
<title>目录 — )" << xml_escape(clean_title) << R"(</title>
<link rel="stylesheet" href="style.css"/>
</head>
<body>
<nav epub:type="toc" id="toc"><h1>目录</h1><ol>
)";
    for (int i = 0; i < static_cast<int>(chapters.size()); ++i) {
                const auto view = make_epub_chapter_view(chapters[i]);
        s << "<li><a href=\"chapter_" << i << ".xhtml\">"
                    << xml_escape(view.title) << "</a></li>\n";
    }
    s << "</ol></nav>\n</body>\n</html>\n";
    return s.str();
}

// ──────────────────────────────────────────────────────────────────────────────
// OEBPS/chapter_N.xhtml — 章节正文
// ──────────────────────────────────────────────────────────────────────────────
std::string EpubExporter::make_chapter_xhtml(const Chapter& ch, int /*index*/) {
    const auto view = make_epub_chapter_view(ch);
    std::ostringstream s;
    s << R"(<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE html>
<html xmlns="http://www.w3.org/1999/xhtml" lang="zh-CN">
<head><meta charset="UTF-8"/>
<title>)" << xml_escape(view.title) << R"(</title>
<link rel="stylesheet" href="style.css"/>
</head>
<body>
<h2 class="chapter-title">)" << xml_escape(view.title) << R"(</h2>
)";
    // 按换行拆段
    std::istringstream iss(view.content);
    std::string line;
    while (std::getline(iss, line)) {
        if (!line.empty())
            s << "<p>" << xml_escape(line) << "</p>\n";
    }
    s << "</body>\n</html>\n";
    return s.str();
}

// ──────────────────────────────────────────────────────────────────────────────
// OEBPS/cover.xhtml
// ──────────────────────────────────────────────────────────────────────────────
std::string EpubExporter::make_cover_xhtml(const Book& book) {
    const auto clean_title = clean_inline_text(book.title);
    const auto clean_author = clean_inline_text(book.author);
    const auto clean_abstract = clean_inline_text(book.abstract);
    return R"(<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE html>
<html xmlns="http://www.w3.org/1999/xhtml" lang="zh-CN">
<head><meta charset="UTF-8"/><title>封面</title>
<link rel="stylesheet" href="style.css"/>
</head>
<body class="cover-page">
<div class="cover-box">
    <h1 class="book-title">)" + xml_escape(clean_title) + R"(</h1>
    <p class="book-author">)" + xml_escape(clean_author) + R"(</p>
    <p class="book-abstract">)" + xml_escape(clean_abstract) + R"(</p>
</div>
</body>
</html>)";
}

// ──────────────────────────────────────────────────────────────────────────────
// OEBPS/style.css
// ──────────────────────────────────────────────────────────────────────────────
std::string EpubExporter::make_stylesheet() {
    return R"(body {
  font-family: "Noto Serif CJK SC", "Source Han Serif SC", serif;
  font-size: 1em;
  line-height: 1.8;
  margin: 1em 2em;
  color: #222;
  background: #fafaf8;
}
h1, h2 { color: #333; }
p { text-indent: 2em; margin: 0.4em 0; }
.cover-page { text-align: center; padding-top: 20%; }
.book-title  { font-size: 2em; }
.book-author { font-size: 1.2em; color: #666; }
.book-abstract { font-size: 0.9em; color: #888; margin-top: 2em; max-width: 36em; display: inline-block; text-align: left; }
.chapter-title { margin: 1em 0; border-bottom: 1px solid #ccc; padding-bottom: 0.3em; }
)";
}

// ──────────────────────────────────────────────────────────────────────────────
// 主导出函数
// ──────────────────────────────────────────────────────────────────────────────
std::string EpubExporter::export_book(const Book& book,
                                      const std::vector<Chapter>& chapters,
                                      const EpubOptions& opts,
                                      std::function<void(int, int)> progress_cb) {
    // 确保输出目录存在
    fs::create_directories(opts.output_dir);

    // 构建输出路径（非法字符替换）
    std::string safe_title = text_sanitizer::sanitize_filename(book.title);
    fs::path out_path = fs::path(opts.output_dir) /
                        (safe_title + opts.filename_suffix + ".epub");

    int err = 0;
    zip_t* za = zip_open(out_path.string().c_str(), ZIP_CREATE | ZIP_TRUNCATE, &err);
    if (!za) return {};

    // ── mimetype（必须第一个，不压缩）──────────────────────────
    {
        std::string mt = make_mimetype();
        zip_source_t* src = zip_source_from_string_copy(za, mt);
        if (!src) {
            zip_discard(za);
            return {};
        }
        zip_int64_t idx = zip_file_add(za, "mimetype", src,
                                       ZIP_FL_OVERWRITE | ZIP_FL_ENC_UTF_8);
        if (idx < 0) {
            zip_source_free(src);
            zip_discard(za);
            return {};
        }
        if (zip_set_file_compression(za, idx, ZIP_CM_STORE, 0) < 0) {
            zip_discard(za);
            return {};
        }
    }

    // ── META-INF/ ─────────────────────────────────────────────
    if (!zip_add_str(za, "META-INF/container.xml", make_container_xml())) {
        zip_discard(za);
        return {};
    }

    // ── OEBPS/ ───────────────────────────────────────────────
    if (!zip_add_str(za, "OEBPS/content.opf", make_opf(book, chapters)) ||
        !zip_add_str(za, "OEBPS/toc.ncx",     make_ncx(book, chapters)) ||
        !zip_add_str(za, "OEBPS/nav.xhtml",   make_nav_xhtml(book, chapters)) ||
        !zip_add_str(za, "OEBPS/cover.xhtml", make_cover_xhtml(book)) ||
        !zip_add_str(za, "OEBPS/style.css",   make_stylesheet())) {
        zip_discard(za);
        return {};
    }

    int total = static_cast<int>(chapters.size());
    for (int i = 0; i < total; ++i) {
        std::string fname = "OEBPS/chapter_" + std::to_string(i) + ".xhtml";
        if (!zip_add_str(za, fname, make_chapter_xhtml(chapters[i], i))) {
            zip_discard(za);
            return {};
        }
        if (progress_cb) progress_cb(i + 1, total);
    }

    if (zip_close(za) < 0) {
        zip_discard(za);
        return {};
    }
    return fs::absolute(out_path).string();
}

} // namespace fanqie
