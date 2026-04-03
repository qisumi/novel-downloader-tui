#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "source/host/http_service.h"

namespace novel {

class HttpService;

class HostApi {
public:
    explicit HostApi(std::shared_ptr<HttpService> http_service);

    std::optional<HttpResponse> http_get(
        const std::string& url,
        const std::vector<std::pair<std::string, std::string>>& headers = {},
        int timeout_seconds = 30) const;
    std::optional<HttpResponse> http_request(const HttpRequest& request) const;
    std::optional<std::string>  env_get(
        const std::string& name,
        const std::optional<std::string>& fallback = std::nullopt) const;
    std::string url_encode(const std::string& value) const;

    void log_info(const std::string& message) const;
    void log_warn(const std::string& message) const;
    void log_error(const std::string& message) const;

private:
    std::shared_ptr<HttpService> http_service_;
};

} // namespace novel
