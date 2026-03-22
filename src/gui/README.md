# WinUI 3 GUI

这里是项目的 WinUI 3 图形界面目录。

## 目录说明

- `backend/`：C++ native bridge，导出 `novel-gui-bridge.dll`
- `NovelDownloaderGui/`：WinUI 3 C# GUI

## 开发顺序

先构建 bridge：

```powershell
cmake --preset windows-x64-debug-msvc-tui
cmake --build --preset windows-x64-debug-msvc-tui --target novel-gui-bridge
```

再构建并运行 GUI：

```powershell
dotnet build .\src\gui\NovelDownloaderGui\NovelDownloaderGui.csproj -p:Platform=x64
dotnet run --project .\src\gui\NovelDownloaderGui\NovelDownloaderGui.csproj -p:Platform=x64
```

## 发布

```powershell
.\scripts\publish-gui.ps1 -CleanPackageDir
```

默认产物：

- `NovelDownloaderGui.exe`
- `novel-gui-bridge.dll`
- `plugins\`
- `.env` 或 `.env.example`

当前仓库只保留 MSVC 相关 CMake preset，不再维护 clang preset。
