#pragma once
#include <string>
#include <vector>
#include <optional>
#include <functional>
#include "models/book.h"

namespace fanqie {

/// 番茄小说 API 客户端
/// 所有网络请求均为同步调用，建议在工作线程中使用
class FanqieClient {
public:
    explicit FanqieClient(std::string api_key = "");

    // ── 搜索 ──────────────────────────────────────────────────
    /// 搜索书籍，page 从 0 开始（偏移量 = page * 10）
    std::vector<Book> search(const std::string& keywords, int page = 0);

    // ── 书籍信息 ───────────────────────────────────────────────
    std::optional<Book> get_book_info(const std::string& book_id);

    // ── 目录 ──────────────────────────────────────────────────
    std::vector<TocItem> get_toc(const std::string& book_id);

    // ── 章节内容 ───────────────────────────────────────────────
    std::optional<Chapter> get_chapter(const std::string& item_id);

    /// 批量下载章节，progress_cb(current, total) 用于汇报进度
    std::vector<Chapter> download_chapters(
        const std::vector<TocItem>& toc,
        std::function<void(int, int)> progress_cb = nullptr);

    // ── 配置 ──────────────────────────────────────────────────
    void set_api_key(const std::string& key) { api_key_ = key; }
    void set_timeout(int seconds)            { timeout_s_ = seconds; }

private:
    std::string base_url_ = "http://v3.rain.ink/fanqie/";
    std::string api_key_;
    int         timeout_s_ = 30;

    /// 发送 GET 请求，返回响应体字符串；失败则返回 std::nullopt
    std::optional<std::string> http_get(const std::string& url);

    /// 构造带 apikey 和 type 的完整 URL
    std::string build_url(int type, const std::string& extra_params = "") const;
};

} // namespace fanqie
