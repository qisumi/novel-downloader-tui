# AGENTS.md - AI 编程助手指南

本文档为 AI 编程助手提供项目上下文，帮助生成符合当前仓库架构与构建方式的代码。

## 项目概述

**小说下载 GUI 工具** —— 一个基于 Windows WebView 的小说搜索、下载与导出工具，使用 C++20 编写。项目通过 **JS 插件书源** 搜索、缓存小说内容，当前内置番茄小说与七猫小说书源，支持导出 EPUB/TXT。

当前界面层不再是旧 TUI / WinUI，而是：

- C++ GUI 宿主：负责初始化运行时、加载插件、暴露桥接 API、启动 WebView
- 前端静态资源：位于 `src/gui/frontend/`
- JS 插件书源：位于 `plugins/`

## 当前核心功能

- 搜索书籍
- 多书源支持（当前内置 `fanqie` / `qimao`）
- 书架管理
- 目录浏览
- 批量下载
- 导出 EPUB / TXT
- 书源清单与切换

## 技术栈

| 组件 | 用途 | 头文件/目录 |
|------|------|------------|
| WebView | Windows GUI 宿主 + JS 插件运行时 | `<webview/webview.h>` |
| WinHTTP | HTTP/HTTPS 网络请求 | `<winhttp.h>` |
| nlohmann/json | JSON 解析 / GUI bridge / JS 结果校验 | `<nlohmann/json.hpp>` |
| SQLiteCpp | SQLite C++ 封装 | `<SQLiteCpp/SQLiteCpp.h>` |
| tinyxml2 | EPUB XML 生成 | `<tinyxml2.h>` |
| libzip | EPUB ZIP 打包 | `<zip.h>` |
| CLI11 | 保留依赖 | `<CLI/CLI.hpp>` |
| spdlog | 日志 | `<spdlog/spdlog.h>` |
| VCPKG | 依赖管理 | 默认全局包（保留 `vcpkg.json` 依赖声明） |

## 目录结构

```text
novel-downloader-tui/
├── CMakeLists.txt
├── CMakePresets.json
├── QUICK_CMD.md
├── README.md
├── AGENTS.md
├── cmake/
│   └── sync_resources.cmake   # GUI 前端/插件资源同步
├── plugins/
│   ├── README.md
│   ├── fanqie.js
│   ├── qimao.js
│   └── _shared/
│       └── common.js
├── reference/
│   └── fanqie.json
└── src/
    ├── dotenv.h
    ├── logger.h
    ├── models/
    │   └── book.h
    ├── source/
    │   ├── domain/             # 书源接口与错误模型
    │   ├── host/               # C++ 插件宿主 API（HTTP / env / log）
    │   ├── js/                 # WebView JS 运行时与插件适配层
    │   └── runtime/            # 插件扫描与书源管理
    ├── application/
    │   ├── library_service.*
    │   ├── download_service.*
    │   └── export_service.*
    ├── db/
    │   ├── database.h
    │   └── database.cpp
    ├── export/
    │   ├── epub_exporter.*
    │   ├── txt_exporter.*
    │   └── text_sanitizer.*
    └── gui/
        ├── main.cpp            # GUI 入口
        ├── app_runtime.*       # GUI 运行时初始化
        ├── bridge.*            # C++ <-> JS 桥接
        ├── frontend.*          # 前端导航与 file:// URL 处理
        ├── user_paths.*        # 运行目录 / 插件 / 前端路径解析
        └── frontend/
            ├── index.html
            ├── app.js
            └── app.css
```

## 编码规范

### 语言与标准

- **C++20**
- 使用 `#pragma once`
- 命名空间统一为 `novel::`

### 命名约定

| 类型 | 风格 | 示例 |
|------|------|------|
| 命名空间 | 小写下划线 | `novel::` |
| 类/结构体 | PascalCase | `SourceManager`, `GuiAppRuntime`, `JsPluginRuntime` |
| 函数/方法 | snake_case | `load_from_directory()`, `navigate_frontend()` |
| 变量 | snake_case | `book_id`, `plugin_dir` |
| 成员变量 | snake_case + 尾下划线 | `host_api_`, `plugin_runtime_` |

### 代码风格

- 缩进：4 空格
- 大括号：K&R 风格
- 注释：
  - 分隔注释：`// ── 标题 ─────────────`
  - 文档注释：`/// `

## 关键模块

### `src/source/runtime/source_manager.*`

负责扫描 `plugins/`、注册 JS 模块、等待 WebView 内的 JS 插件 bootstrap 完成并维护当前书源。

### `src/source/js/js_plugin_runtime.*`

负责：

- 向 WebView 注入 JS 插件运行时
- 注册 `plugins/` 下的 JS 模块
- 提供 CommonJS 风格 `require(...)`
- 将插件方法调用转成 C++ <-> JS RPC

### `src/source/js/js_book_source.*`

负责将 JS 插件适配到 `IBookSource`，并校验 `manifest` / `Book` / `TocItem` / `Chapter` 返回结构。

