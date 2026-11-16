// ===========================================================================
//  public/public.cpp  ——  游戏服务器公共层实现（重建版）
// ===========================================================================
#include "public.h"
#include <csignal>

#ifndef _WIN32
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <unistd.h>
#  include <fcntl.h>
#  include <errno.h>
#  include <sys/select.h>
#  include <signal.h>
#  ifdef __APPLE__
#    include <sys/time.h>
#  endif
#endif

#include <ctype.h>
#include <algorithm>

#ifdef _WIN32
#  include <conio.h>
#endif

// ---------------------------------------------------------------------------
// 方向表（0=上，顺时针）
// ---------------------------------------------------------------------------
const int g_dir_dx[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };
const int g_dir_dy[8] = { -1, -1, 0, 1, 1, 1, 0, -1 };

// ---------------------------------------------------------------------------
// 基础工具
// ---------------------------------------------------------------------------
char* o_strncpy(char* pszDst, const char* pszSrc, size_t nMaxChars)
{
    if (pszDst == NULL) return NULL;
    if (pszSrc == NULL)
    {
        pszDst[0] = '\0';
        return pszDst;
    }
    size_t n = 0;
    while (n < nMaxChars && pszSrc[n] != '\0')
    {
        pszDst[n] = pszSrc[n];
        ++n;
    }
    pszDst[n] = '\0';
    return pszDst;
}

int SearchParam(char* pszString, char** ppszParams, int nMaxParam, char cSeparator)
{
    if (pszString == NULL || ppszParams == NULL || nMaxParam <= 0) return 0;
    int nCount = 0;
    char* p = pszString;
    char* pStart = pszString;
    while (p != NULL && nCount < nMaxParam)
    {
        if (*p == cSeparator || *p == '\0')
        {
            char cSave = *p;
            *p = '\0';
            // 去掉首尾空白
            char* s = pStart;
            while (*s == ' ' || *s == '\t') ++s;
            char* e = s + strlen(s);
            while (e > s && (e[-1] == ' ' || e[-1] == '\t')) --e;
            *e = '\0';
            ppszParams[nCount++] = s;
            if (cSave == '\0') return nCount;
            pStart = p + 1;
        }
        ++p;
    }
    return nCount;
}

int Getrand(int nMax)
{
    if (nMax <= 1) return 0;
    return (int)(((unsigned int)rand()) % (unsigned int)nMax);
}

DWORD o_GetTickCount(void)
{
    return timeGetTime();
}

// ---------------------------------------------------------------------------
// 消息编码（逐字节可逆异或；见 public.h 末尾说明）
// ---------------------------------------------------------------------------
static BYTE CodeKey(int nIndex)
{
    return (BYTE)(0x5A + (nIndex & 0xFF) * 0x1D);
}

int _CodeGameCode(const BYTE* pSrc, int nLen, BYTE* pDst)
{
    if (pSrc == NULL || pDst == NULL || nLen <= 0) return 0;
    for (int i = 0; i < nLen; ++i) pDst[i] = (BYTE)(pSrc[i] ^ CodeKey(i));
    return nLen;
}

int _UnGameCode(const BYTE* pSrc, int nLen, BYTE* pDst)
{
    return _CodeGameCode(pSrc, nLen, pDst);
}

void _UnGameCode(const char* pszSrc, BYTE* pHeader16)
{
    if (pszSrc == NULL || pHeader16 == NULL) return;
    _CodeGameCode((const BYTE*)pszSrc, (int)sizeof(MIRMSGHEADER), pHeader16);
}

// ---------------------------------------------------------------------------
// CNameHash / CIntHash
// ---------------------------------------------------------------------------
static std::string ToLower(const std::string& s)
{
    std::string r = s;
    for (size_t i = 0; i < r.size(); ++i) r[i] = (char)tolower((unsigned char)r[i]);
    return r;
}

BOOL CNameHash::HAdd(const char* pszName, LPVOID pValue)
{
    if (pszName == NULL) return FALSE;
    std::string key = ToLower(pszName);
    if (key.empty()) return FALSE;
    m_map[key] = pValue;
    return TRUE;
}

