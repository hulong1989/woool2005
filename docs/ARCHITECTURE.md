# 架构（ARCHITECTURE）

> 本文描述当前工作树中的 C++ 服务端实现，面向需要改代码或排查问题的读者。
> 所有行号均按当前源码核对；路径相对于仓库根目录。背景与逐项改动见
> [docs/RECONSTRUCTION.md](RECONSTRUCTION.md)。

## 1. 定位与边界
### 1.1 三个代码来源

| 来源 | 位置 | 特征 | 在系统中的作用 |
|---|---|---|---|
| 2005 原始快照 | `original_gbk/` | 40 个 `.cpp/.h`，GBK，VC++ 7.1 工程；只含服务端逻辑层 | 被重建、被转换为 UTF-8 的母本；仓库中保持原样，不参与编译 |
| 重建层 | `public/`、根目录缺失头文件、`gsClientObj.cpp` | 按调用点补齐的引擎公共层与业务实现 | 让快照可编译、可启动、可接受连接 |
| 新增玩法 | 根目录 `Monster/Npc/Quest/Guild/Boss/Dungeon/...` 等 | 快照中完全没有的系统 | 把骨架扩展成可单机运行的 MMO 服务端 |

原始快照缺什么，按 `GameServer.vcproj` 的 `<File>` 列表与所有 `#include` 的并集确定：

| 缺失件 | 性质 | 重建结果 |
|---|---|---|
| `public/public.h`、`public/public.cpp`；`res/resource.h`、`res/public.rc` | 引擎公共库与资源 ID | 类型、协议、容器、ini/文本读取、TCP、控制台全部重建；资源目录保留，骨架不编译资源 |
| `Server.h`、`GameServer.h`、`LogicMap.h`、`Area.h`、`PhysicsMap.h/.cpp`、`ItemBox.h` | 快照缺失的类定义/实现 | 按调用点重建（`Server.h:9-53`、`LogicMap.h:11-97`、`Area.h:8-56`、`PhysicsMap.h:7-16`、`ItemBox.h:8-48`） |
| `gsClientObj.cpp` | 单连接业务（登录/进图/分发） | 重建（`gsClientObj.cpp:1-1820`） |
### 1.2 重建原则

1. **以调用点为准**：先用空实现让全部 `#include` 与成员访问成立，再按语义补行为；编译器是规格。
2. **保持原接口**：类名、函数签名、数据流、`xListHost` 链表用法不改，只补缺失定义。
3. **不确定处显式标注**：协议号、消息编码、TCP 分帧、`.nmp` 格式均为重建，代码注释与文档写明。
4. **对原代码只修确定错误**：语法错误、未初始化、越界判断、所有权问题；不改玩法数值与流程。

依据见 [docs/RECONSTRUCTION.md:31-36](RECONSTRUCTION.md)。一个直接后果是：本服务端**不能直连 2005 年真实客户端**（见 `docs/PROTOCOL.md` 第 11 节）。

当前数据规模（核对时）：89 个根目录 C++ 文件、60 张逻辑地图、27 张物理阻挡图、151 件物品、95 种怪物、50 个 NPC、8 个副本（24 个实例地图）、42 个 Boss 刷新点、35 个技能。

---

## 2. 分层图

```
客户端/工具（game_client.py / desktop_client.py / test_client.py / web_game.py）
        │ TCP 7100：[4B LE 长度]['#' + XOR 头 + XOR 负载 + '!']
┌───────▼────────────────────────────────────────────────────────────┐
│ 入口 GameServer.cpp::_tWinMain                                     │
│   g_Form.Create → g_pServer = CServer::GetInstance() → EnterMessageLoop │
├────────────────────────────────────────────────────────────────────┤
│ CServerForm（public/public.cpp）：PumpNetwork → CServer::Update → 控制台 → Sleep │
├────────────────────────────────────────────────────────────────────┤
│ CBaseServer / CClientObject（public/public.cpp）：listen/accept、非阻塞收发、拆包 │
├────────────────────────────────────────────────────────────────────┤
│ CClientObj（gsClientObj.cpp）：文本命令、二进制 SM_*、登录/选角/快照/GM │
├────────────────────────────────────────────────────────────────────┤
│ CGameWorld + 各 Mgr：玩家/怪物/NPC/物品/存档/副本/行会/活动          │
├────────────────────────────────────────────────────────────────────┤
│ 地图：CLogicMapMgr → CLogicMap → CArea（九宫格）                     │
│       CPhysicsMapMgr → CPhysicsMap（阻挡图）                         │
├────────────────────────────────────────────────────────────────────┤
│ 对象：CMapObject → CAliveObject → CHumanPlayer / CMonster            │
│       CMapObject → CNpc / CDownItemObject                           │
├────────────────────────────────────────────────────────────────────┤
│ 数据：data/*.txt + data/*.dat + data/players/*.dat                  │
└────────────────────────────────────────────────────────────────────┘
     CSCClientObj / CDBClientObj：选人服/DB 桩，只打印日志
```

关键边界：

- `public/` 只依赖标准库与 `compat/`（非 Windows 的 Win32 兼容头），不依赖具体玩法。
- `CClientObj` 依赖所有玩法 Mgr，是“协议 → 领域”的唯一适配层；玩法层不反向依赖 `CClientObj`。
- `CGameWorld` 组合地图、玩家、怪物、NPC、物品模板，不直接持有网络连接。
- SC/DB 是桩：`CSCClientObj::SendMsgAcrossServer`、`CDBClientObj::SendCreateItem` 只打印日志（`public/public.cpp:546-570`）。

---

## 3. 启动顺序与每帧调用链
### 3.1 进程启动

