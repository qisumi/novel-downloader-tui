/// @file frontend.cpp
/// @brief 前端页面导航实现
///
/// 实现 file:// URL 构建和前端入口页面导航逻辑。

#include "gui/frontend.h"

#include <cstdlib>
#include <stdexcept>
#include <string_view>

#include <spdlog/spdlog.h>
#include <webview/webview.h>

namespace novel {

namespace {

/// 对字符串进行 URI 百分号编码
///
/// 保留安全字符（字母、数字、/-_.~:）不变，其余字符编码为 %XX 形式
static std::string percent_encode(std::string_view input)
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

/// 将文件路径转换为 file:// URL
///
/// 步骤：
///   1. 规范化路径（解析 .. 和软链接）
///   2. 对 Windows 盘符路径（如 C:/）在开头补 '/'
///   3. 进行百分号编码
std::string to_file_url(const std::filesystem::path& path)
{
    auto generic = std::filesystem::weakly_canonical(path).generic_string();
    // Windows 路径如 "C:/..." 需要在前面加 '/' 变成 "/C:/..."
    if (generic.size() > 1 && generic[1] == ':') {
        generic.insert(generic.begin(), '/');
    }
    return "file://" + percent_encode(generic);
}

/// 导航到前端入口页面
///
/// 检查 NOVEL_GUI_DEV_SERVER 环境变量：
///   - 已设置且非空 -> 导航到 dev server URL（用于前端热更新开发）
///   - 未设置 -> 导航到本地 file:// 的 index.html
void navigate_frontend(
    webview::webview& window,
    const std::filesystem::path& frontend_dir)
{
    // 开发模式：使用 dev server
    if (const char* dev_server = std::getenv("NOVEL_GUI_DEV_SERVER")) {
        std::string url(dev_server);
        if (!url.empty()) {
            spdlog::info("GUI navigating to dev server frontend: {}", url);
            window.navigate(url);
            return;
        }
    }

    // 生产模式：使用本地文件
    const auto index_path = frontend_dir / "index.html";
    if (!std::filesystem::exists(index_path)) {
        throw std::runtime_error("GUI frontend entry not found: " + index_path.string());
    }
    const auto index_url = to_file_url(index_path);
    spdlog::info("GUI navigating to frontend file: {}", index_url);
    window.navigate(index_url);
}

} // namespace novel
