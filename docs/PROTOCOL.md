# 协议（PROTOCOL）

> 本文描述 `GameServer` 与客户端之间的线协议。它是**重建实现**：消息号、16 字节头、XOR 编码与 4 字节分帧都是按用法定义的，不是 2005 年官方协议。
> 行号按当前源码核对。客户端侧实现见 `client/` 与 `tools/`，服务端侧见 `public/`、`gsClientObj.cpp`。
## 1. 传输层与分帧

| 项 | 值 | 依据 |
|---|---|---|
| 传输 | TCP | `public/public.cpp:634-678` |
| 监听 | `gameserver.ini` 的 `[server] addr/port`，默认 `0.0.0.0:7100` | `public/public.cpp:702-704`、`gameserver.ini:4-7` |
| 分帧 | 4 字节小端无符号长度 + 该长度的载荷 | `public/public.cpp:321-335`、`390-404` |
| 长度上限 | 8192；读到 0 或 >8192 直接断开 | `public/public.cpp:57`、`500-503` |
| 收发模式 | 非阻塞 socket；`PumpRecv` 读到 `EAGAIN`，`FlushSend` 缓冲未发送部分 | `public/public.cpp:433-510` |
| 连接池 | `CIndexList<CClientObj, MAX_CLIENT>`，最多 1024 | `Server.h:49`、`public/public.h:55` |

线上字节布局：

```
[0..3]  LE32 n                      // 后面整包长度，不包含这 4 字节
[4]     '#'
[5..20] XOR(MIRMSGHEADER, 16 字节)
[21..]  XOR(payload, n-18 字节)
[n+3]   '!'
```

即一帧 = `LE32(n)` + 一个以 `#` 开头、`!` 结尾的编码包。`n` 由 `SendMsg` 生成（`public/public.cpp:418-430`），`PostMsg` 只负责加长度前缀并调用 `FlushSend`（`public/public.cpp:390-396`）。

文本通道复用同一分帧：4 字节长度 + UTF-8 文本，没有 `#`/`!` 包装。服务端在 `OnFrame` 中按“首字节不是 `#` 或长度 < 18”路由到 `OnUnCodeMsg`（`public/public.cpp:512-521`）。
## 2. 下行编码
### 2.1 头结构

`MIRMSGHEADER` 固定 16 字节（`public/public.h:220-238`），`sizeof` 已核对为 16：

| 偏移 | 字段 | 类型 | 说明 |
|---|---|---|---|
| 0 | `dwFlag` | `DWORD` | 消息主语/目标 id；部分消息另有含义（见第 3 节） |
| 4 | `wCmd` | `WORD` | `SM_*` 命令字 |
| 6 | `wParam[0]` | `WORD` | 参数 0 |
| 8 | `wParam[1]` | `WORD` | 参数 1 |
| 10 | `wParam[2]` | `WORD` | 参数 2 |
| 12 | `dwReserved` | `DWORD` | 保留，发送时置 0 |

负载不是 `MIRMSG::data[1]` 直接传输，而是紧随 16 字节头之后；`MIRMSG` 只是给服务端 `ProcClientMsg` 用的视图（`public/public.h:240-249`）。
### 2.2 XOR 编码

`_CodeGameCode` / `_UnGameCode` 是同一个可逆逐字节异或（`public/public.cpp:96-111`）：

```c
key(i) = (BYTE)(0x5A + i * 0x1D);      // i 为段内下标，0 起
out[i] = in[i] ^ key(i);
```

关键点是**按段重置下标**：

- 头部 16 字节从 `i=0` 编到 15；
- 负载从 `i=0` 重新开始；
- 客户端解码时也分两段：`decode(pkt[1:17])` 与 `decode(pkt[17:-1])`（`client/game_client.py:372-375`）。

编码输出：`'#' + coded(header,16) + coded(payload,n) + '!'`（`MapObject.cpp:62-77`、`public/public.cpp:418-430`）。字符串负载不带长度前缀，长度由整帧长度决定；`datasize < 0` 时按 `strlen` 计算（`public/public.cpp:422-427`）。
### 2.3 发送路径

| 入口 | 适用 | 位置 |
|---|---|---|
| `CClientObject::SendMsg(flag,cmd,p0,p1,p2,data,size)` | 单连接定向发送 | `public/public.cpp:406-431` |
| `CClientObject::PostMsg(coded,size)` | 已编码整包（`MapObject::EncodeMsg` 的输出） | `public/public.cpp:390-396` |
| `CMapObject::EncodeMsg` | 编码但不发送；用于区域广播 | `MapObject.cpp:62-77` |
| `CAliveObject::SendMsg/SendAroundMsg/SendMapMsg` | 自身/九宫格/全图广播 | `AliveObject.cpp:182-206` |
| `CArea::SendMsg/SendAroundMsg` | 区域广播，跳过发送者与非接收者 | `Area.cpp:13-36` |

单包负载上限是 `MIR_MAX_PACKET - 32 = 8160` 字节（`public/public.cpp:425`）。
## 3. `dwFlag` 与对象 id

大多数“实体类”消息的 `dwFlag` 是对象 id，编码为 `pool_id | (OBJ_TYPE << 24)`（`HumanPlayerMgr.cpp:40-41`、`Monster.cpp:423`、`Npc.cpp:110`）：

