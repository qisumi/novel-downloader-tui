#include "source/lua/lua_runtime.h"

#include <filesystem>
#include <stdexcept>
#include <string>

#include "source/host/host_api.h"

namespace novel {

namespace {

void prepend_package_path(lua_State* L, const std::string& plugin_path) {
    namespace fs = std::filesystem;

    const auto plugin_dir = fs::path(plugin_path).parent_path().generic_string();
    if (plugin_dir.empty()) {
        return;
    }

    lua_getglobal(L, "package");
    lua_getfield(L, -1, "path");
    std::string current_path = lua_tostring(L, -1);
    lua_pop(L, 1);

    const std::string extra_paths =
        plugin_dir + "/?.lua;" +
        plugin_dir + "/?/init.lua;";
    const std::string merged_path = extra_paths + current_path;

    lua_pushlstring(L, merged_path.data(), merged_path.size());
    lua_setfield(L, -2, "path");
    lua_pop(L, 1);
}

} // namespace

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
    prepend_package_path(state_, plugin_path);

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

} // namespace novel
