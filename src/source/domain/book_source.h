#pragma once

#include <optional>
#include <string>
#include <vector>

#include "models/book.h"
#include "source/domain/source_types.h"

namespace novel {

/// 书源抽象接口，定义统一的书籍搜索、目录获取、章节读取等操作
/// JS 插件书源（JsBookSource）和未来其他类型书源均需实现此接口
class IBookSource {
public:
    virtual ~IBookSource() = default;

    /// 获取书源基本信息（ID、名称、版本等）
    virtual const SourceInfo& info() const = 0;
    /// 获取书源能力声明（支持哪些操作）
    virtual const SourceCapabilities& capabilities() const = 0;

    /// 配置/初始化书源（如读取环境变量、建立连接等）
    virtual void configure() = 0;

    /// 搜索书籍，返回匹配结果列表
    /// @param keywords  搜索关键词
    /// @param page      页码（从 1 开始）
    virtual std::vector<Book> search(const std::string& keywords, int page) = 0;

    /// 获取指定书籍的详细信息
    /// @param book_id  书籍 ID
    /// @return 若书源不支持或未找到，返回 std::nullopt
    virtual std::optional<Book> get_book_info(const std::string& book_id) = 0;

    /// 获取指定书籍的完整目录
    /// @param book_id  书籍 ID
    virtual std::vector<TocItem> get_toc(const std::string& book_id) = 0;

    /// 获取指定章节的正文内容
    /// @param book_id  书籍 ID
    /// @param item_id  章节 ID
    virtual std::optional<Chapter> get_chapter(
        const std::string& book_id,
        const std::string& item_id) = 0;
};

} // namespace novel
