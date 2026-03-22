#pragma once
#include <memory>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include "tui/app.h"

namespace novel {

/// 搜索页面：关键词输入 + 结果列表
ftxui::Component make_search_screen(
    std::shared_ptr<AppContext> ctx,
    ftxui::ScreenInteractive&   screen);

} // namespace novel
