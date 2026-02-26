# 番茄小说 TUI 下载器

基于 **番茄小说 API** 的终端界面（TUI）小说下载与 EPUB/TXT 导出工具，  
使用 C++20 编写。

## 功能一览

| 功能 | 说明 |
|------|------|
| 搜索 | 关键词搜索书名 / 作者 |
| 书架 | 本地收藏管理 |
| 目录 | 查看完整章节列表，显示本地缓存状态 |
| 下载 | 批量下载全书章节并缓存到 SQLite |
| EPUB/TXT | 支持 EPUB 或 TXT 导出，支持按章节范围导出 |

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
fanqie-downloader-tui/
├── CMakeLists.txt
├── vcpkg.json                  # VCPKG 清单
├── reference/
│   └── fanqie.json             # API 参考（阅读 3 书源）
└── src/
    ├── main.cpp                # 程序入口
    ├── models/
    │   └── book.h              # Book / TocItem / Chapter 数据模型
    ├── api/
    │   ├── fanqie_client.h     # 番茄小说 API 客户端接口
    │   └── fanqie_client.cpp
    ├── db/
    │   ├── database.h          # SQLite 持久化层接口
    │   └── database.cpp
    ├── export/
    │   ├── epub_exporter.h     # EPUB 导出接口
    │   ├── epub_exporter.cpp
    │   ├── txt_exporter.h      # TXT 导出接口
    │   └── txt_exporter.cpp
    └── ui/
        ├── app.h               # 应用上下文 & run_app()
        ├── app.cpp             # 主循环、标签栏
        ├── screens/
        │   ├── search_screen.*       # 搜索页
        │   ├── bookshelf_screen.*    # 书架页
        │   ├── book_detail_screen.*  # 书籍详情 + 目录
        └── components/
            └── loading_indicator.*   # 旋转加载动画
```

## 构建

### 前置条件

- CMake ≥ 3.20
- Visual Studio 2022 （或带 MSVC / clang-cl）
- [VCPKG](https://github.com/microsoft/vcpkg) 并设置 `VCPKG_ROOT` 环境变量

```powershell
# 1. 安装依赖（首次约需 10-20 分钟编译）
vcpkg install

# 2. 配置（x64-windows triplet）
cmake -B build -S . `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows `
  -DCMAKE_BUILD_TYPE=Release

# 3. 编译
cmake --build build --config Release

# 4. 运行
.\build\Release\fanqie-downloader-tui.exe
```

## 键盘快捷键

### 全局
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
| `↑` / `↓` | 选择书籍 |
| `Enter` | 进入详情 |
| `r` | 刷新书架 |
| `d` | 从书架删除 |

### 书籍详情 / 目录
| 按键 | 功能 |
|------|------|
| `↑` / `↓` | 浏览章节 |
| `g` | 批量下载全部章节 |
| `[` | 设置导出起点（当前章节） |
| `]` | 设置导出终点（当前章节） |
| `c` | 清除导出范围（恢复全部章节） |
| `e` | 导出 EPUB（按当前范围） |
| `t` | 导出 TXT（按当前范围） |
| `a` | 保存到书架 |
| `Esc` | 返回 |

> 章节范围规则：
> - 未设置起点/终点：导出全部章节
> - 仅设置起点：从起点导出到最后一章
> - 仅设置终点：从第一章导出到终点
> - 同时设置起点和终点：导出两者之间的章节（自动处理先后顺序）

## 环境变量

| 变量 | 说明 | 默认值 |
|------|------|--------|
| `FANQIE_APIKEY` | API 密钥 | 内置默认 Key |
| `FANQIE_DB` | SQLite 数据库路径 | `fanqie.db` |
| `FANQIE_EPUB_DIR` | EPUB/TXT 输出目录 | 当前目录 |

## 数据库 Schema 概览

```
books              — 书架  
toc                — 章节目录缓存  
chapters           — 章节正文缓存  
```

## 免责声明

本项目仅用于学习 C++ / TUI / SQLite 技术，请勿用于商业或违法用途。