| 类型 | 值 | 示例 |
|---|---|---|
| `OBJ_DOWNITEM` | 2 | 地面物品只使用 `dwMakeIndex`，不用该编码 |
| `OBJ_MONSTER` | 3 | 池 id 1 → `0x03000001` |
| `OBJ_NPC` | 4 | 池 id 1 → `0x04000001` |
| `OBJ_PLAYER` | 5 | 池 id 1 → `0x05000001` |

`dwFlag` 并非总是对象 id，按消息复用：

| 消息 | `dwFlag` 含义 |
|---|---|
| `SM_SETMAP` / `SM_SETPLAYER` / `SM_SETPLAYERNAME` | 玩家自己的对象 id |
| `SM_APPEAR` / `SM_DISAPPEAR` / `SM_WALK` / `SM_RUN` / `SM_ATTACK` / `SM_BEATTACK` / `SM_SPELL` / `SM_DEATH` / `SM_LEVELUP` | 该实体/玩家的对象 id |
| `SM_CHAT` | 说话者对象 id；系统消息为 0（`SendFirstDlg`、`Game_Announce`） |
| `SM_UPDATEPROP` | 金币数量（不是 id） |
| `SM_ADDITEM` / `SM_EQUIPRESULT` / `SM_UNEQUIPRESULT` / `SM_DOWNITEMAPPEAR` / `SM_DOWNITEMDISAPPEAR` | 物品实例号 `dwMakeIndex` |
| `SM_READY` | 固定魔数 `0xf2d505d7` |
| `SM_SETMAPNAME` | 0（广播语义） |

`SendFirstDlg` 固定用 `dwFlag=0`、`wParam[0]=0xff00` 的 `SM_CHAT`（`gsClientObj.cpp:954-957`）；`Game_Announce` 用 `0xff00` 发给全部玩家（`GameActions.cpp:46-60`）。聊天颜色约定：`0xff00` 系统/普通，`0xffd700` 金色提示，`0xff8800` 橙色，`0xff8888` 交易错误。
## 4. `SM_*` 枚举表（0x0101–0x0127）

枚举在 `public/public.h:86-127` 顺序定义，数值连续。方向列中 `S→C` 表示服务端发出，`C→S` 表示服务端接受上行；`双向` 表示两者都有。

