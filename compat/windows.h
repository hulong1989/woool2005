// compat/windows.h
// ---------------------------------------------------------------------------
// 仅用于非 Windows 平台（macOS/Linux）的 Win32 兼容层。
// 目的：让 2005 年的 Win32 源码可以在 POSIX 上编译、运行，便于验证骨架。
// 在真正的 Windows 构建中不会包含此文件（stdafx.h 直接 include 系统 windows.h）。
// ---------------------------------------------------------------------------
#pragma once

#ifndef _WIN32

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <fnmatch.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <ctype.h>

typedef int             BOOL;
typedef void            VOID;
typedef char            CHAR;
typedef unsigned char   BYTE;
typedef unsigned short  WORD;
typedef unsigned int    DWORD;
typedef unsigned int    UINT;
typedef long            LONG;
typedef unsigned long   ULONG;
typedef void*           LPVOID;
typedef const void*     LPCVOID;
typedef char*           LPSTR;
typedef const char*     LPCSTR;
typedef unsigned long   COLORREF;
typedef unsigned long   DWORD_PTR;
typedef void*           HANDLE;
typedef void*           HINSTANCE;
typedef void*           HWND;
typedef void*           HDC;
typedef char*           LPTSTR;
typedef const char*     LPCTSTR;

typedef int             SOCKET;

#ifndef TRUE
#define TRUE  1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#define APIENTRY
#define WINAPI
#define CALLBACK
#define __stdcall

#define INVALID_SOCKET      (-1)
#define SOCKET_ERROR        (-1)
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)

#define MAX_PATH            260
#define FILE_ATTRIBUTE_DIRECTORY 0x10
#define FILE_ATTRIBUTE_NORMAL    0x80

#define RGB(r,g,b) ((COLORREF)(((BYTE)(r)) | (((WORD)((BYTE)(g))) << 8) | (((DWORD)((BYTE)(b))) << 16)))

#ifndef ZeroMemory
#define ZeroMemory(p, n) memset((p), 0, (n))
#endif
#ifndef CopyMemory
#define CopyMemory(d, s, n) memcpy((d), (s), (n))
#endif

#define Sleep(ms) usleep((useconds_t)((ms) * 1000))

typedef struct tagWIN32_FIND_DATA
{
    DWORD dwFileAttributes;
    char  cFileName[MAX_PATH];
    char  cAlternateFileName[14];
    DWORD nFileSizeLow;
} WIN32_FIND_DATA, *LPWIN32_FIND_DATA;

// 目录枚举（CFindFile 使用）
HANDLE FindFirstFile(const char* pszPattern, WIN32_FIND_DATA* pFindData);
BOOL   FindNextFile(HANDLE hFind, WIN32_FIND_DATA* pFindData);
BOOL   FindClose(HANDLE hFind);

// 路径拼接（CFindFile 使用）
void _makepath(char* pszBuffer, const char* pszDrive, const char* pszDir,
               const char* pszName, const char* pszExt);

// 毫秒计时（AliveObject / Server 使用）
DWORD timeGetTime(void);
DWORD GetTickCount(void);

// 控制台
void  SetConsoleTitleA(const char* pszTitle);
#define SetConsoleTitle SetConsoleTitleA

#endif // !_WIN32
