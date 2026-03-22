# 小说下载工具

基于多书源 Lua 插件的小说下载与导出工具，核心使用 C++20 编写，当前前端为 WinUI 3 GUI。

- WinUI 3 图形界面
- SQLite 缓存
- EPUB / TXT 导出
- Lua 插件书源

WinUI 3 GUI 位于 `src/gui/NovelDownloaderGui`，通过 `novel-gui-bridge.dll` 复用现有 C++ 核心能力。

## 功能一览

| 功能 | 说明 |
|------|------|
| 搜索 | 按书名或作者搜索 |
| 书架 | 本地收藏与管理 |
| 目录 | 查看章节列表与缓存状态 |
| 下载 | 批量下载章节到 SQLite |
| 导出 | 导出 EPUB 或 TXT，支持章节范围 |
| 插件书源 | 从 `plugins/` 加载 Lua 书源 |
| WinUI 3 GUI | 搜索、书架、设置页 |

## 目录结构

```text
novel-downloader/
├── CMakeLists.txt
├── CMakePresets.json
├── README.md
├── QUICK_CMD.md
├── scripts/
│   └── publish-gui.ps1
├── plugins/
├── src/
│   ├── application/
│   ├── db/
│   ├── export/
│   ├── models/
│   ├── source/
│   └── gui/
│       ├── backend/            # C++ native bridge
│       └── NovelDownloaderGui/ # WinUI 3 C# GUI
└── vcpkg.json
```

## 构建要求

- Windows
- Visual Studio / MSVC
- CMake 3.21+
- .NET 8 SDK
- Windows App SDK / WinUI 3 开发环境
- `VCPKG_ROOT` 已配置

项目根 CMake 现在只负责构建原生 GUI bridge。

## 构建原生 bridge

```powershell
vcpkg install

cmake --preset windows-x64-debug-msvc
cmake --build --preset windows-x64-debug-msvc --target novel-gui-bridge
```

Release 版本：

```powershell
cmake --preset windows-x64-release-msvc
cmake --build --preset windows-x64-release-msvc --target novel-gui-bridge
```

静态链接 Release：

```powershell
cmake --preset windows-x64-release-static-msvc
cmake --build --preset windows-x64-release-static-msvc --target novel-gui-bridge
```

## WinUI 3 GUI 开发

本地调试顺序：

```powershell
cmake --preset windows-x64-debug-msvc
cmake --build --preset windows-x64-debug-msvc --target novel-gui-bridge

dotnet build .\src\gui\NovelDownloaderGui\NovelDownloaderGui.csproj -p:Platform=x64
dotnet run --project .\src\gui\NovelDownloaderGui\NovelDownloaderGui.csproj -p:Platform=x64
```

GUI 会优先从应用目录查找 `novel-gui-bridge.dll`，找不到时再回退到仓库 `build\` 目录。

## GUI 发布

推荐发布形态：

- `NovelDownloaderGui.exe`
- `novel-gui-bridge.dll`
- `plugins\`
- `.env`

一键打包：

```powershell
.\scripts\publish-gui.ps1 -CleanPackageDir
```

脚本会：

1. 发布单文件 WinUI 3 GUI
2. 用 `windows-x64-release-static-msvc` 构建静态依赖版 bridge
3. 组装 `dist\gui-win-x64\`
4. 自动生成 `dist\gui-win-x64.zip`

## 配置

配置优先级：

1. 系统环境变量
2. 项目根目录 `.env`

首次使用可复制：

```powershell
Copy-Item .env.example .env
```

示例：

```dotenv
NOVEL_DB=novel.db
NOVEL_EPUB_DIR=.
NOVEL_PLUGIN_DIR=plugins
NOVEL_SOURCE=fanqie
```

## GUI 设置建议

- `RepositoryRoot` 指向仓库根目录
- `PluginDirectory` 通常填 `plugins`
- `DatabasePath` 可复用 `novel.db`
- `OutputDirectory` 为 EPUB / TXT 导出目录

## 相关文档

- [QUICK_CMD.md](QUICK_CMD.md)
- [plugins/README.md](plugins/README.md)