| 值 | 名称 | 方向 | dwFlag / 参数 | 含义与负载 | 源码 |
|---|---|---|---|---|---|
| 0x0101 | `SM_SETMAP` | S→C | 玩家 id；p0/p1=坐标 | 进入地图；负载=地图 `name` | `HumanPlayer.cpp:133` |
| 0x0102 | `SM_SETPLAYER` | S→C | 玩家 id；p0/p1=坐标，p2=0x100 | 玩家外观/位置；负载 4×DWORD（首个为 `MAKEFEATHER`） | `HumanPlayer.cpp:134-135` |
| 0x0103 | `SM_SETPLAYERNAME` | S→C | 玩家 id；p0=0xff | 玩家名；负载=名字 | `HumanPlayer.cpp:136` |
| 0x0104 | `SM_SETMAPNAME` | S→C | 0 | 地图标题；负载=title | `HumanPlayer.cpp:138` |
| 0x0105 | `SM_READY` | S→C | 0xf2d505d7 | 进图就绪，无负载 | `HumanPlayer.cpp:139` |
| 0x0106 | `SM_APPEAR` | S→C | 实体 id；p0/p1=坐标，p2=方向/外形 | 实体出现；负载按类型（见 5.1） | `HumanPlayer.cpp:161`、`Monster.cpp:158`、`Npc.cpp:47` |
| 0x0107 | `SM_DISAPPEAR` | S→C | 实体 id；p0/p1=坐标 | 实体消失，无负载 | `AliveObject.cpp:253` |
| 0x0108 | `SM_WALK` | 双向 | 实体 id；p0/p1=目标，p2=方向 | 走路；负载 8 字节 `[feather,0]` | `AliveObject.cpp:59`；上行 `gsClientObj.cpp:912-920` |
| 0x0109 | `SM_RUN` | 双向 | 同上 | 跑步；负载 8 字节 | `AliveObject.cpp:74`；上行 `gsClientObj.cpp:921-929` |
| 0x010A | `SM_TURN` | C→S | p0=方向 | 转身；服务端接受，但下行转身用 `SM_APPEAR` | `gsClientObj.cpp:930-932` |
| 0x010B | `SM_ATTACK` | 双向 | 实体 id；p0/p1=坐标，p2=方向 | 攻击动作；负载 8 字节 | `AliveObject.cpp:91`；上行 `gsClientObj.cpp:933-935` |
| 0x010C | `SM_BEATTACK` | S→C | 受击者 id；p0=HP，p1=MP，p2=伤害 | 受击；负载 16 字节 `[feather,0,attackerId,damage]` | `AliveObject.cpp:127-137` |
| 0x010D | `SM_CHAT` | 双向 | 说话者 id / 0；p0=颜色 | 聊天/系统提示；负载=UTF-8 文本 | `AliveObject.cpp:158-179`；上行 `gsClientObj.cpp:940-946` |
| 0x010E | `SM_BACK` | S→C | 实体 id；p0/p1=回拉坐标，p2=方向 | 位置回拉/纠正；负载 10 字节 | `AliveObject.cpp:302-312` |
| 0x010F | `SM_UPDATEPROP` | S→C | 金币；p0=hair<<8\|class，p1=sex | 属性镜像；负载=`HUMANPROP`（44 字节） | `HumanPlayer.cpp:923-964` |
| 0x0110 | `SM_DOWNITEMAPPEAR` | S→C | makeIndex；p0/p1=坐标，p2=image | 地面物品出现；负载=物品名（长度=`btNameLength`） | `DownItemObject.cpp:18-24` |
| 0x0111 | `SM_DOWNITEMDISAPPEAR` | S→C | makeIndex；p0/p1=坐标 | 地面物品消失，无负载 | `DownItemObject.cpp:26-30` |
| 0x0112 | `SM_ADDITEM` | S→C | makeIndex | 获得物品；负载=完整 `ITEM`（64 字节） | `gsClientObj.cpp:893-896` |
| 0x0113 | `SM_EQUIPRESULT` | S→C | makeIndex；p0=成功 0/1，p1=位置 | 穿装备结果，无负载 | `gsClientObj.cpp:898-901` |
| 0x0114 | `SM_UNEQUIPRESULT` | S→C | makeIndex；p0=成功 0/1，p1=位置 | 脱装备结果，无负载 | `gsClientObj.cpp:903-906` |
| 0x0115 | `SM_ACTIONRESULT` | S→C | 连接 id；p0=x，p1=y，p2=成功 | 动作结果（当前无调用方，预留） | `gsClientObj.cpp:888-891` |
| 0x0116 | `SM_DEATH` | S→C | 玩家 id | 死亡，无负载 | `HumanPlayer.cpp:200` |
| 0x0117 | `SM_SPELL` | 双向 | 施法者 id；p0=技能 id，p1=方向 | 施法；负载 8 字节 `[skillId,x]` | `HumanPlayer.cpp:382`；上行 `gsClientObj.cpp:936-939` |
| 0x0118 | `SM_LEVELUP` | S→C | 玩家 id；p0=新等级，p1=连升数 | 升级，无负载 | `HumanPlayer.cpp:310` |
| 0x0119 | `SM_BAG` | S→C | 0 | 背包快照（见 5.2） | `gsClientObj.cpp:1224-1247` |
| 0x011A | `SM_EQUIP` | S→C | 0 | 装备快照 | `gsClientObj.cpp:1250-1278` |
| 0x011B | `SM_SHOP` | S→C | 0 | 商店货架 | `gsClientObj.cpp:1281-1307` |
| 0x011C | `SM_SKILLS` | S→C | 0 | 技能列表 | `gsClientObj.cpp:1453-1482` |
| 0x011D | `SM_QUESTS` | S→C | 0 | 任务列表 | `gsClientObj.cpp:1486-1525` |
| 0x011E | `SM_MAPDATA` | S→C | p0=W，p1=H | 地形窗口（61×41） | `gsClientObj.cpp:1529-1564` |
| 0x011F | `SM_TITLES` | S→C | 0 | 称号收集 | `gsClientObj.cpp:1568-1609` |
| 0x0120 | `SM_CODEX` | S→C | 0 | 图鉴收集 | `gsClientObj.cpp:1613-1643` |
| 0x0121 | `SM_DIALOG` | S→C | 0 | NPC 对话 + 选项 | `gsClientObj.cpp:1069-1091` |
| 0x0122 | `SM_AUTH` | S→C | 0 | 会话 token（32 字符 hex） | `gsClientObj.cpp:1094-1097` |
| 0x0123 | `SM_CHARLIST` | S→C | 0 | 账号角色列表 | `gsClientObj.cpp:1100-1128` |
| 0x0124 | `SM_PARTY` | S→C | 0 | 队伍快照 | `gsClientObj.cpp:1131-1149` |
| 0x0125 | `SM_GUILD` | S→C | 0 | 行会快照 | `gsClientObj.cpp:1152-1181` |
| 0x0126 | `SM_DUNGEONS` | S→C | 0 | 副本/排行/实例快照 | `gsClientObj.cpp:1184-1221` |
| 0x0127 | `SM_TRADE` | S→C | 0 | 交易窗口 | `Trade.cpp:42-90` |

**0x02c4（不在枚举内）**：进图时发的“设置地图属性”包，`dwFlag=1` 表示战斗地图、0 表示非战斗（`HumanPlayer.cpp:137`）。客户端把它命名为 `SM_SETFIGHTMAP`（`client/game_client.py:39`），但没有 `SM_` 常量。
## 5. 负载格式
### 5.1 `SM_APPEAR` 的四种负载

| 实体 | 负载 | 依据 |
|---|---|---|
| 玩家 | 12 字节（`MAKEFEATHER` + 0 + `0x00640064`）+ `"名字/255"` | `HumanPlayer.cpp:152-162` |
| 怪物 | `DWORD hp` + `DWORD maxhp` + 名字（无 `/` 后缀） | `Monster.cpp:150-160` |
| NPC | `"名字/类型"` | `Npc.cpp:43-49` |
| 地面物品 | 物品名（长度取 `btNameLength`，不是 `strlen`） | `DownItemObject.cpp:18-24` |

