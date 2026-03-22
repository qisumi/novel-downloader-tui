# 常用指令速查

## 依赖管理（vcpkg）

```powershell
# 安装 / 更新所有依赖（首次约需 10–20 分钟）
vcpkg install

# 查看已安装的包
vcpkg list
```

---

## 配置（CMake Configure）

### 组合规则

```text
Clang / GCC  -> 仅支持 TUI
MSVC         -> 支持 TUI 或 GUI
GUI          -> 当前为 WinUI 3 预留占位入口，需使用 VS 工具链
```

```powershell
# Debug 配置（Clang TUI，输出目录：build/debug/）
cmake --preset windows-x64-debug

# Release 配置（Clang TUI，动态依赖，输出目录：build/release/）
cmake --preset windows-x64-release

# Release 配置（Clang TUI，静态链接，优先用于单 exe 发布，输出目录：build/release-static/）
cmake --preset windows-x64-release-static

# Debug 配置（MSVC TUI，输出目录：build/debug-msvc-tui/）
cmake --preset windows-x64-debug-msvc-tui

# Release 配置（MSVC TUI，输出目录：build/release-msvc-tui/）
cmake --preset windows-x64-release-msvc-tui

# Debug 配置（MSVC GUI 占位入口，输出目录：build/debug-msvc-gui/）
cmake --preset windows-x64-debug-msvc-gui

# Release 配置（MSVC GUI 占位入口，输出目录：build/release-msvc-gui/）
cmake --preset windows-x64-release-msvc-gui

# Release 配置（MSVC TUI，静态链接，输出目录：build/release-static-msvc/）
cmake --preset windows-x64-release-static-msvc
```

### 手动指定前端

```powershell
# Clang TUI（手动配置）
cmake -S . -B build\custom-clang-tui -G Ninja `
  -DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake `
  -DCMAKE_C_COMPILER=clang-cl `
  -DCMAKE_CXX_COMPILER=clang-cl `
  -DNOVEL_FRONTEND=TUI

# MSVC GUI（手动配置）
cmake -S . -B build\custom-msvc-gui `
  -G "Visual Studio 18 2026" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake `
  -DNOVEL_FRONTEND=GUI
```

---

## 构建（Build）

```powershell
# Debug 构建
cmake --build --preset windows-x64-debug

# Release 构建（Clang TUI，动态依赖）
cmake --build --preset windows-x64-release

# Release 构建（Clang TUI，静态链接，单 exe 优先）
cmake --build --preset windows-x64-release-static

# Debug 构建（MSVC TUI）
cmake --build --preset windows-x64-debug-msvc-tui

# Release 构建（MSVC TUI）
cmake --build --preset windows-x64-release-msvc-tui

# Debug 构建（MSVC GUI）
cmake --build --preset windows-x64-debug-msvc-gui

# Release 构建（MSVC GUI）
cmake --build --preset windows-x64-release-msvc-gui

# Release 构建（MSVC TUI，静态链接）
cmake --build --preset windows-x64-release-static-msvc

# 仅重新编译更改的文件（增量构建，同上命令）

# 强制全量重新构建
cmake --build --preset windows-x64-debug --clean-first
cmake --build --preset windows-x64-release --clean-first
cmake --build --preset windows-x64-release-static --clean-first
cmake --build --preset windows-x64-debug-msvc-tui --clean-first
cmake --build --preset windows-x64-release-msvc-tui --clean-first
cmake --build --preset windows-x64-debug-msvc-gui --clean-first
cmake --build --preset windows-x64-release-msvc-gui --clean-first
cmake --build --preset windows-x64-release-static-msvc --clean-first
```

---

## 运行

```powershell
# Debug
.\build\debug\novel-downloader-tui.exe

# Release（Clang TUI，动态依赖）
.\build\release\novel-downloader-tui.exe

# Release（Clang TUI，静态链接）
.\build\release-static\novel-downloader-tui.exe

# Debug（MSVC TUI）
.\build\debug-msvc-tui\novel-downloader-tui.exe

# Release（MSVC TUI）
.\build\release-msvc-tui\novel-downloader-tui.exe

# Release（MSVC GUI）
.\build\release-msvc-gui\novel-downloader-tui.exe

# Release（静态链接，MSVC TUI）
.\build\release-static-msvc\novel-downloader-tui.exe

# 查看帮助
.\build\debug\novel-downloader-tui.exe --help
```

### 命令行参数

```powershell
# 指定 API Key
.\build\release\novel-downloader-tui.exe -k <your_api_key>

# 指定数据库路径
.\build\release\novel-downloader-tui.exe --db C:\data\novel.db

# 指定 EPUB/TXT 输出目录
.\build\release\novel-downloader-tui.exe -o C:\books

# 列出当前可用书源
.\build\release\novel-downloader-tui.exe --list-sources

# 指定插件目录和书源
.\build\release\novel-downloader-tui.exe --plugin-dir .\plugins --source fanqie

# 组合使用
.\build\release\novel-downloader-tui.exe -k <key> --db .\data.db -o .\epub_out --source fanqie
```

### 环境变量（替代命令行参数）

