#include "source/host/host_api.h"

#include <cstdlib>
#include <spdlog/spdlog.h>

namespace novel {

HostApi::HostApi(std::shared_ptr<HttpService> http_service)
    : http_service_(std::move(http_service)) {}

std::optional<HttpResponse> HostApi::http_get(
    const std::string& url,
    const std::vector<std::pair<std::string, std::string>>& headers,
    int timeout_seconds) const {
    return http_service_->get(url, headers, timeout_seconds);
}

std::optional<HttpResponse> HostApi::http_request(const HttpRequest& request) const {
    return http_service_->send(request);
}

std::optional<std::string> HostApi::env_get(
    const std::string& name,
    const std::optional<std::string>& fallback) const {
    if (const char* value = std::getenv(name.c_str())) {
        return std::string(value);
    }
    return fallback;
}

std::string HostApi::url_encode(const std::string& value) const {
    return novel::url_encode(value);
}

void HostApi::log_info(const std::string& message) const {
    spdlog::info("[plugin] {}", message);
}

void HostApi::log_warn(const std::string& message) const {
    spdlog::warn("[plugin] {}", message);
}

void HostApi::log_error(const std::string& message) const {
    spdlog::error("[plugin] {}", message);
}

} // namespace novel
