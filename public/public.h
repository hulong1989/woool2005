// ===========================================================================
//  public/public.h  ——  游戏服务器公共层（重建版）
// ---------------------------------------------------------------------------
//  原始工程引用了三个不在快照中的外部库/资源：
//      ..\public\public.h    引擎公共库（基础类型、协议、容器、服务器基类）
//      ..\res\resource.h     资源 ID
//      ..\res\public.rc      资源脚本
//  本文件按现有源码中的实际用法，重建了这套公共层的最小可用接口：
//    · 基础数据类型、方向/坐标辅助、随机数、字符串工具
//    · 客户端-服务器消息协议（MIRMSG）、物品与角色数据结构
//    · xListHost 之外的两个容器：CIndexList（带 id 的对象池）、CNameHash/CIntHash
//    · ini/文本配置读取（CDataFile / CSettingFile / CStringFile）
//    · 底层连接对象 CClientObject，以及 CBaseServer / CServerForm 骨架
//
//  注意：这是“能编译、能跑起来”的骨架，不是原版商业实现。
//  消息编码、协议号、网络分帧格式都是重建的（见文件末尾说明）。
// ===========================================================================
#pragma once

// ---------------------------------------------------------------------------
// 平台头
// ---------------------------------------------------------------------------
#ifdef _WIN32
#  ifndef _WINSOCKAPI_
#    include <winsock2.h>
#  endif
#  include <windows.h>
#  include <mmsystem.h>
#  ifdef _MSC_VER
#    pragma comment(lib, "winmm.lib")
#    pragma comment(lib, "ws2_32.lib")
#    pragma warning(disable: 4996)   // 2005 年的代码用了 sprintf/strncpy 等
#  endif
#else
#  include <windows.h>               // 由 -Icompat 提供兼容实现
#endif

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include <vector>
#include <string>
#include <map>
#include <deque>

#include "localsupport.h"            // xListHost / xListNode

// ---------------------------------------------------------------------------
// 基本常量
// ---------------------------------------------------------------------------
#define SERVER_NAME             "GameServer"
#define MAX_CLIENT              1024
#define MAX_ENTER               256
#define MIR_MAX_PACKET          8192

enum SERVER_ERROR
{
    SE_OK = 0,
    SE_SERVERFULL,
    SE_NOTFOUND,
    SE_ERROR
};

// 客户端连接所处的阶段
enum gameserverusermode
{
    GUM_NONE = 0,
    GUM_VERIFING,
    GUM_ENTERGAME,
    GUM_PLAYING
};

// 跨服务器消息
#define MAS_ENTERGAMESERVER     0x0301
#define MST_SINGLE              0
#define SCM_FINDSERVER          0x0401
#define ST_SELCHARSERVER        1

// ---------------------------------------------------------------------------
// 客户端/服务器消息号
// （原版由客户端与服务端共用的协议头定义，这里按用途重建，数值仅保证唯一）
// ---------------------------------------------------------------------------
enum
{
    SM_SETMAP = 0x0101,
    SM_SETPLAYER,
    SM_SETPLAYERNAME,
    SM_SETMAPNAME,
    SM_READY,
    SM_APPEAR,
    SM_DISAPPEAR,
    SM_WALK,
    SM_RUN,
    SM_TURN,
    SM_ATTACK,
    SM_BEATTACK,
    SM_CHAT,
    SM_BACK,
    SM_UPDATEPROP,
    SM_DOWNITEMAPPEAR,
    SM_DOWNITEMDISAPPEAR,
    SM_ADDITEM,
    SM_EQUIPRESULT,
    SM_UNEQUIPRESULT,
    SM_ACTIONRESULT,
    SM_DEATH,
    SM_SPELL,
    SM_LEVELUP,
    SM_BAG,          // 背包快照
    SM_EQUIP,        // 装备快照
    SM_SHOP,         // 商店货架
    SM_SKILLS,       // 技能列表
    SM_QUESTS,       // 任务列表
    SM_MAPDATA,      // 玩家周围的地形窗口
    SM_TITLES,       // 称号收集
    SM_CODEX,        // 图鉴收集
    SM_DIALOG,       // NPC 对话（文本 + 选项）
    SM_AUTH,         // 登录成功后的会话 token
    SM_CHARLIST,     // 账号下的角色列表
    SM_PARTY,        // 队伍快照
    SM_GUILD,        // 行会快照
    SM_DUNGEONS,     // 副本列表/排行/实例
    SM_TRADE         // 玩家交易窗口
};

