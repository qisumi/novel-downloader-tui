/// @file epub_exporter.cpp
/// @brief EPUB 导出器实现
///
/// 完整流程：
///   1. 创建输出目录，构建安全文件名
///   2. 创建 ZIP 归档，写入 mimetype（不压缩）
///   3. 写入 META-INF/container.xml
///   4. 生成并写入 OEBPS/ 下的所有内容文件（OPF、NCX、导航、封面、样式、章节）
///   5. 关闭 ZIP 归档，返回绝对路径

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
#include <string_view>
#include <chrono>
#include <iomanip>
#include <cctype>

namespace fs = std::filesystem;
using namespace tinyxml2;

namespace novel {

// ──────────────────────────────────────────────────────────────────────────────
// 辅助工具函数
// ──────────────────────────────────────────────────────────────────────────────

static fs::path path_from_utf8(std::string_view value) {
    return fs::path(std::u8string(reinterpret_cast<const char8_t*>(value.data()),
                                  reinterpret_cast<const char8_t*>(value.data()) + value.size()));
}

static std::string path_to_utf8(const fs::path& path) {
    const auto utf8 = path.u8string();
    return std::string(reinterpret_cast<const char*>(utf8.data()), utf8.size());
}

static zip_t* open_zip_archive(const fs::path& out_path) {
#ifdef _WIN32
    zip_error_t error;
    zip_error_init(&error);

    zip_source_t* source = zip_source_win32w_create(out_path.c_str(), 0, -1, &error);
    if (!source) {
        zip_error_fini(&error);
        return nullptr;
    }

    zip_t* archive = zip_open_from_source(source, ZIP_CREATE | ZIP_TRUNCATE, &error);
    if (!archive) {
        zip_source_free(source);
        zip_error_fini(&error);
        return nullptr;
    }

    zip_error_fini(&error);
    return archive;
#else
    int err = 0;
    return zip_open(out_path.string().c_str(), ZIP_CREATE | ZIP_TRUNCATE, &err);
#endif
}

/// XML 属性值转义：将 & < > " ' 替换为对应实体
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

/// 去除字符串首尾的 ASCII 空白字符
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

/// 清洗富文本为适合内联使用的纯文本（将换行替换为空格）
static std::string clean_inline_text(const std::string& raw) {
    std::string cleaned = text_sanitizer::html_to_plain_text(raw);
    for (char& c : cleaned) {
        if (c == '\n' || c == '\r') c = ' ';
    }
    return trim_ascii(cleaned);
}

/// EPUB 章节视图：保存清洗后的标题与正文
struct EpubChapterView {
    std::string title;    ///< 章节标题（已清洗）
    std::string content;  ///< 章节正文（已清洗）
};

/// 将原始 Chapter 转换为 EpubChapterView
///
/// 自动处理标题提取逻辑：
///   - 优先使用 chapter.title
///   - 若 title 为空则取正文首行作为标题
///   - 若首行与标题相同则从正文中去除首行（避免重复显示）
static EpubChapterView make_epub_chapter_view(const Chapter& ch) {
    EpubChapterView view;
    view.title = clean_inline_text(ch.title);

    // 将 HTML 正文转换为纯文本
    std::string content = text_sanitizer::html_to_plain_text(ch.content);
    std::istringstream iss(content);
    std::string first_line;
    if (!std::getline(iss, first_line)) {
        view.content = content;
        if (view.title.empty()) view.title = "未命名章节";
        return view;
    }

    const std::string normalized_first_line = clean_inline_text(first_line);
    // 若标题为空，使用首行作为标题
    if (view.title.empty() && !normalized_first_line.empty()) {
        view.title = normalized_first_line;
    }

    // 若首行与标题完全相同，则跳过首行避免正文重复显示
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

/// 获取当前 UTC 日期，格式为 YYYY-MM-DD
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

/// 创建 zip_source，复制一份字符串数据交给 libzip 管理
///
/// 使用 malloc 复制数据，设置 free 标志让 libzip 在用完后自动释放
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

/// 向 ZIP 归档中添加一个字符串文件
///
/// @param za      ZIP 归档句柄
/// @param path    归档内的文件路径
/// @param content 文件内容
/// @return 添加成功返回 true
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

static bool zip_add_blob(zip_t* za,
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
// META-INF/container.xml — 指向 OPF 包文档的入口
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
//
// 声明所有资源（manifest）和阅读顺序（spine），包含元数据（标题、作者、语言等）
// ──────────────────────────────────────────────────────────────────────────────
std::string EpubExporter::make_opf(const Book& book,
                                   const std::vector<Chapter>& chapters,
                                   const EpubOptions& opts) {
    const auto clean_title = clean_inline_text(book.title);
    const auto clean_author = clean_inline_text(book.author);
    const auto clean_abstract = clean_inline_text(book.abstract);
    std::ostringstream s;
    s << R"(<?xml version="1.0" encoding="UTF-8"?>
<package version="3.0" xmlns="http://www.idpf.org/2007/opf"
         unique-identifier="book-id" xml:lang="zh-CN">
  <metadata xmlns:dc="http://purl.org/dc/elements/1.1/"
            xmlns:opf="http://www.idpf.org/2007/opf">
    <dc:identifier id="book-id">novel-)" << xml_escape(book.book_id) << R"(</dc:identifier>
    <dc:title>)" << xml_escape(clean_title) << R"(</dc:title>
    <dc:creator>)" << xml_escape(clean_author) << R"(</dc:creator>
    <dc:language>zh-CN</dc:language>
    <dc:date>)" << today_iso() << R"(</dc:date>
    <dc:description>)" << xml_escape(clean_abstract) << R"(</dc:description>
)";
    if (opts.has_cover_image()) {
        s << R"(    <meta name="cover" content="cover-image"/>
)";
    }
    s << R"(    <meta property="dcterms:modified">)" << today_iso() << R"(T00:00:00Z</meta>
  </metadata>
  <manifest>
    <item id="nav" href="nav.xhtml" media-type="application/xhtml+xml" properties="nav"/>
    <item id="ncx" href="toc.ncx"   media-type="application/x-dtbncx+xml"/>
    <item id="css" href="style.css"  media-type="text/css"/>
    <item id="cover" href="cover.xhtml" media-type="application/xhtml+xml"/>
)";
    if (opts.has_cover_image()) {
        s << "    <item id=\"cover-image\" href=\""
          << xml_escape(opts.cover_image_filename)
          << "\" media-type=\""
          << xml_escape(opts.cover_image_media_type)
          << "\" properties=\"cover-image\"/>\n";
    }
    // 在 manifest 中声明每个章节文件
    for (int i = 0; i < static_cast<int>(chapters.size()); ++i) {
        s << "    <item id=\"ch" << i << "\" href=\"chapter_" << i
          << ".xhtml\" media-type=\"application/xhtml+xml\"/>\n";
    }
    s << R"(  </manifest>
  <spine toc="ncx">
    <itemref idref="cover"/>
)";
    if (opts.include_toc_page) {
        s << "    <itemref idref=\"nav\"/>\n";
    }
    // 在 spine 中按顺序列出每个章节
    for (int i = 0; i < static_cast<int>(chapters.size()); ++i) {
        s << "    <itemref idref=\"ch" << i << "\"/>\n";
    }
    s << "  </spine>\n</package>\n";
    return s.str();
}