LPVOID CNameHash::HGet(const char* pszName)
{
    if (pszName == NULL) return NULL;
    std::map<std::string, LPVOID>::iterator it = m_map.find(ToLower(pszName));
    if (it == m_map.end()) return NULL;
    return it->second;
}

BOOL CNameHash::HDel(const char* pszName)
{
    if (pszName == NULL) return FALSE;
    return m_map.erase(ToLower(pszName)) > 0 ? TRUE : FALSE;
}

VOID CNameHash::Clear()
{
    m_map.clear();
}

BOOL CIntHash::HAdd(UINT nKey, UINT nValue)
{
    m_map[nKey] = nValue;
    return TRUE;
}

UINT CIntHash::HGet(UINT nKey)
{
    std::map<UINT, UINT>::iterator it = m_map.find(nKey);
    if (it == m_map.end()) return 0;
    return it->second;
}

BOOL CIntHash::HDel(UINT nKey)
{
    return m_map.erase(nKey) > 0 ? TRUE : FALSE;
}

VOID CIntHash::Clear()
{
    m_map.clear();
}

// ---------------------------------------------------------------------------
// 路径规范化："\\" -> "/"（Win32 同样接受正斜杠）
// ---------------------------------------------------------------------------
static std::string NormalizePath(const char* pszPath)
{
    std::string s = pszPath ? pszPath : "";
    for (size_t i = 0; i < s.size(); ++i)
        if (s[i] == '\\') s[i] = '/';
    return s;
}

static std::string Trim(const std::string& s)
{
    size_t b = 0, e = s.size();
    while (b < e && (unsigned char)s[b] <= ' ') ++b;
    while (e > b && (unsigned char)s[e - 1] <= ' ') --e;
    return s.substr(b, e - b);
}

// ---------------------------------------------------------------------------
// CDataFile
// ---------------------------------------------------------------------------
CDataFile::CDataFile() : m_bOpened(FALSE) {}
CDataFile::~CDataFile() { Close(); }

BOOL CDataFile::Open(const char* pszFilename)
{
    Close();
    if (pszFilename == NULL) return FALSE;

    std::string path = NormalizePath(pszFilename);
    FILE* fp = fopen(path.c_str(), "rb");
    if (fp == NULL) return FALSE;

    m_szFilename = path;
    m_bOpened = TRUE;

    std::string section;
    char szLine[2048];
    while (fgets(szLine, sizeof(szLine), fp) != NULL)
    {
        std::string line = Trim(std::string(szLine));
        if (line.empty()) continue;
        if (line[0] == ';' || line[0] == '#') continue;
        if (line[0] == '[')
        {
            size_t e = line.find(']');
            if (e != std::string::npos) section = ToLower(Trim(line.substr(1, e - 1)));
            continue;
        }
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = ToLower(Trim(line.substr(0, eq)));
        std::string value = Trim(line.substr(eq + 1));
        if (key.empty()) continue;
        m_values[section + "/" + key] = value;
    }
    fclose(fp);
    return TRUE;
}

VOID CDataFile::Close()
{
    m_values.clear();
    m_szFilename.clear();
    m_bOpened = FALSE;
}

const char* CDataFile::GetString(const char* pszSection, const char* pszKey, const char* pszDefault) const
{
    if (pszSection == NULL || pszKey == NULL) return pszDefault;
    std::string k = ToLower(pszSection) + "/" + ToLower(pszKey);
    std::map<std::string, std::string>::const_iterator it = m_values.find(k);
    if (it == m_values.end()) return pszDefault;
    return it->second.c_str();
}

int CDataFile::GetInteger(const char* pszSection, const char* pszKey, int nDefault) const
{
    const char* pszValue = GetString(pszSection, pszKey, NULL);
    if (pszValue == NULL) return nDefault;
    return (int)strtol(pszValue, NULL, 0);
}

// ---------------------------------------------------------------------------
// CStringFile
// ---------------------------------------------------------------------------
CStringFile::CStringFile() {}
CStringFile::CStringFile(const char* pszFilename) { Open(pszFilename); }
CStringFile::~CStringFile() { Close(); }

