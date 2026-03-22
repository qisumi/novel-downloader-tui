# AGENTS.md - AI 编程助手指南

本文档为 AI 编程助手（如 Claude、Cursor、Copilot 等）提供项目上下文，帮助生成符合项目规范的代码。

## 项目概述

**小说下载 TUI 工具** —— 一个基于终端界面（TUI）的小说下载与导出工具，使用 C++20 编写。项目通过 **Lua 插件书源** 搜索、缓存小说内容，当前内置番茄小说与七猫小说书源，支持导出为 EPUB/TXT 格式。

### 核心功能

- 搜索书籍（关键词搜索书名/作者）
- 多书源支持（当前内置 `fanqie` / `qimao`，可通过 Lua 插件扩展）
- 书架管理（本地收藏）
- 目录浏览（章节列表、缓存状态）
- 批量下载（SQLite 缓存）
- 导出 EPUB/TXT（支持章节范围选择）
- 书源清单与切换（CLI `--source` / `--list-sources`）

## 技术栈

| 组件 | 用途 | 头文件/命名空间 |
|------|------|----------------|
| FTXUI | TUI 渲染与交互 | `<ftxui/...>` |
| cpp-httplib | HTTP/HTTPS 网络请求 | `<httplib.h>` |
| nlohmann/json | JSON 解析 | `<nlohmann/json.hpp>` |
| SQLiteCpp | SQLite C++ 封装 | `<SQLiteCpp/SQLiteCpp.h>` |
| tinyxml2 | EPUB XML 生成 | `<tinyxml2.h>` |
| libzip | EPUB ZIP 打包 | `<zip.h>` |
| OpenSSL | HTTPS 支持 | 系统库 |
| Lua + LuaBridge3 | 脚本支持 | `<lua.hpp>`, `<LuaBridge/LuaBridge.h>` |
| CLI11 | 命令行解析 | `<CLI/CLI.hpp>` |
| spdlog | 日志 | `<spdlog/spdlog.h>` |
| VCPKG | 依赖管理 | 清单模式 |

## 目录结构

```
novel-downloader-tui/
├── CMakeLists.txt              # CMake 构建配置
├── CMakePresets.json           # CMake 预设（Debug/Release）
├── vcpkg.json                  # VCPKG 依赖清单
├── README.md                   # 用户文档
├── QUICK_CMD.md                # 开发命令速查
├── .env.example                # 环境变量示例
├── plugins/
│   ├── README.md               # Lua 插件开发说明
│   ├── fanqie.lua              # 小说默认书源
│   ├── qimao.lua               # 七猫小说书源
│   └── _shared/
│       └── common.lua          # 插件共享 helper
├── reference/
│   └── fanqie.json             # API 参考 JSON
└── src/
    ├── main.cpp                # 程序入口、CLI 解析、TUI 启动
    ├── dotenv.h                # .env 文件加载
    ├── logger.h                # spdlog 初始化
    ├── models/
    │   └── book.h              # Book / TocItem / Chapter 数据模型
    ├── source/
    │   ├── domain/             # 书源接口与领域类型
    │   ├── host/               # Lua 插件宿主 API
    │   ├── lua/                # Lua 运行时与插件适配层
    │   └── runtime/            # 插件加载与书源管理
    ├── application/
    │   ├── library_service.*   # 搜索 / 书架 / 目录
    │   ├── download_service.*  # 下载与缓存
    │   └── export_service.*    # 导出协调
    ├── db/
    │   ├── database.h          # 数据库接口
    │   └── database.cpp        # SQLite 操作（书架、缓存）
    ├── export/
    │   ├── epub_exporter.h     # EPUB 导出接口
    │   ├── epub_exporter.cpp   # EPUB 生成逻辑
    │   ├── txt_exporter.h      # TXT 导出接口
    │   ├── txt_exporter.cpp    # TXT 生成逻辑
    │   ├── text_sanitizer.h    # 文本清洗接口
    │   └── text_sanitizer.cpp  # 文本清洗实现
    └── ui/
        ├── app.h               # AppContext 定义
        ├── app.cpp              # 主循环、标签栏
        ├── screens/
        │   ├── search_screen.*       # 搜索页
        │   ├── bookshelf_screen.*    # 书架页
        │   └── book_detail_screen.*  # 书籍详情 + 目录
        └── components/
            └── loading_indicator.*   # 加载动画组件
```