// ──────────────────────────────────────────────────────────────────────────────
// OEBPS/toc.ncx — EPUB 2 兼容目录
//
// 为旧版阅读器提供导航信息，每个 navPoint 对应一个章节
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
    <meta name="dtb:uid" content="novel-)" << xml_escape(book.book_id) << R"("/>
    <meta name="dtb:depth" content="1"/>
    <meta name="dtb:totalPageCount" content="0"/>
    <meta name="dtb:maxPageNumber" content="0"/>
  </head>
    <docTitle><text>)" << xml_escape(clean_title) << R"(</text></docTitle>
  <navMap>
)";
    // 为每个章节生成一个导航点
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
//
// 使用 HTML <nav epub:type="toc"> 结构，供 EPUB 3 阅读器解析目录
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
    // 生成目录列表项
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
//
// 每个章节生成独立的 XHTML 文件，正文按行拆分为 <p> 段落
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
    // 按换行拆段，每行生成一个 <p> 元素
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
// OEBPS/cover.xhtml — 封面页
//
// 显示书名、作者和简介，作为 EPUB 打开后的第一页
// ──────────────────────────────────────────────────────────────────────────────
std::string EpubExporter::make_cover_xhtml(const Book& book, const EpubOptions& opts) {
    const auto clean_title = clean_inline_text(book.title);
    const auto clean_author = clean_inline_text(book.author);
    const auto clean_abstract = clean_inline_text(book.abstract);
    const auto cover_markup = opts.has_cover_image()
        ? R"(<div class="cover-art"><img src=")" + xml_escape(opts.cover_image_filename) + R"(" alt="封面"/></div>)"
        : std::string{};
    return R"(<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE html>
<html xmlns="http://www.w3.org/1999/xhtml" lang="zh-CN">
<head><meta charset="UTF-8"/><title>封面</title>
<link rel="stylesheet" href="style.css"/>
</head>
<body class="cover-page">
<div class="cover-box">
)" + cover_markup + R"(
    <h1 class="book-title">)" + xml_escape(clean_title) + R"(</h1>
    <p class="book-author">)" + xml_escape(clean_author) + R"(</p>
    <p class="book-abstract">)" + xml_escape(clean_abstract) + R"(</p>
</div>
</body>
</html>)";
}