BOOL CStringFile::Open(const char* pszFilename)
{
    Close();
    if (pszFilename == NULL) return FALSE;
    std::string path = NormalizePath(pszFilename);
    FILE* fp = fopen(path.c_str(), "rb");
    if (fp == NULL) return FALSE;

    char szLine[2048];
    while (fgets(szLine, sizeof(szLine), fp) != NULL)
    {
        std::string line(szLine);
        while (!line.empty() && (line[line.size() - 1] == '\n' || line[line.size() - 1] == '\r'))
            line.erase(line.size() - 1);
        m_lines.push_back(line);
    }
    fclose(fp);
    return TRUE;
}

VOID CStringFile::Close()
{
    m_lines.clear();
}

const char* CStringFile::operator[](int nIndex) const
{
    if (nIndex < 0 || (size_t)nIndex >= m_lines.size()) return "";
    return m_lines[nIndex].c_str();
}

// ---------------------------------------------------------------------------
// 网络小工具
// ---------------------------------------------------------------------------
#ifdef _WIN32
typedef int socklen_type;
#else
typedef socklen_t socklen_type;
#endif

static void NetShutdown(SOCKET s)
{
    if (s == INVALID_SOCKET) return;
#ifdef _WIN32
    closesocket(s);
#else
    close(s);
#endif
}

static void WriteU32LE(std::string& buffer, unsigned int value)
{
    buffer.push_back((char)(value & 0xFF));
    buffer.push_back((char)((value >> 8) & 0xFF));
    buffer.push_back((char)((value >> 16) & 0xFF));
    buffer.push_back((char)((value >> 24) & 0xFF));
}

static unsigned int ReadU32LE(const char* p)
{
    return (unsigned int)((unsigned char)p[0]) |
           ((unsigned int)((unsigned char)p[1]) << 8) |
           ((unsigned int)((unsigned char)p[2]) << 16) |
           ((unsigned int)((unsigned char)p[3]) << 24);
}

// ---------------------------------------------------------------------------
// CClientObject
// ---------------------------------------------------------------------------
CClientObject::CClientObject() : m_Id(0), m_Socket(INVALID_SOCKET) {}
CClientObject::~CClientObject() { CloseSocket(); }

VOID CClientObject::Clean()
{
    m_RecvBuffer.clear();
    m_SendBuffer.clear();
    m_Id = 0;
}

VOID CClientObject::Update()
{
    FlushSend();
}

VOID CClientObject::OnConnect() {}
VOID CClientObject::OnDisconnect() {}
VOID CClientObject::OnCodedMsg(PMIRMSG pMsg, int datasize) { (void)pMsg; (void)datasize; }
VOID CClientObject::OnUnCodeMsg(const char* pszMsg, int size) { (void)pszMsg; (void)size; }
VOID CClientObject::OnDBMsg(PMIRMSG pMsg, int datasize) { (void)pMsg; (void)datasize; }
VOID CClientObject::OnVerifyString(const char* pszString) { (void)pszString; }

VOID CClientObject::SetSocket(SOCKET s)
{
    CloseSocket();
    m_Socket = s;
#ifndef _WIN32
    if (m_Socket != INVALID_SOCKET)
    {
        int flags = fcntl(m_Socket, F_GETFL, 0);
        if (flags >= 0) fcntl(m_Socket, F_SETFL, flags | O_NONBLOCK);
    }
#else
    if (m_Socket != INVALID_SOCKET)
    {
        u_long mode = 1;
        ioctlsocket(m_Socket, FIONBIO, &mode);
    }
#endif
}

VOID CClientObject::CloseSocket()
{
    if (m_Socket != INVALID_SOCKET)
    {
        NetShutdown(m_Socket);
        m_Socket = INVALID_SOCKET;
    }
}

VOID CClientObject::PostMsg(const char* pszMsg, int nSize)
{
    if (pszMsg == NULL || nSize <= 0) return;
    WriteU32LE(m_SendBuffer, (unsigned int)nSize);
    m_SendBuffer.append(pszMsg, (size_t)nSize);
    FlushSend();
}

