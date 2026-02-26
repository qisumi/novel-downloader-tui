#pragma once
#include <string>
#include <vector>
#include <functional>
#include "models/book.h"

namespace fanqie {

struct EpubOptions {
    bool include_toc_page = true;    ///< 是否生成 NCX/导航页
    bool split_by_volume  = false;   ///< 按卷分文件（暂未实现，预留）
    std::string output_dir = ".";    ///< 输出目录
    std::string filename_suffix;      ///< 输出文件名后缀（如 _ch001-010）
};

/// EPUB 2.0 / 3.0 导出器
/// 使用 tinyxml2 生成 OPF / NCX / XHTML，使用 libzip 打包成 .epub
class EpubExporter {
public:
    /// 导出书籍，progress_cb(current, total) 汇报章节写入进度
    /// @return 生成的 .epub 文件绝对路径，失败时返回空字符串
    static std::string export_book(
        const Book&              book,
        const std::vector<Chapter>& chapters,
        const EpubOptions&       opts = {},
        std::function<void(int, int)> progress_cb = nullptr);

private:
    // ── 内部生成器 ─────────────────────────────────────────────
    static std::string make_mimetype();
    static std::string make_container_xml();
    static std::string make_opf(const Book& book, const std::vector<Chapter>& chapters);
    static std::string make_ncx(const Book& book, const std::vector<Chapter>& chapters);
    static std::string make_nav_xhtml(const Book& book, const std::vector<Chapter>& chapters);
    static std::string make_chapter_xhtml(const Chapter& ch, int index);
    static std::string make_cover_xhtml(const Book& book);
    static std::string make_stylesheet();
};

} // namespace fanqie
