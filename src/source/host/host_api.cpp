#include "source/host/host_api.h"

#include <cstdlib>
#include <optional>

#include <lua.hpp>
#include <luabridge3/LuaBridge/LuaBridge.h>
#include <spdlog/spdlog.h>

#include "source/host/http_service.h"
#include "source/host/json_bridge.h"

namespace fanqie {

namespace {

HostApi* get_host_api(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "__fanqie_host_api");
    auto* ptr = static_cast<HostApi*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    return ptr;
}

int lua_http_get(lua_State* L) {
    const char* url = luaL_checkstring(L, 1);
    HostApi* host = get_host_api(L);
    if (host == nullptr) {
        return luaL_error(L, "host api unavailable");
    }

    auto response = host->http_service()->get(url);
    if (!response) {
        lua_pushnil(L);
        lua_pushstring(L, "request failed");
        return 2;
    }

    lua_pushlstring(L, response->body.data(), response->body.size());
    return 1;
}

int lua_json_parse(lua_State* L) {
    const char* text = luaL_checkstring(L, 1);
    try {
        push_json_to_lua(L, text);
        return 1;
    } catch (const std::exception& e) {
        return luaL_error(L, "json_parse failed: %s", e.what());
    }
}

std::optional<std::string> env_get(
    const std::string& name,
    const std::optional<std::string>& fallback = std::nullopt) {
    if (const char* value = std::getenv(name.c_str())) {
        return std::string(value);
    }
    return fallback;
}

void log_info(const std::string& message) { spdlog::info("[lua] {}", message); }
void log_warn(const std::string& message) { spdlog::warn("[lua] {}", message); }
void log_error(const std::string& message) { spdlog::error("[lua] {}", message); }

} // namespace

HostApi::HostApi(std::shared_ptr<HttpService> http_service)
    : http_service_(std::move(http_service)) {}

void HostApi::register_with(lua_State* L) {
    lua_pushlightuserdata(L, this);
    lua_setfield(L, LUA_REGISTRYINDEX, "__fanqie_host_api");

    luabridge::getGlobalNamespace(L)
        .beginNamespace("host")
            .addFunction("http_get", &lua_http_get)
            .addFunction("json_parse", &lua_json_parse)
            .addFunction("url_encode", &url_encode)
            .addFunction("env_get", &env_get)
            .addFunction("log_info", &log_info)
            .addFunction("log_warn", &log_warn)
            .addFunction("log_error", &log_error)
        .endNamespace();
}

} // namespace fanqie