### `src/gui/app_runtime.*`

负责：

- 解析 GUI 运行路径
- 加载 `.env`
- 初始化 logger
- 初始化 `HostApi` / `JsPluginRuntime` / `SourceManager` / `Database` / 各应用服务
- 启动时注册插件资源并配置首选书源

### `src/gui/bridge.*`

负责将 C++ 能力暴露给前端 `window.app.*`，包括：

- `get_sources`
- `select_source`
- `search_books`
- `get_book_detail`
- `get_toc`
- `download_book`
- `export_book`
- `list_bookshelf`
- `save_bookshelf`
- `remove_bookshelf`

### `src/gui/frontend.*`

负责前端入口导航：

- 若设置 `NOVEL_GUI_DEV_SERVER`，导航到 dev server
- 否则导航到本地 `file://.../gui/index.html`

### `src/gui/user_paths.*`

负责 GUI 运行时路径解析。需要特别注意：

- 插件目录会从运行目录 / 可执行文件目录向上回溯查找
- 前端目录也会按类似方式回溯查找
- 目标是兼容源码运行、输出目录运行、打包运行三种方式

## JS 插件约定

插件位于 `plugins/`，是普通 CommonJS 风格 JS 模块，需通过 `module.exports` 导出对象，并至少实现：

```js
module.exports = {
  manifest: {
    id: "fanqie",
    name: "番茄小说",
    version: "1.1.0",
    required_envs: ["FANQIE_APIKEY"],
    optional_envs: [],
  },
  async configure() {},
  async search(keywords, page) {},
  async get_book_info(bookId) {},
  async get_toc(bookId) {},
  async get_chapter(bookId, itemId) {},
};
```

宿主 API：

- `host.http_get(url, headers?, timeoutSeconds?)`
- `host.http_request({ method, url, headers?, body?, timeout_seconds? })`
- `host.url_encode(text)`
- `host.env_get(name, defaultValue?)`
- `host.config_error(message)`
- `host.log_info(msg)` / `host.log_warn(msg)` / `host.log_error(msg)`

共享模块通过 `require("_shared/common")` 复用；以下划线开头的路径段不会被当作书源候选插件加载。

## 构建与运行

### 常用构建

```powershell
vcpkg install --triplet x64-windows nlohmann-json sqlitecpp tinyxml2 libzip cli11 spdlog webview2
vcpkg install --triplet x64-windows-static nlohmann-json sqlitecpp tinyxml2 libzip cli11 spdlog webview2

cmake --preset windows-x64-debug-msvc
cmake --build --preset windows-x64-debug-msvc --target novel-downloader-gui

cmake --preset windows-x64-release-static-msvc
cmake --build --preset windows-x64-release-static-msvc --target novel-downloader-gui
```

默认 preset 已关闭 `VCPKG_MANIFEST_MODE`，优先使用 `VCPKG_ROOT` 下的全局安装包；如需强制走仓库内 `vcpkg.json`，可在配置时追加 `-DVCPKG_MANIFEST_MODE=ON`。

### GUI 运行

```powershell
.\build\debug-msvc\bin\Debug\novel-downloader-gui.exe
.\build\release-static-msvc\bin\Release\novel-downloader-gui.exe
```

## 资源同步规则

构建 `novel-downloader-gui` 时，CMake 会自动同步：

- `src/gui/frontend/` -> 输出目录 `gui/`
- `plugins/` -> 输出目录 `plugins/`

同步脚本位于 `cmake/sync_resources.cmake`，会先清目标目录再复制，避免旧资源残留。

如果修改了：

- `src/gui/frontend/*.html|js|css`
- `plugins/*.js`
- `plugins/_shared/*.js`

应重新执行对应的 GUI build，确保输出目录资源更新。

## 调试建议

### 插件相关

- 优先看 `novel-gui.log` 里的：
  - `Plugin load start`
  - `Discovered JS module`
  - `Loaded source`
  - `Failed to evaluate JS plugin`
  - `No valid source plugins found`

### 前端相关

- 优先看：
  - `GUI navigating to frontend file`
  - `GUI navigating to dev server frontend`

### 资源未更新相关

- 优先检查输出目录：
  - `build/<preset>/bin/<config>/gui`
  - `build/<preset>/bin/<config>/plugins`

## 注意事项

1. Windows / MSVC 优先
2. GUI 当前依赖 WebView2 运行环境
3. 插件加载依赖 `plugins/` 目录，不要漏打包
4. 前端脚本当前以普通 `<script src="./app.js">` 方式加载，不要随意改回 `type="module"`，除非确认 `file://` 场景兼容
5. 修改 GUI 资源或插件脚本后，应重新执行 GUI build，让资源同步目标生效

## 相关文档

- [README.md](README.md)
- [QUICK_CMD.md](QUICK_CMD.md)
- [plugins/README.md](plugins/README.md)
- [reference/fanqie.json](reference/fanqie.json)
