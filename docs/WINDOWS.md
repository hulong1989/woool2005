# Windows 构建与运行

## 环境

- Windows 10/11
- MSYS2（MinGW64）
- 安装：`pacman -S mingw-w64-x86_64-gcc make`
- 把 `C:\msys64\mingw64\bin` 加入 PATH

## 构建

```powershell
powershell -ExecutionPolicy Bypass -File tools\build_windows.ps1
```

脚本会：

- 用 MinGW 构建 `build-windows\GameServer.exe`
- 复制 `data/`、`tools/`、`client/`
- 生成 `dist\woool-server-windows.zip`

## 运行

```powershell
cd dist\woool-server-windows
.\GameServer.exe
```

管理后台和网页客户端需要 Python 3.9+：

```powershell
python tools\server_dashboard.py
python tools\web_game.py
```

说明：Windows 包未在当前 macOS 环境实际编译验证；脚本按 MinGW Makefile 编写。
