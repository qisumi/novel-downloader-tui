# 常用指令速查

## 先做一次

```powershell
Copy-Item .env.example .env
vcpkg install
cmake --list-presets
```

常用环境变量：

```dotenv
FANQIE_APIKEY=your_fanqie_api_key
# QIMAO_APIKEY=your_qimao_api_key

NOVEL_DB=novel.db
NOVEL_EPUB_DIR=exports
NOVEL_PLUGIN_DIR=plugins
NOVEL_SOURCE=fanqie
NOVEL_GUI_DEV_SERVER=
```

## 最常用：Debug 构建并运行 GUI

```powershell
.\run.ps1 --build-msvc-debug

cmake --preset windows-x64-debug-msvc
cmake --build --preset windows-x64-debug-msvc --target novel-downloader-gui
.\build\debug-msvc\bin\Debug\novel-downloader-gui.exe
```

## Release 构建并运行 GUI

```powershell
.\run.ps1 --build-release

cmake --preset windows-x64-release-msvc
cmake --build --preset windows-x64-release-msvc --target novel-downloader-gui
.\build\release-msvc\bin\Release\novel-downloader-gui.exe
```

## 静态 Release 构建并运行 GUI

```powershell
cmake --preset windows-x64-release-static-msvc
cmake --build --preset windows-x64-release-static-msvc --target novel-downloader-gui
.\build\release-static-msvc\bin\Release\novel-downloader-gui.exe
```

## 一键脚本入口

```powershell
.\run.ps1 --build-msvc-debug
.\run.ps1 --build-release
.\run.ps1 --build-dist
.\run.ps1 --update-clangd
.\run.ps1 --help
```

说明：

- `--build-msvc-debug`：配置并构建 Debug GUI
- `--build-release`：配置并构建 Release GUI
- `--build-dist`：配置并构建静态 Release GUI，并安装到 `dist\`
- `--update-clangd`：重新生成并同步 `compile_commands.json`

## 只构建核心库

```powershell
cmake --build --preset windows-x64-debug-msvc --target novel-core
```

## 资源同步验证

GUI 构建会自动同步前端与插件资源到输出目录：

```powershell
Get-ChildItem .\build\debug-msvc\bin\Debug\gui
Get-ChildItem .\build\debug-msvc\bin\Debug\plugins

Get-ChildItem .\build\release-msvc\bin\Release\gui
Get-ChildItem .\build\release-msvc\bin\Release\plugins
```

如果只改了 `src\gui\frontend\` 或 `plugins\`，直接重新执行对应 `cmake --build --preset ... --target novel-downloader-gui` 即可。

## 前端开发期接入 dev server

```powershell
$env:NOVEL_GUI_DEV_SERVER = "http://127.0.0.1:5173"
.\build\debug-msvc\bin\Debug\novel-downloader-gui.exe
```

清掉这个变量：

```powershell
Remove-Item Env:NOVEL_GUI_DEV_SERVER
```

## 查看日志

```powershell
Get-Content .\novel-gui.log -Tail 100
```

如果 GUI 从输出目录启动，也可以直接看：

```powershell
Get-Content .\build\debug-msvc\bin\Debug\novel-gui.log -Tail 100
Get-Content .\build\release-static-msvc\bin\Release\novel-gui.log -Tail 100
```

## 刷新 clangd 的编译数据库

```powershell
.\run.ps1 --update-clangd

cmd /c "C:\PROGRA~1\MICROS~3\18\COMMUN~1\Common7\Tools\VsDevCmd.bat -arch=x64 -host_arch=x64 && cmake --fresh --preset windows-x64-debug-clangd && cmake --build --preset windows-x64-debug-clangd --target novel-sync-compile-commands"
```

生成后检查：

```powershell
Get-Item .\compile_commands.json | Format-List FullName,LastWriteTime,Length
Get-Item .\build\debug-clangd\compile_commands.json | Format-List FullName,LastWriteTime,Length
```

如果编辑器仍然报旧错误，通常是根目录 `compile_commands.json` 没有刷新，或者 clangd 还缓存着旧结果。可以先重新执行上面的命令，再重载编辑器窗口。

## 快速排错

列出预设：

```powershell
cmake --list-presets
```

重新配置某个构建目录：

```powershell
cmake --fresh --preset windows-x64-debug-msvc
```

检查 GUI 是否能正常启动：

```powershell
$p = Start-Process -FilePath ".\build\debug-msvc\bin\Debug\novel-downloader-gui.exe" -PassThru
Start-Sleep -Seconds 3
if ($p.HasExited) { $p.ExitCode } else { Stop-Process -Id $p.Id }
```

检查前端资源导航和插件加载：

```powershell
rg -n "GUI navigating|Plugin load start|Loaded source" .\novel-gui.log
```

## 清理缓存和旧产物

```powershell
Remove-Item -Recurse -Force .\build\debug-msvc -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force .\build\release-msvc -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force .\build\release-static-msvc -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force .\build\debug-clangd -ErrorAction SilentlyContinue
Remove-Item -Force .\compile_commands.json -ErrorAction SilentlyContinue
```

## 常用源码定位

```powershell
rg --files src plugins cmake
rg -n "novel-downloader-gui|sync_resources|NOVEL_GUI_DEV_SERVER" CMakeLists.txt src\gui QUICK_CMD.md README.md AGENTS.md
rg -n "SourceManager|LibraryService|DownloadService|ExportService" src
```