客户端按 `(dwFlag >> 24) & 0xFF` 区分类型并拆负载（`client/game_client.py:394-410`）。
### 5.2 结构化快照

所有快照中的字符串都是 `u8 长度 + UTF-8 字节`（`gsClientObj.cpp:1056-1067`），整数为小端。

| 消息 | 负载布局 |
|---|---|
| `SM_BAG` | `u16 n`；每项 `u32 makeIndex, str name, u16 count, u8 upgrade, u8 element, u8 elementPower, u16 curDura, u16 maxDura, u8 stdMode, u8 useType` |
| `SM_EQUIP` | `u16 n`；每项 `u8 pos, u32 makeIndex, str name, u8 upgrade, u8 element, u8 power, u16 curDura, u16 maxDura` |
| `SM_SHOP` | `u16 n`；每项 `str name, u32 price` |
| `SM_SKILLS` | `u16 n`；每项 `u8 id, str name, u16 needLevel, u16 mp, u32 cooldown, u8 learned, u8 type, u8 range` |
| `SM_QUESTS` | `u16 n`；每项 `u8 id, str name, str target, u16 count, u16 progress, u8 state, u8 type, u32 exp, u32 gold, str npc, u8 needLevel, str reward, u16 rewardCount, u8 prev, str dialog` |
| `SM_MAPDATA` | `u16 W, u16 H, u16 ox, u16 oy` + `W*H` 字节（0 可走 / 1 阻挡 / 2 越界） |
| `SM_TITLES` | `u32 availablePoints, u16 n`；每项 `u16 id, str name, u8 kind(0=成就/1=商店), u32 cost, u8 owned, u8 equipped` |
| `SM_CODEX` | `u32 codexCount, u32 shopTitleCount, u32 achievementCount, u16 n`；每项 `str set, u8 found, u8 total, u8 complete` |
| `SM_DIALOG` | `str npc, str text, u16 n`；每项 `u8 index, str text, str action, str param` |
| `SM_CHARLIST` | `u16 n`；每项 `str name, u8 class, u16 level` |
| `SM_PARTY` | `u16 n`；每项 `str name, u16 level, u8 class, u16 hp, u16 maxhp, u8 online` |
| `SM_GUILD` | `str name`（空=无行会）；否则 `str leader, str vice, u8 level, u32 funds, u16 n`，每成员 `str name, u16 level, u8 online, u8 rank(0成员/1副会长/2会长)`，最后 `u16 warehouseCount` |
| `SM_DUNGEONS` | `u16 defs`；每项 `u8 id, str name, str boss, u8 firstCleared, u8 mapCount`。随后 `u16 records`（`str dungeon, str party, u32 seconds, str date`），再 `u16 slots`（`u16 mapId, str party`） |
| `SM_TRADE` | `u8 status`；`1` 时 `str partner, u32 myGold, u32 otherGold, u8 myLock, u8 otherLock, u8 myConfirm, u8 otherConfirm, u16 myItems`（每项 `u32 makeIndex, str name, u16 count`），再同样一组 `otherItems`；`0` 时仅 `str ""` 表示关闭 |

`SM_UPDATEPROP` 的 `HUMANPROP` 是原始结构体，`sizeof=44`；客户端按固定偏移解析（`client/game_client.py:446-456`）。`SM_ADDITEM` 直接发 `sizeof(ITEM)=64` 的原始结构，依赖两端 ABI 一致。
## 6. 上行通道
### 6.1 文本命令

- 帧：`LE32(len) + UTF-8 文本`，无 `#`/`!`。
- 入口：`CClientObj::OnUnCodeMsg`（`gsClientObj.cpp:322-854`）。
- 命令解析是前缀 `compare`/`==` 的 if 链；未识别命令只打印日志，不回包（`gsClientObj.cpp:853`）。
- 文本命令不要求先登录：`enter` 可以在没有 token 的情况下直接建角色/进图（`gsClientObj.cpp:349-379`）。
### 6.2 二进制 `SM_*`

- 帧：`LE32(len) + '#' + XOR(header) + XOR(payload) + '!'`。
- 入口：`CClientObject::OnFrame` 识别 `#` → `CClientObj::OnCodedMsg` → `ProcClientMsg`（`public/public.cpp:512-537`、`gsClientObj.cpp:856-860`）。
- `ProcClientMsg` 只处理 6 个命令（`gsClientObj.cpp:908-952`）：

| 上行 `wCmd` | 参数 | 处理 |
|---|---|---|
| `SM_WALK` | `wParam[0] = dir & 7` | `Walk(dir)`；成功则向本客户端补发 `SM_WALK` |
| `SM_RUN` | `wParam[0] = dir & 7` | `Run(dir)`；成功则下发 `SM_WALK`（不是 `SM_RUN`） |
| `SM_TURN` | `wParam[0] = dir & 7` | `Turn(dir)` |
| `SM_ATTACK` | `wParam[0] = dir & 7` | `Attack(dir)` |
| `SM_SPELL` | `wParam[0]=skillId`，`wParam[1]=dir & 7` | `CastSkill(skillId, dir)` |
| `SM_CHAT` | 负载=文本（`datasize` 字节） | `Say("%s", text)` |

