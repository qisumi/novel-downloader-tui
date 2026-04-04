#include "source/host/http_service.h"

#include <spdlog/spdlog.h>
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <memory>
#include <optional>
#include <sstream>
#include <type_traits>

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#endif

namespace novel {

namespace {

struct SplitUrlResult {
    std::string scheme;
    std::string host;
    int         port = 80;
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
    result.scheme = scheme_end == std::string::npos ? "http" : url.substr(0, scheme_end);
    if (host_end == std::string::npos) {
        result.scheme_host = url;
        result.path = "/";
    } else {
        result.scheme_host = url.substr(0, host_end);
        result.path = url.substr(host_end);
    }
    const std::size_t host_start = scheme_end == std::string::npos ? 0 : scheme_end + 3;
    const auto host_port = url.substr(host_start, (host_end == std::string::npos ? url.size() : host_end) - host_start);
    const auto colon_pos = host_port.rfind(':');
    result.host = colon_pos == std::string::npos ? host_port : host_port.substr(0, colon_pos);
    result.port = result.scheme == "https" ? 443 : 80;
    if (colon_pos != std::string::npos) {
        try {
            result.port = std::stoi(host_port.substr(colon_pos + 1));
        } catch (...) {
        }
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

std::string normalize_header_name(std::string name) {
    for (char& ch : name) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return name;
}

std::string truncate_for_log(const std::string& value, std::size_t max_length = 160) {
    if (value.size() <= max_length) {
        return value;
    }
    return value.substr(0, max_length) + "...";
}

std::string request_path_without_query(const std::string& path) {
    const auto query_pos = path.find('?');
    if (query_pos == std::string::npos) {
        return path.empty() ? "/" : path;
    }
    return query_pos == 0 ? "/" : path.substr(0, query_pos);
}

bool path_matches_cookie(const std::string& request_path, const std::string& cookie_path) {
    const auto normalized_cookie_path = cookie_path.empty() ? "/" : cookie_path;
    if (normalized_cookie_path == "/") {
        return true;
    }
    return request_path.rfind(normalized_cookie_path, 0) == 0;
}

bool domain_matches_cookie(const std::string& request_host, const std::string& cookie_domain) {
    if (cookie_domain.empty()) {
        return false;
    }
    if (request_host == cookie_domain) {
        return true;
    }
    if (request_host.size() <= cookie_domain.size()) {
        return false;
    }

    const auto offset = request_host.size() - cookie_domain.size();
    return request_host.compare(offset, cookie_domain.size(), cookie_domain) == 0
        && request_host[offset - 1] == '.';
}

std::vector<std::string> split_cookie_parts(const std::string& value) {
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (start <= value.size()) {
        const auto next = value.find(';', start);
        auto part = value.substr(start, next == std::string::npos ? std::string::npos : next - start);
        const auto first = part.find_first_not_of(" \t");
        const auto last = part.find_last_not_of(" \t");
        if (first != std::string::npos) {
            parts.push_back(part.substr(first, last - first + 1));
        }
        if (next == std::string::npos) {
            break;
        }
        start = next + 1;
    }
    return parts;
}

std::vector<std::string> split_set_cookie_header_values(const std::string& value) {
    std::vector<std::string> cookies;
    std::size_t start = 0;
    bool in_expires = false;

    for (std::size_t index = 0; index < value.size(); ++index) {
        const char ch = value[index];
        if (ch == ';') {
            in_expires = false;
            continue;
        }

        if ((ch == 'e' || ch == 'E') && index + 8 <= value.size()) {
            auto lower = value.substr(index, 8);
            std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            if (lower == "expires=") {
                in_expires = true;
                index += 7;
                continue;
            }
        }

        if (ch != ',' || in_expires) {
            continue;
        }

        std::size_t next = index + 1;
        while (next < value.size() && std::isspace(static_cast<unsigned char>(value[next]))) {
            ++next;
        }
        const auto equals_pos = value.find('=', next);
        const auto semicolon_pos = value.find(';', next);
        if (equals_pos == std::string::npos || (semicolon_pos != std::string::npos && equals_pos > semicolon_pos)) {
            continue;
        }

        auto token = value.substr(start, index - start);
        const auto first = token.find_first_not_of(" \t");
        const auto last = token.find_last_not_of(" \t");
        if (first != std::string::npos) {
            cookies.push_back(token.substr(first, last - first + 1));
        }
        start = next;
    }

    auto token = value.substr(start);
    const auto first = token.find_first_not_of(" \t");
    const auto last = token.find_last_not_of(" \t");
    if (first != std::string::npos) {
        cookies.push_back(token.substr(first, last - first + 1));
    }

    return cookies;
}

std::string format_cookie_descriptors(const std::vector<HttpService::CookieEntry>& cookies) {
    if (cookies.empty()) {
        return "<none>";
    }

    std::string text;
    for (std::size_t index = 0; index < cookies.size(); ++index) {
        if (index > 0) {
            text += ", ";
        }
        text += cookies[index].name + "@"
            + (cookies[index].domain.empty() ? "<host>" : cookies[index].domain)
            + cookies[index].path;
        if (cookies[index].secure) {
            text += ";Secure";
        }
    }
    return text;
}

bool is_supported_method(const std::string& method) {
    return method == "GET"
 || method == "POST" || method == "PUT"
 || method == "PATCH" || method == "DELETE" || method == "HEAD";
}

#ifdef _WIN32

std::wstring widen(std::string_view value) {
    if (value.empty()) {
        return {};
    }
    const int size = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (size <= 0) {
        return std::wstring(value.begin(), value.end());
    }
    std::wstring wide(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), wide.data(), size);
    return wide;
}

std::string narrow(std::wstring_view value) {
    if (value.empty()) {
        return {};
    }
    const int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        std::string text;
        text.reserve(value.size());
        for (wchar_t ch : value) {
            text.push_back(ch >= 0 && ch <= 0x7f ? static_cast<char>(ch) : '?');
        }
        return text;
    }
    std::string text(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), text.data(), size, nullptr, nullptr);
    return text;
}

std::vector<std::pair<std::string, std::string>> parse_raw_headers(std::wstring_view raw_headers) {
    std::vector<std::pair<std::string, std::string>> headers;
    std::size_t start = 0;
    bool first_line = true;
    while (start < raw_headers.size()) {
        const auto end = raw_headers.find(L"\r\n", start);
        const auto line = raw_headers.substr(start, end == std::wstring_view::npos ? std::wstring_view::npos : end - start);
        start = end == std::wstring_view::npos ? raw_headers.size() : end + 2;
        if (line.empty()) { continue; }
        if (first_line) { first_line = false; continue; }
        const auto colon_pos = line.find(L':');
        if (colon_pos == std::wstring_view::npos) { continue; }
        auto key = line.substr(0, colon_pos);
        auto value = line.substr(colon_pos + 1);
        while (!value.empty() && (value.front() == L' ' || value.front() == L'\t')) {
            value.remove_prefix(1);
        }
        headers.emplace_back(narrow(key), narrow(value));
    }
    return headers;
}

std::optional<HttpResponse> send_with_winhttp(
    const HttpRequest& request,
    const SplitUrlResult& url,
    const std::string& method,
    const std::vector<std::pair<std::string, std::string>>& headers) {
    HINTERNET session = WinHttpOpen(L"novel-downloader/0.2",
                                    WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                    WINHTTP_NO_PROXY_NAME,
                                    WINHTTP_NO_PROXY_BYPASS,
                                    0);
    if (!session) {
        spdlog::error("WinHTTP open failed: url={}", request.url);
        return std::nullopt;
    }

    const auto close_handle = [](HINTERNET handle) {
        if (handle) { WinHttpCloseHandle(handle); }
    };

    std::unique_ptr<std::remove_pointer_t<HINTERNET>, decltype(close_handle)> session_guard(session, close_handle);

    WinHttpSetTimeouts(session,
                       request.timeout_seconds * 1000,
                       request.timeout_seconds * 1000,
                       request.timeout_seconds * 1000,
                       request.timeout_seconds * 1000);

    auto connect = WinHttpConnect(session, widen(url.host).c_str(), static_cast<INTERNET_PORT>(url.port), 0);
    if (!connect) {
        spdlog::error("WinHTTP connect failed: host={} port={}", url.host, url.port);
        return std::nullopt;
    }
    std::unique_ptr<std::remove_pointer_t<HINTERNET>, decltype(close_handle)> connect_guard(connect, close_handle);    DWORD flags = url.scheme == "https" ? WINHTTP_FLAG_SECURE : 0;
    auto req = WinHttpOpenRequest(connect,
                                  widen(method).c_str(),
                                  widen(url.path).c_str(),
                                  nullptr,
                                  WINHTTP_NO_REFERER,
                                  WINHTTP_DEFAULT_ACCEPT_TYPES,
                                  flags);
    if (!req) {
        spdlog::error("WinHTTP open request failed: method={} url={}", method, request.url);
        return std::nullopt;
    }
    std::unique_ptr<std::remove_pointer_t<HINTERNET>, decltype(close_handle)> request_guard(req, close_handle);

    if (!request.follow_redirects) {
        DWORD disable = WINHTTP_DISABLE_REDIRECTS;
        WinHttpSetOption(req, WINHTTP_OPTION_DISABLE_FEATURE, &disable, sizeof(disable));
    }

    std::wstring header_block;
    for (const auto& [key, value] : headers) {
        header_block += widen(key);
        header_block += L": ";
        header_block += widen(value);
        header_block += L"\r\n";
    }

    const auto content_type = detect_content_type(request);
    if (!request.body.empty()) {
        const auto has_content_type = std::any_of(headers.begin(), headers.end(), [](const auto& entry) {
            return normalize_header_name(entry.first) == "content-type";
        });
        if (!has_content_type) {
            header_block += L"Content-Type: ";
            header_block += widen(content_type);
            header_block += L"\r\n";
        }
    }

    auto* body_ptr = request.body.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<char*>(request.body.data());
    const auto body_size = static_cast<DWORD>(request.body.size());
    if (!WinHttpSendRequest(req,
                            header_block.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : header_block.c_str(),
                            header_block.empty() ? 0 : static_cast<DWORD>(header_block.size()),
                            body_ptr,
                            body_size,
                            body_size,
                            0)) {
        spdlog::error("WinHTTP send failed: method={} url={}", method, request.url);
        return std::nullopt;
    }

    if (!WinHttpReceiveResponse(req, nullptr)) {
        spdlog::error("WinHTTP receive failed: method={} url={}", method, request.url);
        return std::nullopt;
    }

    DWORD status = 0;
    DWORD status_size = sizeof(status);
    if (!WinHttpQueryHeaders(req,
                             WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX,
                             &status,
                             &status_size,
                             WINHTTP_NO_HEADER_INDEX)) {
        spdlog::error("WinHTTP status query failed: method={} url={}", method, request.url);
        return std::nullopt;
    }

    DWORD raw_size = 0;
    WinHttpQueryHeaders(req,
                        WINHTTP_QUERY_RAW_HEADERS_CRLF,
                        WINHTTP_HEADER_NAME_BY_INDEX,
                        WINHTTP_NO_OUTPUT_BUFFER,
                        &raw_size,
                        WINHTTP_NO_HEADER_INDEX);

    std::wstring raw_headers;
    if (GetLastError() == ERROR_INSUFFICIENT_BUFFER && raw_size > 0) {
        raw_headers.resize(raw_size / sizeof(wchar_t));
        if (WinHttpQueryHeaders(req,
                                WINHTTP_QUERY_RAW_HEADERS_CRLF,
                                WINHTTP_HEADER_NAME_BY_INDEX,
                                raw_headers.data(),
                                &raw_size,
                                WINHTTP_NO_HEADER_INDEX)) {
            if (!raw_headers.empty() && raw_headers.back() == L'\0') {
                raw_headers.pop_back();
            }
        } else {
            raw_headers.clear();
        }
    }

    std::string body;
    for (;;) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(req, &available)) {
            spdlog::error("WinHTTP query data failed: method={} url={}", method, request.url);
            return std::nullopt;
        }
        if (available == 0) { break; }
        std::string chunk(static_cast<std::size_t>(available), '\0');
        DWORD read = 0;
        if (!WinHttpReadData(req, chunk.data(), available, &read)) {
            spdlog::error("WinHTTP read failed: method={} url={}", method, request.url);
            return std::nullopt;
        }
        chunk.resize(read);
        body += chunk;
    }

    HttpResponse response;
    response.status = static_cast<int>(status);
    response.body = std::move(body);
    response.headers = parse_raw_headers(raw_headers);
    return response;

}

