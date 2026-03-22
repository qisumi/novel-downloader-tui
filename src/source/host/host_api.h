#pragma once

#include <memory>

struct lua_State;

namespace novel {

class HttpService;

class HostApi {
public:
    explicit HostApi(std::shared_ptr<HttpService> http_service);

    void register_with(lua_State* L);
    std::shared_ptr<HttpService> http_service() const { return http_service_; }

private:
    std::shared_ptr<HttpService> http_service_;
};

} // namespace novel
