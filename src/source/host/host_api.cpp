#include "source/host/host_api.h"

#include <cstdlib>
#include <optional>
#include <string>
#include <vector>

#include <lua.hpp>
#include <luabridge3/LuaBridge/LuaBridge.h>
#include <spdlog/spdlog.h>

#include "source/domain/source_errors.h"
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

int lua_prefixed_error(lua_State* L, SourceErrorCode code, const std::string& message) {
    return luaL_error(L, "%s", prefix_source_error(code, message).c_str());
}

void push_prefixed_error_string(lua_State* L, SourceErrorCode code, const std::string& message) {
    const auto text = prefix_source_error(code, message);
    lua_pushlstring(L, text.data(), text.size());
}

std::string checked_lua_string_field(lua_State* L, int index, const char* field_name) {
    index = lua_absindex(L, index);
    lua_getfield(L, index, field_name);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        return "";
    }
    if (!lua_isstring(L, -1)) {
        std::string message = std::string("http_request field must be a string: ") + field_name;
        lua_pop(L, 1);
        lua_prefixed_error(L, SourceErrorCode::PluginRequestError, message);
        return "";
    }
    std::string value = lua_tostring(L, -1);
    lua_pop(L, 1);
    return value;
}

int checked_lua_integer_field(lua_State* L, int index, const char* field_name, int fallback) {
    index = lua_absindex(L, index);
    lua_getfield(L, index, field_name);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        return fallback;
    }
    if (!lua_isinteger(L, -1)) {
        std::string message = std::string("http_request field must be an integer: ") + field_name;
        lua_pop(L, 1);
        lua_prefixed_error(L, SourceErrorCode::PluginRequestError, message);
        return fallback;
    }
    int value = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    return value;
}

std::string lua_scalar_to_string(lua_State* L, int index, const char* context) {
    index = lua_absindex(L, index);
    int type = lua_type(L, index);
    if (type == LUA_TSTRING) {
        return lua_tostring(L, index);
    }
    if (type == LUA_TNUMBER) {
        if (lua_isinteger(L, index)) {
            return std::to_string(lua_tointeger(L, index));
        }
        return std::to_string(lua_tonumber(L, index));
    }
    if (type == LUA_TBOOLEAN) {
        return lua_toboolean(L, index) ? "true" : "false";
    }

    std::string message = std::string(context) + " must be a string/number/boolean";
    lua_prefixed_error(L, SourceErrorCode::PluginRequestError, message);
    return "";
}

std::vector<std::pair<std::string, std::string>> parse_headers(lua_State* L, int index) {
    index = lua_absindex(L, index);
    std::vector<std::pair<std::string, std::string>> headers;

    lua_getfield(L, index, "headers");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        return headers;
    }
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_prefixed_error(L, SourceErrorCode::PluginRequestError,
                           "http_request field must be a table: headers");
        return headers;
    }

    int headers_index = lua_absindex(L, -1);
    lua_pushnil(L);
    while (lua_next(L, headers_index) != 0) {
        if (!lua_isstring(L, -2)) {
            lua_pop(L, 2);
            lua_prefixed_error(L, SourceErrorCode::PluginRequestError,
                               "http_request header name must be a string");
            return headers;
        }

        headers.emplace_back(lua_tostring(L, -2), lua_scalar_to_string(L, -1, "http_request header value"));
        lua_pop(L, 1);
    }

    lua_pop(L, 1);
    return headers;
}

HttpRequest parse_http_request(lua_State* L, int index) {
    index = lua_absindex(L, index);
    if (!lua_istable(L, index)) {
        lua_prefixed_error(L, SourceErrorCode::PluginRequestError,
                           "http_request expects a Lua table");
        return {};
    }

    HttpRequest request;
    request.method = checked_lua_string_field(L, index, "method");
    if (request.method.empty()) {
        request.method = "GET";
    }

    request.url = checked_lua_string_field(L, index, "url");
    if (request.url.empty()) {
        lua_prefixed_error(L, SourceErrorCode::PluginRequestError,
                           "http_request requires a non-empty url");
        return {};
    }

    request.timeout_seconds = checked_lua_integer_field(L, index, "timeout_seconds", 30);
    request.headers = parse_headers(L, index);

    lua_getfield(L, index, "body");
    if (!lua_isnil(L, -1)) {
        if (lua_istable(L, -1)) {
            request.body = lua_to_json_string(L, -1);
            request.content_type = "application/json";
        } else if (lua_isstring(L, -1)) {
            request.body = lua_tostring(L, -1);
        } else {
            lua_pop(L, 1);
            lua_prefixed_error(L, SourceErrorCode::PluginRequestError,
                               "http_request body must be a string or table");
            return {};
        }
    }
    lua_pop(L, 1);

    return request;
}

