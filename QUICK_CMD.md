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

```powershell
# Debug 配置（输出目录：build/debug/）
cmake --preset windows-x64-debug

# Release 配置（输出目录：build/release/）
cmake --preset windows-x64-release
```

---

## 构建（Build）

```powershell
# Debug 构建
cmake --build --preset windows-x64-debug

# Release 构建
cmake --build --preset windows-x64-release

# 仅重新编译更改的文件（增量构建，同上命令）

# 强制全量重新构建
cmake --build --preset windows-x64-debug --clean-first
cmake --build --preset windows-x64-release --clean-first
```

---

## 运行

```powershell
# Debug
.\build\debug\fanqie-downloader-tui.exe

# Release
.\build\release\fanqie-downloader-tui.exe

# 查看帮助
.\build\debug\fanqie-downloader-tui.exe --help
```

### 命令行参数

```powershell
# 指定 API Key
.\build\release\fanqie-downloader-tui.exe -k <your_api_key>

# 指定数据库路径
.\build\release\fanqie-downloader-tui.exe --db C:\data\fanqie.db

# 指定 EPUB/TXT 输出目录
.\build\release\fanqie-downloader-tui.exe -o C:\books

# 组合使用
.\build\release\fanqie-downloader-tui.exe -k <key> --db .\data.db -o .\epub_out
```

### 环境变量（替代命令行参数）

```powershell
$env:FANQIE_APIKEY   = "your_api_key"      # API 密钥
$env:FANQIE_DB       = "C:\data\fanqie.db" # SQLite 数据库路径
$env:FANQIE_EPUB_DIR = "C:\books"          # EPUB/TXT 输出目录

.\build\release\fanqie-downloader-tui.exe
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

# 清理 Release 构建产物
cmake --build --preset windows-x64-release --target clean

# 删除全部构建目录（彻底重置）
Remove-Item -Recurse -Force .\build\debug
Remove-Item -Recurse -Force .\build\release
```

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

# 2. 配置 + 构建（Debug）
cmake --preset windows-x64-debug
cmake --build --preset windows-x64-debug

# 3. 运行
.\build\debug\fanqie-downloader-tui.exe
```
