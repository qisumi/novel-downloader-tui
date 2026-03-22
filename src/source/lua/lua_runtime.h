#pragma once

#include <memory>
#include <string>

#include <lua.hpp>
#include <luabridge3/LuaBridge/LuaBridge.h>

namespace novel {

class HostApi;

class LuaRuntime {
public:
    explicit LuaRuntime(std::shared_ptr<HostApi> host_api);
    ~LuaRuntime();

    LuaRuntime(const LuaRuntime&) = delete;
    LuaRuntime& operator=(const LuaRuntime&) = delete;

    luabridge::LuaRef load_plugin(const std::string& plugin_path);
    lua_State* state() const { return state_; }

private:
    lua_State*               state_ = nullptr;
    std::shared_ptr<HostApi> host_api_;
};

} // namespace novel
