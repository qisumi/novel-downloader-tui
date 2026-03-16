#pragma once
#include <memory>
#include <string>
#include <atomic>
#include "api/fanqie_client.h"
#include "db/database.h"
#include "models/book.h"

namespace fanqie {

/// 整个 TUI 应用的上下文（共享状态），通过指针在各屏幕间传递
struct AppContext {
    std::shared_ptr<FanqieClient> client;
    std::shared_ptr<Database>     db;

    // 当前选中的书籍
    Book          current_book;

    // 全局设置
    std::string   api_key;
    std::string   epub_output_dir = ".";

    // 书架脏标志：搜索页加书后置 true，书架页刷新后清零
    std::atomic<bool> bookshelf_dirty{false};
    // 书架页需要刷新标志：切换到书架页时置 true，书架页刷新后清零
    std::atomic<bool> bookshelf_needs_refresh{false};
    // 全局退出请求：子页面可置 true，请求主循环退出
    std::atomic<bool> app_exit_requested{false};
};

/// 启动 TUI 主循环
/// @return 进程退出码
int run_app(std::shared_ptr<AppContext> ctx);

} // namespace fanqie
