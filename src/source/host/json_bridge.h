#pragma once

#include <string>

struct lua_State;

namespace fanqie {

void push_json_to_lua(lua_State* L, const std::string& json_text);

} // namespace fanqie
