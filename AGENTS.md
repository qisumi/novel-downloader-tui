# AGENTS.md - AI 编程助手指南

本文档为 AI 编程助手（如 Claude、Cursor、Copilot 等）提供项目上下文，帮助生成符合项目规范的代码。

## 项目概述

**番茄小说 TUI 下载器** —— 一个基于终端界面（TUI）的小说下载与导出工具，使用 C++20 编写。通过番茄小说 API 搜索、缓存小说内容，支持导出为 EPUB/TXT 格式。

### 核心功能

- 搜索书籍（关键词搜索书名/作者）
- 书架管理（本地收藏）
- 目录浏览（章节列表、缓存状态）
- 批量下载（SQLite 缓存）
- 导出 EPUB/TXT（支持章节范围选择）

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
fanqie-downloader-tui/
├── CMakeLists.txt              # CMake 构建配置
├── CMakePresets.json           # CMake 预设（Debug/Release）
├── vcpkg.json                  # VCPKG 依赖清单
├── README.md                   # 用户文档
├── QUICK_CMD.md                # 开发命令速查
├── reference/
│   └── fanqie.json             # API 参考 JSON
└── src/
    ├── main.cpp                # 程序入口、CLI 解析、TUI 启动
    ├── dotenv.h                # .env 文件加载
    ├── logger.h                # spdlog 初始化
    ├── models/
    │   └── book.h              # Book / TocItem / Chapter 数据模型
    ├── api/
    │   ├── fanqie_client.h     # API 客户端接口
    │   └── fanqie_client.cpp   # API 实现（搜索、目录、章节）
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
- 命名空间统一为 `fanqie::`

### 命名约定

| 类型 | 风格 | 示例 |
|------|------|------|
| 命名空间 | 小写下划线 | `fanqie::` |
| 类/结构体 | PascalCase | `FanqieClient`, `Book` |
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
namespace fanqie {

// 3. 类定义
class Example {
public:
    // 公共方法
private:
    // 私有成员
};

} // namespace fanqie
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

### API 客户端 (`src/api/fanqie_client.h`)

负责与番茄小说 API 通信，所有请求为同步调用。

```cpp
class FanqieClient {
public:
    // 搜索书籍，page 从 0 开始
    std::vector<Book> search(const std::string& keywords, int page = 0);
    
    // 获取书籍详情
    std::optional<Book> get_book_info(const std::string& book_id);
    
    // 获取目录
    std::vector<TocItem> get_toc(const std::string& book_id);
    
    // 获取单章内容
    std::optional<Chapter> get_chapter(const std::string& item_id);
    
    // 批量下载（带进度回调）
    std::vector<Chapter> download_chapters(
        const std::vector<TocItem>& toc,
        std::function<void(int, int)> progress_cb = nullptr);
};
```

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
    std::shared_ptr<FanqieClient> client;    // API 客户端
    std::shared_ptr<Database>     db;        // 数据库
    Book          current_book;               // 当前选中书籍
    std::string   api_key;                    // API 密钥
    std::string   epub_output_dir = ".";      // 导出目录
    std::atomic<bool> bookshelf_dirty{false}; // 书架脏标志
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
# 直接运行（会提示输入 API Key）
.\build\release\fanqie-downloader-tui.exe

# 命令行参数
.\build\release\fanqie-downloader-tui.exe -k <api_key> --db <db_path> -o <epub_dir>

# 环境变量
$env:FANQIE_APIKEY = "your_key"
$env:FANQIE_DB = "fanqie.db"
$env:FANQIE_EPUB_DIR = "."
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
    auto chapters = ctx->client->download_chapters(toc, progress_cb);
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

### 添加新的 API 接口

1. 在 `fanqie_client.h` 声明方法
2. 在 `fanqie_client.cpp` 实现请求逻辑
3. 使用 `http_get()` 发送请求
4. 用 `nlohmann::json` 解析响应

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

## 日志

日志文件：`fanqie.log`

```cpp
#include "logger.h"

// 初始化（main.cpp 中已调用）
fanqie::init_logger("fanqie.log");

// 使用
spdlog::info("消息");
spdlog::error("错误: {}", error_msg);
```

## 注意事项

1. **Windows 平台优先**：项目主要针对 Windows 开发，使用 Clang 编译
2. **UTF-8 编码**：main.cpp 中设置了 UTF-8 控制台输出
3. **同步 API**：`FanqieClient` 的所有方法都是同步的，UI 中需在后台线程调用
4. **API Key 必需**：运行时必须提供有效的番茄小说 API Key
5. **线程安全**：`AppContext::bookshelf_dirty` 使用 `std::atomic` 保证线程安全

## 相关文档

- [README.md](README.md) - 用户文档
- [QUICK_CMD.md](QUICK_CMD.md) - 开发命令速查
- [reference/fanqie.json](reference/fanqie.json) - API 参考