入口只有一条路径（`GameServer.cpp:7-23`）：`g_Form.Create` → `InitToolbar/InitStatueBar` → `CServer::GetInstance()` → `SetServerName/SetIoConsole/SetInputListener/SetServer` → `EnterMessageLoop()`（注册 `SIGTERM/SIGINT`，首次进入时 `Start()`，`public/public.cpp:944-950`）。

`CBaseServer::Start` 的顺序固定（`public/public.cpp:680-711`）：读 `gameserver.ini`（失败用 `7100/0.0.0.0`）→ `SetServerName` → `InitServer`（失败只警告，**仍继续启动**）→ `OpenListenSocket`（端口占用则离线运行）→ 启动横幅。

`CServer::InitServer` 的真实加载顺序（`Server.cpp:50-91`）：

| # | 加载内容 | 数据文件 |
|---|---|---|
| 1 | 职业成长 + 等级经验（`CPlayerClassMgr`） | `data/classes.txt`、`data/levels.txt` |
| 2 | 技能表（`CSkillMgr`） | `data/skills.txt` |
| 3 | 世界（`CGameWorld::Init`：物理图 → 逻辑图 → `baseitem.txt`） | 见下 |
| 4 | 怪物模板 / 多掉落 / 刷怪点（`CMonsterMgr`） | `monsters.txt`、`drops.txt`、`spawns.txt` |
| 5 | NPC / 任务（`CNpcMgr`、`CQuestMgr`） | `npcs.txt`、`quests.txt` |
| 6 | 行会 + 仓库 / 套装 / 成就 / 称号 / GM | `guilds.dat`、`guildware.dat`、`sets.txt`、`achievements.txt`、`titles.txt`、`gm.txt` |
| 7 | Boss + 首杀（`CBossMgr`） | `bosses.txt`、`boss_kills.dat` |
| 8 | 副本 + 赛季 + 首通 + 排行（`CDungeonMgr`） | `dungeons.txt`、`season.dat`、`dungeon_first.dat`、`dungeon_rank.dat` |
| 9 | 对话树 / 活动排期（`CDialogueMgr`、`CEventMgr`） | `dialogues.txt`、`events.txt` |

`CGameWorld::Init` 内部再分三步（`GameWorld.cpp:20-44`）：`CPhysicsMapMgr::Load(".\\data\\maps\\physics")`（先）→ `CLogicMapMgr::Load(".\\data\\maps\\logic")`（逻辑图按 `blockmap` 查物理图，必须后）→ `CItemManager` + `CStringFile("data/baseitem.txt")`。路径中的反斜杠由 `CDataFile`/`CStringFile` 归一化为 `/`（`public/public.cpp:183-189`）。
### 3.2 每帧 Update

主循环在 `CServerForm::EnterMessageLoop`（`public/public.cpp:952-974`），每一轮：

```
PumpNetwork()
  ├─ AcceptClients()                    accept 循环，新建 CClientObj
  └─ PumpClients()
       ├─ PumpRecv() → OnFrame         非 '#'/不足 18B → OnUnCodeMsg（文本）
       │                               '#'+16B 头+负载+'!' → OnCodedMsg → ProcClientMsg
       ├─ FlushSend()；断开 → OnDisconnect → 删除连接
CServer::Update()
  ├─ 客户端 Update()（仅 FlushSend）；SC/DB 桩 Update
  ├─ CGameWorld::Update()              测试角色随机走 + 真实玩家 Update
  ├─ CMonsterMgr / CBossMgr / CDungeonMgr Update
  ├─ HotReloadData()                   距上次 >1000ms（Server.cpp:176-184）
  ├─ CEventMgr::Update()；全员 RegenTick（3 秒 5%）
  └─ 距上次 >60000ms → SaveAll()（Server.cpp:204-211）
读控制台一行 → OnInputCommand
Sleep(10)
```

时序上的注意点：

- **收包先于逻辑更新**：客户端命令在 `PumpClients` 内就调用 `Walk/Attack/CastSkill`，`PostMsg` 内部直接 `FlushSend`（`public/public.cpp:390-396`）。
- **动作不是即时完成**：`Walk` 只设置动作与目标坐标，`Update` 在计时结束后调用 `CompleteAction` 才改坐标（`AliveObject.cpp:227-249`）。
- **定时器全部轮询**：没有独立线程；自然恢复、Boss、副本、自动存档、热更新都挂在主循环。
- **退出路径**：`exit/quit` 或信号 → `Stop()` → 每个连接 `OnDisconnect`（存档）→ `CleanServer()`（全量存档 + 行会/仓库/Boss/副本/赛季落盘）（`public/public.cpp:944-978`、`Server.cpp:93-107`）。

---

## 4. 对象模型
### 4.1 继承关系

```
CMapObject                        地图对象基类：坐标、地图/区域指针、3 条链表节点
├── CAliveObject                  可行动对象：方向、动作状态、HP/MP、毒伤、宠物链
│   ├── CHumanPlayer              玩家：CHARDBINFO + 背包/装备/任务/技能/成就/称号/图鉴
│   └── CMonster                  怪物：模板字段、目标 id、死亡/宠物标记
├── CNpc                          NPC：类型（商店/传送/副本入口）、目标点、对话
└── CDownItemObject               地面物品：ITEM 实例 + 归属 id
```

- `CMapObject` 定义见 `MapObject.h:14-61`；它含有 `m_pMap`、`m_pArea`、`m_wX/m_wY` 和 `m_DLinkNodes[LNI_MAX]`。`CNpc` 不继承 `CAliveObject`（`Npc.h:8-9`），只占格与对话；`CDownItemObject` 也直接继承 `CMapObject`，且没有类型位 id。
### 4.2 对象 id 编码

