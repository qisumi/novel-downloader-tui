/// @file txt_exporter.cpp
/// @brief TXT 导出器实现
///
/// 流程：创建输出目录 -> 构建安全文件名 -> 写入书籍头信息 -> 逐章写入正文 -> 返回绝对路径

#include "export/txt_exporter.h"
#include "export/text_sanitizer.h"

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace novel {

/// 将 std::string 转换为 std::u8string，用于 filesystem 路径操作
static std::u8string to_u8string(const std::string& s) {
    return std::u8string(reinterpret_cast<const char8_t*>(s.data()),
                         reinterpret_cast<const char8_t*>(s.data()) + s.size());
}

std::string TxtExporter::export_book(const Book& book,
                                     const std::vector<Chapter>& chapters,
                                     const TxtOptions& opts,
                                     std::function<void(int, int)> progress_cb) {
    // 创建输出目录
    fs::path output_dir = fs::path(to_u8string(opts.output_dir));
    fs::create_directories(output_dir);

    // 替换文件名中的非法字符，拼接后缀
    std::string safe_title = text_sanitizer::sanitize_filename(book.title);
    fs::path out_path = output_dir /
                        fs::path(to_u8string(safe_title + opts.filename_suffix + ".txt"));

    // 以二进制模式打开文件，避免 Windows 自动转换换行符
    std::ofstream out(out_path, std::ios::binary);
    if (!out.is_open()) return {};

    // 写入书籍头部信息
    out << text_sanitizer::html_to_plain_text(book.title) << "\n";
    out << "作者：" << text_sanitizer::html_to_plain_text(book.author) << "\n";
    out << "分类：" << text_sanitizer::html_to_plain_text(book.category) << "\n";
    out << "字数：" << text_sanitizer::html_to_plain_text(book.word_count) << "\n\n";
    if (!book.abstract.empty()) {
        out << "简介：\n" << text_sanitizer::html_to_plain_text(book.abstract) << "\n\n";
    }
    out << "========================================\n\n";

    // 逐章写入正文内容
    int total = static_cast<int>(chapters.size());
    for (int i = 0; i < total; ++i) {
        const auto& ch = chapters[i];
        out << "\n";
        out << text_sanitizer::html_to_plain_text(ch.content);
        if (progress_cb) progress_cb(i + 1, total);
    }

    // 刷新缓冲区，检查写入状态
    out.flush();
    if (!out.good()) return {};

    // 返回绝对路径
    auto abs_u8 = fs::absolute(out_path).u8string();
    return std::string(abs_u8.begin(), abs_u8.end());
}

} // namespace novel
