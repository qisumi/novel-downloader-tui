# 小说下载 TUI 工具

基于 **多书源插件** 的终端界面（TUI）小说下载与 EPUB/TXT 导出工具，  
使用 C++20 编写。

当前终端界面实现位于 `src/tui/`，并预留 `src/gui/` 作为后续 Windows 平台 WinUI 3 图形界面迁移入口。

当前 CMake 约束如下：

- `Clang` / `GNU` 工具链仅支持 `TUI`
- `MSVC` 工具链支持 `TUI` 或 `GUI`
- `GUI` 目前是可编译的占位入口，后续可直接替换为 WinUI 3 工程

支持通过命令行参数、系统环境变量或项目根目录下的 `.env` 文件配置 API Key、数据库路径、导出目录和书源插件目录。

## 功能一览

| 功能 | 说明 |
|------|------|
| 搜索 | 关键词搜索书名 / 作者 |
| 书架 | 本地收藏管理，显示章节总数和缓存状态 |
| 目录 | 查看完整章节列表，显示本地缓存状态 |
| 下载 | 批量下载全书章节并缓存到 SQLite |
| EPUB/TXT | 支持 EPUB 或 TXT 导出，支持按章节范围导出 |
| 鼠标支持 | 滚轮滚动、左键点击选择、按钮点击操作 |
| 插件书源 | 从程序同级 `plugins/` 目录加载 Lua 书源 |

## 技术栈

