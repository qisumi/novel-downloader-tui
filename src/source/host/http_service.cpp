#include "source/host/http_service.h"

#include <httplib.h>
#include <spdlog/spdlog.h>

#include <cctype>
#include <iomanip>
#include <sstream>

namespace novel {

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

std::string normalize_method(std::string method) {
    for (char& ch : method) {
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    }
    return method;
}

std::string detect_content_type(const HttpRequest& request) {
    for (const auto& [key, value] : request.headers) {
        std::string lower_key;
        lower_key.reserve(key.size());
        for (char ch : key) {
            lower_key.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
        }
        if (lower_key == "content-type") {
            return value;
        }
    }

    if (!request.content_type.empty()) {
        return request.content_type;
    }
    return "application/json";
}

} // namespace

std::optional<HttpResponse> HttpService::send(const HttpRequest& request) const {
    SplitUrlResult url = split_url(request.url);
    std::string    method = normalize_method(request.method);
    spdlog::debug("HTTP {} {} | host={} path={}",
                  method, request.url, url.scheme_host, url.path);

    httplib::Client cli(url.scheme_host);
    cli.set_connection_timeout(request.timeout_seconds, 0);
    cli.set_read_timeout(request.timeout_seconds, 0);
    cli.set_follow_location(true);

    httplib::Headers headers;
    for (const auto& [key, value] : request.headers) {
        headers.emplace(key, value);
    }

    httplib::Result result;
    if (method == "GET") {
        result = cli.Get(url.path, headers);
    } else if (method == "POST") {
        result = cli.Post(url.path, headers, request.body, detect_content_type(request));
    } else if (method == "PUT") {
        result = cli.Put(url.path, headers, request.body, detect_content_type(request));
    } else if (method == "PATCH") {
        result = cli.Patch(url.path, headers, request.body, detect_content_type(request));
    } else if (method == "DELETE") {
        if (!request.body.empty()) {
            spdlog::warn("HTTP DELETE body is currently ignored: {}", request.url);
        }
        result = cli.Delete(url.path, headers);
    } else if (method == "HEAD") {
        result = cli.Head(url.path, headers);
    } else {
        spdlog::error("HTTP request failed: unsupported method={} url={}", method, request.url);
        return std::nullopt;
    }

    if (!result) {
        spdlog::error("HTTP request failed: {} {}", method, request.url);
        return std::nullopt;
    }

    HttpResponse response;
    response.status = result->status;
    response.body = result->body;
    for (const auto& [key, value] : result->headers) {
        response.headers.emplace_back(key, value);
    }

    return response;
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
    auto response = send(request);
    if (!response) {
        return std::nullopt;
    }
    if (response->status < 200 || response->status >= 300) {
        spdlog::warn("HTTP non-2xx status={} body={}",
                     response->status, response->body.substr(0, 512));
        return std::nullopt;
    }
    return response;
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

} // namespace novel