## 编码规范

### 语言与标准

- **C++20** 标准
- 使用 `#pragma once` 作为头文件保护
- 命名空间统一为 `novel::`

### 命名约定

| 类型 | 风格 | 示例 |
|------|------|------|
| 命名空间 | 小写下划线 | `novel::` |
| 类/结构体 | PascalCase | `SourceManager`, `Book` |
| 函数/方法 | snake_case | `get_book_info()`, `save_book()` |
| 变量 | snake_case | `book_id`, `toc_items` |
| 成员变量 | snake_case + 尾下划线 | `api_key_`, `timeout_s_` |
| 常量 | 大写下划线 | `MAX_RETRY_COUNT` |
| 枚举值 | PascalCase | `Color::Red` |

### 代码风格

- 缩进：4 空格
- 大括号：K&R 风格（函数定义换行，控制语句同行）
- 注释：
  - 分隔注释使用 `// ── 标题 ─────────────` 格式
  - 文档注释使用 `/// ` 开头

### 文件组织

```cpp
// 1. 头文件包含（按逻辑分组，空行分隔）
#include <标准库>
#include <第三方库>
#include "项目头文件"

// 2. 命名空间
namespace novel {

// 3. 类定义
class Example {
public:
    // 公共方法
private:
    // 私有成员
};

} // namespace novel
```

## 核心模块详解

### 数据模型 (`src/models/book.h`)

```cpp
// 书籍基本信息（搜索结果 / 书架条目）
struct Book {
    std::string book_id;          // 唯一标识
    std::string title;            // 书名
    std::string author;           // 作者
    std::string cover_url;        // 封面 URL
    std::string abstract;         // 简介
    std::string category;         // 分类
    std::string word_count;       // 字数
    double      score = 0.0;      // 评分
    int         gender = 0;       // 0=未知 1=男频 2=女频
    int         creation_status = 0; // 0=连载 1=完结
    std::string last_chapter_title;
    int64_t     last_update_time = 0;
};

// 目录条目（单章）
struct TocItem {
    std::string item_id;          // 章节 ID
    std::string title;            // 章节标题
    std::string volume_name;      // 卷名
    int         word_count = 0;
    int64_t     update_time = 0;
};

// 章节正文
struct Chapter {
    std::string item_id;
    std::string title;
    std::string content;          // 已解密的纯文本
};
```

### 书源运行时 (`src/source/runtime/source_manager.h`)

负责加载 Lua 插件、选择当前书源，并向上层暴露统一书源接口。

```cpp
class SourceManager {
public:
    void load_from_directory(const std::string& plugin_dir);
    std::vector<SourceInfo> list_sources() const;
    bool select_source(const std::string& source_id);
    std::shared_ptr<IBookSource> current_source() const;
    std::optional<SourceInfo> current_info() const;
    void configure_current();
};
```

### 插件清单与宿主 API

Lua 插件位于 `plugins/`，需要返回一个 table，并至少实现：

```lua
return {
    manifest = {
        id = "fanqie",
        name = "小说",
        version = "1.1.0",
        required_envs = { "FANQIE_APIKEY" },
        optional_envs = {},
    },
    configure = function() end,         -- 可选，但推荐
    search = function(keywords, page) end,
    get_book_info = function(book_id) end, -- 可选
    get_toc = function(book_id) end,
    get_chapter = function(book_id, item_id) end,
}
```

宿主当前为 Lua 暴露的常用 API：

- `host.http_get(url)`
- `host.http_request({ method, url, headers?, body?, timeout_seconds? })`
- `host.json_parse(text)`
- `host.json_stringify(value)`
- `host.url_encode(text)`
- `host.env_get(name[, default])`
- `host.config_error(message)`
- `host.log_info(msg)` / `host.log_warn(msg)` / `host.log_error(msg)`

约定：

- 插件通过 `manifest.required_envs` / `manifest.optional_envs` 声明配置需求
- `.env` 由宿主统一加载，插件通过 `host.env_get(...)` 或共享 helper 读取
- 配置缺失或非法时，优先在插件侧调用 `host.config_error("...")`
- 可复用逻辑优先抽到 `plugins/_shared/*.lua`
- 更完整的插件约定以 `plugins/README.md` 为准

### 数据库层 (`src/db/database.h`)

