#include "tui/app.h"
#include "tui/screens/search_screen.h"
#include "tui/screens/bookshelf_screen.h"
#include "tui/screens/book_detail_screen.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

using namespace ftxui;

namespace novel {

// ──────────────────────────────────────────────────────────────────────────────
// 顶部标签栏（书架 / 搜索）
// ──────────────────────────────────────────────────────────────────────────────

int run_app(std::shared_ptr<AppContext> ctx) {
    auto screen = ScreenInteractive::Fullscreen();

    // 两个主标签页
    int selected_tab = 0;
    std::vector<std::string> tab_labels = {"  书架  ", "  搜索  "};

    // 创建各屏幕组件
    auto bookshelf = make_bookshelf_screen(ctx, screen);
    auto search    = make_search_screen(ctx, screen);

    // tab_toggle 仅用于视觉渲染，不参与键盘焦点链，
    // 这样事件可以直接路由到活动屏幕（书架/搜索）的事件处理器。
    auto tab_toggle = Toggle(&tab_labels, &selected_tab);
    auto go_back = [&]() {
        if (selected_tab > 0) selected_tab -= 1;
    };
    auto close_app = [&]() {
        ctx->app_exit_requested = true;
        screen.ExitLoopClosure()();
    };
    ButtonOption nav_btn_opt = ButtonOption::Ascii();
    auto back_btn = Button(" 返回上一级 ", go_back, nav_btn_opt);
    auto close_btn = Button(" 关闭 ", close_app, nav_btn_opt);
    auto header_controls = Container::Horizontal({
        back_btn,
        close_btn,
        tab_toggle,
    });

    auto main_container = Container::Tab(
        {bookshelf, search},
        &selected_tab);
    auto root_container = Container::Vertical({
        header_controls,
        main_container,
    });
    main_container->TakeFocus();

    // root 只包含内容区，使其直接获得焦点。
    auto root_renderer = Renderer(root_container, [&] {
        return vbox({
            // ── 顶部标题栏 ────────────────────────────────────
            hbox({
                text(" 🍅 小说下载 TUI ") | bold | color(Color::Red),
                text("[" + ctx->current_source_name + "]") | color(Color::Yellow),
                text("  "),
                back_btn->Render(),
                close_btn->Render(),
                filler(),
                tab_toggle->Render(),
            }) | bgcolor(Color::Black) | color(Color::White),
            separator(),
            // ── 主内容 ────────────────────────────────────────
            main_container->Render() | flex,
            // ── 底部状态栏 ────────────────────────────────────
            separator(),
            hbox({
                text(" q") | bold,
                text(":退出  "),
                text(" Tab") | bold,
                text(":切换标签  "),
                text(" Enter") | bold,
                text(":确认  "),
                text(" ↑↓/jk") | bold,
                text(":导航  "),
                text(" 鼠标") | bold,
                text(":点击按钮/选择"),
            }) | color(Color::GrayDark),
        });
    });

    // 全局快捷键：在内容区事件之前捕获
    auto event_handler = CatchEvent(root_renderer, [&](Event ev) {
        if (ctx->app_exit_requested.exchange(false)) {
            screen.ExitLoopClosure()();
            return true;
        }
        if (ev == Event::Character('q') || ev == Event::Character('Q')) {
            ctx->app_exit_requested = true;
            screen.ExitLoopClosure()();
            return true;
        }
        // Tab 键切换书架/搜索标签页
        if (ev == Event::Tab) {
            int old_tab = selected_tab;
            selected_tab = (selected_tab + 1) % static_cast<int>(tab_labels.size());
            // 切换到书架页时，设置刷新标志
            if (old_tab != 0 && selected_tab == 0) {
                ctx->bookshelf_needs_refresh = true;
            }
            return true;
        }
        if (ev == Event::TabReverse) {
            int old_tab = selected_tab;
            selected_tab = (selected_tab + static_cast<int>(tab_labels.size()) - 1)
                           % static_cast<int>(tab_labels.size());
            // 切换到书架页时，设置刷新标志
            if (old_tab != 0 && selected_tab == 0) {
                ctx->bookshelf_needs_refresh = true;
            }
            return true;
        }
        // 书架页按键兜底转发，避免焦点链异常导致按键失效
        if (selected_tab == 0) {
            if (ev == Event::ArrowDown || ev == Event::ArrowUp ||
                ev == Event::Character('j') || ev == Event::Character('k') ||
                ev == Event::Character('d') || ev == Event::Character('r') ||
                ev == Event::Return) {
                return bookshelf->OnEvent(ev);
            }
        }
        return false;
    });

    screen.Loop(event_handler);
    return 0;
}

} // namespace novel
