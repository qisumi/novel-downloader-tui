# 小说下载工具

基于 C++20、Windows WebView GUI 和 JS 书源插件的小说搜索、下载、缓存与导出工具。核心能力仍由 C++ 服务层提供，书源脚本改为运行在 WebView 的 JS 插件运行时中，当前内置 `fanqie` 与 `qimao` 两个书源，支持导出 EPUB / TXT。

## 当前状态

- 当前可运行形态：Windows WebView GUI `novel-downloader-gui.exe`
- 核心能力：搜索、书源切换、目录获取、章节缓存、批量下载、EPUB/TXT 导出
- 书源机制：运行时从 `plugins/` 加载 JS 插件
- 当前构建：顶层 CMake 同时构建核心库 `novel-core` 与 GUI 宿主

## 核心能力

| 能力 | 说明 |
|------|------|
| 搜索 | 通过 JS 书源按书名或作者搜索 |
| 书源切换 | 加载并切换 `fanqie`、`qimao` 等插件书源 |
| 目录获取 | 拉取并缓存章节目录 |
| 章节缓存 | 使用 SQLite 缓存正文 |
| 批量下载 | 按目录批量抓取章节 |
| 导出 | 导出 EPUB / TXT |
| 插件扩展 | 可新增 JS 书源插件 |

## 目录结构

```text
novel-downloader-tui/
├── CMakeLists.txt
├── CMakePresets.json
├── README.md
├── QUICK_CMD.md
├── AGENTS.md
├── cmake/
│   └── sync_resources.cmake   # GUI 前端/插件资源同步脚本
├── plugins/
│   ├── fanqie.js
│   ├── qimao.js
│   ├── README.md
│   └── _shared/
│       └── common.js
├── reference/
│   └── fanqie.json
└── src/
    ├── application/           # Library / Download / Export 服务
    ├── db/                    # SQLite 持久化
    ├── export/                # EPUB / TXT 导出
    ├── gui/                   # WebView GUI 宿主、桥接、前端静态资源
    ├── models/                # Book / TocItem / Chapter
    ├── source/
    │   ├── domain/            # 书源接口与错误模型
    │   ├── host/              # C++ 宿主 API（HTTP / env / log）
    │   ├── js/                # WebView JS 插件运行时与适配层
    │   └── runtime/           # 插件扫描与书源管理
    ├── dotenv.h
    └── logger.h
```

## 依赖

- Windows
- CMake 3.21+
- Visual Studio / MSVC
- `VCPKG_ROOT`
- vcpkg 依赖声明保留在 `vcpkg.json`
- 默认 CMake preset 使用 `VCPKG_ROOT` 下的全局安装包（classic mode）

## 配置

配置优先级：

1. 系统环境变量
2. 可执行文件同目录 `.env`

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
NOVEL_GUI_DEV_SERVER=
```

说明：

- `FANQIE_APIKEY`、`QIMAO_APIKEY` 是否必填，以对应插件 `manifest.required_envs` 为准
- GUI 启动时会优先读取可执行文件同目录 `.env`
- `NOVEL_GUI_DEV_SERVER` 非空时，GUI 宿主会直接导航到该前端 dev server

## 构建与运行

Debug：

```powershell
vcpkg install --triplet x64-windows nlohmann-json sqlitecpp tinyxml2 libzip cli11 spdlog webview2
vcpkg install --triplet x64-windows-static nlohmann-json sqlitecpp tinyxml2 libzip cli11 spdlog webview2
cmake --preset windows-x64-debug-msvc
cmake --build --preset windows-x64-debug-msvc --target novel-downloader-gui
.\build\debug-msvc\bin\Debug\novel-downloader-gui.exe
```

如需临时切回项目清单模式，可在配置时追加 `-DVCPKG_MANIFEST_MODE=ON`。

Release：

```powershell
cmake --preset windows-x64-release-msvc
cmake --build --preset windows-x64-release-msvc --target novel-downloader-gui
.\build\release-msvc\bin\Release\novel-downloader-gui.exe
```

静态 Release：

```powershell
cmake --preset windows-x64-release-static-msvc
cmake --build --preset windows-x64-release-static-msvc --target novel-downloader-gui
.\build\release-static-msvc\bin\Release\novel-downloader-gui.exe
```

只构建核心库：

```powershell
cmake --build --preset windows-x64-debug-msvc --target novel-core
```

## GUI 资源同步

GUI 依赖两类运行时资源：

- `src/gui/frontend/` 下的前端静态文件
- `plugins/` 下的 JS 插件与共享脚本

当前 CMake 会在构建 `novel-downloader-gui` 时自动同步这两类资源到目标输出目录：

- `build/<preset>/bin/<config>/gui`
- `build/<preset>/bin/<config>/plugins`

同步逻辑会先清理目标目录，再复制最新资源，用来避免旧文件残留导致的“代码已改但运行仍旧异常”。

## 前端开发

如果前端有单独 dev server：

```powershell
$env:NOVEL_GUI_DEV_SERVER = "http://127.0.0.1:5173"
.\build\debug-msvc\bin\Debug\novel-downloader-gui.exe
```

恢复本地静态资源模式：

```powershell
Remove-Item Env:NOVEL_GUI_DEV_SERVER
```

## 日志与排错

GUI 日志默认写入项目运行目录下的 `novel-gui.log`。

常见排查点：

- 检查插件目录是否被正确解析
- 检查 JS 插件是否成功完成 bootstrap
- 检查 GUI 最终导航到的前端 URL
- 检查输出目录下 `gui/` 与 `plugins/` 是否为最新文件

快速查看：

```powershell
Get-Content .\novel-gui.log -Tail 100
Get-ChildItem .\build\debug-msvc\bin\Debug\gui
Get-ChildItem .\build\debug-msvc\bin\Debug\plugins -Recurse
```

## 相关文档

- [QUICK_CMD.md](QUICK_CMD.md)
- [plugins/README.md](plugins/README.md)
- [AGENTS.md](AGENTS.md)
