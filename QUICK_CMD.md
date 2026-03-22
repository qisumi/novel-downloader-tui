# 常用指令速查

## 依赖

```powershell
vcpkg install
vcpkg list
```

## CMake 预设

- `windows-x64-debug-msvc`
- `windows-x64-release-msvc`
- `windows-x64-release-static-msvc`

根 CMake 只负责编译 `novel-gui-bridge.dll`。

## 构建原生 GUI bridge

```powershell
cmake --preset windows-x64-debug-msvc
cmake --build --preset windows-x64-debug-msvc --target novel-gui-bridge

cmake --preset windows-x64-release-msvc
cmake --build --preset windows-x64-release-msvc --target novel-gui-bridge

cmake --preset windows-x64-release-static-msvc
cmake --build --preset windows-x64-release-static-msvc --target novel-gui-bridge
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

## 清理

```powershell
cmake --build --preset windows-x64-debug-msvc --target clean
cmake --build --preset windows-x64-release-msvc --target clean
cmake --build --preset windows-x64-release-static-msvc --target clean

Remove-Item -Recurse -Force .\src\gui\NovelDownloaderGui\bin -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force .\src\gui\NovelDownloaderGui\obj -ErrorAction SilentlyContinue

Remove-Item -Recurse -Force .\build\debug-msvc -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force .\build\release-msvc -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force .\build\release-static-msvc -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force .\dist\gui-win-x64 -ErrorAction SilentlyContinue
Remove-Item -Force .\dist\gui-win-x64.zip -ErrorAction SilentlyContinue
```

## 首次拉起 GUI

```powershell
vcpkg install

cmake --preset windows-x64-debug-msvc
cmake --build --preset windows-x64-debug-msvc --target novel-gui-bridge

dotnet build .\src\gui\NovelDownloaderGui\NovelDownloaderGui.csproj -p:Platform=x64
dotnet run --project .\src\gui\NovelDownloaderGui\NovelDownloaderGui.csproj -p:Platform=x64
```
