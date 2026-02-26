#include "ui/screens/search_screen.h"
#include "ui/screens/book_detail_screen.h"

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <spdlog/spdlog.h>
#include <thread>
#include <atomic>
#include <mutex>

using namespace ftxui;

namespace fanqie {

ftxui::Component make_search_screen(
    std::shared_ptr<AppContext> ctx,
    ScreenInteractive&           screen)
{
    // ── 状态 ──────────────────────────────────────────────────
    struct SearchState {
        std::string         keyword;
        std::vector<Book>   results;
        int                 selected  = 0;
        bool                loading   = false;
        std::string         status_msg;
        std::mutex          mtx;
    };
    auto state = std::make_shared<SearchState>();

    // ── 组件 ──────────────────────────────────────────────────
    InputOption input_opt;
    input_opt.placeholder = "输入书名或作者搜索…";
    auto input_kw = Input(&state->keyword, input_opt);

    // 搜索结果列表（MenuEntry 列表）
    auto menu = Container::Vertical({}, &state->selected);

    // ── 异步搜索 ───────────────────────────────────────────────
    auto do_search = [=, &screen]() {
        if (state->keyword.empty()) {
            spdlog::debug("search_screen: do_search called with empty keyword, skipping");
            return;
        }
        spdlog::info("search_screen: searching for '{}'", state->keyword);
        {
            std::lock_guard lock(state->mtx);
            state->loading    = true;
            state->status_msg  = "正在搜索…";
            state->results.clear();
        }
        screen.PostEvent(Event::Custom);

        std::thread([=, &screen]() {
            auto results = ctx->client->search(state->keyword);
            spdlog::info("search_screen: received {} results for '{}'",
                         results.size(), state->keyword);
            std::lock_guard lock(state->mtx);
            state->results   = std::move(results);
            state->loading   = false;
            state->status_msg = state->results.empty() ? "无结果" : "";
            state->selected  = 0;
            screen.PostEvent(Event::Custom);
        }).detach();
    };

    // ── 回车搜索 ───────────────────────────────────────────────
    auto search_btn = Button(" 搜索 ", do_search, ButtonOption::Ascii());

    auto top_bar = Container::Horizontal({input_kw, search_btn});

    // ── 进入详情 ───────────────────────────────────────────────
    auto open_detail = [=, &screen]() {
        std::lock_guard lock(state->mtx);
        if (state->results.empty()) return;
        if (state->selected < 0 ||
            state->selected >= static_cast<int>(state->results.size())) return;
        ctx->current_book = state->results[state->selected];
        // 弹出详情对话框（模态）
        auto detail = make_book_detail_screen(ctx, screen);
        screen.Loop(detail); // 阻塞直到详情页退出
    };

    auto container = Container::Vertical({top_bar});

    // ── 渲染 ──────────────────────────────────────────────────
    auto renderer = Renderer(container, [=, &screen]() mutable {
        std::lock_guard lock(state->mtx);

        Elements items;
        for (int i = 0; i < static_cast<int>(state->results.size()); ++i) {
            const auto& b = state->results[i];
            std::string label = b.title + "  (" + b.author + ")  "
                              + (b.creation_status == 1 ? "[完结]" : "[连载]")
                              + "  " + b.category;
            auto row = hbox({
                text(i == state->selected ? " ▶ " : "   "),
                text(label) | (i == state->selected ? bold : nothing),
                filler(),
                text(b.word_count + "字") | color(Color::GrayDark),
            });
            items.push_back(row);
        }

        return vbox({
            hbox({
                text(" 搜索 ") | bold,
                separator(),
                input_kw->Render() | flex,
                search_btn->Render(),
            }),
            separator(),
            state->loading
                ? (text("  正在搜索…") | color(Color::Yellow))
                : (vbox(items) | frame | flex),
            separator(),
            hbox({
                text(state->status_msg.empty()
                     ? (state->results.empty() ? "" : " ↑↓选择  Enter查看详情  a加入书架")
                     : "  " + state->status_msg)
                    | color(state->status_msg.empty() ? Color::GrayDark : Color::Green),
            }),
        });
    });

    auto handler = CatchEvent(renderer, [=, &screen](Event ev) {
        if (ev == Event::Return && !state->loading) {
            // 若焦点在输入框则搜索
            if (input_kw->Focused()) { do_search(); return true; }
            // 否则打开详情
            open_detail(); return true;
        }
        if (ev == Event::ArrowDown || ev == Event::Character('j')) {
            std::lock_guard lock(state->mtx);
            if (state->selected < static_cast<int>(state->results.size()) - 1)
                ++state->selected;
            return true;
        }
        if (ev == Event::ArrowUp || ev == Event::Character('k')) {
            std::lock_guard lock(state->mtx);
            if (state->selected > 0) --state->selected;
            return true;
        }
        if (ev == Event::Character('a')) {
            std::lock_guard lock(state->mtx);
            if (!state->results.empty() &&
                state->selected < static_cast<int>(state->results.size())) {
                const auto& book = state->results[state->selected];
                ctx->db->save_book(book);
                ctx->bookshelf_dirty = true;
                state->status_msg = "已加入书架：" + book.title;
                spdlog::info("search_screen: added book '{}' (id={}) to bookshelf",
                             book.title, book.book_id);
            }
            screen.PostEvent(Event::Custom);
            return true;
        }
        return false;
    });

    return handler;
}

} // namespace fanqie