#endif

std::optional<HttpService::CookieEntry> parse_set_cookie(
    const std::string& host,
    const std::string& path,
    const std::string& header_value) {
    const auto parts = split_cookie_parts(header_value);
    if (parts.empty()) {
        return std::nullopt;
    }

    const auto name_end = parts[0].find('=');
    if (name_end == std::string::npos || name_end == 0) {
        return std::nullopt;
    }

    HttpService::CookieEntry cookie;
    cookie.name = parts[0].substr(0, name_end);
    cookie.value = parts[0].substr(name_end + 1);
    cookie.domain = host;
    cookie.path = "/";

    const auto request_path = request_path_without_query(path);
    const auto slash_pos = request_path.rfind('/');
    if (slash_pos != std::string::npos && slash_pos > 0) {
        cookie.path = request_path.substr(0, slash_pos);
    }

    for (std::size_t index = 1; index < parts.size(); ++index) {
        const auto& part = parts[index];
        const auto attr_end = part.find('=');
        const auto attr_name = normalize_header_name(
            attr_end == std::string::npos ? part : part.substr(0, attr_end));
        const auto attr_value = attr_end == std::string::npos ? "" : part.substr(attr_end + 1);

        if (attr_name == "domain" && !attr_value.empty()) {
            cookie.domain = attr_value.front() == '.' ? attr_value.substr(1) : attr_value;
        } else if (attr_name == "path" && !attr_value.empty()) {
            cookie.path = attr_value;
        } else if (attr_name == "secure") {
            cookie.secure = true;
        } else if (attr_name == "max-age") {
            try {
                if (std::stoi(attr_value) <= 0) {
                    cookie.value.clear();
                }
            } catch (...) {
            }
        }
    }

    if (cookie.path.empty()) {
        cookie.path = "/";
    }

    return cookie;
}

} // namespace

