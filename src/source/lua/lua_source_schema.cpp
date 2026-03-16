#include "source/lua/lua_source_schema.h"

#include <cstdint>

#include "source/domain/source_errors.h"

namespace fanqie {

namespace {

template <typename T>
T require_field(const luabridge::LuaRef& table, const char* key, const std::string& source_id) {
    luabridge::LuaRef value = table[key];
    if (value.isNil()) {
        throw SourceException({SourceErrorCode::InvalidReturnField, source_id, "", key,
                               "missing required field: " + std::string(key)});
    }
    auto result = value.cast<T>();
    if (!result) {
        throw SourceException({SourceErrorCode::InvalidReturnField, source_id, "", key,
                               "invalid field type: " + std::string(key)});
    }
    return result.value();
}

template <typename T>
T optional_field(const luabridge::LuaRef& table, const char* key, T fallback) {
    luabridge::LuaRef value = table[key];
    if (value.isNil()) {
        return fallback;
    }
    auto result = value.cast<T>();
    if (!result) {
        return fallback;
    }
    return result.value();
}

template <typename T>
std::vector<T> parse_list(
    const luabridge::LuaRef& table,
    const std::string& source_id,
    T (*parser)(const luabridge::LuaRef&, const std::string&)) {
    if (!table.isTable()) {
        throw SourceException({SourceErrorCode::InvalidReturnType, source_id, "", "list",
                               "expected Lua table"});
    }

    std::vector<T> results;
    for (luabridge::Iterator it(table); !it.isNil(); ++it) {
        results.push_back(parser(it.value(), source_id));
    }
    return results;
}

} // namespace

Book parse_book_table(const luabridge::LuaRef& table, const std::string& source_id) {
    if (!table.isTable()) {
        throw SourceException({SourceErrorCode::InvalidReturnType, source_id, "", "book",
                               "expected book table"});
    }

    Book book;
    book.book_id = require_field<std::string>(table, "book_id", source_id);
    book.title = require_field<std::string>(table, "title", source_id);
    book.author = optional_field<std::string>(table, "author", "");
    book.cover_url = optional_field<std::string>(table, "cover_url", "");
    book.abstract = optional_field<std::string>(table, "abstract", "");
    book.category = optional_field<std::string>(table, "category", "");
    book.word_count = optional_field<std::string>(table, "word_count", "");
    book.score = optional_field<double>(table, "score", 0.0);
    book.gender = optional_field<int>(table, "gender", 0);
    book.creation_status = optional_field<int>(table, "creation_status", 0);
    book.last_chapter_title = optional_field<std::string>(table, "last_chapter_title", "");
    book.last_update_time = optional_field<std::int64_t>(table, "last_update_time", 0);
    return book;
}

TocItem parse_toc_item_table(const luabridge::LuaRef& table, const std::string& source_id) {
    if (!table.isTable()) {
        throw SourceException({SourceErrorCode::InvalidReturnType, source_id, "", "toc_item",
                               "expected toc item table"});
    }

    TocItem item;
    item.item_id = require_field<std::string>(table, "item_id", source_id);
    item.title = require_field<std::string>(table, "title", source_id);
    item.volume_name = optional_field<std::string>(table, "volume_name", "");
    item.word_count = optional_field<int>(table, "word_count", 0);
    item.update_time = optional_field<std::int64_t>(table, "update_time", 0);
    return item;
}

Chapter parse_chapter_table(const luabridge::LuaRef& table, const std::string& source_id) {
    if (!table.isTable()) {
        throw SourceException({SourceErrorCode::InvalidReturnType, source_id, "", "chapter",
                               "expected chapter table"});
    }

    Chapter chapter;
    chapter.item_id = optional_field<std::string>(table, "item_id", "");
    chapter.title = optional_field<std::string>(table, "title", "");
    chapter.content = require_field<std::string>(table, "content", source_id);
    return chapter;
}

std::vector<Book> parse_book_list(const luabridge::LuaRef& table, const std::string& source_id) {
    return parse_list<Book>(table, source_id, &parse_book_table);
}

std::vector<TocItem> parse_toc_list(const luabridge::LuaRef& table, const std::string& source_id) {
    return parse_list<TocItem>(table, source_id, &parse_toc_item_table);
}

std::optional<Book> parse_optional_book(const luabridge::LuaRef& table, const std::string& source_id) {
    if (table.isNil()) {
        return std::nullopt;
    }
    return parse_book_table(table, source_id);
}

std::optional<Chapter> parse_optional_chapter(const luabridge::LuaRef& table, const std::string& source_id) {
    if (table.isNil()) {
        return std::nullopt;
    }
    return parse_chapter_table(table, source_id);
}

} // namespace fanqie
