#include "ui/screens/bookshelf_screen.h"
#include "ui/screens/book_detail_screen.h"

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <chrono>

using namespace ftxui;

namespace fanqie {

ftxui::Component make_bookshelf_screen(
    std::shared_ptr<AppContext> ctx,
    ScreenInteractive&           screen)
{
    struct BookshelfState {
        std::vector<Book> books;
        int               selected     = 0;
        int               scroll_offset = 0;
        std::string       status_msg;
        // 双击检测
        std::chrono::steady_clock::time_point last_click_time;
        int last_clicked_index = -1;
    };
    auto state = std::make_shared<BookshelfState>();

    // 刷新书架列表
    auto refresh = [=]() {
        state->books = ctx->db->list_bookshelf();
        state->selected = std::min(state->selected,
                                   std::max(0, static_cast<int>(state->books.size()) - 1));
        state->status_msg.clear();
    };
    refresh();

    // ── 进入详情 ───────────────────────────────────────────────
    auto open_detail = [=, &screen]() {
        if (state->books.empty()) return;
        state->selected = std::min(state->selected,
                                   std::max(0, static_cast<int>(state->books.size()) - 1));
        ctx->current_book = state->books[state->selected];
        auto detail = make_book_detail_screen(ctx, screen);
        screen.Loop(detail);
        refresh(); // 退出详情后刷新
    };

    // ── 删除书籍 ───────────────────────────────────────────────
    auto delete_book = [=]() {
        if (!state->books.empty()) {
            state->selected = std::min(state->selected,
                                       std::max(0, static_cast<int>(state->books.size()) - 1));
            std::string bid = state->books[state->selected].book_id;
            ctx->db->remove_book(bid);
            state->status_msg = "已从书架删除";
            refresh();
        }
    };

    // ── 可视窗口辅助（每项 3 行：2 内容 + 1 分隔）──────────────────
    // app 开销(4) + 书架开销(4) = 8
    constexpr int BS_ITEM_H   = 3;
    constexpr int BS_OVERHEAD = 8;
    auto bs_vis_count = [&screen]() {
        return std::max(1, (screen.dimy() - BS_OVERHEAD) / BS_ITEM_H);
    };
    auto bs_ensure_vis = [=, &screen]() {
        int vis = std::max(1, (screen.dimy() - BS_OVERHEAD) / BS_ITEM_H);
        if (state->selected < state->scroll_offset)
            state->scroll_offset = state->selected;
        if (state->selected >= state->scroll_offset + vis)
            state->scroll_offset = state->selected - vis + 1;
        state->scroll_offset = std::max(0, state->scroll_offset);
    };

    // ── 按钮组件（支持鼠标点击）──────────────────────────────────
    ButtonOption btn_opt = ButtonOption::Ascii();
    auto refresh_btn = Button(" 刷新 ", refresh, btn_opt);
    auto delete_btn = Button(" 删除 ", delete_book, btn_opt);
    auto detail_btn = Button(" 详情 ", open_detail, btn_opt);

    auto button_bar = Container::Horizontal({
        refresh_btn,
        delete_btn,
        detail_btn,
    });

    auto container = Container::Vertical({
        button_bar,
    });

    auto renderer = Renderer(container, [=, &screen]() mutable {
        // 搜索页加书后置脏标志，书架页渲染时自动刷新
        if (ctx->bookshelf_dirty.exchange(false)) {
            refresh();
        }
        // 切换到书架页时自动刷新
        if (ctx->bookshelf_needs_refresh.exchange(false)) {
            refresh();
        }

        // ── 计算可视窗口 ───────────────────────────────────────
        int vis = bs_vis_count();
        bs_ensure_vis();
        int list_start = state->scroll_offset;
        int list_end   = std::min(list_start + vis, static_cast<int>(state->books.size()));

        // ── 构建可视区域内的列表行 ────────────────────────────
        Elements rows;
        for (int i = list_start; i < list_end; ++i) {
            const auto& b = state->books[i];
            int cached = ctx->db->cached_chapter_count(b.book_id);
            int toc_total = ctx->db->toc_count(b.book_id);
            std::string status_flag = b.creation_status == 1 ? "完结" : "连载";
            std::string toc_text = toc_total > 0
                ? ("章节:" + std::to_string(toc_total))
                : "章节:?";
            bool is_selected = (i == state->selected);

            auto row = hbox({
                text(is_selected ? " ▶ " : "   "),
                vbox({
                    hbox({
                        text(b.title) | bold
                            | (is_selected ? color(Color::Cyan) : color(Color::White)),
                        text("  " + b.author) | color(Color::GrayLight),
                        filler(),
                        text("[" + status_flag + "]")
                            | color(b.creation_status == 1 ? Color::Green : Color::Yellow),
                    }),
                    hbox({
                        text(b.category + "  "),
                        text(b.word_count + "字  ") | color(Color::GrayDark),
                        text(toc_text + "  ") | color(Color::GrayDark),
                        text("缓存:" + std::to_string(cached) + "章") | color(Color::GrayDark),
                    }),
                }) | flex,
            }) | (is_selected ? inverted : nothing);

            rows.push_back(row);
            if (i < list_end - 1) rows.push_back(separator());
        }

        // 滚动位置指示文字
        std::string pos_hint;
        int total = static_cast<int>(state->books.size());
        if (total > vis) {
            pos_hint = std::to_string(list_start + 1) + "-"
                     + std::to_string(list_end) + "/"
                     + std::to_string(total);
        } else {
            pos_hint = std::to_string(total) + " 本书";
        }

        Element list_area = state->books.empty()
            ? (text("  书架空空如也，请在搜索页添加书籍 (a)") | flex)
            : (vbox(rows) | flex);

        return vbox({
            // 标题栏
            hbox({
                text(" 书架 ") | bold,
                filler(),
                text(pos_hint) | color(Color::GrayDark),
            }) | color(Color::GrayDark),
            separator(),
            // 书籍列表（支持鼠标点击选择）
            list_area | flex,
            separator(),
            // 底部操作栏
            hbox({
                refresh_btn->Render(),
                delete_btn->Render(),
                detail_btn->Render(),
                filler(),
                text(state->status_msg.empty() ? "" : "  " + state->status_msg)
                    | color(Color::Green),
            }),
        });
    });

    // 每个书籍项占用的行数（内容 2 行 + separator 1 行 = 3 行）

    auto handler = CatchEvent(renderer, [=, &screen](Event ev) {
        // ── 鼠标滚轮 ────────────────────────────────────────────
        if (ev.is_mouse() && ev.mouse().button == Mouse::WheelUp) {
            if (state->selected > 0) --state->selected;
            bs_ensure_vis();
            return true;
        }
        if (ev.is_mouse() && ev.mouse().button == Mouse::WheelDown) {
            if (state->selected < static_cast<int>(state->books.size()) - 1)
                ++state->selected;
            bs_ensure_vis();
            return true;
        }

        // ── 鼠标左键点击（Pressed 事件）─────────────────────────
        if (ev.is_mouse() && ev.mouse().button == Mouse::Left &&
            ev.mouse().motion == Mouse::Pressed) {
            int mouse_y = static_cast<int>(ev.mouse().y);

            // app 标题栏(1)+分隔(1) + 书架标题栏(1)+分隔(1) = 4 行
            constexpr int HEADER_LINES = 4;

            if (mouse_y >= HEADER_LINES && !state->books.empty()) {
                int clicked_index = state->scroll_offset
                                  + (mouse_y - HEADER_LINES) / BS_ITEM_H;

                // 超出列表范围 → 放行给按钮
                if (clicked_index >= static_cast<int>(state->books.size())) {
                    return false;
                }
                clicked_index = std::max(0, clicked_index);

                auto now        = std::chrono::steady_clock::now();
                auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - state->last_click_time).count();

                // 双击：同一项、50~400ms 内再次按下
                if (clicked_index == state->last_clicked_index &&
                    elapsed_ms >= 50 && elapsed_ms < 400) {
                    state->selected           = clicked_index;
                    state->last_clicked_index = -1;
                    open_detail();
                    return true;
                }

                // 单击选择
                state->selected           = clicked_index;
                state->last_click_time    = now;
                state->last_clicked_index = clicked_index;
                return true;
            }
        }

        // ── 键盘导航 ────────────────────────────────────────────
        if (ev == Event::ArrowDown || ev == Event::Character('j')) {
            if (state->selected < static_cast<int>(state->books.size()) - 1)
                ++state->selected;
            bs_ensure_vis();
            return true;
        }
        if (ev == Event::ArrowUp || ev == Event::Character('k')) {
            if (state->selected > 0) --state->selected;
            bs_ensure_vis();
            return true;
        }

        // 快捷键
        if (ev == Event::Character('r')) {
            refresh();
            state->status_msg = "书架已刷新";
            return true;
        }
        if (ev == Event::Character('d')) {
            delete_book();
            return true;
        }
        if (ev == Event::Return) {
            open_detail();
            return true;
        }

        return false;
    });

    return handler;
}

} // namespace fanqie
