#pragma once
#include <string>
#include <vector>
#include <functional>
#include "models/book.h"

namespace fanqie {

struct TxtOptions {
    std::string output_dir = ".";    ///< 输出目录
    std::string filename_suffix;      ///< 输出文件名后缀（如 _ch001-010）
};

class TxtExporter {
public:
    /// 导出书籍为 TXT，progress_cb(current, total) 汇报章节写入进度
    /// @return 生成的 .txt 文件绝对路径，失败时返回空字符串
    static std::string export_book(
        const Book&                 book,
        const std::vector<Chapter>& chapters,
        const TxtOptions&           opts = {},
        std::function<void(int, int)> progress_cb = nullptr);
};

} // namespace fanqie
