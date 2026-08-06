# Build Open Bingo on Windows (MinGW + Qt 6).
# Prérequis : Qt 6.8 MinGW, CMake, Ninja, libsodium/secp256k1 (vcpkg ou MSYS2).
param(
    [string]$QtPath = $env:QT_DIR,
    [string]$BuildType = "Release"
)

$ErrorActionPreference = "Stop"
if (-not $QtPath) { throw "Set QT_DIR to your Qt 6 MinGW install (e.g. C:\Qt\6.8.2\mingw_64)" }

$repo = Split-Path -Parent $MyInvocation.MyCommand.Path | Split-Path -Parent
$build = Join-Path $repo "build-win"

$env:PATH = "$QtPath\bin;$env:PATH"
cmake -S $repo -B $build -G Ninja `
    -DCMAKE_BUILD_TYPE=$BuildType `
    -DCMAKE_PREFIX_PATH=$QtPath
cmake --build $build
Write-Host "Binary: $build\src\openbingo.exe"