玩家、怪物、NPC 的对象 id 由“高 8 位类型 + 低 24 位对象池 id”组成：

```
id = pool_id | (OBJ_TYPE << 24)
```

| 类型 | 值 | 产生位置 | 解码 |
|---|---|---|---|
| `OBJ_UNKNOWN` / `OBJ_GROUNDMAGIC` | 0 / 1 | — / 预留 | — |
| `OBJ_DOWNITEM` | 2 | 地面物品 | 仅用池 id，不用类型位；`SM_DOWNITEMAPPEAR` 用 `dwMakeIndex` 作为 `dwFlag`（`DownItemObject.cpp:20-22`） |
| `OBJ_MONSTER` | 3 | `Monster.cpp:423` | `FindById` 直接比较完整 id |
| `OBJ_NPC` | 4 | `Npc.cpp:110` | 直接比较完整 id |
| `OBJ_PLAYER` | 5 | `HumanPlayerMgr.cpp:40-41` | `FindbyId` 先 `id & 0xffffff` 再查池（`HumanPlayerMgr.cpp:21-26`） |

类型枚举在 `vMap.h:20-28`。例：玩家池 id 1 → `0x05000001`；怪物池 id 1 → `0x03000001`。注意连接 id 与玩家对象 id 是两套东西：`CClientObject::GetId()` 是连接池 id（`Server.cpp:34-42`），`CHumanPlayer::GetId()` 才带类型位；`SM_ACTIONRESULT` 用的是连接 id（`gsClientObj.cpp:888-891`）。池 id 会被回收复用，对象 id 不是永久唯一标识；物品的 `dwMakeIndex` 才近似单调（`ItemManager.cpp:52-57`）。
### 4.3 `CIndexList` 对象池语义

`CIndexList<T, MAXCOUNT>` 是带 id 的对象池（`public/public.h:401-519`）：

| 操作 | 语义 |
|---|---|
| `New(&p)` | 取空闲 id（LIFO）或追加槽位；`new T`；id 从 1 开始，0 表示失败 |
| `Del(id)` | `delete` 对象、槽位置空、id 推入空闲表 |
| `Get(id)` | 按 id 取指针；id 越界/空槽返回 `NULL` |
| `First()/Next()` | 有状态迭代：`First` 重置迭代器，`Next` 跳过空槽 |
| `Create(n)` / `Clean()` | 预分配 n 槽 / 删除对象但保留槽位容量 |

容量（定义处）：客户端 1024、进服账号 256（`public/public.h:55-56`、`Server.h:49-50`）；玩家 128（`HumanPlayerMgr.h:3,35`）；怪物 4096（`Monster.h:114`）；NPC 256（`Npc.h:57`）；地面物品 20000（`DownItemMgr.h:24`）；逻辑地图 1024（`LogicMapMgr.h:4,36`）；背包 46 格（`HumanPlayer.cpp:24`）；堆叠上限 99（`ItemBox.h:6`）。
### 4.4 三条链（`LNI_AREA / LNI_MAP / LNI_WORLD`）

每个 `CMapObject` 在构造时创建 3 个链表节点（`MapObject.cpp:5-12`），由 `xListHost` 持有；`addNode` 会先让节点离开旧宿主（`localsupport.h:86-99`），所以**一个节点同一时刻只能属于一个宿主**。

| 链 | 宿主 | 加入时机 | 用途 |
|---|---|---|---|
| `LNI_AREA` | `CArea::m_xObjList` | `CLogicMap::AddObject` → `pArea->AddObject`（`LogicMap.cpp:29-51`） | 本区域广播/查找 |
| `LNI_MAP` | `CLogicMap::m_xObjList` | 同上 | 全图广播/查找 |
| `LNI_WORLD` | `CGameWorld::m_xTestPlayerList` | 仅 `CGameWorld::AddTestPlayer`（`GameWorld.cpp:82-86`） | 控制台测试角色的全量遍历 |

真实客户端玩家**只进 AREA + MAP**：`CGameWorld::AddPlayer` 不碰 `LNI_WORLD`（`GameWorld.cpp:63-71`），`RemovePlayer` 只从地图移除（`GameWorld.cpp:73-80`）。测试角色（`addbot`）才走 `AddTestPlayer`，所以控制台的随机走动只作用于测试角色。

生命周期：`PlayerFactory_Create → NewPlayer/Init → AddPlayer → 地图/区域 AddObject → OnEnterMap（发进图包）`；断线/删除：`OnDisconnect → Save → RemovePlayer → 地图/区域 RemoveObject → OnLeaveMap（发消失包）`。

---

## 5. 地图、区域与视野
### 5.1 物理图与逻辑图

| 层 | 类 | 加载器 | 文件 | 职责 |
|---|---|---|---|---|
| 阻挡 | `CPhysicsMap` | `CPhysicsMapMgr` | `data/maps/physics/*.nmp` | 宽高、阻挡位图、名字 |
| 逻辑 | `CLogicMap` | `CLogicMapMgr` | `data/maps/logic/*.ini` | 引用物理图、划分 Area、对象列表、锁位图 |

`CLogicMap::LoadMap` 只读取 `[define]` 的 `blockmap`（物理图名，缺失则失败）、`name`（默认 `NoName`）、`areawidth`/`areaheight`（默认 `MAP_AREA_WIDTH/HEIGHT=1`，`vMap.h:30-31`）；`title` 由 `GetTitle` 懒读并回退到 `name`（`LogicMap.cpp:174-203`、`LogicMap.h:52-57`）。

