# 常用指令速查

## 当前状态

- 旧 GUI/TUI 已删除
- 文档已切换为“C++ 核心 + 后续系统 WebView UI”
- 顶层 CMake 已临时切到核心静态库 `novel-core`

## 依赖

```powershell
vcpkg install
vcpkg list
```

## 基础检查

```powershell
cmake --list-presets
rg --files src plugins
rg -n "WinUI|gui|publish-gui|webview" README.md QUICK_CMD.md CMakeLists.txt vcpkg.json
```

## 配置环境

```powershell
Copy-Item .env.example .env
```

常用环境变量：

```dotenv
FANQIE_APIKEY=your_fanqie_api_key
# QIMAO_APIKEY=your_qimao_api_key

NOVEL_DB=novel.db
NOVEL_EPUB_DIR=exports
NOVEL_PLUGIN_DIR=plugins
NOVEL_SOURCE=fanqie
```

## CMake 预设

- `windows-x64-debug-msvc`
- `windows-x64-release-msvc`
- `windows-x64-release-static-msvc`

配置并构建核心库：

```powershell
cmake --preset windows-x64-debug-msvc
cmake --build --preset windows-x64-debug-msvc
```

构建产物：

- `build\debug-msvc\Debug\novel-core.lib`
- `build\release-msvc\Release\novel-core.lib`
- `build\release-static-msvc\Release\novel-core.lib`

## 清理旧产物

```powershell
Remove-Item -Recurse -Force .\build\debug-msvc -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force .\build\release-msvc -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force .\build\release-static-msvc -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force .\dist -ErrorAction SilentlyContinue
```

## 插件相关

```powershell
Get-ChildItem .\plugins
Get-Content .\plugins\README.md
Get-Content .\plugins\fanqie.lua
Get-Content .\plugins\qimao.lua
```

## 后续迁移时可用

```powershell
rg -n "novel-gui-bridge|publish-gui|WinUI|gui/backend" .
rg -n "SourceManager|LibraryService|DownloadService|ExportService" src
```
