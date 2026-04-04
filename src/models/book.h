#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace novel {

struct Book {
    std::string book_id;
    std::string title;
    std::string author;
    std::string cover_url;
    std::string abstract;
    std::string category;
    std::string word_count;
    double      score          = 0.0;
    int         gender         = 0;
    int         creation_status= 0;
    std::string last_chapter_title;
    int64_t     last_update_time = 0;
};

inline void to_json(nlohmann::json& j, const Book& book) {
    j = nlohmann::json{
        {"book_id", book.book_id},
        {"title", book.title},
        {"author", book.author},
        {"cover_url", book.cover_url},
        {"abstract", book.abstract},
        {"category", book.category},
        {"word_count", book.word_count},
        {"score", book.score},
        {"gender", book.gender},
        {"creation_status", book.creation_status},
        {"last_chapter_title", book.last_chapter_title},
        {"last_update_time", book.last_update_time},
    };
}

inline void from_json(const nlohmann::json& j, Book& book) {
    book.book_id            = j.value("book_id", "");
    book.title              = j.value("title", "");
    book.author             = j.value("author", "");
    book.cover_url          = j.value("cover_url", "");
    book.abstract           = j.value("abstract", "");
    book.category           = j.value("category", "");
    book.word_count         = j.value("word_count", "");
    book.score              = j.value("score", 0.0);
    book.gender             = j.value("gender", 0);
    book.creation_status    = j.value("creation_status", 0);
    book.last_chapter_title = j.value("last_chapter_title", "");
    book.last_update_time   = j.value("last_update_time", static_cast<std::int64_t>(0));
}

struct TocItem {
    std::string item_id;
    std::string title;
    std::string volume_name;
    int         word_count  = 0;
    int64_t     update_time = 0;
};

inline void to_json(nlohmann::json& j, const TocItem& item) {
    j = nlohmann::json{
        {"item_id", item.item_id},
        {"title", item.title},
        {"volume_name", item.volume_name},
        {"word_count", item.word_count},
        {"update_time", item.update_time},
    };
}

struct Chapter {
    std::string item_id;
    std::string title;
    std::string content;
};

} // namespace novel