**当前实现不读取 `type=`**：`m_Type` 只在声明处初始化为 `EMT_NOFIGHT`（`LogicMap.h:88`），`m_Flag` 初始化为 0（`LogicMap.h:87`）且没有赋值点。因此 `TestFlag(EMF_*)` 恒为假，`EMF_CANWALKTHROUGH` 的“穿人”分支永远不会启用；实际“战斗地图”由进图时硬编码的 `0x2c4` 包宣布（`HumanPlayer.cpp:137`）。`data/maps/logic/*.ini` 里 60 个文件都写了 `type=1`，但它目前是保留字段。
### 5.2 坐标与 Area 换算

设逻辑图宽 `W`、高 `H`，Area 块宽 `aw`、块高 `ah`：

```
areaCols = ceil(W / aw)                  // LogicMap.cpp:86
areaRows = ceil(H / ah)                  // LogicMap.cpp:87
areaX    = x / aw                        // 整数除，LogicMap.cpp:120-128
areaY    = y / ah
areaIndex = areaY * areaCols + areaX     // LogicMap.cpp:113
```

`CArea` 保存 `m_dwAreaIndex`、`m_wAreaX`、`m_wAreaY` 和 `m_pAroundArea[8]`（`Area.h:49-55`）；8 个邻居在 `InitAreas` 中用 `GETNEXTPOS` 按 8 方向建立（`LogicMap.cpp:102-115`）。数据里 `areawidth=areaheight=32`，所以 512×512 的图有 16×16=256 个 Area。
### 5.3 九宫格视野

`CArea::SendAroundMsg` = 自己 + 8 个邻居（`Area.cpp:26-36`）；每个区域内部再遍历 `m_xObjList`，只发给 `CanRecvMsg()` 且不是发送者的对象（`Area.cpp:13-24`）。

| 动作 | 实现 | 结果 |
|---|---|---|
| 进入地图 | `CHumanPlayer::OnEnterMap` 对自身区域 + 8 邻居调用 `AppearFor(this)`（`HumanPlayer.cpp:141-148`） | 自己收到周围实体的 `SM_APPEAR` |
| 自己出现 | `CMapObject::OnEnterMap` 调 `SendAroundMsg`（`MapObject.cpp:79-91`） | 自己周围 9 个区域收到自己的 `SM_APPEAR`（自己除外） |
| 离开地图 | `CMapObject::OnLeaveMap` → `SendAroundMsg(SM_DISAPPEAR)`（`MapObject.cpp:92-106`） | 周围看到自己消失 |
| 区域切换 | `CLogicMap::ChangeArea`（`LogicMap.cpp:233-334`） | 邻接区域只处理“新增/消失的环”，跳跃则全量消失+出现 |
| 目标查找 | `CArea::FindObject(x,y,type)` 线性遍历（`Area.cpp:105-118`） | 同格同类型的第一个对象 |

跨 Area 的优化表 `g_outviewarea[8][6]` 在 `LogicMap.cpp:222-232`：当从当前区域进入相邻方向 `itoDir` 时，只对旧区域中“不再可见”的 3–5 个邻区发消失；进入新区域后方向翻转 `(itoDir+4)%8`，只对“新可见”的邻区发出现。
### 5.4 `LockPos` 位图

`CLogicMap` 为每个格子维护 1 bit 的占用/锁定位图（`LogicMap.h:32-49`）：

```cpp
idx  = y * m_iWidth + x;
mask = 1u << (idx & 31);
m_pLockLayer[idx >> 5] |= mask;    // LockPos
m_pLockLayer[idx >> 5] &= ~mask;   // UnLockPos
```

- 位图大小 `(W*H + 31) / 32` 个 `DWORD`，在 `SetPhysicsMap` 中分配并清零（`LogicMap.cpp:205-221`）。
- `IsBlocked(x,y)` = 越界 || 锁定 || 物理阻挡（`LogicMap.cpp:166-172`）。
- 玩家/怪物进入地图时锁当前格（`AliveObject.cpp:296-300`）；移动完成时先解旧格、再锁目标格（`AliveObject.cpp:227-244`）。NPC 进入时锁自己的格（`Npc.cpp:58-64`），所以不能穿过 NPC。
- 一个已知细节：`CompleteAction` 没有检查 `LockPos` 的返回值，目标格在“预检查”和“完成”之间被别人锁住时仍会 `setXY` 过去（`AliveObject.cpp:239-241`）。物理阻挡在 `Walk/Run` 的预检查里已经拦掉（`AliveObject.cpp:46-75`），所以影响主要是“穿人”窗口，而不是穿墙。

---

## 6. 属性系统
### 6.1 两套结构

| 结构 | 所在 | 用途 | 关键字段 |
|---|---|---|---|
| `CHARDBINFO` | `public/public.h:319-349`，120 字节 | 持久化“基础值” | 名字、职业、性别、发型、等级、坐标、地图、HP/MP/上限、金币、经验、负重、AC/MAC/DC/MC/SC |
| `HUMANPROP` | `public/public.h:351-382`，44 字节 | 下行镜像（`SM_UPDATEPROP` 负载） | 等级、经验/升级所需、背包/腕力/体重、HP/MP、攻击/防御/魔法/道术 |

`HUMANPROP` 的字段偏移被客户端硬编码读取（`client/game_client.py:447-456`），因此**不要在不改客户端的情况下调整字段顺序**。
### 6.2 `GetPropValue` 的实时合成

`CHumanPlayer::GetPropValue`（`HumanPlayer.cpp:866-921`）每次调用都重新合成，不缓存最终值（装备层自己有缓存）：

```
最终值 = 职业基础值(CHARDBINFO) + 装备属性(CEquipment::GetProp) + Buff + 称号加成
```

