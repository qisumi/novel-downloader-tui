#pragma once
#include <string>
#include <ftxui/component/component.hpp>

namespace novel {

/// 简单旋转动画加载指示器
/// @param message 指向要显示的文字的指针（可 nullptr）
ftxui::Component make_loading_indicator(const std::string* message = nullptr);

} // namespace novel
