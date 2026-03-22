# 常用指令速查

## 依赖

```powershell
vcpkg install
vcpkg list
```

## CMake 预设

项目现在只保留 MSVC 预设：

- `windows-x64-debug-msvc-tui`
- `windows-x64-release-msvc-tui`
- `windows-x64-release-static-msvc`

GUI 不通过根 CMake 直接编译，而是：

- CMake 负责编译 `novel-gui-bridge.dll`
- `dotnet` 负责编译 `src/gui/NovelDownloaderGui`

## TUI 配置与构建

```powershell
cmake --preset windows-x64-debug-msvc-tui
cmake --build --preset windows-x64-debug-msvc-tui

cmake --preset windows-x64-release-msvc-tui
cmake --build --preset windows-x64-release-msvc-tui

cmake --preset windows-x64-release-static-msvc
cmake --build --preset windows-x64-release-static-msvc
```

## 单独构建 GUI bridge

```powershell
cmake --preset windows-x64-debug-msvc-tui
cmake --build --preset windows-x64-debug-msvc-tui --target novel-gui-bridge

cmake --preset windows-x64-release-static-msvc
cmake --build .\build\release-static-msvc --target novel-gui-bridge --config Release
```

## 运行 TUI

```powershell
.\build\debug-msvc-tui\Debug\novel-downloader-tui.exe
.\build\release-msvc-tui\Release\novel-downloader-tui.exe
.\build\release-static-msvc\Release\novel-downloader-tui.exe
```

## 构建与运行 GUI

```powershell
dotnet build .\src\gui\NovelDownloaderGui\NovelDownloaderGui.csproj -p:Platform=x64
dotnet run --project .\src\gui\NovelDownloaderGui\NovelDownloaderGui.csproj -p:Platform=x64
```

直接运行已构建 GUI：

```powershell
.\src\gui\NovelDownloaderGui\bin\x64\Debug\net8.0-windows10.0.19041.0\win-x64\NovelDownloaderGui.exe
```

## 一键打包 GUI

```powershell
.\scripts\publish-gui.ps1
.\scripts\publish-gui.ps1 -CleanPackageDir
.\scripts\publish-gui.ps1 -CleanPackageDir -SkipZip
.\scripts\publish-gui.ps1 -SkipBackendBuild
```

打包结果默认位于：

```text
dist\gui-win-x64\
dist\gui-win-x64.zip
```

目录内默认包含：

- `NovelDownloaderGui.exe`
- `novel-gui-bridge.dll`
- `plugins\`
- `.env` 或 `.env.example`

## 常用命令行参数

```powershell
.\build\release-msvc-tui\Release\novel-downloader-tui.exe --help
.\build\release-msvc-tui\Release\novel-downloader-tui.exe --list-sources
.\build\release-msvc-tui\Release\novel-downloader-tui.exe --plugin-dir .\plugins --source fanqie
.\build\release-msvc-tui\Release\novel-downloader-tui.exe --db .\novel.db -o .\books
```

## 清理

```powershell
cmake --build --preset windows-x64-debug-msvc-tui --target clean
cmake --build --preset windows-x64-release-msvc-tui --target clean
cmake --build --preset windows-x64-release-static-msvc --target clean

Remove-Item -Recurse -Force .\src\gui\NovelDownloaderGui\bin -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force .\src\gui\NovelDownloaderGui\obj -ErrorAction SilentlyContinue

Remove-Item -Recurse -Force .\build\debug-msvc-tui -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force .\build\release-msvc-tui -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force .\build\release-static-msvc -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force .\dist\gui-win-x64 -ErrorAction SilentlyContinue
Remove-Item -Force .\dist\gui-win-x64.zip -ErrorAction SilentlyContinue
```

## 首次拉起 GUI

```powershell
vcpkg install

cmake --preset windows-x64-debug-msvc-tui
cmake --build --preset windows-x64-debug-msvc-tui --target novel-gui-bridge

dotnet build .\src\gui\NovelDownloaderGui\NovelDownloaderGui.csproj -p:Platform=x64
dotnet run --project .\src\gui\NovelDownloaderGui\NovelDownloaderGui.csproj -p:Platform=x64
```