VOID CClientObject::SendRaw(const void* pData, int nSize)
{
    if (pData == NULL || nSize <= 0) return;
    WriteU32LE(m_SendBuffer, (unsigned int)nSize);
    m_SendBuffer.append((const char*)pData, (size_t)nSize);
    FlushSend();
}

VOID CClientObject::SendMsg(DWORD dwFlag, WORD wCmd, WORD w1, WORD w2, WORD w3,
                            LPVOID lpdata, int datasize)
{
    char szPacket[MIR_MAX_PACKET];
    MIRMSGHEADER header;
    memset(&header, 0, sizeof(header));
    header.dwFlag = dwFlag;
    header.wCmd = wCmd;
    header.wParam[0] = w1;
    header.wParam[1] = w2;
    header.wParam[2] = w3;

    int n = 1;
    szPacket[0] = '#';
    n += _CodeGameCode((const BYTE*)&header, (int)sizeof(header), (BYTE*)szPacket + n);

    if (lpdata != NULL)
    {
        if (datasize < 0) datasize = (int)strlen((char*)lpdata);
        if (datasize > MIR_MAX_PACKET - 32) datasize = MIR_MAX_PACKET - 32;
        if (datasize > 0)
            n += _CodeGameCode((const BYTE*)lpdata, datasize, (BYTE*)szPacket + n);
    }
    szPacket[n++] = '!';
    PostMsg(szPacket, n);
}

VOID CClientObject::FlushSend()
{
    if (m_Socket == INVALID_SOCKET || m_SendBuffer.empty()) return;

    size_t nOffset = 0;
    while (nOffset < m_SendBuffer.size())
    {
#ifdef _WIN32
        int nSent = (int)send(m_Socket, m_SendBuffer.data() + nOffset,
                              (int)(m_SendBuffer.size() - nOffset), 0);
#else
        int nSent = (int)send(m_Socket, m_SendBuffer.data() + nOffset,
                              (int)(m_SendBuffer.size() - nOffset), MSG_NOSIGNAL);
#endif
        if (nSent > 0)
        {
            nOffset += (size_t)nSent;
            continue;
        }
        if (nSent < 0)
        {
#ifdef _WIN32
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK) break;
#else
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
#endif
        }
        break;
    }
    if (nOffset > 0) m_SendBuffer.erase(0, nOffset);
}

VOID CClientObject::PumpRecv()
{
    if (m_Socket == INVALID_SOCKET) return;

    char szBuffer[4096];
    for (;;)
    {
        int nRecv = (int)recv(m_Socket, szBuffer, sizeof(szBuffer), 0);
        if (nRecv > 0)
        {
            m_RecvBuffer.append(szBuffer, (size_t)nRecv);
            continue;
        }
        if (nRecv == 0)
        {
            CloseSocket();
            return;
        }
        // nRecv < 0
#ifdef _WIN32
        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK) break;
#else
        if (errno == EAGAIN || errno == EWOULDBLOCK) break;
#endif
        CloseSocket();
        return;
    }

    // 拆包：4 字节长度 + 载荷
    for (;;)
    {
        if (m_RecvBuffer.size() < 4) break;
        unsigned int nLen = ReadU32LE(m_RecvBuffer.data());
        if (nLen == 0 || nLen > (unsigned int)MIR_MAX_PACKET)
        {
            CloseSocket();
            return;
        }
        if (m_RecvBuffer.size() < 4 + (size_t)nLen) break;
        std::string frame = m_RecvBuffer.substr(4, nLen);
        m_RecvBuffer.erase(0, 4 + (size_t)nLen);
        OnFrame(frame.data(), (int)frame.size());
    }
}