// ---------------------------------------------------------------------------
// 属性索引
// ---------------------------------------------------------------------------
enum PROP_INDEX
{
    PI_MINAC = 0,
    PI_MAXAC,
    PI_MINMAC,
    PI_MAXMAC,
    PI_MINDC,
    PI_MAXDC,
    PI_MINMC,
    PI_MAXMC,
    PI_MINSC,
    PI_MAXSC,
    PI_HITRATE,
    PI_ESCAPE,
    PI_MAGESCAPE,
    PI_POISONESCAPE,
    PI_ATTACKSPEED,
    PI_LUCKY,
    PI_DAWN,
    PI_HPRECOVER,
    PI_MPRECOVER,
    PI_POISONRECOVER,
    PI_HARD,
    PI_HOLLY,
    PI_LEVEL,
    PI_CURBAGWEIGHT,
    PI_MAXBAGWEIGHT,
    PI_CURHANDWEIGHT,
    PI_MAXHANDWEIGHT,
    PI_CURBODYWEIGHT,
    PI_MAXBODYWEIGHT,
    PI_CURHP,
    PI_CURMP,
    PI_MAXHP,
    PI_MAXMP,
    PI_PROP_COUNT
};

// 装备需求类型
enum
{
    INT_LEVEL = 0,
    INT_DC,
    INT_MC,
    INT_SC
};

// 装备位（共 10 格，与 Equipment.cpp 中的 10 一致）
enum
{
    _U_WEAPON = 0,
    _U_DRESS,
    _U_HELMET,
    _U_NECKLACE,
    _U_RINGL,
    _U_RINGR,
    _U_ARMRINGL,
    _U_ARMRINGR,
    _U_BELT,
    _U_BOOTS,
    _U_MAX
};

// ---------------------------------------------------------------------------
// 方向 / 坐标
// ---------------------------------------------------------------------------
enum e_direction
{
    ED_UP = 0,
    ED_UPRIGHT,
    ED_RIGHT,
    ED_DOWNRIGHT,
    ED_DN,
    ED_DOWNLEFT,
    ED_LEFT,
    ED_UPLEFT,
    ED_MAX
};

extern const int g_dir_dx[8];
extern const int g_dir_dy[8];

#define GETNEXTPOS(x, y, dir) \
    do { (x) += g_dir_dx[((dir) & 7)]; (y) += g_dir_dy[((dir) & 7)]; } while (0)

// ---------------------------------------------------------------------------
// 协议消息
// ---------------------------------------------------------------------------
struct MIRMSGHEADER
{
    DWORD dwFlag;        // 目标对象 id（0 表示广播）
    WORD  wCmd;          // 命令字 SM_xxx
    WORD  wParam[3];     // 三个短参数
    DWORD dwReserved;

    MIRMSGHEADER()
    {
        memset( this, 0, sizeof( *this ) );
    }
    MIRMSGHEADER( DWORD dwFlag_, WORD wCmd_, WORD w1, WORD w2, WORD w3 )
        : dwFlag( dwFlag_ ), wCmd( wCmd_ ), dwReserved( 0 )
    {
        wParam[0] = w1;
        wParam[1] = w2;
        wParam[2] = w3;
    }
};

struct MIRMSG
{
    DWORD dwFlag;
    WORD  wCmd;
    WORD  wParam[3];
    DWORD dwReserved;
    char  data[1];       // 变长负载
};

typedef MIRMSG* PMIRMSG;

struct SERVERADDR
{
    char addr[64];
    int  nPort;
};

struct ENTERGAMESERVER
{
    UINT   nClientId;
    UINT   nLoginId;
    UINT   nSelCharId;
    char   szAccount[11];
    DWORD  dwEnterTime;
    SERVER_ERROR result;
};