void push_headers_table(lua_State* L, const HttpResponse& response) {
    lua_newtable(L);
    for (const auto& [key, value] : response.headers) {
        lua_pushlstring(L, value.data(), value.size());
        lua_setfield(L, -2, key.c_str());
    }
}

void push_http_response(lua_State* L, const HttpResponse& response) {
    lua_newtable(L);

    lua_pushinteger(L, response.status);
    lua_setfield(L, -2, "status");

    lua_pushlstring(L, response.body.data(), response.body.size());
    lua_setfield(L, -2, "body");

    push_headers_table(L, response);
    lua_setfield(L, -2, "headers");
}

int lua_http_get(lua_State* L) {
    const char* url = luaL_checkstring(L, 1);
    HostApi* host = get_host_api(L);
    if (host == nullptr) {
        return lua_prefixed_error(L, SourceErrorCode::PluginRuntimeError, "host api unavailable");
    }

    auto response = host->http_service()->get(url);
    if (!response) {
        lua_pushnil(L);
        push_prefixed_error_string(L, SourceErrorCode::NetworkError,
                                   std::string("request failed: GET ") + url);
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
        spdlog::warn("[lua][json_parse] {}", e.what());
        return lua_prefixed_error(L, SourceErrorCode::PluginDataError,
                                  std::string("json_parse failed: ") + e.what());
    }
}

int lua_json_stringify(lua_State* L) {
    luaL_checkany(L, 1);
    try {
        auto text = lua_to_json_string(L, 1);
        lua_pushlstring(L, text.data(), text.size());
        return 1;
    } catch (const std::exception& e) {
        spdlog::warn("[lua][json_stringify] {}", e.what());
        return lua_prefixed_error(L, SourceErrorCode::PluginDataError,
                                  std::string("json_stringify failed: ") + e.what());
    }
}

int lua_http_request(lua_State* L) {
    HostApi* host = get_host_api(L);
    if (host == nullptr) {
        return lua_prefixed_error(L, SourceErrorCode::PluginRuntimeError, "host api unavailable");
    }

    HttpRequest request = parse_http_request(L, 1);
    auto response = host->http_service()->send(request);
    if (!response) {
        lua_pushnil(L);
        std::string message = "request failed: " + request.method + " " + request.url;
        push_prefixed_error_string(L, SourceErrorCode::NetworkError, message);
        return 2;
    }

    push_http_response(L, *response);
    return 1;
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
int lua_config_error(lua_State* L) {
    const char* message = luaL_checkstring(L, 1);
    spdlog::warn("[lua][config] {}", message);
    return lua_prefixed_error(L, SourceErrorCode::PluginConfigError, message);
}

} // namespace

HostApi::HostApi(std::shared_ptr<HttpService> http_service)
    : http_service_(std::move(http_service)) {}

void HostApi::register_with(lua_State* L) {
    lua_pushlightuserdata(L, this);
    lua_setfield(L, LUA_REGISTRYINDEX, "__fanqie_host_api");

    luabridge::getGlobalNamespace(L)
        .beginNamespace("host")
            .addFunction("http_get", &lua_http_get)
            .addFunction("http_request", &lua_http_request)
            .addFunction("json_parse", &lua_json_parse)
            .addFunction("json_stringify", &lua_json_stringify)
            .addFunction("url_encode", &url_encode)
            .addFunction("env_get", &env_get)
            .addFunction("log_info", &log_info)
            .addFunction("log_warn", &log_warn)
            .addFunction("log_error", &log_error)
            .addFunction("config_error", &lua_config_error)
        .endNamespace();
}

} // namespace fanqie