```powershell
# $env:FANQIE_APIKEY = "your_api_key"      # 书源插件专属变量
$env:NOVEL_DB       = "C:\data\novel.db" # SQLite 数据库路径
$env:NOVEL_EPUB_DIR = "C:\books"          # EPUB/TXT 输出目录
$env:NOVEL_PLUGIN_DIR = ".\plugins"       # Lua 插件目录
$env:NOVEL_SOURCE     = "fanqie"          # 默认书源 ID

.\build\release\novel-downloader-tui.exe
```

---

## 运行时操作

### 鼠标操作

| 操作 | 功能 |
|------|------|
| 滚轮滚动 | 上下滚动列表 |
| 左键点击 | 选择列表项 / 点击按钮 |
| 左键双击 | 书架页双击进入详情 |

### 全局快捷键

| 按键 | 功能 |
|------|------|
| `q` / `Q` | 退出程序 |
| `Tab` | 切换书架 / 搜索标签 |

### 搜索页

| 按键 | 功能 |
|------|------|
| `Enter`（输入框）| 执行搜索 |
| `↑` / `↓` / `j` / `k` | 选择结果 |
| `Enter`（列表） | 进入书籍详情 |
| `a` | 加入书架 |

### 书架页

| 按键 | 功能 |
|------|------|
| `↑` / `↓` / `j` / `k` | 选择书籍 |
| `Enter` | 进入详情 |
| `r` | 刷新书架 |
| `d` | 从书架删除 |

### 书籍详情 / 目录页

| 按键 | 功能 |
|------|------|
| `↑` / `↓` / `j` / `k` | 浏览章节 |
| `g` | 批量下载章节（按当前范围） |
| `u` | 更新章节（重新从服务器获取目录） |
| `c` | 清除导出范围（恢复全部章节） |
| `e` | 导出 EPUB（按当前范围） |
| `t` | 导出 TXT（按当前范围） |
| `a` | 保存到书架 |
| `Esc` | 返回上一级 |

> **导出范围设置**：通过输入框输入起始和结束章节号，按 `Enter` 应用范围。

---

## 清理

```powershell
# 清理 Debug 构建产物
cmake --build --preset windows-x64-debug --target clean

# 清理 Release 构建产物（动态依赖）
cmake --build --preset windows-x64-release --target clean

# 清理 Release 构建产物（静态链接）
cmake --build --preset windows-x64-release-static --target clean

# 清理 Debug 构建产物（MSVC TUI）
cmake --build --preset windows-x64-debug-msvc-tui --target clean

# 清理 Release 构建产物（MSVC TUI）
cmake --build --preset windows-x64-release-msvc-tui --target clean

# 清理 Debug 构建产物（MSVC GUI）
cmake --build --preset windows-x64-debug-msvc-gui --target clean

# 清理 Release 构建产物（MSVC GUI）
cmake --build --preset windows-x64-release-msvc-gui --target clean

# 清理 Release 构建产物（静态链接，MSVC TUI）
cmake --build --preset windows-x64-release-static-msvc --target clean

# 删除全部构建目录（彻底重置）
Remove-Item -Recurse -Force .\build\debug
Remove-Item -Recurse -Force .\build\release
Remove-Item -Recurse -Force .\build\release-static
Remove-Item -Recurse -Force .\build\debug-msvc-tui
Remove-Item -Recurse -Force .\build\release-msvc-tui
Remove-Item -Recurse -Force .\build\debug-msvc-gui
Remove-Item -Recurse -Force .\build\release-msvc-gui
Remove-Item -Recurse -Force .\build\release-static-msvc
```

---

## 发布

```powershell
# 推荐：静态链接，尽量生成单 exe 发布版
cmake --preset windows-x64-release-static
cmake --build --preset windows-x64-release-static

# 产物：
#   .\build\release-static\novel-downloader-tui.exe
#   .\build\release-static\plugins\
```

```text
GitHub Actions：
- 推送形如 v1.2.3 的 tag 后，会自动构建 windows-static 包并发布 GitHub Release
- CI 发布默认使用 windows-x64-release-static-msvc 预设，走 MSVC TUI 工具链
- 工作流文件：.github/workflows/release-windows-static.yml
```

```powershell
# 备选：动态链接发布
cmake --preset windows-x64-release
cmake --build --preset windows-x64-release

# 构建后会自动把依赖 DLL 复制到 exe 同目录
# 直接打包整个 .\build\release\ 目录即可
```

> `x64-windows-static` 首次构建会由 vcpkg 重新安装一套静态库，耗时通常明显高于普通 Release。

---

## 生成 compile_commands.json（用于 clangd / IDE）

```powershell
# 配置时自动生成（CMakePresets.json 中已启用 CMAKE_EXPORT_COMPILE_COMMANDS）
cmake --preset windows-x64-debug

# 生成后位于：
#   build/debug/compile_commands.json
```

---

## 一键流程（首次设置）

```powershell
# 1. 安装依赖
vcpkg install

# 2. 配置 + 构建（Clang TUI Debug）
cmake --preset windows-x64-debug
cmake --build --preset windows-x64-debug

# 3. 运行
.\build\debug\novel-downloader-tui.exe
```
