/// @file epub_exporter.h
/// @brief EPUB 格式导出器
///
/// 使用 tinyxml2 生成 OPF / NCX / XHTML 内容文件，
/// 使用 libzip 将所有文件打包成符合 EPUB 2.0/3.0 标准的 .epub 电子书。

#pragma once
#include <string>
#include <vector>
#include <functional>
#include "models/book.h"

namespace novel {

/// EPUB 导出选项
struct EpubOptions {
    bool include_toc_page = true;    ///< 是否生成 NCX/导航页
    bool split_by_volume  = false;   ///< 按卷分文件（暂未实现，预留）
    std::string output_dir = ".";    ///< 输出目录
    std::string filename_suffix;      ///< 输出文件名后缀（如 _ch001-010）
};

/// EPUB 2.0 / 3.0 导出器
///
/// 生成标准 EPUB 结构：
///   - mimetype             （不压缩，必须为首个文件）
///   - META-INF/container.xml
///   - OEBPS/content.opf    （包文档，声明所有资源与阅读顺序）
///   - OEBPS/toc.ncx        （EPUB 2 兼容目录）
///   - OEBPS/nav.xhtml      （EPUB 3 导航文件）
///   - OEBPS/cover.xhtml    （封面页）
///   - OEBPS/style.css      （全局样式）
///   - OEBPS/chapter_N.xhtml（章节正文）
class EpubExporter {
public:
    /// 导出书籍为 EPUB 文件
    ///
    /// @param book         书籍元数据
    /// @param chapters     已下载的章节列表
    /// @param opts         导出选项（输出目录、文件名后缀等）
    /// @param progress_cb  进度回调 progress_cb(current, total)，每写完一章调用一次
    /// @return 生成的 .epub 文件绝对路径；失败时返回空字符串
    static std::string export_book(
        const Book&              book,
        const std::vector<Chapter>& chapters,
        const EpubOptions&       opts = {},
        std::function<void(int, int)> progress_cb = nullptr);

private:
    // ── 内部生成器 ─────────────────────────────────────────────

    /// 生成 mimetype 文件内容
    static std::string make_mimetype();
    /// 生成 META-INF/container.xml
    static std::string make_container_xml();
    /// 生成 OEBPS/content.opf（OPF 包文档）
    static std::string make_opf(const Book& book, const std::vector<Chapter>& chapters);
    /// 生成 OEBPS/toc.ncx（EPUB 2 兼容目录）
    static std::string make_ncx(const Book& book, const std::vector<Chapter>& chapters);
    /// 生成 OEBPS/nav.xhtml（EPUB 3 导航文件）
    static std::string make_nav_xhtml(const Book& book, const std::vector<Chapter>& chapters);
    /// 生成 OEBPS/chapter_N.xhtml（章节正文 XHTML）
    static std::string make_chapter_xhtml(const Chapter& ch, int index);
    /// 生成 OEBPS/cover.xhtml（封面页）
    static std::string make_cover_xhtml(const Book& book);
    /// 生成 OEBPS/style.css（全局样式表）
    static std::string make_stylesheet();
};

} // namespace novel
