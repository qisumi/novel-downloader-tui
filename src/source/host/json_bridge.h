#pragma once

#include <string>

struct lua_State;

namespace novel {

void push_json_to_lua(lua_State* L, const std::string& json_text);
std::string lua_to_json_string(lua_State* L, int index);

} // namespace novel