VOID CClientObject::OnFrame(const char* pData, int nSize)
{
    if (pData == NULL || nSize <= 0) return;

    // 未编码的文本消息（调试客户端可直接发文本）
    if (pData[0] != '#' || nSize < (int)(1 + sizeof(MIRMSGHEADER) + 1))
    {
        OnUnCodeMsg(pData, nSize);
        return;
    }

    std::vector<char> buffer(sizeof(MIRMSG) + nSize + 1, 0);
    MIRMSG* pMsg = (MIRMSG*)&buffer[0];

    // 头部 16 字节
    _CodeGameCode((const BYTE*)pData + 1, (int)sizeof(MIRMSGHEADER), (BYTE*)&pMsg->dwFlag);

    // 负载直到结尾的 '!'
    const char* pPayload = pData + 1 + sizeof(MIRMSGHEADER);
    int nCoded = nSize - 1 - (int)sizeof(MIRMSGHEADER) - 1;
    if (nCoded < 0) nCoded = 0;
    if (nCoded > 0)
        _CodeGameCode((const BYTE*)pPayload, nCoded, (BYTE*)pMsg->data);

    OnCodedMsg(pMsg, nCoded);
}

// ---------------------------------------------------------------------------
// CSCClientObj / CDBClientObj
// ---------------------------------------------------------------------------
CSCClientObj::CSCClientObj() : m_bReady(FALSE) {}
CSCClientObj::~CSCClientObj() {}
VOID CSCClientObj::Update() {}

BOOL CSCClientObj::SendMsgAcrossServer(UINT nClientId, WORD wCmd, WORD wType, WORD wIndex,
                                       const char* pszData, int datasize)
{
    printf("[SC] 跨服消息 -> client=%u cmd=0x%04x type=%u index=%u size=%d (未连接, 丢弃)\n",
           nClientId, wCmd, wType, wIndex, datasize);
    (void)pszData;
    return TRUE;
}

CDBClientObj::CDBClientObj() : m_bReady(FALSE) {}
CDBClientObj::~CDBClientObj() {}
VOID CDBClientObj::Update() {}

BOOL CDBClientObj::SendCreateItem(DWORD dwId, DWORD dwKey, DWORD dwOwner, BYTE btFlag,
                                  WORD wPos, ITEM* pItem)
{
    if (pItem != NULL)
    {
        printf("[DB] 建物品请求 name=%s makeindex=%u key=%u owner=%u pos=%u\n",
               pItem->baseitem.szName, (unsigned)dwId, (unsigned)dwKey,
               (unsigned)dwOwner, (unsigned)wPos);
    }
    (void)btFlag;
    return TRUE;
}

// ---------------------------------------------------------------------------
// CBaseServer
// ---------------------------------------------------------------------------
CBaseServer::CBaseServer()
    : m_pIoConsole(NULL),
      m_pInputListener(NULL),
      m_ServerName(SERVER_NAME),
      m_ListenSocket(INVALID_SOCKET),
      m_bRunning(FALSE),
      m_bInited(FALSE)
{
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
}

CBaseServer::~CBaseServer()
{
    Stop();
#ifdef _WIN32
    WSACleanup();
#endif
}

VOID CBaseServer::SetServerName(const char* pszName)
{
    if (pszName != NULL) m_ServerName = pszName;
}

VOID CBaseServer::Log(DWORD dwColor, const char* pszFormat, ...)
{
    char szBuffer[2048];
    va_list vl;
    va_start(vl, pszFormat);
    vsnprintf(szBuffer, sizeof(szBuffer), pszFormat, vl);
    va_end(vl);

    if (m_pIoConsole != NULL) m_pIoConsole->OutPut(dwColor, "%s", szBuffer);
    else printf("%s", szBuffer);
}

BOOL CBaseServer::InitServer(CSettingFile& s)
{
    (void)s;
    return TRUE;
}

VOID CBaseServer::CleanServer() {}
VOID CBaseServer::Update() {}
VOID CBaseServer::OnDBMsg(PMIRMSG pMsg, int datasize) { (void)pMsg; (void)datasize; }
VOID CBaseServer::OnMASMsg(WORD wCmd, WORD wType, WORD wIndex, const char* pszData, int datasize)
{
    (void)wCmd; (void)wType; (void)wIndex; (void)pszData; (void)datasize;
}
VOID CBaseServer::OnSCMsg(PMIRMSG pMsg, int datasize) { (void)pMsg; (void)datasize; }
VOID CBaseServer::OnSCServerReady() {}