// ---------------------------------------------------------------------------
// 物品
// ---------------------------------------------------------------------------
struct ITEMBASE
{
    char  szName[16];
    BYTE  btNameLength;
    BYTE  btStdMode;
    BYTE  btShape;
    WORD  wImageIndex;
    BYTE  btWeight;
    BYTE  btSpecialpower;

    BYTE  btMinDef;
    BYTE  btMaxDef;
    BYTE  btMinMagDef;
    BYTE  btMaxMagDef;

    BYTE  btMinAtk;
    BYTE  btMaxAtk;
    BYTE  btMinMagAtk;
    BYTE  btMaxMagAtk;
    BYTE  btMinSouAtk;
    BYTE  btMaxSouAtk;

    WORD  wMaxDura;

    BYTE  needtype;
    BYTE  needvalue;

    //	重建时补充：药品使用（0 不可用 / 1 回 HP / 2 回 MP）
    BYTE  btUseType;
    WORD  wUseValue;

    int   nPrice;
};

struct ITEM
{
    DWORD    dwMakeIndex;   // 唯一实例 id，0 表示空
    ITEMBASE baseitem;
    WORD     wCurDura;
    WORD     wMaxDura;
    WORD     wCount;        // 堆叠数量（不可堆叠物品为 1）
    BYTE     btUpgradeLevel; // 强化等级（0-9）
    BYTE     btElement;      // 附魔元素：0 无 1 火 2 冰 3 雷 4 毒
    BYTE     btElementPower; // 元素强度 0-5
};

// ---------------------------------------------------------------------------
// 角色
// ---------------------------------------------------------------------------
struct CHARDBINFO
{
    DWORD  dwDBId;
    char   szName[32];
    BYTE   btClass;
    BYTE   btSex;
    BYTE   btHair;
    WORD   wLevel;

    WORD   x;
    WORD   y;
    UINT   mapid;

    WORD   hp;
    WORD   mp;
    WORD   maxhp;
    WORD   maxmp;

    DWORD  dwGold;
    DWORD  dwCurExp;

    int    handweight;
    int    weight;
    int    bodyweight;

    int    minac, maxac;
    int    minmac, maxmac;
    int    mindc, maxdc;
    int    minmc, maxmc;
    int    minsc, maxsc;
};

struct HUMANPROP
{
    WORD  wLevel;
    DWORD dwCurexp;
    DWORD dwMaxexp;
    WORD  wCurBagWeight;
    WORD  wCurHp;
    WORD  wCurMp;
    WORD  wMaxBagWeight;
    WORD  wMaxHp;
    WORD  wMaxMp;
    BYTE  btCurBodyWeight;
    BYTE  btCurHandWeight;

    BYTE  btMaxAtk;
    BYTE  btMinAtk;
    BYTE  btMaxDef;
    BYTE  btMinDef;

    BYTE  btMaxMagAtk;
    BYTE  btMinMagAtk;
    BYTE  btMaxMagicDef;
    BYTE  btMinMagicDef;

    BYTE  btMaxSprAtk;
    BYTE  btMinSprAtk;

    BYTE  btMaxBodyWeight;
    BYTE  btMaxHandWeight;

    DWORD dwUnknown1;
};

// ---------------------------------------------------------------------------
// 辅助函数（实现见 public.cpp）
// ---------------------------------------------------------------------------
char* o_strncpy(char* pszDst, const char* pszSrc, size_t nMaxChars);
int   SearchParam(char* pszString, char** ppszParams, int nMaxParam, char cSeparator);
int   Getrand(int nMax);              // [0, nMax)
DWORD o_GetTickCount(void);

int   _CodeGameCode(const BYTE* pSrc, int nLen, BYTE* pDst);
int   _UnGameCode(const BYTE* pSrc, int nLen, BYTE* pDst);
void  _UnGameCode(const char* pszSrc, BYTE* pHeader16);

// ---------------------------------------------------------------------------
// 容器：按 id 存取的对象池
//   New/Del/Get/First/Next，语义与原工程一致：id 从 1 开始，0 表示失败。
//   MAXCOUNT > 0 时限制容量；0 表示不限（可再调用 Create 设定容量）。
// ---------------------------------------------------------------------------
template <class T, int MAXCOUNT = 0>
class CIndexList
{
public:
    CIndexList() : m_capacity((UINT)MAXCOUNT), m_iter(-1) {}
    ~CIndexList() { Cleanup(); }