SQLite 持久化，管理书架和章节缓存。

```cpp
class Database {
public:
    // 书架管理
    void save_book(const Book& book);
    bool remove_book(const std::string& book_id);
    std::vector<Book> list_bookshelf();
    std::optional<Book> get_book(const std::string& book_id);
    bool is_in_bookshelf(const std::string& book_id);
    
    // 目录缓存
    void save_toc(const std::string& book_id, const std::vector<TocItem>& toc);
    std::vector<TocItem> get_toc(const std::string& book_id);
    
    // 章节缓存
    void save_chapter(const std::string& book_id, const Chapter& ch);
    std::optional<Chapter> get_chapter(const std::string& item_id);
    bool chapter_cached(const std::string& item_id);
    int cached_chapter_count(const std::string& book_id);
};
```

### 应用上下文 (`src/ui/app.h`)

全局共享状态，在各屏幕间传递：

```cpp
struct AppContext {
    std::shared_ptr<SourceManager>   source_manager;
    std::shared_ptr<LibraryService>  library_service;
    std::shared_ptr<DownloadService> download_service;
    std::shared_ptr<ExportService>   export_service;
    std::shared_ptr<Database>        db;
    Book          current_book;
    std::string   plugin_dir = "plugins";
    std::string   current_source_id;
    std::string   current_source_name;
    std::string   epub_output_dir = ".";
    std::atomic<bool> bookshelf_dirty{false};
};
```

## 构建与运行

### 构建命令

```powershell
# 安装依赖（首次）
vcpkg install

# Debug 构建
cmake --preset windows-x64-debug
cmake --build --preset windows-x64-debug

# Release 构建
cmake --preset windows-x64-release
cmake --build --preset windows-x64-release
```

### 运行参数

```powershell
# 直接运行（插件会自行从环境变量或 .env 读取所需配置）
.\build\release\novel-downloader-tui.exe

# 命令行参数
.\build\release\novel-downloader-tui.exe --db <db_path> -o <epub_dir> --plugin-dir plugins --source fanqie

# 列出当前可用书源
.\build\release\novel-downloader-tui.exe --plugin-dir plugins --list-sources

# 环境变量
$env:FANQIE_APIKEY = "your_key"
$env:QIMAO_APIKEY = "your_key"
$env:NOVEL_DB = "novel.db"
$env:NOVEL_EPUB_DIR = "."
$env:NOVEL_PLUGIN_DIR = "plugins"
$env:NOVEL_SOURCE = "fanqie"
```

### 配置优先级

命令行参数 > 系统环境变量 > .env 文件

## UI 组件开发

### FTXUI 基本模式

```cpp
using namespace ftxui;

// 1. 定义状态
std::string input;
int selected = 0;

// 2. 创建组件
auto input_box = Input(&input, "占位文本");
auto menu = Menu(&entries, &selected);

// 3. 组合布局
auto layout = Container::Vertical({
    input_box,
    menu,
});

// 4. 渲染
auto renderer = Renderer(layout, [&] {
    return vbox({
        text("标题"),
        input_box->Render(),
        menu->Render(),
    });
});

// 5. 事件处理
auto event_handler = CatchEvent(renderer, [&](Event ev) {
    if (ev == Event::Enter) {
        // 处理回车
        return true;
    }
    return false;
});
```

### 常用组件

- `Input` - 文本输入框
- `Menu` - 可选择列表
- `Button` - 按钮
- `Checkbox` - 复选框
- `Radiobox` - 单选框
- `Toggle` - 切换开关

### 布局元素

- `vbox` - 垂直布局
- `hbox` - 水平布局
- `flex` - 弹性填充
- `filler` - 空白填充
- `separator` - 分隔线
- `border` - 边框
- `size(WIDTH/HEIGHT, EQUAL/LESS_THAN/GREATER_THAN, value)` - 尺寸约束

### 快捷键约定

- `q` / `Q` - 退出
- `Tab` - 切换标签
- `Enter` - 确认
- `↑` / `↓` / `j` / `k` - 导航
- `Esc` - 返回/取消

## 异步操作模式

FTXUI 主线程用于渲染，网络请求等耗时操作需在后台执行：