| 属性组 | 基础值来源 | 装备 | Buff | 称号 |
|---|---|---|---|---|
| AC / MAC | `dbinfo.min/maxac`、`min/maxmac` | 是 | `GetBuffValue(1/2)`（神圣战甲/幽灵盾） | 是 |
| DC / MC / SC | `dbinfo.min/maxdc`、`mc`、`sc` | 是 | 否 | 是 |
| 命中/躲避/攻速/幸运等 | 0 | `m_Equipments.GetProp` | 否 | 是 |
| `PI_CURHP/MAXHP` | `dbinfo.hp/maxhp` | 否 | 否 | 是（称号 HP） |
| `PI_CURMP/MAXMP` | `dbinfo.mp/maxmp` | 否 | 否 | 是（称号 MP） |
| 背包/腕力/体重 | 背包重量 / 装备重量 / `dbinfo` 上限 | — | 否 | 否（称号无这些索引） |

末尾统一 `sRet += GetTitleBonus(index)`（`HumanPlayer.cpp:919`）。装备属性缓存由 `CEquipment::ResetPropCache` 在穿/脱/强化/读档恢复后重算，并同时应用套装加成（`Equipment.cpp:329-354`）。
### 6.3 `ApplyClassStats` 的调用时机

`ApplyClassStats`（`HumanPlayer.cpp:318-346`）按 `data/classes.txt` 的成长值从 1 级重算基础属性：

```
maxhp=50+hpGrow*(L-1)      maxmp=20+mpGrow*(L-1)
minac=0+minAcGrow*(L-1)    maxac=1+maxAcGrow*(L-1)
mindc=1+minDcGrow*(L-1)    maxdc=2+maxDcGrow*(L-1)
minmc=0+mcGrow*(L-1)       maxmc=(class==法师?1:0)+mcGrow*(L-1)
minsc=0+scGrow*(L-1)       maxsc=(class==道士?1:0)+scGrow*(L-1)
weight=300+weightGrow*(L-1)  handweight=50+2*(L-1)
bodyweight=60+3*(L-1)        hp/mp=maxhp/maxmp
```

调用点只有两处：

1. **新角色初始化**：`CHumanPlayer::Init` 中 `desc.dbinfo.wLevel == 0` 时（`HumanPlayer.cpp:37-55`）。
2. **升级**：`AddExp` 结算 `nLevelUps > 0` 后（`HumanPlayer.cpp:286-313`）。

读档角色**不会**在登录时调用 `ApplyClassStats`，而是直接使用存档中的 `CHARDBINFO` 基础值（`PlayerFactory.cpp:36-56`）。因此改 `classes.txt` 不会追溯修改老角色，直到它再次升级。
### 6.4 `UpdateProp` 的发送时机

`UpdateProp` 组装 `HUMANPROP` 并发送 `SM_UPDATEPROP`；`dwFlag` 是金币，`wParam[0] = hair << 8 | class`，`wParam[1] = sex`（`HumanPlayer.cpp:923-964`）。没有客户端时直接返回（测试角色）。

主要触发点：

| 时机 | 位置 |
|---|---|
| 进入地图（进图包序列内） | `HumanPlayer.cpp:140` |
| 经验变化/升级；死亡复活；施法；`RestoreAll` | `HumanPlayer.cpp:313-314,228,521,354` |
| 使用药品/技能书；交任务；金币增减 | `HumanPlayer.cpp:721,634,738` |
| 自然恢复（MP 变化）；佩戴/切换称号 | `HumanPlayer.cpp:762,1024` |
| 装备/卸下/购买/出售（命令处理器） | `gsClientObj.cpp:1332,1356,1368,1387,1447` |
| `PlayerFactory` 恢复完背包/装备后 | `PlayerFactory.cpp:83` |

注意进图流程中 `PlayerFactory` 是**先** `AddPlayer`（触发 `OnEnterMap` → 第一次 `UpdateProp`）**再**恢复背包/装备/技能/任务，最后再 `UpdateProp` 一次（`PlayerFactory.cpp:58-83`）。所以带装备的角色登录时会收到两个 `SM_UPDATEPROP`，第二个才是最终值。

---

## 7. 持久化
### 7.1 `data/players/*.dat`：GSDB11

`CPlayerStore` 用固定版本二进制格式保存单个角色（`PlayerStore.cpp:15-16`、`72-158`）。字段顺序如下（全部本地字节序、原始结构体写入）：

| # | 字段 | 类型/长度 | 说明 |
|---|---|---|---|
| 1 | magic | 6 字节 | 固定 `"GSDB11"`（`PlayerStore.cpp:15`） |
| 2 | version | `DWORD` = 5 | `STORE_VERSION`（`PlayerStore.cpp:16`） |
| 3 | `CHARDBINFO` | 120 字节 | 基础属性/坐标/金币/经验（`PlayerStore.cpp:87`） |
| 4 | `dwLearnedSkills` | `DWORD` | 技能 id 位图（`PlayerStore.cpp:88`） |
| 5 | 任务数 + 任务数组 | `int` + N×12 字节 | `PLAYERQUEST{ nId, nProgress, nState }`（`Quest.h:27-33`） |
| 6 | 背包数 + 物品数组 | `int` + N×64 字节 | `ITEM`（`ItemManager.cpp:96` 取出，最多 46） |
| 7 | 装备数 + (位置+物品) | `UINT` + N×(1+64) 字节 | `BYTE pos` + `ITEM`（`PlayerStore.cpp:100-111`） |
| 8 | 成就数 + 成就 id 数组 | `int` + N×4 字节 | `std::vector<int>` |
| 9 | 当前称号 id | `int` | 成就会员/商店称号共用一个 id 空间 |
| 10 | 已消费成就点 | `int` | 商店兑换累计 |
| 11 | 商店称号数 + id 数组 | `int` + N×4 字节 | 已购买/首通授予的称号 |
| 12 | 图鉴数 + 条目 | `int` + N×(4 字节长度 + UTF-8 字节) | 见过的物品名 |
| 13 | 图鉴奖励数 + 条目 | `int` + N×(4 字节长度 + UTF-8 字节) | 已发过奖励的套装名 |

