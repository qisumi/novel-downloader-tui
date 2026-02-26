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
        int                 selected      = 0;
        int                 scroll_offset = 0;
        bool                loading       = false;
        std::string         status_msg;
        std::mutex          mtx;
    };
    auto state = std::make_shared<SearchState>();

    // ── 组件 ──────────────────────────────────────────────────
    InputOption input_opt;
    input_opt.placeholder = "输入书名或作者搜索…";
    auto input_kw = Input(&state->keyword, input_opt);

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
            state->selected = 0;
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

    // ── 搜索按钮（支持鼠标点击）──────────────────────────────────
    ButtonOption btn_opt = ButtonOption::Ascii();
    auto search_btn = Button(" 搜索 ", do_search, btn_opt);

    // ── 进入详情 ───────────────────────────────────────────────
    auto open_detail = [=, &screen]() {
        Book selected_book;
        {
            std::lock_guard lock(state->mtx);
            if (state->results.empty()) return;
            if (state->selected < 0 ||
                state->selected >= static_cast<int>(state->results.size())) return;
            selected_book = state->results[state->selected];
        }
        ctx->current_book = selected_book;
        // 弹出详情对话框（模态）
        auto detail = make_book_detail_screen(ctx, screen);
        screen.Loop(detail); // 阻塞直到详情页退出
    };

    // ── 加入书架 ───────────────────────────────────────────────
    auto add_to_bookshelf = [=, &screen]() {
        std::lock_guard lock(state->mtx);
        if (!state->results.empty() &&
            state->selected >= 0 &&
            state->selected < static_cast<int>(state->results.size())) {
            const auto& book = state->results[state->selected];
            ctx->db->save_book(book);
            ctx->bookshelf_dirty = true;
            state->status_msg = "已加入书架：" + book.title;
            spdlog::info("search_screen: added book '{}' (id={}) to bookshelf",
                         book.title, book.book_id);
        }
        screen.PostEvent(Event::Custom);
    };

    // 详情按钮
    auto detail_btn = Button(" 详情 ", open_detail, btn_opt);
    // 加入书架按钮
    auto add_btn = Button(" 加入书架 ", add_to_bookshelf, btn_opt);

    // 搜索屏幕可视行数：
    // app 开销(4) + 搜索栏(1)+分隔(1)+分隔(1)+底部栏(1) = 8
    // 每项 1 行
    constexpr int SS_OVERHEAD = 8;
    auto ss_vis_count = [&screen]() {
        return std::max(1, screen.dimy() - SS_OVERHEAD);
    };
    auto ss_ensure_vis = [=, &screen]() {
        int vis = std::max(1, screen.dimy() - SS_OVERHEAD);
        if (state->selected < state->scroll_offset)
            state->scroll_offset = state->selected;
        if (state->selected >= state->scroll_offset + vis)
            state->scroll_offset = state->selected - vis + 1;
        state->scroll_offset = std::max(0, state->scroll_offset);
    };

    auto top_bar = Container::Horizontal({input_kw, search_btn});
    auto bottom_bar = Container::Horizontal({detail_btn, add_btn});
    auto container = Container::Vertical({
        top_bar,
        bottom_bar,
    });

    // ── 渲染 ──────────────────────────────────────────────────
    auto renderer = Renderer(container, [=, &screen]() mutable {
        std::lock_guard lock(state->mtx);

        // ── 计算可视窗口 (锁已持有) ──────────────────────────────────
        int vis = std::max(1, screen.dimy() - SS_OVERHEAD);

        // ensure_vis（不单独调用 ss_ensure_vis 是因为已在锁内）
        if (!state->loading) {
            if (state->selected < state->scroll_offset)
                state->scroll_offset = state->selected;
            if (state->selected >= state->scroll_offset + vis)
                state->scroll_offset = state->selected - vis + 1;
            state->scroll_offset = std::max(0, state->scroll_offset);
        }

        int r_start = state->scroll_offset;
        int r_end   = std::min(r_start + vis, static_cast<int>(state->results.size()));

        // 构建带缓存状态的显示
        Elements result_elements;
        for (int i = r_start; i < r_end; ++i) {
            const auto& b = state->results[i];
            int cached = ctx->db->cached_chapter_count(b.book_id);
            bool is_selected = (i == state->selected);

            auto row = hbox({
                text(is_selected ? " ▶ " : "   "),
                text(b.title) | bold | (is_selected ? color(Color::Cyan) : color(Color::White)),
                text("  (" + b.author + ")  ") | color(Color::GrayLight),
                text(b.creation_status == 1 ? "[完结]" : "[连载]")
                    | color(b.creation_status == 1 ? Color::Green : Color::Yellow),
                filler(),
                text(b.category + "  ") | color(Color::GrayDark),
                text(b.word_count + "字") | color(Color::GrayDark),
                text(cached > 0 ? "  📥" : "") | color(Color::Green),
            });
            if (is_selected) row = row | inverted;
            result_elements.push_back(row);
        }

        // 滚动位置提示
        int total_r = static_cast<int>(state->results.size());
        std::string pos_hint;
        if (!state->results.empty() && total_r > vis) {
            pos_hint = std::to_string(r_start + 1) + "-" + std::to_string(r_end)
                     + "/" + std::to_string(total_r) + " ↑↓滚动";
        } else if (!state->results.empty()) {
            pos_hint = " ↑↓/jk:导航  Enter:详情";
        }

        return vbox({
            // 搜索栏
            hbox({
                text(" 搜索 ") | bold,
                separator(),
                input_kw->Render() | flex,
                search_btn->Render(),
            }),
            separator(),
            // 结果列表
            state->loading
                ? (text("  正在搜索…") | color(Color::Yellow) | flex)
                : (result_elements.empty()
                    ? (text("  输入关键词搜索") | flex)
                    : (vbox(result_elements) | flex)),
            separator(),
            // 底部操作栏
            hbox({
                detail_btn->Render(),
                add_btn->Render(),
                filler(),
                text(state->status_msg.empty() ? pos_hint : "  " + state->status_msg)
                    | color(state->status_msg.empty() ? Color::GrayDark : Color::Green),
            }),
        });
    });

    auto handler = CatchEvent(renderer, [=, &screen](Event ev) {
        // ── 鼠标滚轮 ────────────────────────────────────────
        if (ev.is_mouse() && ev.mouse().button == Mouse::WheelUp) {
            std::lock_guard lock(state->mtx);
            if (state->scroll_offset > 0) --state->scroll_offset;
            return true;
        }
        if (ev.is_mouse() && ev.mouse().button == Mouse::WheelDown) {
            std::lock_guard lock(state->mtx);
            int vis     = ss_vis_count();
            int max_off = std::max(0, static_cast<int>(state->results.size()) - vis);
            if (state->scroll_offset < max_off) ++state->scroll_offset;
            return true;
        }

        // ── 鼠标左键点击选择 ─────────────────────────────────
        if (ev.is_mouse() && ev.mouse().button == Mouse::Left &&
            ev.mouse().motion == Mouse::Pressed) {
            std::lock_guard lock(state->mtx);
            // app 标题栏(1)+分隔(1) + 搜索栏(1)+分隔(1) = 4 行
            constexpr int HEADER_LINES = 4;
            int mouse_y       = static_cast<int>(ev.mouse().y);
            int clicked_index = state->scroll_offset + (mouse_y - HEADER_LINES);

            if (clicked_index >= 0 && clicked_index < static_cast<int>(state->results.size())) {
                state->selected = clicked_index;
                return true;
            }
        }

        // ── 回车键 ───────────────────────────────────────────────
        if (ev == Event::Return && !state->loading) {
            if (input_kw->Focused()) { do_search(); return true; }
            open_detail(); return true;
        }

        // ── 键盘导航 ─────────────────────────────────────────────
        if (ev == Event::ArrowDown || ev == Event::Character('j')) {
            std::lock_guard lock(state->mtx);
            if (state->selected < static_cast<int>(state->results.size()) - 1)
                ++state->selected;
            ss_ensure_vis();
            return true;
        }
        if (ev == Event::ArrowUp || ev == Event::Character('k')) {
            std::lock_guard lock(state->mtx);
            if (state->selected > 0) --state->selected;
            ss_ensure_vis();
            return true;
        }

        // 快捷键：加入书架
        if (ev == Event::Character('a')) {
            add_to_bookshelf();
            return true;
        }

        return false;
    });

    return handler;
}

} // namespace fanqie
