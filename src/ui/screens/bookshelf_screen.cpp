#include "ui/screens/bookshelf_screen.h"
#include "ui/screens/book_detail_screen.h"

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

using namespace ftxui;

namespace fanqie {

ftxui::Component make_bookshelf_screen(
    std::shared_ptr<AppContext> ctx,
    ScreenInteractive&           screen)
{
    struct BookshelfState {
        std::vector<Book> books;
        int               selected = 0;
        std::string       status_msg;
    };
    auto state = std::make_shared<BookshelfState>();
    // 让页面始终可聚焦，保证键盘事件可达（不参与渲染）
    std::string focus_anchor_text;
    auto focus_anchor = Input(&focus_anchor_text, "");

    // 刷新书架列表
    auto refresh = [=]() {
        state->books    = ctx->db->list_bookshelf();
        state->selected = 0;
        state->status_msg.clear();
    };
    refresh();

    auto container = Container::Vertical({focus_anchor});

    auto renderer = Renderer(container, [=]() mutable {
        // 搜索页加书后置脏标志，书架页渲染时自动刷新
        if (ctx->bookshelf_dirty.exchange(false)) {
            state->books    = ctx->db->list_bookshelf();
            state->selected = std::min(state->selected,
                                       std::max(0, static_cast<int>(state->books.size()) - 1));
            state->status_msg.clear();
        }

        Elements rows;
        for (int i = 0; i < static_cast<int>(state->books.size()); ++i) {
            const auto& b = state->books[i];
            int cached = ctx->db->cached_chapter_count(b.book_id);

            std::string status_flag = b.creation_status == 1 ? "完结" : "连载";

            auto row = hbox({
                text(i == state->selected ? " ▶ " : "   "),
                vbox({
                    hbox({
                        text(b.title) | bold
                            | (i == state->selected ? color(Color::Cyan) : color(Color::White)),
                        text("  " + b.author) | color(Color::GrayLight),
                        filler(),
                        text("[" + status_flag + "]")
                            | color(b.creation_status == 1 ? Color::Green : Color::Yellow),
                    }),
                    hbox({
                        text(b.category + "  "),
                        text(b.word_count + "字  ") | color(Color::GrayDark),
                        text("缓存:" + std::to_string(cached) + "章") | color(Color::GrayDark),
                    }),
                }) | flex,
            }) | (i == state->selected ? inverted : nothing);

            rows.push_back(row);
            rows.push_back(separator());
        }

        Element list_area = state->books.empty()
            ? text("  书架空空如也，请在搜索页添加书籍 (a)")
            : vbox(rows);

        return vbox({
            hbox({
                text(" 书架 ") | bold,
                filler(),
                text(" r") | bold, text(":刷新  "),
                text(" d") | bold, text(":删除  "),
                text(" Enter") | bold, text(":详情"),
            }) | color(Color::GrayDark),
            separator(),
            list_area | frame | flex,
            separator(),
            text(state->status_msg.empty() ? "" : "  " + state->status_msg)
                | color(Color::Green),
        });
    });

    auto handler = CatchEvent(renderer, [=, &screen](Event ev) {
        if (ev == Event::ArrowDown || ev == Event::Character('j')) {
            if (state->selected < static_cast<int>(state->books.size()) - 1)
                ++state->selected;
            return true;
        }
        if (ev == Event::ArrowUp || ev == Event::Character('k')) {
            if (state->selected > 0) --state->selected;
            return true;
        }
        if (ev == Event::Character('r')) {
            refresh();
            state->status_msg = "书架已刷新";
            return true;
        }
        if (ev == Event::Character('d')) {
            if (!state->books.empty()) {
                state->selected = std::min(state->selected,
                                           std::max(0, static_cast<int>(state->books.size()) - 1));
                std::string bid = state->books[state->selected].book_id;
                ctx->db->remove_book(bid);
                state->status_msg = "已从书架删除";
                refresh();
            }
            return true;
        }
        if (ev == Event::Return) {
            if (!state->books.empty()) {
                state->selected = std::min(state->selected,
                                           std::max(0, static_cast<int>(state->books.size()) - 1));
                ctx->current_book = state->books[state->selected];
                auto detail = make_book_detail_screen(ctx, screen);
                screen.Loop(detail);
                refresh(); // 退出详情后刷新
            }
            return true;
        }
        return false;
    });

    focus_anchor->TakeFocus();
    return handler;
}

} // namespace fanqie
