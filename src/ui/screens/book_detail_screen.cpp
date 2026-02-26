#include "ui/screens/book_detail_screen.h"
#include "export/epub_exporter.h"
#include "export/txt_exporter.h"

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <thread>
#include <atomic>
#include <mutex>

using namespace ftxui;

namespace fanqie {

ftxui::Component make_book_detail_screen(
    std::shared_ptr<AppContext> ctx,
    ScreenInteractive&           screen)
{
    struct BookDetailState {
        Book               book;
        std::vector<TocItem> toc;
        int                selected   = 0;
        bool               loading    = false;
        int                dl_current = 0;
        int                dl_total   = 0;
        int                range_start = -1;
        int                range_end   = -1;
        std::string        status_msg;
        std::mutex         mtx;
    };
    auto state = std::make_shared<BookDetailState>();
    state->book = ctx->current_book;

    // ── 异步加载目录 ───────────────────────────────────────────
    auto load_toc = [=, &screen]() {
        {
            std::lock_guard lock(state->mtx);
            state->loading = true;
            state->status_msg = "加载目录中…";
        }
        screen.PostEvent(Event::Custom);

        std::thread([=, &screen]() {
            // 优先读本地缓存
            auto toc = ctx->db->get_toc(state->book.book_id);
            if (toc.empty()) {
                toc = ctx->client->get_toc(state->book.book_id);
                if (!toc.empty())
                    ctx->db->save_toc(state->book.book_id, toc);
            }
            std::lock_guard lock(state->mtx);
            state->toc     = std::move(toc);
            state->loading = false;
            state->status_msg = state->toc.empty() ? "目录加载失败" : "";
            screen.PostEvent(Event::Custom);
        }).detach();
    };
    load_toc();

    // ── 批量下载 ───────────────────────────────────────────────
    auto download_all = [=, &screen]() {
        std::lock_guard lock(state->mtx);
        if (state->toc.empty() || state->loading) return;
        state->loading    = true;
        state->dl_total   = static_cast<int>(state->toc.size());
        state->dl_current = 0;
        state->status_msg = "开始下载…";

        auto toc_copy = state->toc;
        auto book_id  = state->book.book_id;
        std::thread([=, &screen]() {
            for (int i = 0; i < static_cast<int>(toc_copy.size()); ++i) {
                const auto& t = toc_copy[i];
                if (!ctx->db->chapter_cached(t.item_id)) {
                    auto ch = ctx->client->get_chapter(t.item_id);
                    if (ch) {
                        ch->title = t.title;
                        ctx->db->save_chapter(book_id, *ch);
                    }
                }
                {
                    std::lock_guard lock(state->mtx);
                    state->dl_current = i + 1;
                }
                screen.PostEvent(Event::Custom);
            }
            std::lock_guard lock(state->mtx);
            state->loading    = false;
            state->status_msg = "下载完成！";
            screen.PostEvent(Event::Custom);
        }).detach();
    };

    auto resolve_export_range = [=](int toc_size, int start_mark, int end_mark) {
        int start = 0;
        int end = toc_size - 1;
        if (start_mark >= 0 && end_mark >= 0) {
            start = std::min(start_mark, end_mark);
            end   = std::max(start_mark, end_mark);
        } else if (start_mark >= 0) {
            start = start_mark;
        } else if (end_mark >= 0) {
            end = end_mark;
        }
        start = std::clamp(start, 0, std::max(0, toc_size - 1));
        end   = std::clamp(end, 0, std::max(0, toc_size - 1));
        if (start > end) std::swap(start, end);
        return std::pair<int, int>{start, end};
    };

    auto make_range_suffix = [](int start, int end, int total) {
        if (total <= 0 || (start == 0 && end == total - 1)) return std::string{};
        std::ostringstream os;
        os << "_ch" << std::setw(4) << std::setfill('0') << (start + 1)
           << "-" << std::setw(4) << std::setfill('0') << (end + 1);
        return os.str();
    };

    auto export_by_format = [=, &screen](bool as_epub) {
        Book book_copy;
        std::vector<TocItem> toc_copy;
        int start = 0;
        int end = -1;

        {
            std::lock_guard lock(state->mtx);
            if (state->loading || state->toc.empty()) return;
            book_copy = state->book;
            toc_copy  = state->toc;
            auto [s, e] = resolve_export_range(static_cast<int>(toc_copy.size()),
                                               state->range_start, state->range_end);
            start = s;
            end = e;
            state->loading    = true;
            state->dl_total   = std::max(0, end - start + 1);
            state->dl_current = 0;
            state->status_msg = as_epub ? "准备导出 EPUB…" : "准备导出 TXT…";
        }
        screen.PostEvent(Event::Custom);

        std::thread([=, &screen]() {
            std::vector<Chapter> chapters;
            chapters.reserve(std::max(0, end - start + 1));

            for (int i = start; i <= end; ++i) {
                const auto& t = toc_copy[i];
                auto ch = ctx->db->get_chapter(t.item_id);
                if (!ch) {
                    ch = ctx->client->get_chapter(t.item_id);
                    if (ch) {
                        ch->title = t.title;
                        ctx->db->save_chapter(book_copy.book_id, *ch);
                    }
                }
                if (ch) {
                    ch->title = t.title;
                    chapters.push_back(*ch);
                }

                {
                    std::lock_guard lock(state->mtx);
                    state->dl_current = i - start + 1;
                    state->status_msg = "准备章节中…";
                }
                screen.PostEvent(Event::Custom);
            }

            std::string path;
            auto suffix = make_range_suffix(start, end, static_cast<int>(toc_copy.size()));
            if (as_epub) {
                EpubOptions opts;
                opts.output_dir = ctx->epub_output_dir;
                opts.filename_suffix = suffix;
                path = EpubExporter::export_book(book_copy, chapters, opts,
                    [=, &screen](int cur, int tot) {
                        std::lock_guard lock(state->mtx);
                        state->dl_current = cur;
                        state->dl_total   = tot;
                        state->status_msg = "正在打包 EPUB…";
                        screen.PostEvent(Event::Custom);
                    });
            } else {
                TxtOptions opts;
                opts.output_dir = ctx->epub_output_dir;
                opts.filename_suffix = suffix;
                path = TxtExporter::export_book(book_copy, chapters, opts,
                    [=, &screen](int cur, int tot) {
                        std::lock_guard lock(state->mtx);
                        state->dl_current = cur;
                        state->dl_total   = tot;
                        state->status_msg = "正在写入 TXT…";
                        screen.PostEvent(Event::Custom);
                    });
            }

            std::lock_guard lock(state->mtx);
            state->loading    = false;
            if (path.empty()) {
                state->status_msg = as_epub ? "EPUB 导出失败" : "TXT 导出失败";
            } else {
                state->status_msg = "已导出：" + path;
            }
            screen.PostEvent(Event::Custom);
        }).detach();
    };

    auto export_epub = [=, &screen]() { export_by_format(true); };
    auto export_txt  = [=, &screen]() { export_by_format(false); };

    auto container = Container::Vertical({});

    auto renderer = Renderer(container, [=]() mutable {
        std::lock_guard lock(state->mtx);
        auto [range_s, range_e] = resolve_export_range(
            static_cast<int>(state->toc.size()), state->range_start, state->range_end);
        std::string range_text;
        if (state->toc.empty()) {
            range_text = "导出范围：无章节";
        } else if (range_s == 0 && range_e == static_cast<int>(state->toc.size()) - 1) {
            range_text = "导出范围：全部章节";
        } else {
            range_text = "导出范围：第 " + std::to_string(range_s + 1)
                       + " - " + std::to_string(range_e + 1) + " 章";
        }

        // 目录列表
        Elements toc_rows;
        for (int i = 0; i < static_cast<int>(state->toc.size()); ++i) {
            const auto& t   = state->toc[i];
            bool cached     = ctx->db->chapter_cached(t.item_id);
            auto row = hbox({
                text(i == state->selected ? " ▶ " : "   "),
                text(t.title)
                    | (i == state->selected ? bold : nothing)
                    | (cached ? color(Color::Green) : color(Color::White)),
                filler(),
                text(cached ? "✓" : " ") | color(Color::Green),
                text("  " + std::to_string(t.word_count) + "字") | color(Color::GrayDark),
            });
            toc_rows.push_back(row);
        }

        // 进度条（下载中显示）
        Element progress_bar = emptyElement();
        if (state->dl_total > 0) {
            float ratio = static_cast<float>(state->dl_current) / state->dl_total;
            progress_bar = hbox({
                text(" ["),
                gauge(ratio) | flex,
                text("] " + std::to_string(state->dl_current)
                     + "/" + std::to_string(state->dl_total)),
            });
        }

        return vbox({
            // 书籍信息头
            hbox({
                vbox({
                    text(state->book.title) | bold | color(Color::Cyan),
                    text("作者：" + state->book.author),
                    text("分类：" + state->book.category + "  "
                         + (state->book.creation_status == 1 ? "完结" : "连载")),
                    text("字数：" + state->book.word_count),
                }) | flex,
            }) | border,
            // 简介
            paragraph(state->book.abstract) | color(Color::GrayLight) | flex_shrink,
            separator(),
            // 目录
            hbox({
                text(" 目录 (") | bold,
                text(std::to_string(state->toc.size()) + " 章)") | bold,
                filler(),
                text(" g") | bold, text(":下载全部  "),
                text(" [") | bold, text(":起点  "),
                text(" ]") | bold, text(":终点  "),
                text(" c") | bold, text(":清范围  "),
                text(" e") | bold, text(":导出EPUB  "),
                text(" t") | bold, text(":导出TXT  "),
                text(" ESC") | bold, text(":返回"),
            }),
            text("  " + range_text) | color(Color::GrayDark),
            separator(),
            state->loading && state->toc.empty()
              ? text("  加载中…") | color(Color::Yellow)
              : vbox(toc_rows) | frame | flex,
            progress_bar,
            separator(),
            text("  " + state->status_msg) | color(Color::Green),
        });
    });

    auto handler = CatchEvent(renderer, [=, &screen](Event ev) {
        if (ev == Event::Escape) {
            screen.ExitLoopClosure()();
            return true;
        }
        if (ev == Event::ArrowDown || ev == Event::Character('j')) {
            std::lock_guard lock(state->mtx);
            if (state->selected < static_cast<int>(state->toc.size()) - 1)
                ++state->selected;
            return true;
        }
        if (ev == Event::ArrowUp || ev == Event::Character('k')) {
            std::lock_guard lock(state->mtx);
            if (state->selected > 0) --state->selected;
            return true;
        }
        if (ev == Event::Character('g')) { download_all(); return true; }
        if (ev == Event::Character('e')) { export_epub();  return true; }
        if (ev == Event::Character('t')) { export_txt();   return true; }
        if (ev == Event::Character('[')) {
            std::lock_guard lock(state->mtx);
            if (!state->toc.empty()) {
                state->range_start = state->selected;
                state->status_msg = "已设置导出起点：第 "
                                  + std::to_string(state->selected + 1) + " 章";
            }
            return true;
        }
        if (ev == Event::Character(']')) {
            std::lock_guard lock(state->mtx);
            if (!state->toc.empty()) {
                state->range_end = state->selected;
                state->status_msg = "已设置导出终点：第 "
                                  + std::to_string(state->selected + 1) + " 章";
            }
            return true;
        }
        if (ev == Event::Character('c')) {
            std::lock_guard lock(state->mtx);
            state->range_start = -1;
            state->range_end = -1;
            state->status_msg = "已清除导出范围（恢复全部章节）";
            return true;
        }
        if (ev == Event::Character('a')) {
            ctx->db->save_book(state->book);
            std::lock_guard lock(state->mtx);
            state->status_msg = "已加入书架";
            return true;
        }
        return false;
    });

    return handler;
}

} // namespace fanqie