其它命令落到 `default`，只打印 `cmd/p0/p1/p2/size` 日志（`gsClientObj.cpp:947-951`）。登录、选角、背包、商店等**不能**走二进制通道。
### 6.3 `enc` 加密上行

登录成功后服务端下发 `SM_AUTH`，负载是 32 字符 hex token（`gsClientObj.cpp:396-398`、`1094-1098`）。之后客户端可以把任意文本命令包装成：

```
enc <counterHex> <cipherHex>
```

```
# 客户端（desktop_client.py:292-298）
counter += 1; seed = token + LE64(counter); ks = sha256(seed).hexdigest()
cipher[i] = plain[i] ^ ord(ks[i % len(ks)]); 发送 "enc %x %s"

# 服务端（gsClientObj.cpp:287-305）
counter = strtoull(body[0..sp], NULL, 16); seed = token + LE64(counter)
ks = Auth_Sha256Hex(seed); out[i] = hexbyte(i) ^ ks[(i/2) % ks.size()]
```

两者等价：服务端按十六进制字符对循环，`i/2` 对应客户端的 `i`。

约束与风险：

- 只有在 `m_szToken[0] != 0` 时才尝试解密；没有 token 时 `enc ...` 会被当成普通文本命令（`gsClientObj.cpp:330-335`）。
- token 有效期 30 分钟，任何有效命令会刷新有效期（`gsClientObj.cpp:337-346`）。
- 服务端**不校验 counter 单调性**，也不带 MAC/时间戳；同一 `counter` 的密文可以重放。XOR 密钥流来自 SHA-256，安全性仅相当于“混淆”。
- `enc` 是文本通道的包装，不是二进制通道；解密后的明文再走 `OnUnCodeMsg` 的 if 链。
## 7. 文本命令清单

参数按空格分隔；`<>` 为必填，`[]` 为可选。处理位置均指 `gsClientObj.cpp`（另有说明除外）。
### 7.1 账号与会话

| 命令 | 参数 | 作用 | 处理位置 |
|---|---|---|---|
| `register` | `<账号> <密码> [职业]` | 注册账号；职业字段被忽略 | `381-392` |
| `login` | `<账号> <密码> [职业]` | 校验密码，生成 token，下发 `SM_AUTH` + 欢迎 `SM_CHAT` | `381-401` |
| `changepass` | `<旧密码> <新密码>` | 改密；用当前**角色名**当账号名查找（代码现状） | `403-411` |
| `enc` | `<counterHex> <cipherHex>` | 解密后作为命令执行 | `330-335`、`287-305` |
| `enter` | `<角色名> [职业]` | 调试直连：不登录、不校验 token，直接建/读角色并进图 | `349-379` |
| `whoami` | — | 原样返回 `client/state/player` 文本（`SendRaw`，不是 `SM_*`） | `494-501` |
### 7.2 角色

| 命令 | 参数 | 作用 | 处理位置 |
|---|---|---|---|
| `charlist` | — | 下发 `SM_CHARLIST`；要求已登录 | `414-418`、`1100-1128` |
| `createchar` | `<名字> <职业>` | 建角、发新手装、立即存档、刷新角色列表 | `419-442` |
| `selectchar` | `<名字>` | 读档/建角并进图，触发进图包序列 | `443-456` |
| `deletechar` | `<名字>` | 删除角色行 + `data/players/<名字>.dat` | `458-470` |
| `renamechar` | `<旧名> <新名>` | 改角色名 + 重命名存档文件 | `471-492` |
### 7.3 快照/面板

| 命令 | 参数 | 作用 | 处理位置 |
|---|---|---|---|
| `bag` / `equipment` / `shop` | — | 下发 `SM_BAG` / `SM_EQUIP` / `SM_SHOP` | `520-522` |
| `skills` / `questlog` | — | 下发 `SM_SKILLS` / `SM_QUESTS` | `523-524` |
| `mapdata` | — | 下发以玩家为中心的 61×41 地形窗口 | `525`、`1529-1564` |
| `titlelist` / `codex` | — | 下发 `SM_TITLES` / `SM_CODEX` | `526-527` |
| `usetitle` | `<称号id>` | 佩戴已拥有的成就/商店称号 | `528-533` |
| `buytitle` | `<称号id>` | 用成就点兑换称号 | `534-550` |
| `partylist` / `guildinfo` / `dungeonlist` | — | 分别下发 `SM_PARTY` / `SM_GUILD` / `SM_DUNGEONS` | `648-650` |
### 7.4 背包与商店

| 命令 | 参数 | 作用 | 处理位置 |
|---|---|---|---|
| `use` | `<makeIndex>` | 使用药品或技能书；刷新背包与属性 | `551-559`、`1327-1334` |
| `equipauto` | `<makeIndex>` | 按 `stdMode` 推断装备位并穿戴 | `1336-1358` |
| `unequip` | `<位置> <makeIndex>` | 脱装备到背包 | `1360-1369` |
| `sellitem` | `<makeIndex>` | 按半价卖出（最低 1 金币） | `1372-1389` |
| `buy` | `<物品名> [数量]` | 从 `shop.txt` 购买，数量上限 99 | `1391-1449` |
### 7.5 移动、战斗与 NPC

