#include "source/host/json_bridge.h"

#include <lua.hpp>
#include <nlohmann/json.hpp>

namespace fanqie {

namespace {

using json = nlohmann::json;

void push_json_value(lua_State* L, const json& value) {
    if (value.is_null()) {
        lua_pushnil(L);
        return;
    }
    if (value.is_boolean()) {
        lua_pushboolean(L, value.get<bool>());
        return;
    }
    if (value.is_number_integer()) {
        lua_pushinteger(L, value.get<lua_Integer>());
        return;
    }
    if (value.is_number_unsigned()) {
        lua_pushinteger(L, static_cast<lua_Integer>(value.get<std::uint64_t>()));
        return;
    }
    if (value.is_number_float()) {
        lua_pushnumber(L, value.get<lua_Number>());
        return;
    }
    if (value.is_string()) {
        lua_pushstring(L, value.get_ref<const std::string&>().c_str());
        return;
    }
    if (value.is_array()) {
        lua_newtable(L);
        int index = 1;
        for (const auto& item : value) {
            push_json_value(L, item);
            lua_rawseti(L, -2, index++);
        }
        return;
    }

    lua_newtable(L);
    for (auto it = value.begin(); it != value.end(); ++it) {
        push_json_value(L, it.value());
        lua_setfield(L, -2, it.key().c_str());
    }
}

} // namespace

void push_json_to_lua(lua_State* L, const std::string& json_text) {
    auto value = json::parse(json_text);
    push_json_value(L, value);
}

} // namespace fanqie
