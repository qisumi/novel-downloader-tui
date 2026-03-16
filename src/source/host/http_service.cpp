#include "source/host/http_service.h"

#include <httplib.h>
#include <spdlog/spdlog.h>

#include <cctype>
#include <iomanip>
#include <sstream>

namespace fanqie {

namespace {

struct SplitUrlResult {
    std::string scheme_host;
    std::string path;
};

SplitUrlResult split_url(const std::string& url) {
    std::size_t scheme_end = url.find("://");
    std::size_t host_end = std::string::npos;
    if (scheme_end == std::string::npos) {
        host_end = url.find('/');
    } else {
        host_end = url.find('/', scheme_end + 3);
    }

    SplitUrlResult result;
    if (host_end == std::string::npos) {
        result.scheme_host = url;
        result.path = "/";
    } else {
        result.scheme_host = url.substr(0, host_end);
        result.path = url.substr(host_end);
    }
    if (result.path.empty()) {
        result.path = "/";
    }
    return result;
}

} // namespace

std::optional<HttpResponse> HttpService::send(const HttpRequest& request) const {
    SplitUrlResult url = split_url(request.url);
    spdlog::debug("HTTP {} {} | host={} path={}",
                  request.method, request.url, url.scheme_host, url.path);

    httplib::Client cli(url.scheme_host);
    cli.set_connection_timeout(request.timeout_seconds, 0);
    cli.set_read_timeout(request.timeout_seconds, 0);
    cli.set_follow_location(true);

    httplib::Headers headers;
    for (const auto& [key, value] : request.headers) {
        headers.emplace(key, value);
    }

    httplib::Result result;
    if (request.method == "POST") {
        result = cli.Post(url.path, headers, request.body, "application/json");
    } else {
        result = cli.Get(url.path, headers);
    }

    if (!result) {
        spdlog::error("HTTP request failed: {} {}", request.method, request.url);
        return std::nullopt;
    }

    if (result->status < 200 || result->status >= 300) {
        spdlog::warn("HTTP non-2xx status={} body={}",
                     result->status, result->body.substr(0, 512));
        return std::nullopt;
    }

    return HttpResponse{result->status, result->body};
}

std::optional<HttpResponse> HttpService::get(
    const std::string& url,
    const std::vector<std::pair<std::string, std::string>>& headers,
    int timeout_seconds) const {
    HttpRequest request;
    request.method = "GET";
    request.url = url;
    request.headers = headers;
    request.timeout_seconds = timeout_seconds;
    return send(request);
}

std::string url_encode(const std::string& value) {
    std::ostringstream encoded;
    encoded << std::uppercase << std::hex;
    for (unsigned char ch : value) {
        if (std::isalnum(ch) || ch == '-' || ch == '_' || ch == '.' || ch == '~') {
            encoded << ch;
        } else {
            encoded << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(ch);
        }
    }
    return encoded.str();
}

} // namespace fanqie
