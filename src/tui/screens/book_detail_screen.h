#pragma once
#include <memory>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include "tui/app.h"

namespace novel {

/// 书籍详情页面：目录列表 + 下载 + EPUB 导出入口
/// ctx->current_book 必须在调用前已设置
ftxui::Component make_book_detail_screen(
    std::shared_ptr<AppContext> ctx,
    ftxui::ScreenInteractive&   screen);

} // namespace novel
