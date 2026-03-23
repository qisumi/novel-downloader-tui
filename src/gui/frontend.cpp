#include "gui/frontend.h"

#include <cstdlib>
#include <stdexcept>
#include <string_view>

#include <spdlog/spdlog.h>
#include <webview/webview.h>

namespace novel {

namespace {

std::string percent_encode(std::string_view input)
{
    static constexpr char HEX[] = "0123456789ABCDEF";
    std::string output;
    output.reserve(input.size() * 3);

    for (unsigned char ch : input) {
        const bool safe =
            (ch >= 'a' && ch <= 'z') ||
            (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9') ||
            ch == '/' || ch == '-' || ch == '_' || ch == '.' || ch == '~' || ch == ':';
        if (safe) {
            output.push_back(static_cast<char>(ch));
            continue;
        }
        output.push_back('%');
        output.push_back(HEX[(ch >> 4) & 0x0F]);
        output.push_back(HEX[ch & 0x0F]);
    }

    return output;
}

} // namespace

std::string to_file_url(const std::filesystem::path& path)
{
    auto generic = std::filesystem::weakly_canonical(path).generic_string();
    if (generic.size() > 1 && generic[1] == ':') {
        generic.insert(generic.begin(), '/');
    }
    return "file://" + percent_encode(generic);
}

void navigate_frontend(
    webview::webview& window,
    const std::filesystem::path& frontend_dir)
{
    if (const char* dev_server = std::getenv("NOVEL_GUI_DEV_SERVER")) {
        std::string url(dev_server);
        if (!url.empty()) {
            spdlog::info("GUI navigating to dev server frontend: {}", url);
            window.navigate(url);
            return;
        }
    }

    const auto index_path = frontend_dir / "index.html";
    if (!std::filesystem::exists(index_path)) {
        throw std::runtime_error("GUI frontend entry not found: " + index_path.string());
    }
    const auto index_url = to_file_url(index_path);
    spdlog::info("GUI navigating to frontend file: {}", index_url);
    window.navigate(index_url);
}

} // namespace novel