```cpp
// 使用 std::thread 执行后台任务
std::thread([ctx, progress_cb] {
    ctx->download_service->download_book(ctx->current_book, toc, progress_cb);
    // 使用 ScreenInteractive::PostEvent 通知主线程
    screen.PostEvent(Event::Custom);
}).detach();

// 主线程通过原子变量或共享状态获取结果
```

## 数据库 Schema

```sql
-- 书架
CREATE TABLE books (
    book_id TEXT PRIMARY KEY,
    title TEXT NOT NULL,
    author TEXT,
    cover_url TEXT,
    abstract TEXT,
    category TEXT,
    word_count TEXT,
    score REAL,
    gender INTEGER,
    creation_status INTEGER,
    last_chapter_title TEXT,
    last_update_time INTEGER,
    added_at INTEGER
);

-- 目录缓存
CREATE TABLE toc (
    book_id TEXT,
    item_id TEXT PRIMARY KEY,
    title TEXT,
    volume_name TEXT,
    word_count INTEGER,
    update_time INTEGER,
    item_index INTEGER
);

-- 章节缓存
CREATE TABLE chapters (
    item_id TEXT PRIMARY KEY,
    book_id TEXT,
    title TEXT,
    content TEXT,
    cached_at INTEGER
);
```

## 常见开发任务

### 添加新的书源能力

1. 在 `plugins/` 新建或修改 Lua 插件，并补齐 `manifest.id/name/version`
2. 按需声明 `required_envs` / `optional_envs`
3. 通过 `host.http_get()` / `host.http_request()` / `host.json_parse()` 等宿主能力发起请求
4. 返回符合 `Book` / `TocItem` / `Chapter` 结构的数据
5. 配置缺失、请求参数错误、数据解析错误优先在插件侧给出明确错误
6. 通用逻辑优先复用 `plugins/_shared/common.lua`
7. 用 `--source`、`--list-sources` 或 `SourceManager::select_source()` 验证书源加载结果

### 添加新的 UI 屏幕

1. 在 `src/ui/screens/` 创建 `xxx_screen.h` 和 `xxx_screen.cpp`
2. 实现 `Component CreateXxxScreen(std::shared_ptr<AppContext> ctx)` 函数
3. 在 `app.cpp` 中添加标签页入口

### 添加新的导出格式

1. 在 `src/export/` 创建 `xxx_exporter.h` 和 `xxx_exporter.cpp`
2. 参考 `epub_exporter.cpp` 的结构
3. 在 `book_detail_screen.cpp` 添加快捷键绑定

### 添加数据库表

1. 在 `database.cpp` 的 `init_schema()` 添加 CREATE TABLE
2. 添加相应的 CRUD 方法
3. 更新相关数据模型

## 错误处理

- 网络请求使用 `std::optional` 表示可能失败的操作
- 使用 `spdlog` 记录日志：`spdlog::info()`, `spdlog::error()`, `spdlog::warn()`
- 用户界面显示友好的错误提示
- 书源错误统一使用 `SourceError` / `SourceException` 传递上下文
- 插件错误优先区分为：配置错误、请求参数错误、数据处理错误、网络错误
- C++ 侧会补充 `source_id`、`operation`、`plugin_path`，日志中优先保留这些字段

## 日志

日志文件：`novel.log`

```cpp
#include "logger.h"

// 初始化（main.cpp 中已调用）
novel::init_logger("novel.log");

// 使用
spdlog::info("消息");
spdlog::error("错误: {}", error_msg);
```

## 注意事项

1. **Windows 平台优先**：项目主要针对 Windows 开发，使用 Clang 编译
2. **UTF-8 编码**：main.cpp 中设置了 UTF-8 控制台输出
3. **同步书源调用**：Lua 书源中的网络请求仍是同步执行，UI 中需在后台线程调用
4. **插件配置按书源决定**：例如内置 `fanqie` 需要 `FANQIE_APIKEY`，`qimao` 需要 `QIMAO_APIKEY`，以插件 `manifest.required_envs` 为准
5. **线程安全**：`AppContext::bookshelf_dirty` 使用 `std::atomic` 保证线程安全
6. **发布包需携带插件目录**：运行时依赖 `plugins/` 下的 Lua 脚本，发布或打包时不要遗漏

## 相关文档

- [README.md](README.md) - 用户文档
- [QUICK_CMD.md](QUICK_CMD.md) - 开发命令速查
- [reference/fanqie.json](reference/fanqie.json) - API 参考