| 组件 | 用途 |
|------|------|
| [FTXUI](https://github.com/ArthurSonzogni/FTXUI) | TUI 渲染与交互 |
| [cpp-httplib](https://github.com/yhirose/cpp-httplib) | HTTP/HTTPS 网络请求 |
| [nlohmann/json](https://github.com/nlohmann/json) | JSON 解析 |
| [SQLiteCpp](https://github.com/SRombauts/SQLiteCpp) | SQLite C++ 封装 |
| [tinyxml2](https://github.com/leethomason/tinyxml2) | EPUB XML 文件生成 |
| [libzip](https://libzip.org/) | EPUB ZIP 打包 |
| [OpenSSL](https://www.openssl.org/) | HTTPS 支持 |
| [Lua](https://www.lua.org/) | 脚本支持 |
| [LuaBridge3](https://github.com/kunitoki/LuaBridge3) | Lua/C++ 绑定 |
| [VCPKG](https://vcpkg.io/) | 依赖管理 |

## 目录结构

```
novel-downloader-tui/
├── CMakeLists.txt
├── vcpkg.json                  # VCPKG 清单
├── plugins/                    # Lua 书源插件目录
│   ├── fanqie.lua              # 默认番茄书源
│   └── README.md
├── reference/
│   └── fanqie.json             # API 参考（阅读 3 书源）
└── src/
    ├── main.cpp                # 程序入口
    ├── models/
    │   └── book.h              # Book / TocItem / Chapter 数据模型
    ├── db/
    │   ├── database.h          # SQLite 持久化层接口
    │   └── database.cpp
    ├── source/
    │   ├── domain/             # 书源接口与领域类型
    │   ├── host/               # Lua 插件可调用的宿主 API
    │   ├── lua/                # Lua 运行时与插件适配层
    │   └── runtime/            # 书源加载与选择
    ├── application/
    │   ├── library_service.*   # 搜索 / 书架 / 目录
    │   ├── download_service.*  # 下载与缓存
    │   └── export_service.*    # 导出协调
    ├── export/
    │   ├── epub_exporter.h     # EPUB 导出接口
    │   ├── epub_exporter.cpp
    │   ├── txt_exporter.h      # TXT 导出接口
    │   └── txt_exporter.cpp
    ├── tui/
    │   ├── app.h               # 应用上下文 & run_app()
    │   ├── app.cpp             # 主循环、标签栏
    │   ├── screens/
    │   │   ├── search_screen.*       # 搜索页
    │   │   ├── bookshelf_screen.*    # 书架页
    │   │   └── book_detail_screen.*  # 书籍详情 + 目录
    │   └── components/
    │       └── loading_indicator.*   # 旋转加载动画
    └── gui/                     # 预留给 WinUI 3 迁移
```

## 构建

### 前置条件

- CMake ≥ 3.21
- Visual Studio 2022 （或带 MSVC / clang-cl）
- [VCPKG](https://github.com/microsoft/vcpkg) 并设置 `VCPKG_ROOT` 环境变量

```powershell
# 1. 安装依赖（首次约需 10-20 分钟编译）
vcpkg install

# 2. 配置 Debug（Clang TUI）
cmake --preset windows-x64-debug

# 3. 编译
cmake --build --preset windows-x64-debug

# 4. 运行
.\build\debug\novel-downloader-tui.exe
```

### Release 构建

```powershell
# 动态依赖版（Clang TUI，构建后自动复制依赖 DLL 到输出目录）
cmake --preset windows-x64-release
cmake --build --preset windows-x64-release

# 静态链接版（Clang TUI，优先用于单 exe 发布）
cmake --preset windows-x64-release-static
cmake --build --preset windows-x64-release-static

# MSVC TUI
cmake --preset windows-x64-release-msvc-tui
cmake --build --preset windows-x64-release-msvc-tui

# MSVC GUI（当前为 WinUI 3 预留占位入口）
cmake --preset windows-x64-release-msvc-gui
cmake --build --preset windows-x64-release-msvc-gui
```

静态版产物位于 `build/release-static/`，更适合直接分发。  
动态版产物位于 `build/release/`，构建后会自动把依赖 DLL 复制到 exe 同目录，发布时打包整个目录即可。

> `x64-windows-static` 首次构建通常较慢，因为 vcpkg 需要额外安装一套静态库。

### 手动切换前端

除了 preset 之外，也可以直接在 configure 时切换：

```powershell
cmake -S . -B build/custom-clang-tui -G Ninja `
  -DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake `
  -DCMAKE_C_COMPILER=clang-cl `
  -DCMAKE_CXX_COMPILER=clang-cl `
  -DNOVEL_FRONTEND=TUI

cmake -S . -B build/custom-msvc-gui `
  -G "Visual Studio 18 2026" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake `
  -DNOVEL_FRONTEND=GUI
```

如果使用 `g++` / `gcc`，同样只允许 `-DNOVEL_FRONTEND=TUI`。

## 配置

### 配置优先级

程序按以下顺序读取配置，越靠前优先级越高：

1. 命令行参数
2. 系统环境变量
3. 项目根目录下的 `.env` 文件

### 使用 `.env.example`

仓库提供了示例文件 [.env.example](.env.example)。首次使用可以直接复制一份：

```powershell
Copy-Item .env.example .env
```

然后编辑 `.env`：

```dotenv
NOVEL_DB=novel.db
NOVEL_EPUB_DIR=.
NOVEL_PLUGIN_DIR=plugins
NOVEL_SOURCE=fanqie
```

说明：

- `NOVEL_DB`：SQLite 数据库文件路径
- `NOVEL_EPUB_DIR`：EPUB/TXT 导出目录
- `NOVEL_PLUGIN_DIR`：Lua 书源插件目录
- `NOVEL_SOURCE`：默认书源 ID
- 如果当前选中的书源还需要额外配置，请按该插件的说明补充对应的环境变量，例如 `FANQIE_APIKEY`

如果同时设置了 `.env`、系统环境变量和命令行参数，最终以命令行参数为准。

### 命令行示例

```powershell
.\build\release\novel-downloader-tui.exe
.\build\release\novel-downloader-tui.exe --db .\novel.db -o .\books --plugin-dir .\plugins
.\build\release-static\novel-downloader-tui.exe --db .\novel.db -o .\books --source fanqie
```

## 操作方式

### 鼠标操作
| 操作 | 功能 |
|------|------|
| 滚轮滚动 | 上下滚动列表 |
| 左键点击 | 选择列表项 / 点击按钮 |
| 左键双击 | 书架页双击进入详情 |

### 键盘快捷键

#### 全局
| 按键 | 功能 |
|------|------|
| `q` / `Q` | 退出程序 |
| `Tab` | 切换书架 / 搜索标签 |

#### 搜索页
| 按键 | 功能 |
|------|------|
| `Enter`（输入框）| 执行搜索 |
| `↑` / `↓` / `j` / `k` | 选择结果 |
| `Enter`（列表） | 进入书籍详情 |
| `a` | 加入书架 |

#### 书架页
| 按键 | 功能 |
|------|------|
| `↑` / `↓` / `j` / `k` | 选择书籍 |
| `Enter` | 进入详情 |
| `r` | 刷新书架 |
| `d` | 从书架删除 |

#### 书籍详情 / 目录
| 按键 | 功能 |
|------|------|
| `↑` / `↓` / `j` / `k` | 浏览章节 |
| `g` | 批量下载章节（按当前范围） |
| `u` | 更新章节（重新从服务器获取目录） |
| `c` | 清除导出范围（恢复全部章节） |
| `e` | 导出 EPUB（按当前范围） |
| `t` | 导出 TXT（按当前范围） |
| `a` | 保存到书架 |
| `q` | 关闭应用 |
| `Esc` | 返回上一级 |

> **导出范围设置**：通过输入框输入起始和结束章节号，按 `Enter` 应用范围。
> - 输入框留空表示使用默认值（起点=1，终点=最后一章）
> - 系统会自动校正超出范围的值

## 环境变量

| 变量 | 说明 | 默认值 |
|------|------|--------|
| `NOVEL_DB` | SQLite 数据库路径 | `novel.db` |
| `NOVEL_EPUB_DIR` | EPUB/TXT 输出目录 | 当前目录 |
| `NOVEL_PLUGIN_DIR` | Lua 插件目录 | `plugins` |
| `NOVEL_SOURCE` | 默认书源 ID | 首个成功加载的书源 |

## 数据库 Schema 概览

```
books              — 书架  
toc                — 章节目录缓存  
chapters           — 章节正文缓存  
```

## 免责声明

本项目仅用于学习 C++ / TUI / SQLite 技术，请勿用于商业或违法用途。