读档校验（`PlayerStore.cpp:161-328`）：magic/version 不符、任务数 > 16、背包数 > 200、装备数 > 10、成就数 > 256、商店称号数 > 64、图鉴/奖励数 > 256、名字长度 > 64 都会直接返回 `FALSE`，上层按“新角色”处理。由于是原始结构体写入，**不同编译器/架构的 padding 与端序不兼容**，且没有迁移代码。

路径规则：角色名中的字母/数字/>=0x80 字节（中文）保留，其余替换为 `_`（`PlayerStore.cpp:28-45`），文件为 `data/players/<sanitized>.dat`（`PlayerStore.cpp:58-61`）。删除角色和改名用的是未清洗的原始名拼路径（`gsClientObj.cpp:465,483-485`），名字里含空格等字符时可能与保存路径不一致——当前创建/改名命令通过 `istringstream` 取词，实际不会带空格。
### 7.2 `PlayerFactory` 读档流程

`PlayerFactory_Create`（`PlayerFactory.cpp:14-85`）是所有角色创建/登录的统一入口：

```
1. 重名检查；初始化 CHARDBINFO(wLevel=0, mapid=1)   PlayerFactory.cpp:17-24
2. CPlayerStore::Load 读出 11 组数据；有档覆盖名字/职业，无档用参数职业  PlayerFactory.cpp:36-47
3. NewPlayer → Init：无档套 1 级默认值/ApplyClassStats/自动学 1 级技能；
   有档保留属性，只清空背包/装备                         PlayerFactory.cpp:49-56
4. AddPlayer/AddTestPlayer（触发 OnEnterMap，先发一次 SM_UPDATEPROP） PlayerFactory.cpp:58-60
5. 恢复背包/装备(RestoreItem)/技能/任务/称号/图鉴/成就          PlayerFactory.cpp:67-81
6. 再 UpdateProp 一次                                     PlayerFactory.cpp:83
```

装备恢复走 `CEquipment::RestoreItem`，绕过穿戴条件检查（`Equipment.cpp:375-383`）。
### 7.3 其它落盘文件

| 文件 | 格式 | 载入 | 写入 | 写入时机 |
|---|---|---|---|---|
| `data/guilds.dat` | 文本：`name/leader/vice/level/funds/m1,m2,...`（`Guild.cpp:18-96`） | 启动 | `CGuildMgr::Save` | 退出 `CleanServer` |
| `data/guildware.dat` | 二进制：`"GWH1"` + `UINT 行会数` + 每行会 `UINT 名字长度 + 名字 + UINT 物品数 + N×ITEM`（`Guild.cpp:308-374`） | 启动 | `SaveWarehouse` | 退出 |
| `data/boss_kills.dat` | 文本：每行 `地图id/怪物名`（`Boss.cpp:126-147`） | 启动 | `SaveFirstKills` | 首次击杀立即写 + 退出 |
| `data/dungeon_first.dat` | 文本：每行一个副本 id（`Dungeon.cpp:386-411`） | 启动 | `SaveFirstClears` | 首次通关立即写 + 退出 |
| `data/dungeon_rank.dat` | 文本：`副本/队伍/秒数/MM-DD`（`Dungeon.cpp:337-379`） | 启动 | `SaveRecords` | 通关立即写 + 退出 |
| `data/season.dat` | 文本：一个整数 + 换行（`Dungeon.cpp:442-460`） | 启动 | `SaveSeason` | 赛季结算 + 退出 |
| `data/accounts.txt` | 文本：`账号/salt/hash`；hash = SHA-256(salt+password) 小写 hex（`gsClientObj.cpp:114-187`、`Auth.cpp:116-151`） | 首次登录/注册时懒加载 | `gs_AccountSave` | 注册、改密 |
| `data/characters.txt` | 文本：`账号/角色名/职业`（`gsClientObj.cpp:189-261`） | 首次使用懒加载 | `gs_CharSave` | 建角/删角/改名 |
| `data/gm.txt` | 文本：`名字/等级(1-3)`（`Gm.cpp`） | 启动 | `CGmMgr::Save` | `/gm gmadd`、`/gm gmremove` |
| `data/players/*.dat` | 见 7.1 | 登录 | `CPlayerStore::Save` | 下线、60 秒、`save`、退出 |

存档时机汇总：

| 触发 | 位置 | 范围 |
|---|---|---|
| 客户端断开 | `gsClientObj.cpp:873-886` | 该角色（先存再移出世界） |
| 每 60 秒 | `Server.cpp:204-211` | 全部在线角色 |
| 控制台 `save` | `ServerConsole.cpp:2218-2221` → `server_Save` | 全部在线角色 |
| 建角 | `gsClientObj.cpp:435` | 新角色立即落盘 |
| 退出 | `public/public.cpp:976` → `Stop` → `CleanServer`（`Server.cpp:93-107`） | 全部角色 + 行会/仓库/首杀/排行/首通/赛季 |
| 副本/首杀/赛季 | `Dungeon.cpp:337-411,129-172`、`Boss.cpp:106-107` | 排行、首通、首杀、赛季号 |

---

## 8. 模块清单

