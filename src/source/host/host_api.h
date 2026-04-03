#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "source/host/http_service.h"

namespace novel {

class HttpService;

/// 插件宿主 API，为 JS 插件提供 HTTP 请求、环境变量读取、URL 编码、日志等能力
/// 所有方法均为 const，线程安全由底层 HttpService 保证
class HostApi {
public:
    explicit HostApi(std::shared_ptr<HttpService> http_service);

    /// 发送 HTTP GET 请求
    /// @param url             请求 URL
    /// @param headers         请求头
    /// @param timeout_seconds 超时时间（秒）
    std::optional<HttpResponse> http_get(
        const std::string& url,
        const std::vector<std::pair<std::string, std::string>>& headers = {},
        int timeout_seconds = 30) const;

    /// 发送自定义 HTTP 请求（支持 GET/POST/PUT/PATCH/DELETE/HEAD）
    std::optional<HttpResponse> http_request(const HttpRequest& request) const;

    /// 读取环境变量
    /// @param name      环境变量名
    /// @param fallback  若变量不存在，返回此默认值；若也为 nullopt，则返回 nullopt
    std::optional<std::string>  env_get(
        const std::string& name,
        const std::optional<std::string>& fallback = std::nullopt) const;

    /// 对字符串进行 URL 编码
    std::string url_encode(const std::string& value) const;

    /// 输出 INFO 级别日志（带 [plugin] 前缀）
    void log_info(const std::string& message) const;
    /// 输出 WARN 级别日志（带 [plugin] 前缀）
    void log_warn(const std::string& message) const;
    /// 输出 ERROR 级别日志（带 [plugin] 前缀）
    void log_error(const std::string& message) const;

private:
    std::shared_ptr<HttpService> http_service_; ///< HTTP 服务实例
};

} // namespace novel