    CIndexList(const CIndexList&);
    CIndexList& operator=(const CIndexList&);

    UINT New(T** ppObject)
    {
        if (ppObject == NULL) return 0;
        UINT id = 0;
        if (!m_free.empty())
        {
            id = m_free.back();
            m_free.pop_back();
        }
        else
        {
            if (m_capacity != 0 && m_objs.size() >= m_capacity) return 0;
            m_objs.push_back(NULL);
            id = (UINT)m_objs.size();
        }
        T* pObject = new T();
        m_objs[id - 1] = pObject;
        *ppObject = pObject;
        return id;
    }

    BOOL Del(UINT id)
    {
        if (id == 0 || id > m_objs.size()) return FALSE;
        T* pObject = m_objs[id - 1];
        if (pObject == NULL) return FALSE;
        delete pObject;
        m_objs[id - 1] = NULL;
        m_free.push_back(id);
        return TRUE;
    }

    T* Get(UINT id)
    {
        if (id == 0 || id > m_objs.size()) return NULL;
        return m_objs[id - 1];
    }

    T* First()
    {
        m_iter = 0;
        return Next();
    }

    T* Next()
    {
        while (m_iter >= 0 && (size_t)m_iter < m_objs.size())
        {
            T* pObject = m_objs[m_iter++];
            if (pObject != NULL) return pObject;
        }
        m_iter = -1;
        return NULL;
    }

    // 预分配 size 个槽位（ItemBox 使用）
    BOOL Create(int nSize)
    {
        if (nSize <= 0) return FALSE;
        Cleanup();
        m_capacity = (UINT)nSize;
        m_objs.assign((size_t)nSize, (T*)NULL);
        for (int i = nSize; i >= 1; --i) m_free.push_back((UINT)i);
        return TRUE;
    }

    // 清空对象，保留槽位容量
    VOID Clean()
    {
        for (size_t i = 0; i < m_objs.size(); ++i)
        {
            delete m_objs[i];
            m_objs[i] = NULL;
        }
        m_free.clear();
        for (int i = (int)m_objs.size(); i >= 1; --i) m_free.push_back((UINT)i);
        m_iter = -1;
    }

    int GetFreeCount() const
    {
        size_t extra = (m_capacity > m_objs.size()) ? (m_capacity - m_objs.size()) : 0;
        return (int)(m_free.size() + extra);
    }

    int getCount() const
    {
        int n = 0;
        for (size_t i = 0; i < m_objs.size(); ++i)
            if (m_objs[i] != NULL) ++n;
        return n;
    }

    int size() const { return (int)m_objs.size(); }

private:
    VOID Cleanup()
    {
        for (size_t i = 0; i < m_objs.size(); ++i) delete m_objs[i];
        m_objs.clear();
        m_free.clear();
        m_iter = -1;
    }

    std::vector<T*>  m_objs;
    std::vector<UINT> m_free;
    UINT m_capacity;
    int  m_iter;
};

// 字符串 -> 指针 的哈希表
class CNameHash
{
public:
    BOOL   HAdd(const char* pszName, LPVOID pValue);
    LPVOID HGet(const char* pszName);
    BOOL   HDel(const char* pszName);
    VOID   Clear();
    int    GetCount() const { return (int)m_map.size(); }

private:
    std::map<std::string, LPVOID> m_map;
};

// 整数 -> 整数 的哈希表（登录 id -> 进入对象 id）
class CIntHash
{
public:
    BOOL HAdd(UINT nKey, UINT nValue);
    UINT HGet(UINT nKey);
    BOOL HDel(UINT nKey);
    VOID Clear();

private:
    std::map<UINT, UINT> m_map;
};

// ---------------------------------------------------------------------------
// 配置文件 / 文本文件
// ---------------------------------------------------------------------------
class CDataFile
{
public:
    CDataFile();
    ~CDataFile();

    BOOL Open(const char* pszFilename);
    VOID Close();
    BOOL IsOpen() const { return m_bOpened; }

