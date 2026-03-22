#include "source/host/json_bridge.h"

#include <algorithm>
#include <lua.hpp>
#include <nlohmann/json.hpp>

#include <stdexcept>
#include <string>
#include <unordered_set>

namespace fanqie {

namespace {

using json = nlohmann::json;
using VisitedTables = std::unordered_set<const void*>;

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

bool is_array_table(lua_State* L, int index, std::size_t& max_index, std::size_t& item_count) {
    index = lua_absindex(L, index);
    max_index = 0;
    item_count = 0;

    lua_pushnil(L);
    while (lua_next(L, index) != 0) {
        bool valid_index = false;
        if (lua_type(L, -2) == LUA_TNUMBER) {
            lua_Integer key_int = lua_tointeger(L, -2);
            lua_Number  key_num = lua_tonumber(L, -2);
            valid_index = key_int >= 1 && static_cast<lua_Number>(key_int) == key_num;
            if (valid_index) {
                max_index = std::max(max_index, static_cast<std::size_t>(key_int));
                ++item_count;
            }
        }

        lua_pop(L, 1);
        if (!valid_index) {
            return false;
        }
    }

    return item_count > 0 && max_index == item_count;
}

std::string lua_key_to_string(lua_State* L, int index) {
    int type = lua_type(L, index);
    if (type == LUA_TSTRING) {
        return lua_tostring(L, index);
    }
    if (type == LUA_TNUMBER) {
        lua_Integer key_int = lua_tointeger(L, index);
        lua_Number  key_num = lua_tonumber(L, index);
        if (static_cast<lua_Number>(key_int) == key_num) {
            return std::to_string(key_int);
        }
    }
    throw std::runtime_error("unsupported table key type for JSON object");
}

json lua_value_to_json(lua_State* L, int index, VisitedTables& visited) {
    index = lua_absindex(L, index);

    switch (lua_type(L, index)) {
    case LUA_TNIL:
        return nullptr;
    case LUA_TBOOLEAN:
        return lua_toboolean(L, index) != 0;
    case LUA_TNUMBER: {
        lua_Integer int_value = lua_tointeger(L, index);
        lua_Number  num_value = lua_tonumber(L, index);
        if (static_cast<lua_Number>(int_value) == num_value) {
            return int_value;
        }
        return num_value;
    }
    case LUA_TSTRING:
        return std::string(lua_tostring(L, index));
    case LUA_TTABLE: {
        const void* table_ptr = lua_topointer(L, index);
        if (!visited.insert(table_ptr).second) {
            throw std::runtime_error("circular reference detected while encoding JSON");
        }

        std::size_t max_index = 0;
        std::size_t item_count = 0;
        bool        is_array = is_array_table(L, index, max_index, item_count);

        json result = is_array ? json::array() : json::object();
        if (is_array) {
            for (std::size_t i = 1; i <= item_count; ++i) {
                lua_geti(L, index, static_cast<lua_Integer>(i));
                result.push_back(lua_value_to_json(L, -1, visited));
                lua_pop(L, 1);
            }
        } else {
            lua_pushnil(L);
            while (lua_next(L, index) != 0) {
                result[lua_key_to_string(L, -2)] = lua_value_to_json(L, -1, visited);
                lua_pop(L, 1);
            }
        }

        visited.erase(table_ptr);
        return result;
    }
    default:
        throw std::runtime_error(std::string("unsupported Lua type for JSON encoding: ")
                                 + lua_typename(L, lua_type(L, index)));
    }
}

} // namespace

void push_json_to_lua(lua_State* L, const std::string& json_text) {
    auto value = json::parse(json_text);
    push_json_value(L, value);
}

std::string lua_to_json_string(lua_State* L, int index) {
    VisitedTables visited;
    return lua_value_to_json(L, index, visited).dump();
}

} // namespace fanqie