CClientObject* CBaseServer::NewClientObject() { return NULL; }
// 连接对象的释放由 DeleteClientObject 统一负责（派生类可接管，例如放进对象池）
VOID CBaseServer::DeleteClientObject(CClientObject* pObject) { delete pObject; }

BOOL CBaseServer::OpenListenSocket(const char* pszAddr, WORD wPort)
{
    SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == INVALID_SOCKET)
    {
        Log(RGB(255, 0, 0), "创建监听套接字失败\n");
        return FALSE;
    }

    int nReuse = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char*)&nReuse, sizeof(nReuse));

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(wPort);
    addr.sin_addr.s_addr = (pszAddr == NULL || strcmp(pszAddr, "0.0.0.0") == 0)
                         ? INADDR_ANY : inet_addr(pszAddr);

    if (bind(s, (sockaddr*)&addr, sizeof(addr)) != 0)
    {
        Log(RGB(255, 0, 0), "绑定 %s:%u 失败（端口可能被占用），服务器继续以离线模式运行\n",
            pszAddr ? pszAddr : "0.0.0.0", (unsigned)wPort);
        NetShutdown(s);
        return FALSE;
    }
    if (listen(s, 64) != 0)
    {
        Log(RGB(255, 0, 0), "listen 失败\n");
        NetShutdown(s);
        return FALSE;
    }

#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(s, FIONBIO, &mode);
#else
    int flags = fcntl(s, F_GETFL, 0);
    if (flags >= 0) fcntl(s, F_SETFL, flags | O_NONBLOCK);
#endif

    m_ListenSocket = s;
    Log(RGB(0, 255, 0), "监听地址 %s:%u\n", pszAddr ? pszAddr : "0.0.0.0", (unsigned)wPort);
    return TRUE;
}

BOOL CBaseServer::Start(const char* pszConfigFile)
{
    if (m_bRunning) return TRUE;

    CSettingFile cfg;
    if (cfg.Open(pszConfigFile))
    {
        Log(RGB(160, 160, 160), "已加载配置 %s\n", pszConfigFile);
    }
    else
    {
        Log(RGB(255, 255, 0), "未找到配置 %s，使用默认参数\n", pszConfigFile);
    }

    const char* pszName = cfg.GetString("server", "name", SERVER_NAME);
    SetServerName(pszName);

    m_bRunning = TRUE;
    m_bInited = InitServer(cfg);
    if (!m_bInited)
        Log(RGB(255, 255, 0), "初始化返回失败，服务器以最小模式启动（无地图/物品数据）\n");

    const char* pszAddr = cfg.GetString("server", "addr", "0.0.0.0");
    int nPort = cfg.GetInteger("server", "port", 7100);
    OpenListenSocket(pszAddr, (WORD)nPort);

    Log(RGB(0, 255, 255), "==============================================\n");
    Log(RGB(0, 255, 255), "  %s 已启动（重建骨架）\n", GetServerName());
    Log(RGB(0, 255, 255), "  输入 help 查看命令，exit 退出\n");
    Log(RGB(0, 255, 255), "==============================================\n");
    return TRUE;
}

VOID CBaseServer::Stop()
{
    if (!m_bRunning && m_ListenSocket == INVALID_SOCKET && m_Connections.empty()) return;

    m_bRunning = FALSE;

    for (size_t i = 0; i < m_Connections.size(); ++i)
    {
        CClientObject* pObject = m_Connections[i];
        if (pObject == NULL) continue;
        pObject->OnDisconnect();
        DeleteClientObject(pObject);
    }
    m_Connections.clear();

    if (m_ListenSocket != INVALID_SOCKET)
    {
        NetShutdown(m_ListenSocket);
        m_ListenSocket = INVALID_SOCKET;
    }
    CleanServer();
}

