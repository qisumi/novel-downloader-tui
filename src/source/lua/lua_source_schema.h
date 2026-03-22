#pragma once

#include <lua.hpp>
#include <luabridge3/LuaBridge/LuaBridge.h>

#include <optional>
#include <string>
#include <vector>

#include "models/book.h"

namespace novel {

Book parse_book_table(const luabridge::LuaRef& table, const std::string& source_id);
TocItem parse_toc_item_table(const luabridge::LuaRef& table, const std::string& source_id);
Chapter parse_chapter_table(const luabridge::LuaRef& table, const std::string& source_id);

std::vector<Book> parse_book_list(const luabridge::LuaRef& table, const std::string& source_id);
std::vector<TocItem> parse_toc_list(const luabridge::LuaRef& table, const std::string& source_id);
std::optional<Book> parse_optional_book(const luabridge::LuaRef& table, const std::string& source_id);
std::optional<Chapter> parse_optional_chapter(const luabridge::LuaRef& table, const std::string& source_id);

} // namespace novel
