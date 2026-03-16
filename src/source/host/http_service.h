#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace fanqie {

struct HttpRequest {
    std::string method = "GET";
    std::string url;
    std::vector<std::pair<std::string, std::string>> headers;
    std::string body;
    int timeout_seconds = 30;
};

struct HttpResponse {
    int status = 0;
    std::string body;
};

class HttpService {
public:
    std::optional<HttpResponse> send(const HttpRequest& request) const;
    std::optional<HttpResponse> get(
        const std::string& url,
        const std::vector<std::pair<std::string, std::string>>& headers = {},
        int timeout_seconds = 30) const;
};

std::string url_encode(const std::string& value);

} // namespace fanqie
