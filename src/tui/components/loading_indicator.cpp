#include "tui/components/loading_indicator.h"
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>

using namespace ftxui;

namespace novel {

ftxui::Component make_loading_indicator(const std::string* message) {
    static const char* frames[] = {"⠋","⠙","⠹","⠸","⠼","⠴","⠦","⠧","⠇","⠏"};
    auto frame_idx = std::make_shared<int>(0);

    auto container = Container::Vertical({});
    return Renderer(container, [=]() {
        int fi = (*frame_idx % 10);
        ++(*frame_idx);
        return hbox({
            text(std::string(frames[fi])) | color(Color::Cyan),
            text("  "),
            text(message ? *message : "加载中…"),
        });
    });
}

} // namespace novel