| 命令 | 参数 | 作用 | 处理位置 |
|---|---|---|---|
| `walk` | `<方向 0-7>` | 走一步；成功时向本人补发 `SM_WALK` | `840-850` |
| `attack` | `<方向 0-7>` | 攻击一次；目标在相邻格 | `560-564` |
| `pickup` | — | 拾取脚下地面物品 | `565-569` |
| `cast` | `<技能名>` | 按当前朝向施法；方向不能通过命令指定 | `570-574` |
| `restore` | — | 回满 HP/MP | `575-580` |
| `say` | `<文本>` | 附近聊天，`SM_CHAT` 的 flag=说话者 id | `515-519` |
| `talk` | `<NPC 名>` | 横纵差均 ≤3；发对话文本 + 选项 + `SM_DIALOG` | `581-598` |
| `npcshop` | `<NPC 名>` | 需商人类型；把 `shop.txt` 逐行发成 `SM_CHAT` | `599-620` |
| `npcteleport` | `<NPC 名>` | 类型 3 进副本；类型 2 传送到目标点 | `822-839` |
| `choose` | `<NPC 名> <序号>` | 执行对话选项：teleport/close/shop/quests | `783-821` |
### 7.6 社交、任务、行会、交易

| 命令 | 参数 | 作用 | 处理位置 |
|---|---|---|---|
| `quests` | — | 以 `SM_CHAT` 逐条列出任务与进度 | `743-759` |
| `accept` | `<任务名>` | 接任务；需靠近发布 NPC | `760-768` |
| `submit` | `<任务名>` | 交任务；校验进度/收集物品 | `769-782` |
| `party` | — | 下发 `SM_PARTY` 并列队友 | `709-722` |
| `partyinvite` | `<玩家>` | 邀请入队 | `723-730` |
| `partyleave` | — | 离队 | `731-737` |
| `psay` | `<文本>` | 队伍频道 | `738-742` |
| `guild create` | `<行会名>` | 1000 金币创建 | `651-665` |
| `guild join` | `<行会名>` | 加入 | `666-673` |
| `guild leave` | — | 离开（会长自动移交） | `674-679` |
| `guild donate` | `<金币>` | 捐献/升级 | `680-694` |
| `guild list` | — | 列出全部行会 | `695-708` |
| `trade` | `<玩家>` | 发起交易；同图且横纵差 ≤6、无进行中交易 | `621-647` |
| `trade add` / `trade gold` | `<makeIndex>` / `<数量>` | 放入物品 / 设置金币 | `625-638` |
| `trade lock` / `confirm` / `cancel` | — | 锁定 / 确认 / 取消 | `639-641` |
### 7.7 GM

`gm` 分支只要求 `m_pPlayer != NULL`（`gsClientObj.cpp:503-514`）；`HandleGmCommand` 先按角色名查 `data/gm.txt` 的等级（`gsClientObj.cpp:1647-1656`）。

| 命令 | 等级 | 作用 |
|---|---|---|
| `gm give/exp/gold/tp/heal/goto/recall ...` | 1 | 发物品、加经验/金币、传送、回满、去/拉玩家 |
| `gm spawn <怪> [数量]` / `gm announce <内容>` / `gm gmlist` | 2 | 附近刷怪（≤20）、全服公告、列出 GM |
| `gm kick` / `gm gmadd` / `gm gmremove` | 3 | 踢人、增删 GM 并写回 `gm.txt`；`gmhelp` 为帮助 |

源码：`gsClientObj.cpp:1665-1819`。
## 8. 一次完整登录时序

```
客户端                                          服务端
  │  login <账号> <密码>                         │
  │ ───────────────────────────────────────────▶ │ gs_AccountVerify
  │                                              │ m_szToken = Auth_RandomHex(16)
  │ ◀────────────── SM_AUTH(32 hex) ──────────── │ SendAuth
  │ ◀────────────── SM_CHAT("登录成功…") ──────── │ SendFirstDlg
  │  charlist                                    │
  │ ───────────────────────────────────────────▶ │ SendCharList
  │ ◀────────────── SM_CHARLIST(count,…) ─────── │ 只列有存档的角色
  │  selectchar 张三                             │
  │ ───────────────────────────────────────────▶ │ PlayerFactory_Create
  │                                              │  Load 存档 → NewPlayer → AddPlayer
  │ ◀── SM_SETMAP(flag=自己, x,y, name) ───────── │ OnEnterMap
  │ ◀── SM_SETPLAYER(flag=自己, x,y,0x100) ────── │
  │ ◀── SM_SETPLAYERNAME(flag=自己, 0xff, name) ─ │
  │ ◀── 0x02c4(flag=1, 无负载) ───────────────── │ 固定“战斗地图”
  │ ◀── SM_SETMAPNAME(flag=0, title) ──────────── │
  │ ◀── SM_READY(flag=0xf2d505d7) ─────────────── │
  │ ◀── SM_UPDATEPROP(金币, HUMANPROP) ────────── │ 第一次：装备/背包尚未恢复
  │ ◀── SM_APPEAR ×N ─────────────────────────── │ 自己区域 + 8 邻区的实体
  │      （自己的 SM_APPEAR 发给周围 9 区，不回发自己）│
  │                                              │ 恢复背包/装备/技能/任务/成就
  │ ◀── SM_UPDATEPROP(最终属性) ───────────────── │ PlayerFactory.cpp:83
  │ ◀── SM_CHAT("欢迎进入游戏。") ─────────────── │ selectchar 末尾
```