std::optional<HttpResponse> HttpService::send(const HttpRequest& request) const {
    SplitUrlResult url = split_url(request.url);
    std::string    method = normalize_method(request.method);
    if (url.scheme != "http" && url.scheme != "https") {
        spdlog::error("HTTP request failed: unsupported scheme={} url={}", url.scheme, request.url);
        return std::nullopt;
    }
    if (!is_supported_method(method)) {
        spdlog::error("HTTP request failed: unsupported method={} url={}", method, request.url);
        return std::nullopt;
    }

    auto merged_headers = request.headers;
    attach_cookie_header(url.scheme, url.host, url.path, merged_headers);

    auto response = send_with_winhttp(request, url, method, merged_headers);
    if (!response) {
        return std::nullopt;
    }

    store_response_cookies(url.host, url.path, response->headers);

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
    request.follow_redirects = true;
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

void HttpService::attach_cookie_header(
    const std::string& scheme,
    const std::string& host,
    const std::string& path,
    std::vector<std::pair<std::string, std::string>>& headers) const {
    std::vector<CookieEntry> matched_cookies;
    const auto request_path = request_path_without_query(path);

    {
        std::lock_guard lock(cookie_mutex_);
        for (const auto& cookie : cookies_) {
            if (cookie.secure && scheme != "https") {
                continue;
            }
            if (!domain_matches_cookie(host, cookie.domain)) {
                continue;
            }
            if (!path_matches_cookie(request_path, cookie.path)) {
                continue;
            }
            if (cookie.name.empty() || cookie.value.empty()) {
                continue;
            }
            matched_cookies.push_back(cookie);
        }
    }

    if (matched_cookies.empty()) {
        return;
    }

    std::stable_sort(
        matched_cookies.begin(),
        matched_cookies.end(),
        [](const CookieEntry& lhs, const CookieEntry& rhs) {
            if (lhs.path.size() != rhs.path.size()) {
                return lhs.path.size() > rhs.path.size();
            }
            return lhs.name < rhs.name;
        });

    std::vector<std::string> parts;
    parts.reserve(matched_cookies.size());
    for (const auto& cookie : matched_cookies) {
        parts.push_back(cookie.name + "=" + cookie.value);
    }

    std::string jar_value;
    for (std::size_t index = 0; index < parts.size(); ++index) {
        if (index > 0) {
            jar_value += "; ";
        }
        jar_value += parts[index];
    }

    for (auto& [key, value] : headers) {
        if (normalize_header_name(key) == "cookie") {
            if (!value.empty()) {
                value += "; ";
            }
            value += jar_value;
            return;
        }
    }

    headers.emplace_back("Cookie", jar_value);
}

void HttpService::store_response_cookies(
    const std::string& host,
    const std::string& path,
    const std::vector<std::pair<std::string, std::string>>& headers) const {
    std::lock_guard lock(cookie_mutex_);
    for (const auto& [key, value] : headers) {
        if (normalize_header_name(key) != "set-cookie") {
            continue;
        }

        const auto raw_cookies = split_set_cookie_header_values(value);

        for (const auto& raw_cookie : raw_cookies) {
            const auto cookie = parse_set_cookie(host, path, raw_cookie);
            if (!cookie || cookie->name.empty() || cookie->domain.empty()) {
                continue;
            }

            const auto it = std::find_if(
                cookies_.begin(),
                cookies_.end(),
                [&](const CookieEntry& current) {
                    return current.name == cookie->name
                        && current.domain == cookie->domain
                        && current.path == cookie->path;
                });

            if (cookie->value.empty()) {
                if (it != cookies_.end()) {
                    cookies_.erase(it);
                }
                continue;
            }

            if (it != cookies_.end()) {
                *it = *cookie;
            } else {
                cookies_.push_back(*cookie);
            }
        }
    }
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