VOID CBaseServer::AcceptClients()
{
    if (m_ListenSocket == INVALID_SOCKET) return;

    for (;;)
    {
        sockaddr_in addr;
        socklen_type nLen = sizeof(addr);
        SOCKET s = accept(m_ListenSocket, (sockaddr*)&addr, &nLen);
        if (s == INVALID_SOCKET) break;

        CClientObject* pObject = NewClientObject();
        if (pObject == NULL)
        {
            NetShutdown(s);
            continue;
        }
        pObject->SetSocket(s);
        m_Connections.push_back(pObject);
        Log(RGB(0, 255, 0), "客户端接入: id=%u ip=%s\n", pObject->GetId(), inet_ntoa(addr.sin_addr));
        pObject->OnConnect();
    }
}

VOID CBaseServer::PumpClients()
{
    for (size_t i = 0; i < m_Connections.size(); )
    {
        CClientObject* pObject = m_Connections[i];
        if (pObject == NULL) { ++i; continue; }

        pObject->PumpRecv();
        pObject->FlushSend();

        if (!pObject->IsConnected())
        {
            Log(RGB(255, 160, 0), "客户端断开: id=%u\n", pObject->GetId());
            pObject->OnDisconnect();
            DeleteClientObject(pObject);
            m_Connections.erase(m_Connections.begin() + i);
            continue;
        }
        ++i;
    }
}

BOOL CBaseServer::PumpNetwork()
{
    AcceptClients();
    PumpClients();
    return TRUE;
}

CClientObject* CBaseServer::GetClientObject(UINT id)
{
    for (size_t i = 0; i < m_Connections.size(); ++i)
    {
        if (m_Connections[i] != NULL && m_Connections[i]->GetId() == id)
            return m_Connections[i];
    }
    return NULL;
}

VOID CBaseServer::KickClient(UINT id)
{
    CClientObject* pObject = GetClientObject(id);
    if (pObject != NULL) pObject->CloseSocket();
}

CDBClientObj* CBaseServer::GetDBConnection(int nIndex)
{
    (void)nIndex;
    return &m_DBClientObj;
}

VOID CBaseServer::UpdateSCServer() { m_SCClientObj.Update(); }
VOID CBaseServer::UpdateDBServer() { m_DBClientObj.Update(); }

// ---------------------------------------------------------------------------
// CServerForm（控制台窗体）
// ---------------------------------------------------------------------------
CServerForm::CServerForm()
    : m_pServer(NULL), m_pInputListener(NULL), m_Title(SERVER_NAME),
      m_bStdinClosed(FALSE), m_bStop(FALSE)
{
}

CServerForm::~CServerForm() {}

BOOL CServerForm::Create(const char* pszTitle)
{
    if (pszTitle != NULL) m_Title = pszTitle;
#ifdef _WIN32
    // 源码与数据均为 UTF-8，Windows 控制台需要显式切换代码页
    SetConsoleOutputCP(CP_UTF8);
#endif
    SetConsoleTitleA(m_Title.c_str());
    return TRUE;
}

VOID CServerForm::Clear()
{
#ifndef _WIN32
    printf("\033[2J\033[H");
    fflush(stdout);
#endif
}