// ──────────────────────────────────────────────────────────────────────────────
// OEBPS/style.css — 全局样式表
//
// 定义封面页、目录页和章节正文的默认样式
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
.cover-art { margin: 0 auto 2em; max-width: 20em; }
.cover-art img { display: block; width: 100%; height: auto; border-radius: 0.5em; box-shadow: 0 1em 2em rgba(0, 0, 0, 0.18); }
.book-title  { font-size: 2em; }
.book-author { font-size: 1.2em; color: #666; }
.book-abstract { font-size: 0.9em; color: #888; margin-top: 2em; max-width: 36em; display: inline-block; text-align: left; }
.chapter-title { margin: 1em 0; border-bottom: 1px solid #ccc; padding-bottom: 0.3em; }
)";
}

// ──────────────────────────────────────────────────────────────────────────────
// 主导出函数
//
// 按 EPUB 标准顺序将所有文件写入 ZIP 归档：
//   1. mimetype（不压缩，必须为首个文件）
//   2. META-INF/container.xml
//   3. OEBPS/ 下的所有内容文件
// ──────────────────────────────────────────────────────────────────────────────
std::string EpubExporter::export_book(const Book& book,
                                      const std::vector<Chapter>& chapters,
                                      const EpubOptions& opts,
                                      std::function<void(int, int)> progress_cb) {
    // 确保输出目录存在
    fs::path output_dir = path_from_utf8(opts.output_dir);
    fs::create_directories(output_dir);

    // 构建输出路径（非法字符替换）
    std::string safe_title = text_sanitizer::sanitize_filename(book.title);
    fs::path out_path = output_dir /
                        path_from_utf8(safe_title + opts.filename_suffix + ".epub");

    // 创建 ZIP 归档（如果已存在则覆盖）
    zip_t* za = open_zip_archive(out_path);
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
        // mimetype 文件必须使用 STORE 方式（不压缩），符合 EPUB 规范
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
    // 写入元数据文件：OPF 包文档、NCX 目录、EPUB3 导航、封面页、样式表
    if (!zip_add_str(za, "OEBPS/content.opf", make_opf(book, chapters, opts)) ||
        !zip_add_str(za, "OEBPS/toc.ncx",     make_ncx(book, chapters)) ||
        !zip_add_str(za, "OEBPS/nav.xhtml",   make_nav_xhtml(book, chapters)) ||
        !zip_add_str(za, "OEBPS/cover.xhtml", make_cover_xhtml(book, opts)) ||
        !zip_add_str(za, "OEBPS/style.css",   make_stylesheet())) {
        zip_discard(za);
        return {};
    }
    if (opts.has_cover_image()
        && !zip_add_blob(za, "OEBPS/" + opts.cover_image_filename, opts.cover_image_data)) {
        zip_discard(za);
        return {};
    }

    // 写入各章节 XHTML 文件
    int total = static_cast<int>(chapters.size());
    for (int i = 0; i < total; ++i) {
        std::string fname = "OEBPS/chapter_" + std::to_string(i) + ".xhtml";
        if (!zip_add_str(za, fname, make_chapter_xhtml(chapters[i], i))) {
            zip_discard(za);
            return {};
        }
        if (progress_cb) progress_cb(i + 1, total);
    }

    // 关闭 ZIP 归档（将数据写入磁盘）
    if (zip_close(za) < 0) {
        zip_discard(za);
        return {};
    }
    return path_to_utf8(fs::absolute(out_path));
}

} // namespace novel
