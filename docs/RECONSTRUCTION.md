# 重建说明（GameServer / chuanqiServer）

本文记录：快照缺什么、按什么依据补、改了原代码哪里、如何验证、以及 `bangzhu/` 文档的实际作用。

---

## 1. 起点

原始快照是 2005 年 4 月的服务端逻辑层，共 **40 个 `.cpp/.h`**（无子目录），编码 GBK，VC++ 7.1 工程。
内容只有服务端：服务器入口/连接管理、世界、地图与区域、玩家、物品/装备/交易、掉落物。
没有任何客户端渲染、输入、资源代码。

## 2. 缺失清单

按 `GameServer.vcproj` 的 `<File>` 列表 + 所有 `#include` 的并集，缺失如下：

| 缺失文件 | 性质 | 重建结果 |
|---|---|---|
| `public/public.h`、`public/public.cpp` | 引擎公共库（被 `stdafx.h` 引用） | 全部重建（类型/协议/容器/文件/网络/控制台） |
| `res/resource.h`、`res/public.rc` | 资源 ID 与脚本 | 重建（骨架不编译资源） |
| `Server.h` | `CServer` 类定义 | 重建 |
| `GameServer.h` | 入口全局对象声明 | 重建 |
| `LogicMap.h` | `CLogicMap`（继承 `CVLogicMap`） | 重建 |
| `Area.h` | `CArea`（继承 `CVArea`） | 重建 |
| `PhysicsMap.h/.cpp` | `CPhysicsMap`（继承 `CVPhysicsMap`） | 重建（文本地图格式） |
| `ItemBox.h` | `CItemBox` / `itembox_node` | 重建 |
| `gsClientObj.cpp` | 单连接业务实现（登录/进图/GM/分发） | 重建（含调试文本协议） |

`ItemBox::CalcWeight()` 在原 `ItemBox.cpp` 中只有声明没有实现，也一并补上。

## 3. 重建原则

1. **以调用点为准**：先用空实现让全部 `#include` 与成员访问成立，再根据语义补行为；编译器是规格。
2. **保持原接口**：类名、函数签名、数据流、`xListHost` 链表用法都不改，只补缺失定义。
3. **不确定处显式标注**：协议号、消息编码、TCP 分帧、`.nmp` 格式都是重建的，代码注释与 README 中写明。
4. **对原代码只修确定的错误**：语法错误、未初始化、越界判断、所有权问题；不改玩法数值与流程。

## 4. 重建的引擎层（`public/`）

