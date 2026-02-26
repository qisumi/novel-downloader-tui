#include "api/fanqie_client.h"

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <thread>
#include <chrono>

using json = nlohmann::json;

namespace fanqie {

// ──────────────────────────────────────────────────────────────────────────────
// 构造 / 配置
// ──────────────────────────────────────────────────────────────────────────────

FanqieClient::FanqieClient(std::string api_key)
    : api_key_(std::move(api_key)) {}

std::string FanqieClient::build_url(int type, const std::string& extra) const {
    std::string url = base_url_ + "?apikey=" + api_key_
                    + "&type=" + std::to_string(type);
    if (!extra.empty()) url += "&" + extra;
    return url;
}

// ──────────────────────────────────────────────────────────────────────────────
// HTTP 基础
// ──────────────────────────────────────────────────────────────────────────────

std::optional<std::string> FanqieClient::http_get(const std::string& url) {
    // 拆分 scheme+host 和 path
    std::string scheme_host, path;
    if (url.starts_with("https://")) {
        scheme_host = url.substr(0, url.find('/', 8));
        path        = url.substr(scheme_host.size());
    } else {
        scheme_host = url.substr(0, url.find('/', 7));
        path        = url.substr(scheme_host.size());
    }
    if (path.empty()) path = "/";

    spdlog::debug("HTTP GET {} | scheme_host={} path={}", url, scheme_host, path);

    // httplib::Client 支持 "http://host" / "https://host" 两种形式，
    // 配合 set_follow_location(true) 可自动跟随 301/302（含 HTTP→HTTPS 跳转）
    httplib::Client cli(scheme_host);
    cli.set_connection_timeout(timeout_s_, 0);
    cli.set_read_timeout(timeout_s_, 0);
    cli.set_follow_location(true);   // 自动跟随 3xx 重定向

    auto res = cli.Get(path);
    if (!res) {
        spdlog::error("HTTP GET failed (no response): url={} error={}",
                      url, httplib::to_string(res.error()));
        return std::nullopt;
    }
    spdlog::debug("HTTP response status={} body_size={}", res->status, res->body.size());
    if (res->status != 200) {
        spdlog::warn("HTTP non-200 status={} body={}", res->status, res->body.substr(0, 512));
        return std::nullopt;
    }
    spdlog::trace("HTTP body (first 1024): {}", res->body.substr(0, 1024));
    return res->body;
}

// ──────────────────────────────────────────────────────────────────────────────
// 辅助：从 JSON 安全读取字符串 / 数值
// ──────────────────────────────────────────────────────────────────────────────
static std::string safe_str(const json& j, const std::string& key) {
    if (j.contains(key) && j[key].is_string()) return j[key].get<std::string>();
    if (j.contains(key) && j[key].is_number()) return std::to_string(j[key].get<int64_t>());
    return {};
}

static Book parse_book(const json& d) {
    Book b;
    b.book_id         = safe_str(d, "book_id");
    b.title           = safe_str(d, "book_name");
    b.author          = safe_str(d, "author");
    b.cover_url       = safe_str(d, "thumb_url");
    b.abstract        = safe_str(d, "abstract");
    b.category        = safe_str(d, "category");
    b.word_count      = safe_str(d, "word_number");
    b.last_chapter_title = safe_str(d, "last_chapter_title");
    if (d.contains("score") && d["score"].is_number())
        b.score = d["score"].get<double>();
    if (d.contains("gender") && d["gender"].is_number())
        b.gender = d["gender"].get<int>();
    if (d.contains("creation_status") && d["creation_status"].is_number())
        b.creation_status = d["creation_status"].get<int>();
    if (d.contains("last_chapter_update_time") && d["last_chapter_update_time"].is_number())
        b.last_update_time = d["last_chapter_update_time"].get<int64_t>();
    return b;
}

// ──────────────────────────────────────────────────────────────────────────────
// 搜索  type=1
// ──────────────────────────────────────────────────────────────────────────────
std::vector<Book> FanqieClient::search(const std::string& keywords, int page) {
    // URL encode keywords（简单替换空格，完整实现可用 percent-encoding）
    std::string kw = keywords;
    auto url = build_url(1, "keywords=" + kw + "&page=" + std::to_string(page * 10));
    spdlog::info("search() keywords='{}' page={} url={}", keywords, page, url);

    auto body = http_get(url);
    if (!body) {
        spdlog::warn("search() http_get returned nullopt for keywords='{}'" , keywords);
        return {};
    }

    std::vector<Book> results;
    try {
        auto root = json::parse(*body);
        spdlog::debug("search() JSON top-level keys: {}", [&]{
            std::string keys;
            for (auto& [k, v] : root.items()) { if (!keys.empty()) keys += ", "; keys += k; }
            return keys;
        }());

        // 参考 ruleSearch 逻辑：
        // if res.search_tabs  → 找第一个非空 tab.data；每条 item 有 book_data[0]
        // else                → res.data；每条 item 有 book_data[0]
        if (root.contains("search_tabs") && root["search_tabs"].is_array()) {
            spdlog::debug("search() using search_tabs branch, tab count={}",
                          root["search_tabs"].size());
            for (auto& tab : root["search_tabs"]) {
                if (!tab.contains("data") || tab["data"].is_null() || !tab["data"].is_array())
                    continue;
                spdlog::debug("search() tab data size={}", tab["data"].size());
                for (auto& item : tab["data"]) {
                    if (item.contains("book_data") &&
                        item["book_data"].is_array() &&
                        !item["book_data"].empty())
                    {
                        auto book = parse_book(item["book_data"][0]);
                        spdlog::trace("search() book_id={} title={}", book.book_id, book.title);
                        results.push_back(std::move(book));
                    }
                }
                if (!results.empty()) break; // 取第一个有数据的 tab
            }
        } else if (root.contains("data") && root["data"].is_array()) {
            spdlog::debug("search() using data branch, size={}", root["data"].size());
            for (auto& item : root["data"]) {
                if (item.contains("book_data") &&
                    item["book_data"].is_array() &&
                    !item["book_data"].empty())
                {
                    auto book = parse_book(item["book_data"][0]);
                    spdlog::trace("search() book_id={} title={}", book.book_id, book.title);
                    results.push_back(std::move(book));
                } else {
                    auto book = parse_book(item);
                    spdlog::trace("search() (flat) book_id={} title={}", book.book_id, book.title);
                    results.push_back(std::move(book));
                }
            }
        } else {
            spdlog::warn("search() unexpected JSON structure. body snippet: {}",
                         body->substr(0, 512));
        }
    } catch (const std::exception& e) {
        spdlog::error("search() JSON parse exception: {}", e.what());
        spdlog::error("search() raw body snippet: {}", body->substr(0, 512));
    }

    spdlog::info("search() returning {} results for keywords='{}'", results.size(), keywords);
    return results;
}

// ──────────────────────────────────────────────────────────────────────────────
// 书籍详情  type=2
// ──────────────────────────────────────────────────────────────────────────────
std::optional<Book> FanqieClient::get_book_info(const std::string& book_id) {
    auto url = build_url(2, "bookid=" + book_id);
    spdlog::info("get_book_info() book_id={} url={}", book_id, url);
    auto body = http_get(url);
    if (!body) {
        spdlog::warn("get_book_info() http_get failed for book_id={}", book_id);
        return std::nullopt;
    }
    try {
        auto root = json::parse(*body);
        auto& d = root["data"];
        auto book = parse_book(d);
        spdlog::debug("get_book_info() parsed title='{}'", book.title);
        return book;
    } catch (const std::exception& e) {
        spdlog::error("get_book_info() parse error: {} | body: {}", e.what(), body->substr(0, 256));
        return std::nullopt;
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// 目录  type=3
// ──────────────────────────────────────────────────────────────────────────────
std::vector<TocItem> FanqieClient::get_toc(const std::string& book_id) {
    auto url = build_url(3, "bookid=" + book_id);
    spdlog::info("get_toc() book_id={} url={}", book_id, url);
    auto body = http_get(url);
    if (!body) {
        spdlog::warn("get_toc() http_get failed for book_id={}", book_id);
        return {};
    }

    std::vector<TocItem> toc;
    try {
        auto root   = json::parse(*body);
        auto& items = root["data"]["item_data_list"];
        for (auto& it : items) {
            TocItem t;
            t.item_id    = safe_str(it, "item_id");
            t.title      = safe_str(it, "title");
            t.volume_name= safe_str(it, "volume_name");
            if (it.contains("chapter_word_number") && it["chapter_word_number"].is_number())
                t.word_count = it["chapter_word_number"].get<int>();
            if (it.contains("first_pass_time") && it["first_pass_time"].is_number())
                t.update_time = it["first_pass_time"].get<int64_t>();
            toc.push_back(std::move(t));
        }
    } catch (const std::exception& e) {
        spdlog::error("get_toc() parse error: {} | body: {}", e.what(), body->substr(0, 256));
    }
    spdlog::info("get_toc() returning {} items for book_id={}", toc.size(), book_id);
    return toc;
}

// ──────────────────────────────────────────────────────────────────────────────
// 章节正文  type=4
// ──────────────────────────────────────────────────────────────────────────────
std::optional<Chapter> FanqieClient::get_chapter(const std::string& item_id) {
    auto url = build_url(4, "itemid=" + item_id);
    spdlog::debug("get_chapter() item_id={}", item_id);
    auto body = http_get(url);
    if (!body) {
        spdlog::warn("get_chapter() http_get failed for item_id={}", item_id);
        return std::nullopt;
    }
    try {
        auto root = json::parse(*body);
        Chapter ch;
        ch.item_id = item_id;
        ch.content = root["data"]["content"].get<std::string>();
        spdlog::debug("get_chapter() content_size={} for item_id={}", ch.content.size(), item_id);
        return ch;
    } catch (const std::exception& e) {
        spdlog::error("get_chapter() parse error: {} | body: {}", e.what(), body->substr(0, 256));
        return std::nullopt;
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// 批量下载章节
// ──────────────────────────────────────────────────────────────────────────────
std::vector<Chapter> FanqieClient::download_chapters(
    const std::vector<TocItem>& toc,
    std::function<void(int, int)> progress_cb)
{
    std::vector<Chapter> chapters;
    chapters.reserve(toc.size());
    int total = static_cast<int>(toc.size());

    for (int i = 0; i < total; ++i) {
        auto ch = get_chapter(toc[i].item_id);
        if (ch) {
            ch->title = toc[i].title;
            chapters.push_back(std::move(*ch));
        }
        if (progress_cb) progress_cb(i + 1, total);
        // 避免过快请求被限速
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    return chapters;
}

} // namespace fanqie
