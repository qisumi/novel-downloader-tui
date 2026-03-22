param(
    [string]$BackendPreset = "windows-x64-release-static-msvc",
    [string]$GuiProject = "src/gui/NovelDownloaderGui/NovelDownloaderGui.csproj",
    [string]$PublishProfile = "single-file-win-x64",
    [string]$NuGetSource = "https://api.nuget.org/v3/index.json",
    [string]$PackageDir = "dist/gui-win-x64",
    [string]$BackendConfiguration = "Release",
    [switch]$CleanPackageDir,
    [switch]$SkipZip,
    [switch]$SkipBackendBuild
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$guiProjectPath = Join-Path $repoRoot $GuiProject
$packageDirPath = Join-Path $repoRoot $PackageDir
$packageZipPath = "$packageDirPath.zip"

function Reset-Directory([string]$Path) {
    Get-Process NovelDownloaderGui -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue

    if (Test-Path $Path) {
        Remove-Item -Recurse -Force $Path
    }

    New-Item -ItemType Directory -Path $Path | Out-Null
}

function Resolve-BuildDirectory([string]$Preset) {
    switch ($Preset) {
        "windows-x64-debug-msvc-tui" { return (Join-Path $repoRoot "build/debug-msvc-tui") }
        "windows-x64-release-msvc-tui" { return (Join-Path $repoRoot "build/release-msvc-tui") }
        "windows-x64-release-static-msvc" { return (Join-Path $repoRoot "build/release-static-msvc") }
        default { return $null }
    }
}

Write-Host "[1/3] Publishing WinUI GUI as a single-file exe..." -ForegroundColor Cyan
dotnet publish $guiProjectPath -p:PublishProfile=$PublishProfile --source $NuGetSource

if (-not $SkipBackendBuild) {
    Write-Host "[2/3] Building native GUI bridge..." -ForegroundColor Cyan
    cmake --preset $BackendPreset
    $buildDir = Resolve-BuildDirectory $BackendPreset
    if ([string]::IsNullOrWhiteSpace($buildDir)) {
        throw "Unsupported build preset for scripted bridge packaging: $BackendPreset"
    }

    cmake --build $buildDir --target novel-gui-bridge --config $BackendConfiguration
} else {
    Write-Host "[2/3] Skipping native bridge build." -ForegroundColor Yellow
}

$guiPublishDir = Join-Path $repoRoot "src/gui/NovelDownloaderGui/bin/Release/net8.0-windows10.0.19041.0/win-x64/publish-single-file"
$guiExePath = Join-Path $guiPublishDir "NovelDownloaderGui.exe"
$bridgeCandidates = Get-ChildItem -Path (Join-Path $repoRoot "build") -Recurse -Filter "novel-gui-bridge.dll" -ErrorAction SilentlyContinue |
    Sort-Object LastWriteTimeUtc -Descending
$bridgePath = $bridgeCandidates | Select-Object -First 1 -ExpandProperty FullName
$pluginsPath = Join-Path $repoRoot "plugins"
$envPath = Join-Path $repoRoot ".env"
if (-not (Test-Path $envPath)) {
    $envPath = Join-Path $repoRoot ".env.example"
}

if (-not (Test-Path $guiExePath)) {
    throw "GUI publish output not found: $guiExePath"
}

if ([string]::IsNullOrWhiteSpace($bridgePath) -or -not (Test-Path $bridgePath)) {
    throw "Bridge library not found. Expected a built novel-gui-bridge.dll."
}

if (-not (Test-Path $pluginsPath)) {
    throw "Plugins directory not found: $pluginsPath"
}

Write-Host "[3/3] Assembling distributable directory..." -ForegroundColor Cyan
if ($CleanPackageDir) {
    Reset-Directory $packageDirPath
} elseif (-not (Test-Path $packageDirPath)) {
    New-Item -ItemType Directory -Path $packageDirPath | Out-Null
}

Copy-Item $guiExePath (Join-Path $packageDirPath "NovelDownloaderGui.exe") -Force
$nativeOutputDir = Split-Path -Parent $bridgePath
Copy-Item $bridgePath (Join-Path $packageDirPath "novel-gui-bridge.dll") -Force

$packagePluginsPath = Join-Path $packageDirPath "plugins"
if (Test-Path $packagePluginsPath) {
    Remove-Item -Recurse -Force $packagePluginsPath
}
Copy-Item $pluginsPath $packagePluginsPath -Recurse -Force

if (Test-Path $envPath) {
    Copy-Item $envPath (Join-Path $packageDirPath (Split-Path $envPath -Leaf)) -Force
}

if (-not $SkipZip) {
    if (Test-Path $packageZipPath) {
        Remove-Item -Force $packageZipPath
    }

    Compress-Archive -Path (Join-Path $packageDirPath "*") -DestinationPath $packageZipPath
}

Write-Host "[done] Package output ready." -ForegroundColor Green
Write-Host "Package directory: $packageDirPath"
Write-Host "Included:"
Write-Host "  - NovelDownloaderGui.exe"
Write-Host "  - novel-gui-bridge.dll"
Write-Host "  - plugins/"
if (Test-Path $envPath) {
    Write-Host ("  - {0}" -f (Split-Path $envPath -Leaf))
} else {
    Write-Host "  - .env / .env.example (not included)"
}
if (-not $SkipZip) {
    Write-Host "Zip archive: $packageZipPath"
} else {
    Write-Host "Zip archive: skipped"
}
