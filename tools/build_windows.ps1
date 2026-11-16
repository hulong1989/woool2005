# 在 Windows + MSYS2 MinGW64 下打包服务端
# 用法：powershell -ExecutionPolicy Bypass -File tools\build_windows.ps1
$ErrorActionPreference = "Stop"
Set-Location (Split-Path $PSScriptRoot -Parent)

if (-not (Get-Command g++ -ErrorAction SilentlyContinue)) {
    Write-Error "找不到 g++，请先安装 MSYS2 的 mingw-w64-x86_64-gcc 和 make"
}
if (-not (Get-Command make -ErrorAction SilentlyContinue)) {
    Write-Error "找不到 make，请安装 MSYS2 make"
}

Write-Host "构建 Windows 服务端..."
make BUILD=build-windows CXX=g++ CXXFLAGS="-std=c++11 -O2 -Wall -Wno-unused-parameter -Wno-reorder"
if ($LASTEXITCODE -ne 0) { throw "构建失败" }

$out = "dist\woool-server-windows"
if (Test-Path $out) { Remove-Item $out -Recurse -Force }
New-Item -ItemType Directory -Path $out | Out-Null
Copy-Item build-windows\GameServer.exe $out\
Copy-Item data $out\data -Recurse
Copy-Item tools $out\tools -Recurse
Copy-Item client $out\client -Recurse
Copy-Item README-BUILD.md $out\
Copy-Item docs $out\docs -Recurse
Compress-Archive -Path "$out\*" -DestinationPath "dist\woool-server-windows.zip" -Force
Write-Host "完成：dist\woool-server-windows.zip"
