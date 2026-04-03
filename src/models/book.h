#pragma once
#include <string>
#include <vector>

namespace novel {

/// 书籍基本信息（搜索结果 / 书架条目）
struct Book {
    std::string book_id;          ///< 书籍唯一标识
    std::string title;            ///< 书名
    std::string author;           ///< 作者
    std::string cover_url;        ///< 封面图片地址
    std::string abstract;         ///< 简介/摘要
    std::string category;         ///< 分类（如"玄幻"、"言情"）
    std::string word_count;       ///< 字数（字符串形式，兼容不同格式）
    double      score          = 0.0;  ///< 评分
    int         gender         = 0;   ///< 频道性别标签：0=未知 1=男频 2=女频
    int         creation_status= 0;   ///< 连载状态：0=连载 1=完结
    std::string last_chapter_title;   ///< 最新章节标题
    int64_t     last_update_time = 0; ///< 最后更新时间（Unix 时间戳）
};

/// 目录条目（单章）
struct TocItem {
    std::string item_id;          ///< 章节唯一标识
    std::string title;            ///< 章节标题
    std::string volume_name;      ///< 所属卷名
    int         word_count  = 0;  ///< 本章字数
    int64_t     update_time = 0;  ///< 本章更新时间（Unix 时间戳）
};

/// 章节正文
struct Chapter {
    std::string item_id;          ///< 章节唯一标识
    std::string title;            ///< 章节标题
    std::string content;          ///< 已解密的纯文本正文
};

} // namespace novel