| 部分 | 内容 |
|---|---|
| 基础类型/工具 | `BOOL/WORD/DWORD`、`GETNEXTPOS`、`Getrand`、`o_strncpy`、`SearchParam`、`timeGetTime` |
| 协议 | `MIRMSGHEADER`/`MIRMSG`（16 字节头 + 变长负载）、`SM_*`、`PI_*`、`_U_*`、`INT_*` |
| 数据结构 | `ITEM`/`ITEMBASE`/`CHARDBINFO`/`HUMANPROP`/`ENTERGAMESERVER`/`SERVERADDR` |
| 容器 | `CIndexList<T,N>`（id 对象池，`New/Del/Get/First/Next/Create/GetFreeCount`）、`CNameHash`、`CIntHash` |
| 文件 | `CDataFile`（ini）、`CSettingFile`、`CStringFile`（`baseitem.txt`），路径自动把 `\` 归一为 `/` |
| 消息编码 | `_CodeGameCode`/`_UnGameCode`：逐字节可逆异或（`key = 0x5A + i*0x1D`） |
| 网络 | `CClientObject`（4 字节小端长度分帧、非阻塞收发、收包解码）、`CBaseServer`（监听/接入/断开/更新） |
| 控制台 | `CServerForm`（原为 MFC 窗口，这里以控制台重建，保持 `Create/InitToolbar/SetIoConsole/EnterMessageLoop/OutPut` 接口） |
| 跨服桩 | `CSCClientObj`（选人服）、`CDBClientObj`（数据库）仅记录调用 |

## 5. 对原文件的修改（全量）

> 所有文件另做了机械处理：GBK → UTF-8、CRLF → LF、去掉 `".\xxx.h"` 前缀、include 大小写对齐实际文件名。
> 这些不改变语义，原始版本保留在 `original_gbk/`。

| 文件 | 修改 | 原因 |
|---|---|---|
| `ExchangeObj.cpp` | 构造函数初始化 `m_pPlayer/m_pOtherSide/金币/物品栏`；`DoExchange` 缺分号且缺少 `if`；`Cancel` 返回 void 值 | 原文件无法编译，且 `m_pPlayer` 未初始化会崩 |
| `MapObject.h` | `GetLinkNode` 判断 `Index >= 0 \|\| Index < LNI_MAX` → `&&` | 恒为真，越界索引会读数组外 |
| `Equipment.cpp` | `EquipItem` 入口先 `ZeroMemory(outItem)` | 输出参数未初始化；实测会把栈垃圾当成“换下的装备”塞进背包 |
| `HumanPlayer.cpp` | `MAKEFEATHER` 结果转 `DWORD`；`itemout` 清零；`sprintf` → `snprintf`；`GetPropValue` 补 `default` | 消除窄化/未初始化/弃用告警，避免垃圾物品 |
| `AliveObject.cpp` | `m_bPosLocked` 初始化；`sprintf/vsprintf` → `snprintf/vsnprintf` | 未初始化 + 缓冲区安全 |
| `ItemBox.cpp` | 补 `CalcWeight()`；`AddItem` 拒绝 `dwMakeIndex == 0` | 声明缺实现（链接失败）；空物品不应进背包 |
| `HumanPlayerMgr.cpp` | `NewPlayer` 登记名字表、`DeletePlayer` 注销 | 原实现从不登记，`FindbyName` 永远查不到 |
| `HumanPlayer.h` | 新增 `GetEquipments()` | 控制台需要读取装备错误信息 |
| `LogicMap.cpp` | 析构释放区域数组与锁位图；`AddObject` 检查空区域；删除 `if(this == NULL)` | 内存泄漏、空指针解引用、恒真判断 |
| `Server.cpp` | `Update` 中 `m_pGameWorld` 空指针保护 | 初始化失败时仍能运行控制台 |
| `Server.h`（重建） | `AddEnterAccount` 返回 `SERVER_ERROR` | 与 `Server.cpp` 定义及 `ENTERGAMESERVER::result` 一致 |
| `vMap.h` | `CVArea`/`CVPhysicsMap`/`CVLogicMap` 增加虚析构 | 通过基类指针 `delete` 是未定义行为 |
| `VirtualGameWorld.h` | 删除重复的 `DelObject` 声明，补虚析构 | 重复成员声明无法编译 |
| `Equipment.h`/`ExchangeObj.h`/`ItemManager.h` | `char* m_pErrorMsg` → `const char*` | 字面量赋值在 C++11 下非法 |
| `AliveObject.h/.cpp` | `BeAttack` 改为虚函数并真正结算伤害；新增 `GetCurHp/SetCurHp/OnDeath`；攻击目标按攻击者类型选择 | 原实现只发消息不减血，且只能打玩家 |
| `HumanPlayer.h/.cpp` | 实现血量/死亡复活/加经验；`UpdateProp` 无客户端时直接返回；新增 `GetDBInfoExp()` | 测试角色没有客户端，原 `UpdateProp` 会空指针崩溃 |
| `Equipment.h/.cpp` | 新增 `RestoreItem` | 读档恢复装备位不能走穿戴条件检查 |
| `ItemManager.h/.cpp` | 新增 `NewMakeIndex` | 掉落/发放需要全局唯一物品编号 |
| `HumanPlayerMgr.h` | 新增 `GetPlayerList` | 存档需要遍历全部角色 |
| `Server.cpp` | 初始化怪物模板与刷怪、每帧更新怪物、退出/定时存档 | 接入新增模块 |
| `gsClientObj.cpp` | 改用 `PlayerFactory_Create`；下线前先存档 | 持久化与读档 |
| `ServerConsole.cpp` | 新增 `spawn/attack/testcombat/monsters/kill/save` | 玩法验证入口 |
| `HumanPlayer.h/.cpp`（玩法） | 新角色改为 1 级 + 职业成长；`AddExp` 结算升级；新增 `ApplyClassStats/CastSkill/CastSkillByName/RestoreAll` | 参考传奇世界的职业/等级/技能 |
| `ClassTable.h/.cpp`、`Skill.h/.cpp`（新增） | 职业成长表、1..50 级经验表、技能表 | 数值数据驱动，便于调整 |
| `Server.cpp`（玩法） | 启动加载 classes/levels/skills | 初始化新系统 |
| `ServerConsole.cpp`（玩法） | `addbot` 职业参数，新增 `addexp/cast/skills/testskill/restore` | 验证入口 |
| `public/public.h` | `ITEMBASE` 增加 `btUseType/wUseValue` | 药品系统；`ITEM` 布局变化，存档版本升到 `GSDB02` |
| `public/public.cpp` | POSIX 控制台读入改为 `read()` + 行缓冲 | 修掉 `select` 与 stdio 混用导致的管道命令延迟/错序（实际导致升级、商店、传送偶发不生效） |
| `LogicMapMgr.*` | 地图 id 取文件名数字；加载失败时从对象池移除而不是 `delete` | 使地图 id 与刷怪/传送配置一致，并修复池对象二次释放隐患 |
| `HumanPlayer.*` | 新增 `UseItem/AddGold/RegenTick` | 药品、商店、自然恢复 |
| `ItemManager.*` | 解析可选药品字段并写入 `ITEM` | 数据驱动药品 |
| `ServerConsole.cpp` | 新增 `shop/buy/sell/use/hurt/teleport` | 验证入口 |
| `Npc.h/.cpp`（新增） | NPC 实体、数据加载、占格锁位、视野广播、商店/传送属性 | 快照只有 `OBJ_NPC` 枚举，没有任何 NPC 实现 |
| `GameActions.h/.cpp`（新增） | 共用传送动作、与 NPC 的距离判断 | 控制台和客户端复用同一套校验 |
| `Server.cpp` | 启动加载 `data/npcs.txt`、退出清理 NPC | 生命周期 |
| `ServerConsole.cpp`、`gsClientObj.cpp` | `npcs/talk/npcshop/npcteleport` 命令 | 交互入口 |
| `public/public.h` | `ITEM` 增加 `wCount` | 堆叠；存档升到 `GSDB03` |
| `ItemManager.cpp` | 物品名改按缓冲区长度复制（原 14 字节截断） | UTF-8 中文名（如“烈火剑法书”15 字节）会被切坏，导致查不到模板 |
| `ItemBox.*` | `AddItem` 合并堆叠、`RemoveItem` 按个扣减、重量按数量 | 堆叠系统 |
| `HumanPlayer.*` | 已学技能位图、`LearnSkill`、技能书 `useType=3`、释放要求已学 | 技能书学习 |
| `PlayerStore.*` | 存档 `GSDB03` 记录已学技能 | 技能持久化 |
| `ServerConsole.cpp` | `learn`、背包显示数量、可堆叠购买路径 | 验证入口 |
| `Quest.h/.cpp`（新增） | 任务模板、目标/数量/奖励/需求等级/对话 | 快照没有任务系统 |
| `ItemBox.*` | 按名字统计/扣除物品 | 收集类任务 |
| `Monster.cpp` | 死亡时通知玩家 `OnKillMonster` | 杀怪任务进度 |
| `HumanPlayer.*` | 任务接取/提交/进度、奖励发放 | 任务核心逻辑 |
| `PlayerStore.*` | 存档 `GSDB04` 记录任务 id/进度/状态 | 任务持久化 |
| `ServerConsole.cpp`、`gsClientObj.cpp` | `quests/accept/submit` 命令 | 任务入口 |
| `public/public.h` | `ITEM` 增加 `btUpgradeLevel` | 装备强化；存档升到 `GSDB05` |
| `ItemBox.*` | `SplitItem/RemoveAllByName` | 拆分与批量卖出 |
| `Equipment.cpp` | 实现 `ResetPropCache`（原为空） | 强化后重算装备属性 |
| `Party.h/.cpp`（新增） | 组队、队伍频道、经验分享、下线退队 | 快照只有未使用的组队链表骨架 |
| `Guild.h/.cpp`（新增） | 行会创建/加入/退出/移交/频道，持久化 `data/guilds.dat` | 快照没有行会 |
| `Monster.cpp` | 击杀后调用 `ShareExp` | 组队经验 |
| `Server.cpp` | 加载/保存行会，清理 | 生命周期 |
| `ServerConsole.cpp`、`gsClientObj.cpp` | 强化/拆分/组队/行会命令 | 入口 |
| `Boss.h/.cpp`（新增） | Boss 刷新点、实例 id、死亡倒计时 | 快照无刷新机制 |
| `Dialogue.h/.cpp`（新增） | NPC 对话选项（商店/任务/传送/关闭） | 快照只有一句固定对话 |
| `Quest.h/.cpp` | 前置任务与可重复字段 | 任务链/日常 |
| `Server.cpp` | 加载 bosses/dialogues、更新刷新 | 生命周期 |
| `Guild.h/.cpp` | 行会等级/资金/副会长/踢人/任命，持久化格式扩展 | 行会成长与权限 |
| `Event.h/.cpp`（新增） | 双倍经验活动、到期自动结束 | 快照没有活动系统 |
| `GameActions.cpp` | `Game_Announce` 全服公告 | 控制台与客户端共用 |
| `HumanPlayer.cpp` | `AddExp` 受双倍经验影响 | 活动生效 |
| `ItemBox.*` | `RemoveItemStack`（整堆取出） | 行会仓库存取 |
| `Guild.h/.cpp` | 仓库（50 格）、人数上限、经验加成、`guildware.dat` 读写 | 行会成长与共享仓库 |
| `Monster.cpp` | 击杀经验按行会等级加成 | 行会增益 |
| `Monster.h/.cpp` | 多掉落表加载/独立判定 `RollDrops` | 掉落内容 |
| `data/drops.txt`（新增） | 怪物多掉落配置 | 数据驱动 |
| `Guild` 仓库权限 | 取出仅限会长/副会长 | 权限控制 |
| `Event.h/.cpp` | 每日排期 `data/events.txt`、`Trigger`、到期公告 | 活动自动化 |

## 6. 验证记录

**编译**：`make clean && make`，Apple clang 21，`-Wall`，**0 error / 0 warning**。

**启动**：加载 `gameserver.ini`、两张地图（`1.ini`=中州 / `2.ini`=落霞岛）、物品/职业/技能/怪物表，监听 `0.0.0.0:7100`。

**协议**：测试客户端 `enter 王五 法师` 后收到并解码出进图包：

```
SM_SETMAP(0x0101)        flag=0x05000001 p=(477,222,0)
SM_SETPLAYER(0x0102)     flag=0x05000001 p=(477,222,256)
SM_SETPLAYERNAME(0x0103) flag=0x05000001 p=(255,0,0)
0x02c4（设置战斗地图）     flag=0x00000001 p=(0,0,0)
SM_SETMAPNAME(0x0104)    flag=0x00000000 p=(0,0,0)
SM_READY(0x0105)         flag=0xf2d505d7 p=(0,0,0)
SM_CHAT(0x010d)          flag=0x00000000 p=(65280,0,0)
SM_UPDATEPROP(0x010f)    flag=0x000003e8（金币 1000）
```

**职业与升级**（参考传奇世界）：

```
addbot 甲 战士    → 1 级、50 HP、20 MP、攻击 1-2
give 甲 木剑 / equip 甲 0 1001 → 攻击 3-7，腕力 5/50
testcombat 甲 鸡  → 5 次普攻击杀（伤害 7），+10 经验
addexp 甲 1000    → 3 级：170 HP、攻击 7-13、经验 430/1130
```

**技能**：

```
skills 甲             → 攻杀剑术（可学）/ 烈火剑法（10 级）
testskill 甲 攻杀剑术 鸡 → 施放成功，MP 40→37，目标残血
testskill 法 火球术 鸡   → 法师远程（射程 6）成功，MP 20→12
testskill 道 治愈术      → 道士治疗成功，MP 20→10
```

**持久化**：`save` 后重启，职业、等级、经验、背包、装备位全部恢复（3 级 + 木剑 → 攻击 7-13）。

**商店 / 药品 / 传送 / 恢复**：

```
buy 金创药 x2      → 金币 1000 → 900，背包 2 格
hurt 30 / use      → HP 20 → 50（金创药 +30）
sell               → 半价 +5 金币
teleport 2 200 200 → 到达落霞岛，地图=2
hurt 40 + 等 3 秒  → HP 10 → 12（5% 自然恢复）
```

**NPC**：

```
npcs                       → 杂货商(477,215,商店)、传送员(480,230,传送)
talk 甲 杂货商             → 距离 >3：太远了
teleport 甲 1 477 217 / talk → 输出对话
npcshop / npcteleport      → 货架、传送到落霞岛
```

**堆叠与技能书**：

```
buy 金创药 x5        → 背包 1 格 x5，使用后 x4，卖出后 x3
addexp 50000         → 10 级
give 烈火剑法书 / use → 学会烈火剑法
skills                → [已学] 烈火剑法
testskill 烈火剑法    → 施放成功
重启后 skills         → 仍为 [已学]
```

**任务系统**：

```
accept 除害（靠近杂货商） → 击杀 10 只鸡 → 进度 10/10
submit 除害               → +500 经验、+100 金币、金创药 x3
buy 金创药 x5 / accept 收集药材 / submit
                          → 扣除 5 个金创药，+300 经验、+50 金币、魔法药 x2
