#pragma once
#include <string>
#include <vector>

namespace novel {

/// 书籍基本信息（搜索结果 / 书架条目）
struct Book {
    std::string book_id;
    std::string title;
    std::string author;
    std::string cover_url;
    std::string abstract;
    std::string category;
    std::string word_count;
    double      score          = 0.0;
    int         gender         = 0;   // 0=未知 1=男频 2=女频
    int         creation_status= 0;   // 0=连载 1=完结
    std::string last_chapter_title;
    int64_t     last_update_time = 0; // Unix timestamp
};

/// 目录条目（单章）
struct TocItem {
    std::string item_id;
    std::string title;
    std::string volume_name;
    int         word_count  = 0;
    int64_t     update_time = 0; // Unix timestamp
};

/// 章节正文
struct Chapter {
    std::string item_id;
    std::string title;
    std::string content;   // 已解密的纯文本正文
};

} // namespace novel