| 文件 | 职责 | 关键依赖 | 数据表 |
|---|---|---|---|
| `public/public.h/.cpp` | 类型、协议头、`ITEM/CHARDBINFO/HUMANPROP`、`CIndexList`、ini/文本、TCP、控制台 | 标准库、`compat/` | 无 |
| `Server.h/.cpp` | `CServer` 生命周期、连接池、进服账号、启动加载、热更新、主 Update | 全部 Mgr、`CGameWorld` | 启动时加载全部表 |
| `gsClientObj.cpp` | 单连接会话：账号/角色、文本命令、二进制 `SM_*`、背包/装备/商店/技能/任务/称号/图鉴快照、GM | 全部玩法、`PlayerFactory`、`Auth` | `accounts.txt`、`characters.txt`、`shop.txt` |
| `ServerConsole.cpp` | 控制台命令（发物品、战斗、任务、行会、副本、热更新验证） | 全部 Mgr | 运行时读写 |
| `GameWorld.h/.cpp` | 组合地图/玩家/怪物/NPC/物品；测试角色链 | 地图 Mgr、玩家 Mgr、`ItemManager` | `baseitem.txt` |
| `PhysicsMap.h/.cpp` + `PhysicsMapMgr.*` + `FindFile.h` | 文本 `.nmp` 解析、名字哈希、目录扫描 | `public/public.h` | `data/maps/physics/*.nmp` |
| `LogicMap.h/.cpp` + `LogicMapMgr.*` | `.ini` 解析、Area 划分、九宫格、锁位图、对象增删 | `PhysicsMapMgr`、`Area` | `data/maps/logic/*.ini` |
| `vMap.h` + `MapObject.h/.cpp` + `Area.h/.cpp` | 地图/区域/对象抽象、坐标与节点、消息编码、九宫格广播 | `public` | 无 |
| `AliveObject.h/.cpp` | 走/跑/转身/攻击、动作计时、伤害/毒伤、说话、宠物链 | `MapObject`、`LogicMap` | 无 |
| `HumanPlayer.h/.cpp` | 玩家属性、升级、技能、药品、任务、成就/称号/图鉴、自然恢复 | 装备/背包/任务/技能/事件/成就 | 读写玩家存档 |
| `HumanPlayerMgr.h/.cpp` | 玩家池 + 名字哈希 + 对象 id 类型位 | `HumanPlayer` | 无 |
| `Monster.h/.cpp` | 怪物模板、刷怪、简单 AI、掉落、宠物 | `AliveObject`、`Boss/Dungeon/Guild/Party` | `monsters.txt`、`drops.txt`、`spawns.txt` |
| `Npc.h/.cpp` | NPC 实体、类型/目标/对话、占格 | `LogicMapMgr` | `npcs.txt` |
| `DownItemObject.*` + `DownItemMgr.*` | 地面物品对象与池、掉落/拾取 | `MapObject`、`ItemManager` | 无 |
| `ItemBox.h/.cpp` + `Equipment.h/.cpp` | 背包 46 格/堆叠 99/拆分；10 装备位/穿戴条件/属性缓存/强化/附魔/套装 | `CIndexList`、`ItemManager`、`Set` | 无 |
| `ItemManager.h/.cpp` | `baseitem.txt` 解析、模板实例化、全局 makeIndex | `public/public.h` | `baseitem.txt` |
| `ExchangeObj.h/.cpp` + `Trade.h/.cpp` | 旧版点对点交易对象；新版交易会话（同图 6 格、锁定/确认、`SM_TRADE`） | `HumanPlayer`、`HumanPlayerMgr`、`ItemBox` | 无 |
| `ClassTable` + `Skill` + `Quest` | 三职业成长/1..50 级经验；技能模板；任务模板与玩家状态 | 无 | `classes.txt`、`levels.txt`、`skills.txt`、`quests.txt` |
| `Party.h/.cpp` + `Guild.h/.cpp` | 运行时组队/经验分享/队聊；行会等级/权限/仓库/频道、落盘 | `HumanPlayerMgr` | `guilds.dat`、`guildware.dat` |
| `Boss.h/.cpp` | Boss 刷新点、实例 id、首杀 | `MonsterMgr`、`Achievement` | `bosses.txt`、`boss_kills.dat` |
| `Dialogue.h/.cpp` + `Event.h/.cpp` | NPC 对话选项；双倍经验/世界 Boss/每日排期 | `MonsterMgr`、`BossMgr` | `dialogues.txt`、`events.txt` |
| `Set.h/.cpp` + `Achievement` + `Title` | 套装成员/件数/加成；成就点数/称号加成；成就点兑换/首通称号 | `Equipment`、`HumanPlayer` | `sets.txt`、`achievements.txt`、`titles.txt` |
| `Dungeon.h/.cpp` | 副本定义、实例槽、难度、排行、首通、赛季 | 地图/怪物/Boss/队伍/成就/称号 | `dungeons.txt`、4 个 `.dat` |
| `Gm.h/.cpp` + `Auth.h/.cpp` | GM 名单/权限等级；SHA-256/随机 hex/密码哈希 | 无 | `gm.txt` |
| `PlayerStore.h/.cpp` + `PlayerFactory.h/.cpp` | GSDB11 读写/路径清洗；统一建角 + 读档恢复顺序 | `SetMgr`、玩家/世界 | `data/players/*.dat` |
| `GameActions.h/.cpp` | 传送、NPC 距离、全服公告 | 地图/玩家 | 无 |

---

## 9. 单例、全局对象与初始化耦合
### 9.1 全局/单例清单

`g_Form`（`GameServer.cpp:7`）与 `g_pServer`（`:8`）是全局对象；`CServer`、`CGameWorld` 及所有 `CXxxMgr` 都用 `m_pInstance` + `GetInstance()` 惰性创建（`Server.cpp:23-28`、`GameWorld.cpp:8-14`）。`g_CodedMsgBuffer[1024]`（`AliveObject.cpp:10`）、`g_szParseBuffer[2048]`（`ItemManager.cpp:5`）是共享全局缓冲。

