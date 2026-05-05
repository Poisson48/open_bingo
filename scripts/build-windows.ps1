# Build Windows (.exe NSIS) et copie dans releases\vX.Y.Z\
# Usage : .\scripts\build-windows.ps1 [version]
# Prérequis : Rust (rustup), Node.js — executer sur Windows

param([string]$Version = "")

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $RepoRoot

$Conf = "src-tauri\tauri.conf.json"
$Current = (Get-Content $Conf | ConvertFrom-Json).version

if ($Version -eq "") {
    $Parts = $Current -split "\."
    $Version = "$($Parts[0]).$($Parts[1]).$([int]$Parts[2] + 1)"
}

$Dest = "releases\v$Version"

if (Test-Path "$Dest\*.exe") {
    Write-Host "⚠  Un .exe existe deja dans $Dest"
    exit 1
}

Write-Host "→ Version : $Current  ➜  $Version"

# Patch version + targets
$c = Get-Content $Conf | ConvertFrom-Json
$c.version = $Version
$c.bundle.targets = @("nsis")
$c | ConvertTo-Json -Depth 10 | Set-Content $Conf

Write-Host "→ Build Windows..."
npm run build

New-Item -ItemType Directory -Force -Path $Dest | Out-Null
Get-ChildItem "src-tauri\target\release\bundle\nsis\*.exe" | Copy-Item -Destination $Dest

Write-Host ""
Write-Host "✓  releases\v$Version :"
Get-ChildItem $Dest