进图包的实际发送顺序由 `CHumanPlayer::OnEnterMap` 固定（`HumanPlayer.cpp:128-150`）：

1. `SM_SETMAP`（flag=自己，p0/p1=坐标，负载=地图 `name`）
2. `SM_SETPLAYER`（p2=0x100，负载 16 字节）
3. `SM_SETPLAYERNAME`（p0=0xff，负载=名字）
4. 原始 `0x2c4`（flag=1，无负载）
5. `SM_SETMAPNAME`（flag=0，负载=title）
6. `SM_READY`（flag=0xf2d505d7）
7. `UpdateProp()`（第一次）
8. `pArea->AppearFor(this)` + 8 邻区的 `AppearFor`（每个实体一个 `SM_APPEAR`）
9. 自己的 `SM_APPEAR` 通过 `SendAroundMsg` 发给 9 个区域（自己除外）

`enter` 调试路径没有 `SM_AUTH`/`SM_CHARLIST`；建角后还会调用 `gs_GiveStarterKit`，再补一次 `UpdateProp`（`gsClientObj.cpp:74-112`、`368-378`）。因此 `enter` 可能收到 2–3 个 `SM_UPDATEPROP`。
## 9. 代表性请求/响应

| 操作 | C→S | S→C（按发生顺序） |
|---|---|---|
| 移动 | `walk 2` | `SM_WALK`（flag=自己，p=(x,y,2)，8B `[feather,0]`）；文本处理器额外向本人补发一份 |
| 攻击 | `attack 2` | `SM_ATTACK`（攻击者，8B）→ 目标 `SM_BEATTACK`（p=(hp,mp,damage)，16B）→ 可能 `SM_DEATH`/`SM_DISAPPEAR` |
| 施法 | `cast 火球术` | `SM_SPELL`（flag=施法者，p=(skillId,dir,0)，8B `[skillId,x]`）→ 目标 `SM_BEATTACK`；治疗/增益/召唤通常只有 `SM_CHAT` |
| 聊天 | `say 你好` | `SM_CHAT`（flag=说话者，p0=0xff00，`"名字:你好"`）；自己也会收到 |
| 拾取 | `pickup` | 区域 `SM_DOWNITEMDISAPPEAR`（flag=makeIndex）→ 本人 `SM_ADDITEM`（`ITEM` 64B） |
| NPC 对话 | `talk 杂货商` | 若干 `SM_CHAT`（对话/选项文本）→ `SM_DIALOG`（结构化选项） |
| 交易 | `trade 张三` / `trade add` / `trade lock` / `trade confirm` | 双方各一份 `SM_TRADE` 镜像；双方确认后交换物品/金币，再发 `status=0` 关闭 |

关键细节：

- 攻击者本人**不会**收到自己的 `SM_ATTACK`（`SendAroundMsg` 跳过发送者，`Area.cpp:13-24`）；受击包同时发给本人和周围（`AliveObject.cpp:127-137`）。
- `SM_RUN` 上行回包用的是 `SM_WALK`（`gsClientObj.cpp:921-929`）；`SM_SPELL` 在扣 MP 后发送（`HumanPlayer.cpp:382`）。
- `Say` 统一加 `"名字:"` 前缀并截到 120 字节（`AliveObject.cpp:158-179`）。
- `pickup` 在脚下格找 `OBJ_DOWNITEM`（`HumanPlayer.cpp:800-805`）；移除地图对象触发消失包，加入背包触发 `SM_ADDITEM`（`HumanPlayer.cpp:782-793`）。
- `talk` 需要横纵差均 ≤3（`GameActions.cpp:34-44`），先逐条发选项文本再发结构化 `SM_DIALOG`（`gsClientObj.cpp:581-598`、`1069-1092`）。

```text
# NPC 对话负载（SM_DIALOG，flag=0）
str("杂货商") + str(对话文本) + u16(3)
  + [u8(1) str("看看货架")   str("shop")   str("")]
  + [u8(2) str("有什么任务") str("quests") str("")]
  + [u8(3) str("离开")       str("close")  str("")]

# 交易打开（SM_TRADE，status=1）
u8(1) + str("张三") + u32(myGold) + u32(otherGold)
      + u8(myLock) + u8(otherLock) + u8(myConfirm) + u8(otherConfirm)
      + u16(myItems)[u32 makeIndex + str name + u16 count ...]
      + u16(otherItems)[...]
```

`trade` 要求同图、横纵差均 ≤6、双方都不在其它交易中（`Trade.cpp:92-109`）；`SendSide` 为每一方组装“我/对方”两份视图（`Trade.cpp:42-77`）。
## 10. 客户端实现位置