重启后 quests             → 两条均保持 [已完成]
```

**强化 / 拆分 / 组队 / 行会 / 矿洞**：

```
upgrade 木剑 → +1，攻击 3-7 → 4-8
split 金创药 x5 → x2 + x3；sellall → 一次卖出 5 个
partyinvite + psay → 队伍频道；击杀后队友获得 5 经验（10 的 50%）
guild create/join/gsay/leave → 会长移交、落盘、重启保留
npcteleport 矿洞向导 → 地图 3（沃玛教主所在）
```

**Boss / 任务链 / 对话树**：

```
boss → 沃玛教主 存活（死亡后 120 秒重刷）
前置未完成时接任务链 → 接受失败；完成前置后可接
日常任务提交后可立即再接
talk/choose → 选项列表、打开商店、对话传送、关闭
```

**行会成长 / 活动**：

```
addgold 5000 → guild create → join → appoint（副会长）→ donate 5000（升 2 级）→ kick
event doubleexp 60 → testcombat 鸡 → 经验 10 → 20
announce → 控制台与客户端同时收到【公告】
```

**行会仓库 / 等级增益**：

```
guild store/ware/take → 木剑存入、列出、取回
guild donate 5000     → 2 级：成员 1/30、经验 +10%
testcombat 鸡          → 经验 10 → 11（与双倍经验叠加时 10 → 22）
重启后仓库内容仍在（data/guildware.dat）
```

**多掉落 / 仓库权限 / 活动排期**：

```
drops 鸡 → 金创药 100%、木剑 5%
成员 store 可，take 被拒绝；会长 take 成功
event list → 双倍经验 21:00 30 分钟；event trigger 0 → 立即双倍，经验 10→20
```

**回归测试**：`python3 tools/smoke_test.py`（或 `make smoke`）**89 项断言**全部通过；
客户端验收 `make client-test` 5 项全部通过（角色名/地图/等级/HP/NPC）。

**内存**：AddressSanitizer + UndefinedBehaviorSanitizer，双客户端进图 + 走路/说话 + 物品/掉落 + 怪物 + 战斗 + 存档，无任何报告。

## 7. `bangzhu/` 文档的作用

`bangzhu/` 是 2005-05-26 用 Doxygen 1.3.9.1 为**同一份快照**生成的 HTML 文档（工程名 `chuanqiServer`，原始路径 `D:/chuanqiServer`）。

核对方法：把 40 个 `*-source.html` 反解成源码，与 `original_gbk/` 按“去空白后逐行”比对：

- 结果：**0 处内容差异**（仅制表符展开造成的空白差异）。
- 文件列表：40 个，与快照完全一致，**不含** `Server.h`、`LogicMap.h`、`Area.h`、`ItemBox.h`、`PhysicsMap.h`、`GameServer.h`、`gsClientObj.cpp`、`public.h`。
- 类页面：20 个，**没有** `CLogicMap`、`CArea`、`CItemBox`、`CPhysicsMap`、`CServer`。

结论：
1. 这些文件在 2005 年生成文档时就已缺失，不必再找“更完整的历史版本”。
2. 文档可作为**接口规格**使用：`CVArea`/`CVLogicMap`/`CVPhysicsMap`/`CClientObj` 等类页可用于核对重建接口；`bangzhu/index.html` 可浏览类层次与函数索引。
3. 文档**不能**提供 `public` 引擎层与 5 个缺失类的实现。

## 8. 客户端（新增）

原快照只有服务端，没有客户端。为了能“看得见”，新增了独立客户端：

| 文件 | 说明 |
|---|---|
| `client/game_client.py` | tkinter 图形客户端：登录、地图网格、实体、血条、聊天、方向键/WASD 走路、攻击/拾取/对话/命令 |
| `tools/client_test.py` | headless 验收：启动服务器 → 登录 → 校验地图/等级/HP/NPC |

服务端为客户端补了两处：
- 怪物 `SM_APPEAR` 负载追加名字（原来只有血量），客户端才能显示怪物名
- 文本协议新增 `attack <dir>`、`pickup`，供客户端按钮/键盘调用

客户端复用“文本指令 + 编码下行”的现有协议，没有改动任何已有报文格式。

第二轮客户端补全：
- 新增结构化下行 `SM_BAG / SM_EQUIP / SM_SHOP`，承载背包、装备、商店快照
- 新增文本命令 `bag / equipment / shop / use / equipauto / unequip / sellitem / buy`
- 客户端增加背包（B）、装备（C）、商店（V）三个面板，支持使用/装备/出售/卸下/购买
- `tools/client_test.py` 覆盖购买与五类快照（背包/装备/商店/技能/任务），共 7 项断言

第三轮客户端补全：
- 新增 `SM_SKILLS`、`SM_QUESTS` 结构化快照与文本命令 `skills`、`questlog`
- 客户端技能栏：1-8 施放、未学/等级/MP/冷却置灰、冷却倒计时
- 客户端点击交互：NPC 对话/走近、怪物攻击/走近、地面物品拾取/走近
- 客户端任务日志窗口（Q）：状态、进度、奖励

第四轮客户端补全（地形渲染）：
- 新增 `SM_MAPDATA`：服务器按玩家位置下发 61×41 地形窗口（0 可走 / 1 阻挡 / 2 越界）
- 客户端程序化绘制草地与石墙，替换原来的纯网格；移动后节流重新请求
- `tools/client_test.py` 增加地形断言（61×41、99 格阻挡），共 8 项

第五轮补全（副本与小地图）：
- 新增第四张地图 `4.ini`/`map3.nmp`（沃玛寺庙）、寺庙守卫 NPC、沃玛护卫/沃玛战士与寺庙 Boss
- 新增任务链“挑战沃玛教主”（前置 3、需 10 级）
- 客户端右上角小地图：地形窗口 + 自己/怪物/NPC/玩家点位
- 服务端回归 93 项、客户端 8 项

第六轮（内容扩展）：
- 新增将军坟/尸王殿/蛇魔谷三张地图（`5.ini`~`7.ini`、`map4~map6.nmp`）
- 新增 6 种怪物、2 个 Boss 刷新点、16 件装备、5 个 NPC、3 条任务
- 装备覆盖武器/衣服/头盔/项链/戒指/手镯，商店同步上架
- 服务端回归 99 项

第七轮（套装与首杀）：
- `Set.h/.cpp` + `data/sets.txt`：四套套装，按件数追加 DC/AC/MC/SC
- `CEquipment` 穿脱/强化后统一 `ResetPropCache`，套装加成即时生效
- `Boss` 首杀：按地图+名字记录，公告 + 3000 金币/经验，`data/boss_kills.dat` 持久化
- 调试命令 `bosskill`，控制台 `sets`、`firstkills`
- 服务端回归 105 项

第八轮（附魔/洗炼）：
- `ITEM` 增加 `btElement/btElementPower`，存档升到 `GSDB07`
- `CEquipment::AddPropCache` 按元素追加 DC/AC/MC/SC
- 控制台 `enchant`、`refine`，`equipinfo` 显示元素
- 客户端背包/装备快照与面板同步显示元素
- 服务端回归 109 项

第九轮（世界 Boss）：
- `EVENTSCHEDULE` 支持 `doubleexp` / `worldboss` 两类排期
- `Trigger` 刷出 Boss 并登记 `CBossMgr::RegisterWorldBoss`（不自动重刷）
- 世界 Boss 计入首杀、全服公告、持久化
- 服务端回归 114 项

第十轮（组队副本）：
- 新增 `Dungeon.h/.cpp`：单队伍锁、整队传送、Boss 死亡/超时/离场释放
- 新增地图 8「沃玛神殿」、副本使者 NPC、副本怪与 Boss
- 控制台 `dungeon`、`npcteleport` 支持副本入口；客户端文本命令同步
- 服务端回归 119 项

第十一轮（副本排行/首通）：
- 副本通关记录 `data/dungeon_rank.dat`，按用时保留前 5
- 破纪录公告、每日首通 2000 金币/经验
- 控制台 `dungeonrank`
- 服务端回归 122 项

第十二轮（副本多实例）：
- `CDungeonMgr` 重构为 3 个槽位（地图 8/9/10），支持多队伍并行
- `CBossMgr::ForceRespawn` 在进入时重置实例 Boss
- 槽位独立计时/释放，`dungeon` 列出全部槽位
- 服务端回归 126 项

第十三轮（副本怪物重置）：
- `CMonsterMgr::ResetMap`：按 `spawns.txt` 清空并重刷指定地图
- `CBossMgr::ClearMap/ForceRespawn`：进入实例时重置 Boss
- 控制台 `mapmonsters`
- 服务端回归 129 项

第十四轮（成就/称号）：
- `Achievement.h/.cpp` + `data/achievements.txt`：等级/Boss/副本/强化/附魔/任务 6 类触发
- 玩家成就与称号写入存档（`GSDB08`），重登恢复
- 控制台 `achievements`、`title`，`info` 显示称号
- 服务端回归 136 项

第十六轮（称号加成/点数）：
- 成就数据增加点数与称号加成（HP/MP/AC/MAC/DC/MC/SC）
- `GetPropValue` 叠加称号加成，切换称号修正当前 HP
- `achievements` 显示总点数
- 服务端回归 138 项

第十七轮（称号商店/难度/表现）：
- `Title.h/.cpp` + `data/titles.txt`：成就点兑换称号，点数消费入档（`GSDB09`）
- 客户端称号收集面板（L），`SM_TITLES` 快照与 `titlelist/usetitle/buytitle`
- 副本三档难度：普通/困难/地狱（Boss 模板与掉落不同）
- 客户端程序化草叶/砖缝、两帧摆动、WAV 音效（afplay）
- 服务端回归 143 项

第十八轮（多副本）：
- `CDungeonMgr` 通用化：副本定义来自 `data/dungeons.txt`（多实例地图/Boss/成就）
- 新增尸王殿（11/12）、蛇魔谷（13/14）两副本与入口 NPC
- 三个副本独立实例池、重置、通关成就与排行
- 服务端回归 147 项

第十九轮（矿洞/将军坟副本 + PNG 图集）：
- 新增矿洞（15/16）、将军坟（17/18）两个副本，五副本共 11 个实例
- 成就增至 11 条（新增矿洞征服者、将军坟征服者）
- 客户端首次启动生成 PNG 图集（草/石），并为 Tk 8.5 生成 PPM 兼容缓存
- 服务端回归 151 项

第二十轮（副本套装/首通称号）：
- 新增 5 套副本专属装备（15 件），Boss 多掉落
- 每个副本首通记录 `data/dungeon_first.dat`，首通授予全队专属称号
- 称号不进入点数商店；控制台 `titles` 可查看
- 服务端回归 156 项

第二十一轮（祖玛/图鉴/美术）：
- 新增祖玛寺庙副本（19/20/21，含地狱难度）与祖玛套装、首通称号、成就
- 图鉴：物品发现记录入档（`GSDB10`），`codex` 命令与客户端 K 面板
- 客户端实体改为程序化像素小像（玩家/怪物/NPC/物品）
- 服务端回归 162 项

第二十二轮（全地狱档/图鉴奖励）：
- 尸王殿/蛇魔谷/矿洞/将军坟各补地狱实例（22~25），六副本共 18 个实例
- 图鉴集齐套装发 3000 金币/经验并标记，状态入档（`GSDB11`）
- 集齐 10 套解锁“图鉴大师”成就
- 服务端回归 165 项

第二十三轮（分副本排行/图鉴称号）：
- 排行按副本裁剪，每个副本保留前 5，`dungeonrank [副本]` 分组/过滤
- 图鉴集齐 3/6/10 套授予收藏家/鉴宝大师/图鉴之王
- 服务端回归 168 项

第二十四轮（赛季奖励）：
- `season settle` 每副本第一名发 5000 金币/经验 + 赛季冠军称号，清空排行并递增赛季
- 赛季号持久化 `data/season.dat`
- 服务端回归 172 项

第二十五轮（逆魔/禁地/动画帧）：
- 新增逆魔古刹（26~28）、禁地（29~31）两副本，八副本共 24 实例
- 新增两套副本套装、两条首通称号、两条通关成就
- 客户端玩家挥剑/迈步、怪物眨眼、NPC 摆袍两帧动画
- 服务端回归 176 项


## 当前状态（996 结构 + 桌面客户端）

- 服务端：GameGate / LoginSrv / DBSrv / LogSrv / GameSrv 分进程
- 客户端：Pygame 原生桌面窗口，登录 → 选角 → 游戏
- UI：深蓝黑金属面板、红金标题、方角按钮、区服选择
- 交互：左键移动/攻击/对话，右键攻击，F1-F8 技能，1-6 药品
- 数据：60 地图、93 怪物、126 物品、50 NPC、42 Boss、20 套装
- 回归：`tools/smoke_test.py` 176 项通过

## 公开授权 996 风格皮肤

- 使用 CC0 FANTASY GUI 面板，Pygame 灰度+蓝钢调色
- 红金标题缎带、金边宝石按钮
- 登录/选角/游戏面板统一风格
- 不使用 996 原版切图

## 996 服务器控制台

- Pygame 原生服务器状态窗口
- 五个进程在线状态、实时日志、存档数
- 与桌面客户端共用 996 深色金属 UI 风格

## 公开授权素材替换

- 玩家：DCSS 彩色角色（CC0，默认）；Clint Bellanger 等距角色作为可选（CC-BY 3.0）
- 怪物/NPC/物品：DCSS（CC0）
- 地图：Stone Soup（CC0）
- UI：FANTASY GUI（CC0，调色）
- 提供 WOOOL_PLAYER=dcss 回退

## Buff 与施毒 DoT

- 幽灵盾/神圣战甲：20 秒 AC/MAC 增益
- 施毒术/群体施毒术：8 秒每秒毒伤
- AliveObject 毒伤结算与玩家击杀归属
- 技能表 33 个，回归 176 项

## 召唤兽

- 骷髅宝宝/神兽模板
- 宠物跟随、7 格寻怪、近身攻击
- 伤害归属主人
- 主人离线/换图消失
- 召唤实测通过

## 优雅停服与断线重连

- SIGTERM/SIGINT 触发主循环退出并保存
- 996 启动器通过 console exit 停 GameSrv
- 客户端断线自动回登录界面
- 回归 176 项

## 登录/角色流程调整

- 登录页移除职业选择
- 角色页：选择角色、建立角色、删除角色、改名
- 角色卡显示职业和等级

## 传奇世界式选角界面

- 三角色槽 + 中间预览 + 右侧信息
- 进入/建立/删除/返回
- 移除改名

## 服务器管理工具

- 进程状态/启动/停止/备份
- 端口/区服名/备份间隔配置
- 数据表可视化编辑与保存
- GameSrv 日志

## 数据表热更新

- GameSrv 每秒监听 10 个数据文件 mtime
- 自动重载并公告
- 管理工具保存后约 1 秒生效
