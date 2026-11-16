// compat/tchar.h
// 非 Windows 平台的 <tchar.h> 兼容层：把 _tWinMain 映射到 WinMain，
// 由 compat.cpp 中的 main() 负责调用。
#pragma once

#ifndef _WIN32

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifndef _T
#define _T(x) x
#endif
#ifndef TEXT
#define TEXT(x) x
#endif

// 源码里的入口签名是 int APIENTRY _tWinMain(HINSTANCE, HINSTANCE, LPTSTR, int)
#define _tWinMain WinMain

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow);

#endif // !_WIN32