| 文件 | 作用 | 关键位置 |
|---|---|---|
| `client/game_client.py` | tkinter 客户端 + 协议解析 + headless 验收 | `decode` 46-47、`make_frame` 50-51、`GameClient` 318-368、`_on_packet` 369-377、`_apply` 378-709、`run_headless` 715 |
| `client/desktop_client.py` | Pygame 桌面客户端；登录后走 `enc` 加密上行 | `cmd` 278-291、`secure` 292-298、`wait_token` 300-307、`login` 313 |
| `tools/test_client.py` | 命令行调试客户端；发文本、按 `SM_*` 解码打印 | `frame` 50-51、`decode` 54-55、`describe` 58-81、`recv_frames` 84-105 |
| `tools/web_game.py` | 浏览器版桥接：**HTTP/JSON 不是游戏协议** | `PORT=8100` 25、`serialize` 37、`poll_loop` 83、`secure_send` 120-135；内部用 `game_client.GameClient` 连 7100（`tools/web_game.py:171`） |

`tools/web_game.py` 的 `/state`、`/api/*` 只是把 `GameState` 序列化成 JSON 给浏览器 Canvas；真正的线协议仍由 `game_client.py` 的 `GameClient` 与 7100 端口交互。不要把它当作第三种游戏协议。

`client/game_client.py` 的 `SM` 映射只覆盖到 0x0118 + 0x02c4；后续快照编号靠 `_apply` 里的数字分支解析（`client/game_client.py:30-40`、`378-709`）。新增消息时映射和分支都要改。
## 11. 兼容性与扩展
### 11.1 与 2005 年真实客户端

**不能直连**。原因：

- 2005 快照没有 `public/public.h`，原始的消息号、头结构、`_CodeGameCode` 实现均不可考；当前 `SM_*` 数值只保证仓库内唯一（`public/public.h:82-127`）。
- 16 字节 `MIRMSGHEADER`、4 字节小端分帧、XOR key 都是重建约定（`public/public.h:778-791`）。
- 登录流程、`SM_AUTH` token、`enc` 加密是新增的，真实客户端没有这些。
- `ITEM`/`CHARDBINFO` 的二进制布局按当前编译器 ABI 固定（`sizeof(ITEM)=64`、`sizeof(CHARDBINFO)=120`）。

接入真实客户端需要替换：协议常量、`_CodeGameCode/_UnGameCode`、分帧方式、登录握手，以及所有结构体布局；不是改几个常量就能完成。
### 11.2 新增消息号的约定

1. **只追加，不改旧值**：在 `public/public.h:86-127` 的枚举末尾加 `SM_XXX`，下一个可用值是 `0x0128`。不要重排/复用旧编号，客户端按数值分支。
2. **同时更新客户端映射**：`client/game_client.py:30-40` 的 `SM` 与 `tools/test_client.py:21-47` 的 `SM_NAMES`；`tools/web_game.py` 复用 `game_client`，通常不用改。
3. **负载用小端定长/长度前缀**：沿用 `gs_PutU8/U16/U32/Str`（`gsClientObj.cpp:1040-1067`）；字符串是 `u8 长度 + UTF-8`，不要塞 `std::string` 原始内存。
4. **服务端发送**：用 `SendMsg(flag, cmd, p0,p1,p2, data, size)`；大快照先算长度，注意 8192 上限。
5. **上行命令**：如果新消息需要客户端发，必须在 `ProcClientMsg`（`gsClientObj.cpp:908-952`）加 case；否则会落到 default 只打日志。文本命令则加在 `OnUnCodeMsg` 的前缀 if 链。
6. **结构体兼容**：新增字段只能加在 `ITEM`/`CHARDBINFO` 末尾并同步提升 `STORE_VERSION` 与所有客户端解析；否则旧存档/旧客户端会错位。
7. **不要依赖 `dwFlag` 全局语义**：新消息请在自己文档里写清 `dwFlag` 是对象 id、makeIndex、金币还是魔数。
### 11.3 健壮性与安全注意

- **文本/二进制分流靠首字节**：任何以 `#` 开头且长度 ≥18 的文本都会被当作二进制包解析（`public/public.cpp:517-521`）。新增文本命令不要以 `#` 开头。
- **不校验结尾 `!`**：`OnFrame` 只按帧长截取负载，`!` 只是约定；不要依赖它做完整性校验。
- **无版本协商**：服务端不检查客户端版本/协议号；`SM_AUTH` 只是会话 token。
- **`enter` 绕过登录**：任何能连上 7100 的客户端都可以用 `enter` 直接进游戏（`gsClientObj.cpp:349-379`）；生产环境应删除或加白名单。
- **GM 权限来自 `data/gm.txt`**：按角色名匹配，文件可被 `/gm gmadd` 改写；默认 `测试玩家/3`（`data/gm.txt`）。
- **密码存储是加盐 SHA-256**（`Auth.cpp:148-151`），不是慢哈希；`accounts.txt` 中的 salt/hash 可直接用于离线爆破。
- **XOR 不是加密**：`_CodeGameCode` 只防抓包肉眼阅读；`enc` 也只是 SHA-256 密钥流异或，无 MAC、无防重放。
- **单包 8192 上限**：客户端侧 `GameClient.poll` 用 65536 上限（`client/game_client.py:356-362`），服务端只接受 ≤8192；大快照（行会/副本）要注意不要接近上限。
