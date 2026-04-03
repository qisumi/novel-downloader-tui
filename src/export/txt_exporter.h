/// @file txt_exporter.h
/// @brief TXT 纯文本格式导出器
///
/// 将书籍元数据与章节内容导出为 UTF-8 编码的 .txt 文件，
/// 书籍头部包含标题、作者、分类、字数和简介信息。

#pragma once
#include <string>
#include <vector>
#include <functional>
#include "models/book.h"

namespace novel {

/// TXT 导出选项
struct TxtOptions {
    std::string output_dir = ".";    ///< 输出目录
    std::string filename_suffix;      ///< 输出文件名后缀（如 _ch001-010）
};

/// TXT 纯文本导出器
///
/// 负责将 Book + Chapter 数据写入 UTF-8 文本文件。
/// 书籍头部格式包含标题、作者、分类、字数和可选简介，
/// 章节正文按顺序追加写入。
class TxtExporter {
public:
    /// 导出书籍为 TXT 文件
    ///
    /// @param book         书籍元数据
    /// @param chapters     已下载的章节列表
    /// @param opts         导出选项（输出目录、文件名后缀等）
    /// @param progress_cb  进度回调 progress_cb(current, total)，每写完一章调用一次
    /// @return 生成的 .txt 文件绝对路径；失败时返回空字符串
    static std::string export_book(
        const Book&                 book,
        const std::vector<Chapter>& chapters,
        const TxtOptions&           opts = {},
        std::function<void(int, int)> progress_cb = nullptr);
};

} // namespace novel
