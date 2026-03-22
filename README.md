# 小说下载工具

基于 Lua 插件书源的小说下载与导出核心，使用 C++20 编写，负责搜索、目录获取、章节缓存与 EPUB/TXT 导出。

当前仓库已经移除旧的 GUI/TUI 代码，现阶段以核心能力整理为主。后续计划使用 C++ 搭配系统 WebView 重建界面层。

## 当前状态

- 保留内容：书源运行时、应用服务、SQLite 缓存、EPUB/TXT 导出、插件体系
- 已移除内容：旧 TUI、旧 WinUI GUI
- 下一步方向：基于现有 C++ 核心接入系统 WebView UI
- 当前构建：顶层 CMake 已临时收敛为核心静态库 `novel-core`

## 核心能力

| 能力 | 说明 |
|------|------|
| 搜索 | 通过 Lua 书源按书名或作者搜索 |
| 书源切换 | 从 `plugins/` 加载并切换 `fanqie`、`qimao` 等书源 |
| 目录获取 | 拉取并缓存章节目录 |
| 章节缓存 | 使用 SQLite 缓存正文 |
| 批量下载 | 通过应用服务批量抓取章节 |
| 导出 | 导出 EPUB / TXT |
| 插件扩展 | 可新增 Lua 书源插件 |

## 目录结构

```text
fanqie-downloader-tui/
├── CMakeLists.txt
├── CMakePresets.json
├── README.md
├── QUICK_CMD.md
├── .env.example
├── plugins/
│   ├── fanqie.lua
│   ├── qimao.lua
│   ├── README.md
│   └── _shared/
├── reference/
│   └── fanqie.json
├── src/
│   ├── application/           # Library / Download / Export 服务
│   ├── db/                    # SQLite 持久化
│   ├── export/                # EPUB / TXT 导出
│   ├── models/                # Book / TocItem / Chapter
│   ├── source/                # 书源接口、Lua 运行时、宿主 API
│   ├── dotenv.h
│   └── logger.h
└── vcpkg.json
```

## 依赖

- Windows
- CMake 3.21+
- Visual Studio / MSVC
- `VCPKG_ROOT`
- vcpkg 清单依赖：`cpp-httplib`、`nlohmann-json`、`SQLiteCpp`、`tinyxml2`、`libzip`、`OpenSSL`、`Lua`、`LuaBridge3`、`CLI11`、`spdlog`

## 配置

配置优先级：

1. 命令行参数
2. 系统环境变量
3. 项目根目录 `.env`

首次使用：

```powershell
Copy-Item .env.example .env
```

示例：

```dotenv
FANQIE_APIKEY=your_fanqie_api_key
# QIMAO_APIKEY=your_qimao_api_key

NOVEL_DB=novel.db
NOVEL_EPUB_DIR=exports
NOVEL_PLUGIN_DIR=plugins
NOVEL_SOURCE=fanqie
```

说明：

- `FANQIE_APIKEY`、`QIMAO_APIKEY` 是否必填，取决于对应插件的 `manifest.required_envs`
- `NOVEL_DB`、`NOVEL_EPUB_DIR`、`NOVEL_PLUGIN_DIR`、`NOVEL_SOURCE` 是宿主层通用配置约定

## 构建说明

当前顶层 CMake 会构建核心静态库：

```powershell
vcpkg install
cmake --preset windows-x64-debug-msvc
cmake --build --preset windows-x64-debug-msvc
```

当前产物是核心库，不包含可直接启动的 UI 或 CLI 程序入口。这一步的目标是先保持核心代码可持续编译，再为后续 WebView 界面接入预留稳定基础。

## WebView 方向

后续 UI 计划：

- 保持下载、缓存、导出、插件加载等能力继续由 C++ 核心提供
- 新界面使用系统 WebView 承载前端
- 用更清晰的桥接层把搜索、目录、下载、导出能力暴露给 UI
- 后续会在此基础上增加新的 WebView UI 宿主和桥接层

## 相关文档

- [QUICK_CMD.md](QUICK_CMD.md)
- [plugins/README.md](plugins/README.md)
- [AGENTS.md](AGENTS.md)