    // 返回的指针在文件关闭前一直有效（值缓存在 map 中）
    const char* GetString(const char* pszSection, const char* pszKey, const char* pszDefault) const;
    int         GetInteger(const char* pszSection, const char* pszKey, int nDefault) const;

private:
    BOOL m_bOpened;
    std::map<std::string, std::string> m_values;   // "section/key" -> value
    std::string m_szFilename;
};

class CSettingFile : public CDataFile
{
public:
    BOOL GetBool(const char* pszSection, const char* pszKey, BOOL bDefault) const
    {
        int n = GetInteger(pszSection, pszKey, bDefault ? 1 : 0);
        return n != 0;
    }
};

class CStringFile
{
public:
    CStringFile();
    explicit CStringFile(const char* pszFilename);
    ~CStringFile();

    BOOL Open(const char* pszFilename);
    VOID Close();

    int GetLineCount() const { return (int)m_lines.size(); }
    const char* operator[](int nIndex) const;

private:
    std::vector<std::string> m_lines;
};

// ---------------------------------------------------------------------------
// 连接对象 / 服务器基类
// ---------------------------------------------------------------------------
class CClientObject
{
public:
    CClientObject();
    virtual ~CClientObject();

    virtual VOID Clean();
    virtual VOID Update();

    // 业务层实现
    virtual VOID OnConnect();
    virtual VOID OnDisconnect();
    virtual VOID OnCodedMsg(PMIRMSG pMsg, int datasize);
    virtual VOID OnUnCodeMsg(const char* pszMsg, int size);
    virtual VOID OnDBMsg(PMIRMSG pMsg, int datasize);
    virtual VOID OnVerifyString(const char* pszString);

    // 发送：pszMsg 为已经 _CodeGameCode 编码过的整包（MapObject::EncodeMsg 的输出）
    VOID PostMsg(const char* pszMsg, int nSize);
    VOID SendMsg(DWORD dwFlag, WORD wCmd, WORD w1, WORD w2, WORD w3, LPVOID lpdata, int datasize = -1);
    VOID SendRaw(const void* pData, int nSize);

    UINT   GetId() const { return m_Id; }
    VOID   SetId(UINT id) { m_Id = id; }
    SOCKET GetSocket() const { return m_Socket; }
    VOID   SetSocket(SOCKET s);
    BOOL   IsConnected() const { return m_Socket != INVALID_SOCKET; }

    // 引擎内部使用
    VOID PumpRecv();
    VOID FlushSend();
    VOID CloseSocket();

private:
    VOID OnFrame(const char* pData, int nSize);

    UINT   m_Id;
    SOCKET m_Socket;
    std::string m_RecvBuffer;
    std::string m_SendBuffer;
};

// 与 SelectChar 服务器的连接（骨架：仅记录，不实现跨服逻辑）
class CSCClientObj
{
public:
    CSCClientObj();
    ~CSCClientObj();

    BOOL IsReady() const { return m_bReady; }
    VOID Update();

    BOOL SendMsgAcrossServer(UINT nClientId, WORD wCmd, WORD wType, WORD wIndex,
                             const char* pszData, int datasize);

private:
    BOOL m_bReady;
};

// 与 DB 服务器的连接（骨架：物品落库请求在这里被记录）
class CDBClientObj
{
public:
    CDBClientObj();
    ~CDBClientObj();

    BOOL IsReady() const { return m_bReady; }
    VOID Update();

    BOOL SendCreateItem(DWORD dwId, DWORD dwKey, DWORD dwOwner, BYTE btFlag, WORD wPos, ITEM* pItem);

private:
    BOOL m_bReady;
};

class CBaseServer;
class CServerForm;

class CInputListener
{
public:
    virtual ~CInputListener() {}
    virtual VOID OnInputCommand(const char* pszCommand) = 0;
};

class CBaseServer
{
public:
    CBaseServer();
    virtual ~CBaseServer();

    // ---- 生命周期 ----
    BOOL Start(const char* pszConfigFile = "gameserver.ini");
    VOID Stop();
    VOID RequestStop() { m_bRunning = FALSE; }
    BOOL IsRunning() const { return m_bRunning; }

    BOOL PumpNetwork();          // accept + 收包 + 发包

