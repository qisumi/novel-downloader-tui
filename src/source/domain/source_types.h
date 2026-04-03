#pragma once

#include <string>
#include <vector>

namespace novel {

/// 书源基本信息（对应 JS 插件 manifest 字段）
struct SourceInfo {
    std::string id;                          ///< 书源唯一标识，如 "fanqie"
    std::string name;                        ///< 书源显示名称，如 "番茄小说"
    std::string version;                     ///< 书源版本号
    std::string author;                      ///< 书源作者
    std::string description;                 ///< 书源描述
    std::vector<std::string> required_envs;  ///< 必需的环境变量列表
    std::vector<std::string> optional_envs;  ///< 可选的环境变量列表
};

/// 书源能力声明（标识当前书源支持哪些操作）
struct SourceCapabilities {
    bool supports_search = true;      ///< 是否支持搜索
    bool supports_book_info = true;   ///< 是否支持获取书籍详情
    bool supports_toc = true;         ///< 是否支持获取目录
    bool supports_chapter = true;     ///< 是否支持获取章节正文
};

} // namespace novel
