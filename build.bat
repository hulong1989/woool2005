@echo off
rem ===========================================================================
rem  GameServer 重建骨架 —— Windows / MSVC 构建脚本
rem  用法：在 "x64 Native Tools Command Prompt for VS 2017+" 中执行 build.bat
rem  说明：本脚本未在 Windows 上实测（开发机为 macOS），如遇问题请反馈编译输出。
rem ===========================================================================
setlocal

where cl >nul 2>nul
if errorlevel 1 (
    echo [ERROR] 找不到 cl.exe。
    echo         请从 "x64 Native Tools Command Prompt for VS 2017+" 运行本脚本。
    exit /b 1
)

if not exist build mkdir build

cl /nologo /EHsc /std:c++14 /utf-8 /W3 /D_CRT_SECURE_NO_WARNINGS ^
   /I. /Ipublic /Ires ^
   AliveObject.cpp Area.cpp DownItemMgr.cpp DownItemObject.cpp ^
   Equipment.cpp ExchangeObj.cpp GameServer.cpp GameWorld.cpp ^
   HumanPlayer.cpp HumanPlayerMgr.cpp ItemBox.cpp ItemManager.cpp ^
   LogicMap.cpp LogicMapMgr.cpp MapObject.cpp MapObjectControl.cpp ^
   PhysicsMap.cpp PhysicsMapMgr.cpp Server.cpp ServerConsole.cpp ^
   gsClientObj.cpp stdafx.cpp public\public.cpp ^
   /Fe:build\GameServer.exe /Fo:build\ ^
   /link ws2_32.lib winmm.lib

if errorlevel 1 (
    echo [ERROR] 编译失败
    exit /b 1
)

echo.
echo 构建成功: build\GameServer.exe
echo 请在工程根目录运行（需要 gameserver.ini 与 data\ 目录）。
