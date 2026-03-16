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
#include <spdlog/spdlog.h>

using namespace ftxui;

namespace fanqie {

ftxui::Component make_book_detail_screen(
    std::shared_ptr<AppContext> ctx,
    ScreenInteractive&           screen)
{
    spdlog::info("book_detail_screen: opened for '{}' (id={})",
                 ctx->current_book.title, ctx->current_book.book_id);
    struct BookDetailState {
        Book               book;
        std::vector<TocItem> toc;
        int                selected      = 0;
        int                scroll_offset = 0;
        float              abstract_scroll = 0.0f;
        bool               loading       = false;
        bool               show_abstract_modal = false;
        int                dl_current    = 0;
        int                dl_total      = 0;
        int                range_start   = -1;
        int                range_end     = -1;
        std::string        status_msg;
        std::string        range_start_input;
        std::string        range_end_input;
        std::mutex         mtx;
    };
    auto state = std::make_shared<BookDetailState>();
    state->book = ctx->current_book;

    auto apply_default_full_range = [=](int total) {
        if (total <= 0) return;
        state->range_start = 0;
        state->range_end = total - 1;
        state->range_start_input = "1";
        state->range_end_input = std::to_string(total);
    };
    auto open_abstract_modal = [=]() {
        std::lock_guard lock(state->mtx);
        state->show_abstract_modal = true;
        state->abstract_scroll = 0.0f;
    };
    auto close_abstract_modal = [=]() {
        std::lock_guard lock(state->mtx);
        state->show_abstract_modal = false;
    };
    auto scroll_abstract = [=](float delta) {
        std::lock_guard lock(state->mtx);
        state->abstract_scroll = std::clamp(state->abstract_scroll + delta, 0.0f, 1.0f);
    };

    // ── 异步加载目录 ───────────────────────────────────────────
    auto load_toc = [=, &screen](bool force_remote) {
        spdlog::info("book_detail_screen: load_toc() called, force_remote={}", force_remote);
        {
            std::lock_guard lock(state->mtx);
            state->loading = true;
            state->status_msg = force_remote ? "更新目录中…" : "加载目录中…";
        }
        screen.PostEvent(Event::Custom);

        std::thread([=, &screen]() {
            try {
                std::vector<TocItem> toc;
                if (!force_remote) {
                    // 优先读本地缓存
                    spdlog::debug("book_detail_screen: trying to load toc from cache");
                    toc = ctx->library_service->load_toc(state->book, false);
                    spdlog::debug("book_detail_screen: loaded {} items from cache", toc.size());
                }
                if (toc.empty() || force_remote) {
                    spdlog::debug("book_detail_screen: loading toc via library service");
                    toc = ctx->library_service->load_toc(state->book, true);
                }
            std::lock_guard lock(state->mtx);
            int old_total = static_cast<int>(state->toc.size());
            int old_start = state->range_start;
            int old_end = state->range_end;
            bool has_custom_range =
                !state->range_start_input.empty() || !state->range_end_input.empty();

            state->toc     = std::move(toc);
            int new_total = static_cast<int>(state->toc.size());
            state->selected = std::clamp(state->selected, 0, std::max(0, new_total - 1));
            state->scroll_offset = std::clamp(state->scroll_offset, 0, std::max(0, new_total - 1));

            if (new_total > 0) {
                if (!has_custom_range || old_total <= 0) {
                    apply_default_full_range(new_total);
                } else {
                    state->range_start = std::clamp(old_start, 0, new_total - 1);
                    state->range_end = std::clamp(old_end, 0, new_total - 1);
                    if (state->range_start > state->range_end) {
                        std::swap(state->range_start, state->range_end);
                    }
                    state->range_start_input = std::to_string(state->range_start + 1);
                    state->range_end_input = std::to_string(state->range_end + 1);
                }
            } else {
                state->range_start = -1;
                state->range_end = -1;
                state->range_start_input.clear();
                state->range_end_input.clear();
            }

            state->loading = false;
            state->status_msg = state->toc.empty()
                ? (force_remote ? "目录更新失败" : "目录加载失败")
                : (force_remote ? "目录已更新" : "");
            screen.PostEvent(Event::Custom);
            } catch (const std::exception& e) {
                spdlog::error("load_toc() exception: {}", e.what());
                std::lock_guard lock(state->mtx);
                state->loading = false;
                state->status_msg = "目录加载失败：" + std::string(e.what());
                screen.PostEvent(Event::Custom);
            }
        }).detach();
    };
    load_toc(false);

    // ── 批量下载 ───────────────────────────────────────────────
    auto download_all = [=, &screen]() {
        std::lock_guard lock(state->mtx);
        if (state->toc.empty() || state->loading) return;
        state->loading    = true;
        state->dl_total   = static_cast<int>(state->toc.size());
        state->dl_current = 0;
        state->status_msg = "开始下载…";

        auto toc_copy = state->toc;
        auto book_copy = state->book;
        std::thread([=, &screen]() {
            try {
                ctx->download_service->download_book(
                    book_copy,
                    toc_copy,
                    [=, &screen](int current, int total) {
                        std::lock_guard lock(state->mtx);
                        state->dl_current = current;
                        state->dl_total = total;
                        screen.PostEvent(Event::Custom);
                    });
                std::lock_guard lock(state->mtx);
                state->loading = false;
                state->status_msg = "下载完成！";
                screen.PostEvent(Event::Custom);
            } catch (const std::exception& e) {
                std::lock_guard lock(state->mtx);
                state->loading = false;
                state->status_msg = "下载失败：" + std::string(e.what());
                screen.PostEvent(Event::Custom);
            }
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
            try {
                std::string path = ctx->export_service->export_book(
                    book_copy,
                    toc_copy,
                    start,
                    end,
                    as_epub,
                    ctx->epub_output_dir,
                    [=, &screen](int cur, int tot) {
                        std::lock_guard lock(state->mtx);
                        state->dl_current = cur;
                        state->dl_total = tot;
                        state->status_msg = "准备章节中…";
                        screen.PostEvent(Event::Custom);
                    },
                    [=, &screen](int cur, int tot) {
                        std::lock_guard lock(state->mtx);
                        state->dl_current = cur;
                        state->dl_total = tot;
                        state->status_msg = as_epub ? "正在打包 EPUB…" : "正在写入 TXT…";
                        screen.PostEvent(Event::Custom);
                    });

                std::lock_guard lock(state->mtx);
                state->loading = false;
                state->status_msg = path.empty()
                    ? (as_epub ? "EPUB 导出失败" : "TXT 导出失败")
                    : ("已导出：" + path);
                screen.PostEvent(Event::Custom);
            } catch (const std::exception& e) {
                std::lock_guard lock(state->mtx);
                state->loading = false;
                state->status_msg = "导出失败：" + std::string(e.what());
                screen.PostEvent(Event::Custom);
            }
        }).detach();
    };

    auto export_epub = [=, &screen]() { export_by_format(true); };
    auto export_txt  = [=, &screen]() { export_by_format(false); };
    auto refresh_toc = [=, &screen]() {
        {
            std::lock_guard lock(state->mtx);
            if (state->loading) return;
        }
        load_toc(true);
    };

    // ── 清除范围 ───────────────────────────────────────────────
    auto clear_range = [=]() {
        std::lock_guard lock(state->mtx);
        int total = static_cast<int>(state->toc.size());
        if (total > 0) {
            apply_default_full_range(total);
        } else {
            state->range_start = -1;
            state->range_end = -1;
            state->range_start_input.clear();
            state->range_end_input.clear();
        }
        state->status_msg = "已恢复默认导出范围（1~最后一章）";
    };

    // ── 校验并应用范围输入 ───────────────────────────────────────
    auto validate_and_apply_range = [=]() {
        std::lock_guard lock(state->mtx);
        int total = static_cast<int>(state->toc.size());
        if (total == 0) {
            state->status_msg = "目录为空";
            return;
        }

        int start = -1;
        int end = -1;

        // 解析起点
        if (!state->range_start_input.empty()) {
            try {
                int val = std::stoi(state->range_start_input);
                if (val < 1) {
                    state->status_msg = "起点必须 ≥ 1";
                    return;
                }
                if (val > total) {
                    state->status_msg = "起点超出范围 (最大 " + std::to_string(total) + ")";
                    return;
                }
                start = val - 1;  // 转为 0-based
            } catch (...) {
                state->status_msg = "起点格式无效，请输入数字";
                return;
            }
        }

        // 解析终点
        if (!state->range_end_input.empty()) {
            try {
                int val = std::stoi(state->range_end_input);
                if (val < 1) {
                    state->status_msg = "终点必须 ≥ 1";
                    return;
                }
                if (val > total) {
                    state->status_msg = "终点超出范围 (最大 " + std::to_string(total) + ")";
                    return;
                }
                end = val - 1;  // 转为 0-based
            } catch (...) {
                state->status_msg = "终点格式无效，请输入数字";
                return;
            }
        }

        // 自动排序（如果都设置了）
        if (start >= 0 && end >= 0 && start > end) {
            std::swap(start, end);
            // 同步更新输入框
            state->range_start_input = std::to_string(start + 1);
            state->range_end_input = std::to_string(end + 1);
        }

        // 生成状态消息
        if (start < 0 && end < 0) {
            apply_default_full_range(total);
            state->status_msg = "已恢复默认导出范围（1~最后一章）";
        } else if (start < 0) {
            state->range_start = start;
            state->range_end = end;
            state->status_msg = "已设置范围：第 1 - " + std::to_string(end + 1) + " 章";
        } else if (end < 0) {
            state->range_start = start;
            state->range_end = end;
            state->status_msg = "已设置范围：第 " + std::to_string(start + 1) + " - " + std::to_string(total) + " 章";
        } else {
            state->range_start = start;
            state->range_end = end;
            state->status_msg = "已设置范围：第 " + std::to_string(start + 1) + " - " + std::to_string(end + 1) + " 章";
        }
    };

    // ── 组件创建 ───────────────────────────────────────────────
    auto toc_box = std::make_shared<Box>();
    auto abstract_box = std::make_shared<Box>();
    auto abstract_modal_box = std::make_shared<Box>();
    auto mouse_inside = [](const Mouse& mouse, const Box& box) {
        return mouse.x >= box.x_min && mouse.x <= box.x_max &&
               mouse.y >= box.y_min && mouse.y <= box.y_max;
    };
    // 优先使用目录区域真实高度，避免固定 overhead 导致滚动触发偏移。
    auto bd_vis_count = [=, &screen]() {
        int h = toc_box->y_max - toc_box->y_min + 1;
        if (h > 0) return h;
        // 首帧 reflect 尚未生效时的保底值
        constexpr int BD_FALLBACK_OVERHEAD = 17;
        return std::max(1, screen.dimy() - BD_FALLBACK_OVERHEAD);
    };
    auto bd_ensure_vis = [=, &screen]() {
        int vis = bd_vis_count();
        if (state->selected < state->scroll_offset)
            state->scroll_offset = state->selected;
        if (state->selected >= state->scroll_offset + vis)
            state->scroll_offset = state->selected - vis + 1;
        state->scroll_offset = std::max(0, state->scroll_offset);
    };

    // 范围输入框
    InputOption input_opt;
    input_opt.placeholder = "全部";
    auto input_start = Input(&state->range_start_input, input_opt);
    auto input_end = Input(&state->range_end_input, input_opt);

    // 按钮组件（支持鼠标点击）
    ButtonOption btn_opt = ButtonOption::Ascii();
    auto back_to_prev = [=, &screen]() {
        screen.ExitLoopClosure()();
    };
    auto close_app = [=, &screen]() {
        ctx->app_exit_requested = true;
        screen.PostEvent(Event::Custom);
        screen.ExitLoopClosure()();
    };
    auto btn_back = Button(" 返回上一级 ", back_to_prev, btn_opt);
    auto btn_close = Button(" 关闭 ", close_app, btn_opt);
    auto btn_download = Button(" 下载全部 ", download_all, btn_opt);
    auto btn_refresh = Button(" 更新章节 ", refresh_toc, btn_opt);
    auto btn_epub = Button(" 导出EPUB ", export_epub, btn_opt);
    auto btn_txt = Button(" 导出TXT ", export_txt, btn_opt);
    auto btn_clear = Button(" 清除范围 ", clear_range, btn_opt);

    auto nav_bar = Container::Horizontal({
        btn_back,
        btn_close,
    });

    // 按钮容器
    auto button_bar = Container::Horizontal({
        btn_download,
        btn_refresh,
        btn_epub,
        btn_txt,
        btn_clear,
    });

    // 范围输入容器
    auto range_input_bar = Container::Horizontal({
        input_start,
        input_end,
    });

    // 目录列表容器（用于键盘导航）
    auto toc_container = Container::Vertical({});

    // 主容器
    auto container = Container::Vertical({
        nav_bar,
        button_bar,
        range_input_bar,
        toc_container,
    });

    // ── 渲染 ──────────────────────────────────────────────────
    auto renderer = Renderer(container, [=, &screen]() mutable {
        std::lock_guard lock(state->mtx);
        auto [range_s, range_e] = resolve_export_range(
            static_cast<int>(state->toc.size()), state->range_start, state->range_end);
        int total_chapters = static_cast<int>(state->toc.size());
        int selected_count = (range_e - range_s + 1);

        // 目录列表（手动行窗口）
        Elements toc_rows;
        {
            int vis = bd_vis_count();
            // ensure_vis（已在锁内）
            if (state->selected < state->scroll_offset)
                state->scroll_offset = state->selected;
            if (state->selected >= state->scroll_offset + vis)
                state->scroll_offset = state->selected - vis + 1;
            state->scroll_offset = std::max(0, state->scroll_offset);

            int t_start = state->scroll_offset;
            int t_end   = std::min(t_start + vis, static_cast<int>(state->toc.size()));

            for (int i = t_start; i < t_end; ++i) {
                const auto& t = state->toc[i];
                bool cached  = ctx->library_service->chapter_cached(t.item_id);
                bool in_range = (i >= range_s && i <= range_e);

                auto row = hbox({
                    text(i == state->selected ? " ▶ " : "   "),
                    text(t.title)
                        | (i == state->selected ? bold : nothing)
                        | (cached ? color(Color::Green) : color(Color::White)),
                    filler(),
                    text(in_range ? "◆" : " ") | color(Color::Cyan),
                    text(cached ? "✓" : " ") | color(Color::Green),
                    text("  " + std::to_string(t.word_count) + "字") | color(Color::GrayDark),
                });
                if (i == state->selected) row = row | inverted;
                toc_rows.push_back(row);
            }
        } // end toc window block

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

        // 范围信息文本
        std::string range_info;
        if (total_chapters == 0) {
            range_info = "无章节";
        } else if (range_s == 0 && range_e == total_chapters - 1) {
            range_info = "全部 " + std::to_string(total_chapters) + " 章";
        } else {
            range_info = "第 " + std::to_string(range_s + 1) + " - "
                       + std::to_string(range_e + 1) + " 章 (共 "
                       + std::to_string(selected_count) + " 章)";
        }

        Element abstract_preview = vbox({
            paragraph(state->book.abstract)
                | color(Color::GrayLight)
                | yframe
                | flex,
            hbox({
                filler(),
                text("按 i 查看完整简介") | color(Color::GrayDark),
            }),
        }) | reflect(*abstract_box) | size(HEIGHT, EQUAL, 6) | border;

        Element main_view = vbox({
            hbox({
                text(" 书籍详情 ") | bold | color(Color::Cyan),
                text("  "),
                btn_back->Render(),
                btn_close->Render(),
                filler(),
                text("ESC") | bold,
                text(":返回"),
            }) | color(Color::GrayDark),
            separator(),
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
            // 简介预览（固定高度，完整内容通过弹层查看）
            abstract_preview,
            separator(),
            // 操作按钮栏
            hbox({
                text(" 目录 (") | bold,
                text(std::to_string(state->toc.size()) + " 章) ") | bold,
                btn_download->Render(),
                btn_refresh->Render(),
                btn_epub->Render(),
                btn_txt->Render(),
                btn_clear->Render(),
                filler(),
                ([&]() -> Element {
                    int total = static_cast<int>(state->toc.size());
                    int vis   = bd_vis_count();
                    if (total > vis) {
                        int t_end = std::min(state->scroll_offset + vis, total);
                        return text(std::to_string(state->scroll_offset + 1) + "-"
                                  + std::to_string(t_end) + "/"
                                  + std::to_string(total)) | color(Color::GrayDark);
                    }
                    return emptyElement();
                })()
            }),
            // 范围输入栏
            hbox({
                text("  导出范围：第 "),
                input_start->Render() | size(WIDTH, EQUAL, 6),
                text(" 章 到 第 "),
                input_end->Render() | size(WIDTH, EQUAL, 6),
                text(" 章  "),
                text("(当前: " + range_info + ")") | color(Color::GrayDark),
            }),
            separator(),
            // 快捷键提示
            hbox({
                text("  ") | color(Color::GrayDark),
                text("↑↓/jk") | bold, text(":导航  "),
                text("Enter") | bold, text(":应用范围  "),
                text("u") | bold, text(":更新章节  "),
                text("g") | bold, text(":下载  "),
                text("e") | bold, text(":EPUB  "),
                text("t") | bold, text(":TXT  "),
                text("c") | bold, text(":清除  "),
                text("i") | bold, text(":简介  "),
                text("a") | bold, text(":加书架  "),
                text("q") | bold, text(":关闭应用  "),
                text("ESC") | bold, text(":返回"),
            }) | color(Color::GrayDark),
            separator(),
            // 目录列表
            state->loading && state->toc.empty()
              ? ((text("  加载中…") | color(Color::Yellow) | flex) | reflect(*toc_box))
              : ((vbox(toc_rows) | flex) | reflect(*toc_box)),
            progress_bar,
            separator(),
            text("  " + state->status_msg) | color(Color::Green),
        });

        if (!state->show_abstract_modal) return main_view;

        Element abstract_modal = window(
            text(" 完整简介 "),
            vbox({
                paragraph(state->book.abstract.empty() ? "暂无简介" : state->book.abstract)
                    | color(Color::GrayLight)
                    | focusPositionRelative(0.0f, state->abstract_scroll)
                    | yframe
                    | vscroll_indicator
                    | flex,
                separator(),
                hbox({
                    text("↑↓/jk/滚轮") | bold,
                    text(":滚动  "),
                    text("i") | bold,
                    text("/"),
                    text("ESC") | bold,
                    text(":关闭"),
                }) | color(Color::GrayDark),
            }) | reflect(*abstract_modal_box) | size(WIDTH, EQUAL, std::max(40, screen.dimx() - 10))
              | size(HEIGHT, EQUAL, std::max(8, screen.dimy() - 6)),
            DOUBLE);

        return dbox({
            main_view | dim,
            abstract_modal | center,
        });
    });

    auto handler = CatchEvent(renderer, [=, &screen](Event ev) {
        bool abstract_modal_open = false;
        {
            std::lock_guard lock(state->mtx);
            abstract_modal_open = state->show_abstract_modal;
        }

        if (abstract_modal_open) {
            if (ev == Event::Escape || ev == Event::Character('i') || ev == Event::Character('I')) {
                close_abstract_modal();
                return true;
            }
            if (ev == Event::ArrowUp || ev == Event::Character('k')) {
                scroll_abstract(-0.08f);
                return true;
            }
            if (ev == Event::ArrowDown || ev == Event::Character('j')) {
                scroll_abstract(0.08f);
                return true;
            }
            if (ev.is_mouse() && ev.mouse().motion == Mouse::Moved) {
                return true;
            }
            if (ev.is_mouse() && ev.mouse().button == Mouse::WheelUp) {
                if (!mouse_inside(ev.mouse(), *abstract_modal_box)) return true;
                scroll_abstract(-0.08f);
                return true;
            }
            if (ev.is_mouse() && ev.mouse().button == Mouse::WheelDown) {
                if (!mouse_inside(ev.mouse(), *abstract_modal_box)) return true;
                scroll_abstract(0.08f);
                return true;
            }
            return true;
        }

        // ESC 返回
        if (ev == Event::Escape) {
            screen.ExitLoopClosure()();
            return true;
        }
        if (ev == Event::Character('q') || ev == Event::Character('Q')) {
            ctx->app_exit_requested = true;
            screen.PostEvent(Event::Custom);
            screen.ExitLoopClosure()();
            return true;
        }

        if (ev.is_mouse() && ev.mouse().motion == Mouse::Moved) {
            return true;
        }

        // ── 鼠标滚轮 ──────────────────────────────────────────
        if (ev.is_mouse() && ev.mouse().button == Mouse::WheelUp) {
            if (!mouse_inside(ev.mouse(), *toc_box)) return true;
            std::lock_guard lock(state->mtx);
            if (state->selected > 0) --state->selected;
            bd_ensure_vis();
            return true;
        }
        if (ev.is_mouse() && ev.mouse().button == Mouse::WheelDown) {
            if (!mouse_inside(ev.mouse(), *toc_box)) return true;
            std::lock_guard lock(state->mtx);
            if (state->selected < static_cast<int>(state->toc.size()) - 1)
                ++state->selected;
            bd_ensure_vis();
            return true;
        }

        // ── 导航 ────────────────────────────────────────────────
        if (ev == Event::ArrowDown || ev == Event::Character('j')) {
            std::lock_guard lock(state->mtx);
            if (state->selected < static_cast<int>(state->toc.size()) - 1)
                ++state->selected;
            bd_ensure_vis();
            return true;
        }
        if (ev == Event::ArrowUp || ev == Event::Character('k')) {
            std::lock_guard lock(state->mtx);
            if (state->selected > 0) --state->selected;
            bd_ensure_vis();
            return true;
        }

        // Enter 键：如果在输入框则应用范围，否则打开详情（这里没有详情，所以只应用范围）
        if (ev == Event::Return) {
            if (input_start->Focused() || input_end->Focused()) {
                validate_and_apply_range();
                return true;
            }
            // 如果不在输入框，Enter 也应用范围
            validate_and_apply_range();
            return true;
        }

        // 快捷键
        if (ev == Event::Character('g')) { download_all(); return true; }
        if (ev == Event::Character('u')) { refresh_toc();  return true; }
        if (ev == Event::Character('e')) { export_epub();  return true; }
        if (ev == Event::Character('t')) { export_txt();   return true; }
        if (ev == Event::Character('c')) { clear_range();  return true; }
        if (ev == Event::Character('i') || ev == Event::Character('I')) {
            open_abstract_modal();
            return true;
        }

        // 加入书架
        if (ev == Event::Character('a')) {
            ctx->library_service->save_to_bookshelf(state->book);
            std::lock_guard lock(state->mtx);
            state->status_msg = "已加入书架";
            return true;
        }

        return false;
    });

    return handler;
}

} // namespace fanqie
