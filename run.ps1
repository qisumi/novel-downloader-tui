$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $repoRoot

$vsDevCmd = "C:\PROGRA~1\MICROS~3\18\COMMUN~1\Common7\Tools\VsDevCmd.bat"

function Invoke-Step {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Description,
        [Parameter(Mandatory = $true)]
        [scriptblock]$Script
    )

    Write-Host "==> $Description" -ForegroundColor Cyan
    & $Script
}

function Invoke-CMakePreset {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Preset,
        [string]$Target = ""
    )

    Invoke-Step "Configure preset $Preset" { cmake --preset $Preset }

    if ([string]::IsNullOrWhiteSpace($Target)) {
        Invoke-Step "Build preset $Preset" { cmake --build --preset $Preset }
    } else {
        Invoke-Step "Build preset $Preset target $Target" {
            cmake --build --preset $Preset --target $Target
        }
    }
}

function Invoke-ClangdRefresh {
    if (-not (Test-Path $vsDevCmd)) {
        throw "VsDevCmd.bat not found: $vsDevCmd"
    }

    $command = "`"$vsDevCmd`" -arch=x64 -host_arch=x64 && cmake --fresh --preset windows-x64-debug-clangd && cmake --build --preset windows-x64-debug-clangd --target novel-sync-compile-commands"
    Invoke-Step "Refresh clangd compile database" { cmd /c $command }
}

function Invoke-DistBuild {
    Invoke-CMakePreset -Preset "windows-x64-release-static-msvc" -Target "novel-downloader-gui"

    $distDir = Join-Path $repoRoot "dist"
    if (Test-Path $distDir) {
        Invoke-Step "Clean dist directory" { Remove-Item -Recurse -Force $distDir }
    }

    Invoke-Step "Install release artifacts to dist" {
        cmake --install build/release-static-msvc --config Release --prefix $distDir
    }
}

function Show-Usage {
    $lines = @(
        "Usage:",
        "  .\run.ps1 --build-msvc-debug",
        "  .\run.ps1 --build-release",
        "  .\run.ps1 --build-dist",
        "  .\run.ps1 --update-clangd",
        "  .\run.ps1 --help",
        "",
        "Actions:",
        "  --build-msvc-debug  Configure and build Debug GUI",
        "  --build-release     Configure and build Release GUI",
        "  --build-dist        Build static Release GUI and install to dist",
        "  --update-clangd     Regenerate and sync compile_commands.json"
    )
    Write-Host ($lines -join [Environment]::NewLine)
}

if ($args.Count -eq 0 -or $args[0] -eq "--help") {
    Show-Usage
    exit 0
}

switch ($args[0]) {
    "--build-msvc-debug" {
        Invoke-CMakePreset -Preset "windows-x64-debug-msvc" -Target "novel-downloader-gui"
    }
    "--build-release" {
        Invoke-CMakePreset -Preset "windows-x64-release-msvc" -Target "novel-downloader-gui"
    }
    "--build-dist" {
        Invoke-DistBuild
    }
    "--update-clangd" {
        Invoke-ClangdRefresh
    }
    default {
        Write-Error ("Unknown argument: " + $args[0])
        Show-Usage
        exit 1
    }
}