VOID CServerForm::OutPut(DWORD dwColor, const char* pszFormat, ...)
{
    char szBuffer[4096];
    va_list vl;
    va_start(vl, pszFormat);
    vsnprintf(szBuffer, sizeof(szBuffer), pszFormat, vl);
    va_end(vl);

#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    WORD wAttr = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
    BYTE r = (BYTE)(dwColor & 0xFF), g = (BYTE)((dwColor >> 8) & 0xFF), b = (BYTE)((dwColor >> 16) & 0xFF);
    if (r > 128 && g < 128 && b < 128) wAttr = FOREGROUND_RED | FOREGROUND_INTENSITY;
    else if (r < 128 && g > 128 && b < 128) wAttr = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
    else if (r > 128 && g > 128 && b < 128) wAttr = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
    else if (r < 128 && g > 128 && b > 128) wAttr = FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
    else if (r > 128 && g < 128 && b > 128) wAttr = FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
    SetConsoleTextAttribute(hOut, wAttr);
    printf("%s", szBuffer);
    SetConsoleTextAttribute(hOut, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
#else
    BYTE r = (BYTE)(dwColor & 0xFF), g = (BYTE)((dwColor >> 8) & 0xFF), b = (BYTE)((dwColor >> 16) & 0xFF);
    const char* pszAnsi = "\033[0m";
    if (r > 128 && g < 128 && b < 128) pszAnsi = "\033[1;31m";
    else if (r < 128 && g > 128 && b < 128) pszAnsi = "\033[1;32m";
    else if (r > 128 && g > 128 && b < 128) pszAnsi = "\033[1;33m";
    else if (r < 128 && g > 128 && b > 128) pszAnsi = "\033[1;36m";
    else if (r > 128 && g < 128 && b > 128) pszAnsi = "\033[1;35m";
    else if (r < 100 && g < 100 && b < 100) pszAnsi = "\033[0;90m";
    printf("%s%s\033[0m", pszAnsi, szBuffer);
#endif
    fflush(stdout);
}

BOOL CServerForm::ReadConsoleLine(std::string& line, BOOL bWait)
{
    line.clear();
#ifdef _WIN32
    (void)bWait;
    while (_kbhit())
    {
        int c = _getch();
        if (c == '\r')
        {
            printf("\n");
            return !line.empty();
        }
        if (c == '\b')
        {
            if (!line.empty()) line.erase(line.size() - 1);
            continue;
        }
        line.push_back((char)c);
    }
    return FALSE;
#else
    //	注意：这里不能用 fgets + select 混用，stdio 会把一次 read 的多行吞进缓冲区，
    //	select 之后就再也看不到这些数据，导致管道输入的命令延迟执行。
    if (m_bStdinClosed) return FALSE;

    if (m_LineBuffer.find('\n') == std::string::npos)
    {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(0, &readfds);
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = bWait ? 100000 : 0;

        int rc = select(1, &readfds, NULL, NULL, &tv);
        if (rc <= 0) return FALSE;

        char szBuffer[512];
        ssize_t nRead = read(0, szBuffer, sizeof(szBuffer));
        if (nRead <= 0)
        {
            //	stdin 关闭（例如后台运行、/dev/null）：不再读控制台，但服务器继续运行
            m_bStdinClosed = TRUE;
            return FALSE;
        }
        m_LineBuffer.append(szBuffer, (size_t)nRead);
        if (m_LineBuffer.find('\n') == std::string::npos) return FALSE;
    }

    size_t nPos = m_LineBuffer.find('\n');
    line = m_LineBuffer.substr(0, nPos);
    m_LineBuffer.erase(0, nPos + 1);
    if (!line.empty() && line[line.size() - 1] == '\r') line.erase(line.size() - 1);
    return !line.empty();
#endif
}

static CServerForm * g_pSignalForm = NULL;
static volatile sig_atomic_t g_bSignalStop = 0;
static void ServerSignalStop( int )
{
    g_bSignalStop = 1;
    if( g_pSignalForm != NULL ) g_pSignalForm->RequestStop();
}

int CServerForm::EnterMessageLoop()
{
    g_pSignalForm = this;
    std::signal( SIGTERM, ServerSignalStop );
    std::signal( SIGINT, ServerSignalStop );
    if (m_pServer != NULL && !m_pServer->IsRunning())
        m_pServer->Start();

    while (!m_bStop && !g_bSignalStop)
    {
        if (m_pServer != NULL)
        {
            if (!m_pServer->PumpNetwork()) break;
            m_pServer->Update();
            if (!m_pServer->IsRunning()) break;
        }

        std::string line;
        if (ReadConsoleLine(line, FALSE))
        {
            if (line == "exit" || line == "quit")
            {
                m_bStop = TRUE;
                break;
            }
            if (m_pInputListener != NULL)
                m_pInputListener->OnInputCommand(line.c_str());
        }

        Sleep(10);
    }

    if (m_pServer != NULL) m_pServer->Stop();
    return 0;
}
