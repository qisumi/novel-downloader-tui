#pragma once
#include <memory>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include "ui/app.h"

namespace fanqie {

/// 书架页面：列出已保存书籍
ftxui::Component make_bookshelf_screen(
    std::shared_ptr<AppContext> ctx,
    ftxui::ScreenInteractive&   screen);

} // namespace fanqie
