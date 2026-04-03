#include "source/host/host_api.h"

#include <cstdlib>
#include <spdlog/spdlog.h>

namespace novel {

HostApi::HostApi(std::shared_ptr<HttpService> http_service)
    : http_service_(std::move(http_service)) {}

/// 发送 HTTP GET 请求，委托给 HttpService::get
std::optional<HttpResponse> HostApi::http_get(
    const std::string& url,
    const std::vector<std::pair<std::string, std::string>>& headers,
    int timeout_seconds) const {
    return http_service_->get(url, headers, timeout_seconds);
}

/// 发送自定义 HTTP 请求，委托给 HttpService::send
std::optional<HttpResponse> HostApi::http_request(const HttpRequest& request) const {
    return http_service_->send(request);
}

/// 从进程环境变量中读取指定名称的值
/// 若变量不存在且提供了 fallback，则返回 fallback；否则返回 nullopt
std::optional<std::string> HostApi::env_get(
    const std::string& name,
    const std::optional<std::string>& fallback) const {
    if (const char* value = std::getenv(name.c_str())) {
        return std::string(value);
    }
    return fallback;
}

/// 对字符串进行 URL 编码
std::string HostApi::url_encode(const std::string& value) const {
    return novel::url_encode(value);
}

/// 输出 INFO 级别日志
void HostApi::log_info(const std::string& message) const {
    spdlog::info("[plugin] {}", message);
}

/// 输出 WARN 级别日志
void HostApi::log_warn(const std::string& message) const {
    spdlog::warn("[plugin] {}", message);
}

/// 输出 ERROR 级别日志
void HostApi::log_error(const std::string& message) const {
    spdlog::error("[plugin] {}", message);
}

} // namespace novel
