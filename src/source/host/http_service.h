#pragma once

#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace novel {

/// HTTP 请求结构体，封装请求方法、URL、请求头、请求体等参数
struct HttpRequest {
    std::string method = "GET";    ///< 请求方法（GET/POST/PUT/PATCH/DELETE/HEAD）
    std::string url;               ///< 请求 URL
    std::vector<std::pair<std::string, std::string>> headers; ///< 请求头键值对
    std::string body;              ///< 请求体
    std::string content_type;      ///< 请求体内容类型（如 "application/json"）
    bool follow_redirects = true;  ///< 是否自动跟随重定向
    int timeout_seconds = 30;      ///< 超时时间（秒）
};

/// HTTP 响应结构体，封装状态码、响应体、响应头
struct HttpResponse {
    int status = 0;                ///< HTTP 状态码
    std::string body;              ///< 响应体文本
    std::vector<std::pair<std::string, std::string>> headers; ///< 响应头键值对
};

/// HTTP 服务类，提供同步 HTTP 请求能力（底层基于 cpp-httplib）
/// 被 HostApi 调用，为 JS 插件提供网络访问能力
class HttpService {
public:
    struct CookieEntry {
        std::string name;
        std::string value;
        std::string domain;
        std::string path;
        bool        secure = false;
    };

    /// 发送自定义 HTTP 请求
    std::optional<HttpResponse> send(const HttpRequest& request) const;
    /// 发送 GET 请求，仅返回 2xx 状态的响应
    std::optional<HttpResponse> get(
        const std::string& url,
        const std::vector<std::pair<std::string, std::string>>& headers = {},
        int timeout_seconds = 30) const;

private:
    void attach_cookie_header(
        const std::string& scheme,
        const std::string& host,
        const std::string& path,
        std::vector<std::pair<std::string, std::string>>& headers) const;

    void store_response_cookies(
        const std::string& host,
        const std::string& path,
        const std::vector<std::pair<std::string, std::string>>& headers) const;

    mutable std::mutex         cookie_mutex_;
    mutable std::vector<CookieEntry> cookies_;
};

/// 对字符串进行 RFC 3986 兼容的 URL 编码
std::string url_encode(const std::string& value);

} // namespace novel
