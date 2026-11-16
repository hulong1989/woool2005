// compat/compat.cpp
// ---------------------------------------------------------------------------
// 非 Windows 平台兼容层实现：目录枚举、路径拼接、计时、入口桥接。
// ---------------------------------------------------------------------------
#ifndef _WIN32

#include <windows.h>
#include <tchar.h>
#include <string>
#include <vector>
#include <time.h>

static std::string ComPat_Normalize(const char* pszPath)
{
    std::string s = pszPath ? pszPath : "";
    for (size_t i = 0; i < s.size(); ++i)
        if (s[i] == '\\') s[i] = '/';
    return s;
}

struct CFindContext
{
    DIR*  pDir;
    std::string szPattern;
    std::string szDir;
};

static BOOL ComPat_Fill(WIN32_FIND_DATA* pFindData, const char* pszDir, const char* pszName)
{
    struct stat st;
    std::string full = std::string(pszDir) + "/" + pszName;
    memset(pFindData, 0, sizeof(*pFindData));
    strncpy(pFindData->cFileName, pszName, MAX_PATH - 1);
    if (stat(full.c_str(), &st) == 0)
    {
        if (S_ISDIR(st.st_mode)) pFindData->dwFileAttributes |= FILE_ATTRIBUTE_DIRECTORY;
        pFindData->nFileSizeLow = (DWORD)st.st_size;
    }
    return TRUE;
}

HANDLE FindFirstFile(const char* pszPattern, WIN32_FIND_DATA* pFindData)
{
    if (pszPattern == NULL || pFindData == NULL) return INVALID_HANDLE_VALUE;

    std::string path = ComPat_Normalize(pszPattern);
    size_t pos = path.find_last_of('/');
    std::string dir = (pos == std::string::npos) ? "." : path.substr(0, pos);
    std::string pat = (pos == std::string::npos) ? path : path.substr(pos + 1);
    if (dir.empty()) dir = ".";

    DIR* pDir = opendir(dir.c_str());
    if (pDir == NULL) return INVALID_HANDLE_VALUE;

    CFindContext* pCtx = new CFindContext;
    pCtx->pDir = pDir;
    pCtx->szPattern = pat;
    pCtx->szDir = dir;

    struct dirent* pEnt = NULL;
    while ((pEnt = readdir(pDir)) != NULL)
    {
        if (strcmp(pEnt->d_name, ".") == 0 || strcmp(pEnt->d_name, "..") == 0) continue;
        if (fnmatch(pCtx->szPattern.c_str(), pEnt->d_name, 0) != 0) continue;
        ComPat_Fill(pFindData, dir.c_str(), pEnt->d_name);
        return (HANDLE)pCtx;
    }
    closedir(pDir);
    delete pCtx;
    return INVALID_HANDLE_VALUE;
}

BOOL FindNextFile(HANDLE hFind, WIN32_FIND_DATA* pFindData)
{
    if (hFind == NULL || hFind == INVALID_HANDLE_VALUE || pFindData == NULL) return FALSE;
    CFindContext* pCtx = (CFindContext*)hFind;
    struct dirent* pEnt = NULL;
    while ((pEnt = readdir(pCtx->pDir)) != NULL)
    {
        if (strcmp(pEnt->d_name, ".") == 0 || strcmp(pEnt->d_name, "..") == 0) continue;
        if (fnmatch(pCtx->szPattern.c_str(), pEnt->d_name, 0) != 0) continue;
        ComPat_Fill(pFindData, pCtx->szDir.c_str(), pEnt->d_name);
        return TRUE;
    }
    return FALSE;
}

BOOL FindClose(HANDLE hFind)
{
    if (hFind == NULL || hFind == INVALID_HANDLE_VALUE) return FALSE;
    CFindContext* pCtx = (CFindContext*)hFind;
    closedir(pCtx->pDir);
    delete pCtx;
    return TRUE;
}

void _makepath(char* pszBuffer, const char* pszDrive, const char* pszDir,
               const char* pszName, const char* pszExt)
{
    if (pszBuffer == NULL) return;
    std::string s;
    if (pszDrive && *pszDrive) s += pszDrive;
    if (pszDir && *pszDir)
    {
        std::string d = ComPat_Normalize(pszDir);
        s += d;
        if (!s.empty() && s[s.size() - 1] != '/') s += "/";
    }
    if (pszName) s += pszName;
    if (pszExt)
    {
        s += ".";
        s += pszExt;
    }
    strcpy(pszBuffer, s.c_str());
}

DWORD timeGetTime(void)
{
    struct timespec ts;
#if defined(CLOCK_MONOTONIC)
    clock_gettime(CLOCK_MONOTONIC, &ts);
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    ts.tv_sec = tv.tv_sec;
    ts.tv_nsec = tv.tv_usec * 1000;
#endif
    return (DWORD)((unsigned long long)ts.tv_sec * 1000ULL + (unsigned long long)ts.tv_nsec / 1000000ULL);
}

DWORD GetTickCount(void)
{
    return timeGetTime();
}

void SetConsoleTitleA(const char* pszTitle)
{
    if (pszTitle) printf("\033]0;%s\007", pszTitle);
}

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow);

int main(int argc, char** argv)
{
    std::string cmdline;
    for (int i = 1; i < argc; ++i)
    {
        if (i > 1) cmdline += " ";
        cmdline += argv[i];
    }
    std::vector<char> buf(cmdline.begin(), cmdline.end());
    buf.push_back('\0');

    fflush(stdout);
    setvbuf( stdout, NULL, _IONBF, 0 );
    setvbuf( stderr, NULL, _IONBF, 0 );
    int rc = WinMain(NULL, NULL, buf.empty() ? NULL : &buf[0], 1);
    fflush(stdout);
    return rc;
}

#endif // !_WIN32
