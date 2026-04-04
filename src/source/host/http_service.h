#pragma once

#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace novel {

struct HttpRequest {
    std::string method = "GET";
    std::string url;
    std::vector<std::pair<std::string, std::string>> headers;
    std::string body;
    std::string content_type;
    bool follow_redirects = true;
    int timeout_seconds = 30;
};

struct HttpResponse {
    int status = 0;
    std::string body;
    std::vector<std::pair<std::string, std::string>> headers;
};

class HttpService {
public:
    struct CookieEntry {
        std::string name;
        std::string value;
        std::string domain;
        std::string path;
        bool secure = false;
    };

    std::optional<HttpResponse> send(const HttpRequest& request) const;
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

    mutable std::mutex cookie_mutex_;
    mutable std::vector<CookieEntry> cookies_;
};

std::string url_encode(const std::string& value);

} // namespace novel
