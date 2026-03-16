#include "source/lua/lua_runtime.h"

#include <stdexcept>

#include "source/host/host_api.h"

namespace fanqie {

LuaRuntime::LuaRuntime(std::shared_ptr<HostApi> host_api)
    : host_api_(std::move(host_api)) {
    state_ = luaL_newstate();
    if (state_ == nullptr) {
        throw std::runtime_error("failed to create lua state");
    }
    luaL_openlibs(state_);
    host_api_->register_with(state_);
}

LuaRuntime::~LuaRuntime() {
    if (state_ != nullptr) {
        lua_close(state_);
        state_ = nullptr;
    }
}

luabridge::LuaRef LuaRuntime::load_plugin(const std::string& plugin_path) {
    if (luaL_loadfile(state_, plugin_path.c_str()) != LUA_OK) {
        std::string error = lua_tostring(state_, -1);
        lua_pop(state_, 1);
        throw std::runtime_error(error);
    }
    if (lua_pcall(state_, 0, 1, 0) != LUA_OK) {
        std::string error = lua_tostring(state_, -1);
        lua_pop(state_, 1);
        throw std::runtime_error(error);
    }

    luabridge::LuaRef plugin = luabridge::LuaRef::fromStack(state_, -1);
    lua_pop(state_, 1);
    return plugin;
}

} // namespace fanqie