所有 Mgr 都是“首次 `GetInstance()` 时创建、进程退出不显式释放”的模式；这意味着**销毁顺序不可控**，不要在析构函数里依赖别的单例。
### 9.2 初始化顺序上的硬耦合

1. **控制台必须先于 `Start` 存在**：`GameWorld::Init` 解析 `baseitem.txt` 失败时会调用 `CServer::GetInstance()->GetIoConsole()->OutPut`（`GameWorld.cpp:35`）；`_tWinMain` 在 `EnterMessageLoop` 之前已经 `SetIoConsole(&g_Form)`。
2. **物理图先于逻辑图**：`CLogicMap::LoadMap` 通过 `CPhysicsMapMgr::GetPhysicsMapByName(blockmap)` 取图（`LogicMap.cpp:190`）；顺序在 `GameWorld.cpp:23-25` 固定。
3. **逻辑图先于 NPC/刷怪**：`CNpcMgr::Load` 和 `CMonsterMgr::Spawn` 都要求 `GetLogicMapById` 成功（`Npc.cpp:93-95`、`Monster.cpp:410-412`），所以地图必须在 `InitServer` 的第 3 步完成。
4. **物品模板先于任何发物品/掉落**：`CItemManager` 在 `GameWorld::Init` 末尾加载；怪物死亡时的 `RollDrops` 通过 `MakeupTemplateItem` 查模板（`Monster.cpp:379-395`）。
5. **套装表先于读档装备恢复**：`RestoreItem → ResetPropCache → ApplySetBonuses` 会遍历 `CSetMgr`（`Equipment.cpp:329-354`）；`CSetMgr::Load` 在 `InitServer` 第 6 组（行会/套装/成就/称号）完成，早于任何客户端登录。
6. **行会/副本/赛季表先于对应命令**：这些 Mgr 只在启动加载一次，控制台/客户端命令直接查内存。
7. **`CPlayerStore::Save` 依赖 `CSetMgr`**：图鉴奖励名来自 `CSetMgr::GetByIndex`（`PlayerStore.cpp:140-156`）；如果套装表未加载，奖励名列表会写成空。
8. **`CGameWorld::AddPlayer` 是进图包的触发点**：`PlayerFactory` 在恢复背包/装备之前就调用它（`PlayerFactory.cpp:58-83`），这是“先发一次旧属性、再发最终属性”的根因。

---

## 10. 单线程模型与限制

- **一个线程跑全部逻辑**：网络、控制台、AI、存档、活动都在 `CServerForm::EnterMessageLoop` 的同一线程内顺序执行（`public/public.cpp:952-974`）。
- **socket 是非阻塞轮询，不是 `select` 事件循环**：`PumpRecv` 每次 `recv` 到 `EAGAIN` 为止（`public/public.cpp:466-510`）；`select` 只用在校验控制台 stdin（`public/public.cpp:904-925`）。README 中“select 循环”的说法应按实现理解为“非阻塞轮询”。
- **主循环节流 10ms**：`Sleep(10)`（`public/public.cpp:973`），实际 tick 约 100Hz；动作时长 500/900ms、自然恢复 3s、自动存档 60s、热更新 1s 都是在这个粒度上计时。
- **发送队列无上限**：`PostMsg` 直接把编码包追加到 `m_SendBuffer`（`public/public.cpp:390-396`）；`FlushSend` 遇到 `EWOULDBLOCK` 就保留剩余数据。慢客户端可能让内存持续增长。
- **单包上限 8192 字节**：`MIR_MAX_PACKET`（`public/public.h:57`）；收包长度 > 8192 直接断开（`public/public.cpp:500-503`），`SendMsg` 负载最多 `8192-32`（`public/public.cpp:425`）。
- **阻塞 I/O 与共享缓冲**：`SaveAll`/行会/副本落盘同步执行，角色多会卡住网络；`g_CodedMsgBuffer` 单线程下安全，但回调重入会覆盖。
- **迭代器是成员状态**：`CIndexList::First/Next` 的游标在容器里（`public/public.h:449-464`），不能嵌套遍历同一容器。
- **RNG 未播种**：`Getrand` 直接用 `rand()`（`public/public.cpp:82-86`），全仓库没有 `srand`；每次进程启动的随机序列相同，刷怪/掉落/AI 走位在重启后可复现。
- **计时是 32 位毫秒**：`timeGetTime()` 回绕；代码大多用无符号差值比较，`AliveObject.cpp:269` 对动作完成时间做了 `0x7fffffff` 保护。

以上限制不影响单机/小规模验证，但不是生产级网络层。

---

## 11. 阅读路径

建议按“先骨架、后玩法”的顺序读这 8 个文件：

1. `public/public.h` —— 类型、协议、容器、服务器基类；先看 `MIRMSGHEADER`、`ITEM`、`CIndexList`、`CBaseServer`。
2. `Server.cpp` —— 启动加载顺序、每帧 Update、热更新、退出保存。
3. `GameWorld.cpp` —— 地图/物品/玩家如何被组合起来。
4. `gsClientObj.cpp` —— 协议与会话：文本命令、快照、登录/选角、GM。
5. `HumanPlayer.cpp` —— 玩家属性、升级、技能、任务、药品、恢复。
6. `MapObject.h` —— 对象基类、链表节点、`EncodeMsg` 接口。
7. `LogicMap.cpp` —— Area 划分、九宫格、`ChangeArea`、`LockPos`。
8. `PlayerStore.cpp` + `PlayerFactory.cpp` —— 存档格式与读档恢复顺序。

之后按需深入：`Monster.cpp`（AI/掉落）、`Dungeon.cpp`（实例/排行）、`Guild.cpp`（持久化）、`public/public.cpp`（网络分帧/XOR/控制台）。
