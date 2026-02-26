#include "export/txt_exporter.h"
#include "export/text_sanitizer.h"

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace fanqie {

static std::u8string to_u8string(const std::string& s) {
    return std::u8string(reinterpret_cast<const char8_t*>(s.data()),
                         reinterpret_cast<const char8_t*>(s.data()) + s.size());
}

std::string TxtExporter::export_book(const Book& book,
                                     const std::vector<Chapter>& chapters,
                                     const TxtOptions& opts,
                                     std::function<void(int, int)> progress_cb) {
    fs::path output_dir = fs::path(to_u8string(opts.output_dir));
    fs::create_directories(output_dir);

    std::string safe_title = text_sanitizer::sanitize_filename(book.title);
    fs::path out_path = output_dir /
                        fs::path(to_u8string(safe_title + opts.filename_suffix + ".txt"));

    std::ofstream out(out_path, std::ios::binary);
    if (!out.is_open()) return {};

    out << text_sanitizer::html_to_plain_text(book.title) << "\n";
    out << "作者：" << text_sanitizer::html_to_plain_text(book.author) << "\n";
    out << "分类：" << text_sanitizer::html_to_plain_text(book.category) << "\n";
    out << "字数：" << text_sanitizer::html_to_plain_text(book.word_count) << "\n\n";
    if (!book.abstract.empty()) {
        out << "简介：\n" << text_sanitizer::html_to_plain_text(book.abstract) << "\n\n";
    }
    out << "========================================\n\n";

    int total = static_cast<int>(chapters.size());
    for (int i = 0; i < total; ++i) {
        const auto& ch = chapters[i];
        out << "\n";
        out << text_sanitizer::html_to_plain_text(ch.content);
        if (progress_cb) progress_cb(i + 1, total);
    }

    out.flush();
    if (!out.good()) return {};
    auto abs_u8 = fs::absolute(out_path).u8string();
    return std::string(abs_u8.begin(), abs_u8.end());
}

} // namespace fanqie