    // ---- 可重载的业务钩子 ----
    virtual BOOL InitServer(CSettingFile& s);
    virtual VOID CleanServer();
    virtual VOID Update();
    virtual VOID OnDBMsg(PMIRMSG pMsg, int datasize);
    virtual VOID OnMASMsg(WORD wCmd, WORD wType, WORD wIndex, const char* pszData, int datasize);
    virtual VOID OnSCMsg(PMIRMSG pMsg, int datasize);
    virtual VOID OnSCServerReady();
    virtual CClientObject* NewClientObject();
    virtual VOID DeleteClientObject(CClientObject* pObject);

    // ---- 基础设施 ----
    VOID SetServerName(const char* pszName);
    const char* GetServerName() const { return m_ServerName.c_str(); }
    VOID SetIoConsole(CServerForm* pConsole) { m_pIoConsole = pConsole; }
    CServerForm* GetIoConsole() const { return m_pIoConsole; }
    VOID SetInputListener(CInputListener* pListener) { m_pInputListener = pListener; }

    CDBClientObj* GetDBConnection(int nIndex);
    CSCClientObj* GetSCConnection() { return &m_SCClientObj; }
    VOID UpdateSCServer();
    VOID UpdateDBServer();

    CClientObject* GetClientObject(UINT id);
    VOID KickClient(UINT id);
    int  GetClientCount() const { return (int)m_Connections.size(); }

    VOID Log(DWORD dwColor, const char* pszFormat, ...);

protected:
    CSCClientObj  m_SCClientObj;
    CDBClientObj  m_DBClientObj;
    CServerForm*  m_pIoConsole;
    CInputListener* m_pInputListener;
    std::string   m_ServerName;

private:
    BOOL OpenListenSocket(const char* pszAddr, WORD wPort);
    VOID AcceptClients();
    VOID PumpClients();

    std::vector<CClientObject*> m_Connections;
    SOCKET m_ListenSocket;
    BOOL   m_bRunning;
    BOOL   m_bInited;
};

// 控制台窗体（原工程是 MFC 风格窗口，这里以控制台重建，保持同样的调用接口）
class CServerForm
{
public:
    CServerForm();
    ~CServerForm();

    BOOL Create(const char* pszTitle);
    VOID InitToolbar(int nToolbarId) { (void)nToolbarId; }
    VOID InitStatueBar() {}
    VOID SetInputListener(CInputListener* pListener) { m_pInputListener = pListener; }
    VOID SetServer(CBaseServer* pServer) { m_pServer = pServer; }

    int  EnterMessageLoop();
    VOID RequestStop() { m_bStop = TRUE; }

    // 颜色沿用 RGB(r,g,b)，在控制台里映射为 ANSI 颜色
    VOID OutPut(DWORD dwColor, const char* pszFormat, ...);
    VOID Clear();

private:
    BOOL ReadConsoleLine(std::string& line, BOOL bWait);

    CBaseServer*    m_pServer;
    CInputListener* m_pInputListener;
    std::string     m_Title;
    std::string     m_LineBuffer;	//	POSIX 控制台行缓冲（不能和 stdio 混用）
    BOOL            m_bStdinClosed;	//	控制台输入关闭后继续作为服务端运行
    BOOL            m_bStop;
};

// ---------------------------------------------------------------------------
// 协议说明（重建部分）
// ---------------------------------------------------------------------------
// 1. MapObject::EncodeMsg 的输出格式：
//        '#' + CodeGameCode(MIRMSGHEADER, 16 字节) + CodeGameCode(payload) + '!'
//    本实现中 CodeGameCode 为逐字节可逆异或（见 public.cpp），
//    真客户端使用另一套加密/压缩，接入真实客户端时只需替换这两个函数。
//
// 2. 网络分帧：每个整包前加 4 字节小端长度（引擎内部约定），
//    因此 CClientObject 收发与 EncodeMsg 的 '#'/'!' 包装互不冲突。
//
// 3. 服务器拓扑（与原工程一致）：
//        LoginServer -> SelectCharServer(SC) -> GameServer(本程序) -> DBServer
//    SC/DB 连接在骨架中为可选，未配置时服务器仍可独立启动并接受直连客户端。
// ---------------------------------------------------------------------------
