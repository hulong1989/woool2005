# 数据格式（DATA-FORMAT）

> 本文描述 `data/` 下所有表、地图、存档与运行态文件的字段和加载/写入代码。行号按当前源码核对。
> 所有文本表均为 UTF-8、`/` 分隔、`#` 开头整行注释、空行跳过；布尔/枚举字段用整数表示。

## 1. 通用规则
| 规则 | 说明 | 依据 |
|---|---|---|
| 编码 | UTF-8（无 BOM 也可）；`CStringFile` 按字节读入 | `public/public.cpp:271-289` |
| 分隔符 | `/`；字段首尾空白会被 `SearchParam` 去掉 | `public/public.cpp:55-80` |
| 注释 | 仅整行 `#`；**不支持行尾注释** | 各 `Load` 的 `*pszLine == '#'` 判断 |
| 空行 | 跳过；但被跳过的行仍占行号 | 同上 |
| 最少列数 | 每个加载器有自己的下限，不足则整行丢弃 | 见第 3 节 |
| 数值 | `atoi`/`strtoul`，无千分位；非法值按 0 | 各 `Load` |
| ini | `CDataFile` 额外支持 `;` 注释、`[section]`、键名大小写不敏感 | `public/public.cpp:205-239` |

`CStringFile` 把所有行原样保存（包括空行），加载器自己判断；`CDataFile` 则把 `section/key` 存进 map，值会 trim（`public/public.cpp:205-262`）。

## 2. 表与加载点
| 表 | 行数 | 加载位置 | 加载器 |
|---|---|---|---|
| `baseitem.txt` | 151 | 启动，`CGameWorld::Init` | `GameWorld.cpp:28-38` → `ItemManager.cpp:31-75` |
| `monsters.txt` | 95 | 启动，`CServer::InitServer` | `Server.cpp:59` → `Monster.cpp:293-329` |
| `drops.txt` | 150 | 启动 | `Server.cpp:60` → `Monster.cpp:331-370` |
| `spawns.txt` | 134 | 启动 | `Server.cpp:61` → `Monster.cpp:434-480` |
| `classes.txt` | 3 | 启动（最早） | `Server.cpp:52` → `ClassTable.cpp:20-46` |
| `levels.txt` | 50 | 启动 | `Server.cpp:52` → `ClassTable.cpp:67-82` |
| `skills.txt` | 35 | 启动 | `Server.cpp:53` → `Skill.cpp:18-41` |
| `shop.txt` | 53 | **每次读请求时** | `gsClientObj.cpp:1283`、`1403`；`ServerConsole.cpp:513` |
| `npcs.txt` | 50 | 启动 | `Server.cpp:64` → `Npc.cpp:76-119` |
| `quests.txt` | 13 | 启动 | `Server.cpp:65` → `Quest.cpp:14-52` |
| `dungeons.txt` | 8 | 启动 | `Server.cpp:74` → `Dungeon.cpp:31-89` |
| `bosses.txt` | 42 | 启动 | `Server.cpp:72` → `Boss.cpp:21-45` |
| `events.txt` | 2 | 启动 | `Server.cpp:79` → `Event.cpp:30-77` |
| `achievements.txt` | 15 | 启动 | `Server.cpp:69` → `Achievement.cpp:16-101` |
| `titles.txt` | 16 | 启动 | `Server.cpp:70` → `Title.cpp:14-70` |
| `sets.txt` | 20 | 启动 | `Server.cpp:68` → `Set.cpp:32-83` |
| `dialogues.txt` | 36 | 启动 | `Server.cpp:78` → `Dialogue.cpp:14-39` |

“行数”是当前数据文件的**有效行**（去掉注释/空行），下同。

## 3. 逐表
### 3.1 `baseitem.txt`
加载：`GameWorld.cpp:28-38` → `CItemManager::AddItemClassString`（`ItemManager.cpp:31-75`）；实例化在 `MakeupTemplateItem`（`ItemManager.cpp:122-171`）。
字段（20 个必填 + 2 个可选）：
```
名字/stdmode/shape/image/specialpower/ac1/ac2/mac1/mac2/dc1/dc2/mc1/mc2/sc1/sc2/weight/maxdura/price/needtype/needlevel[/useType/useValue]
```

- `stdmode` 决定装备位与属性映射（`Equipment.cpp:83-160`、`283-303`）：5/6 武器，10/11 衣服，15 头盔，19–21 项链，22/23 戒指，24/26 手镯；0 为药品/书/杂项。
- `ac/mac/dc/mc/sc` 各两段为 min/max；`maxdura` 在实例化时 **×1000**（`ItemManager.cpp:137`）。
- `needtype`：0 等级 / 1 DC / 2 MC / 3 SC；`needlevel` 为对应阈值（`Equipment.cpp:307-328`）。
- `useType`：1 回 HP / 2 回 MP / 3 技能书（`useValue`=技能 id）（`HumanPlayer.cpp:679-711`）。
- 名字上限实际受 `ITEMBASE.szName[16]` 限制；`stdmode=10/11` 且名字不带“男/女”时会自动补后缀（`ItemManager.cpp:91-103`）。
示例：`木剑/5/0/1/0/0/0/0/0/2/5/0/0/0/0/5/1/100/0/1`；药品 `金创药/0/0/7/0/0/0/0/0/0/0/0/0/0/0/1/1/10/0/1/1/30`。
### 3.2 `monsters.txt`
加载：`Server.cpp:59` → `Monster.cpp:293-329`；模板结构见 `Monster.h:9-21`。
字段：`名字/最大HP/最小攻击/最大攻击/防御/经验/掉落物品/掉落概率(0-100)/等级/外形`

- 前 8 列必填；等级/外形可省略，默认 1/0（`Monster.cpp:322-323`）。
- `掉落物品/概率` 只在 `drops.txt` **没有**该怪记录时作为单掉落兜底（`Monster.cpp:124-140`）。
- `最大HP <= 0` 会被钳到 1（`Monster.cpp:324`）。
示例：`鸡/30/1/3/0/10/金创药/50/1/0`；`沃玛教主/2000/30/50/10/6000/铁剑/80/20/0`。
### 3.3 `spawns.txt`
加载：`Server.cpp:61` → `Monster.cpp:434-480`；`Spawn` 在 `Monster.cpp:397-431`。
字段：`地图id/x/y/数量/怪物名`

- 实际坐标在 `x±2, y±2` 随机（`Getrand(5)-2`，`Monster.cpp:470-471`）。
- 数量上限 50（`Monster.cpp:457`）；地图/怪物名任一不存在则该只刷不出。
- 刷怪点会保存在 `CMonsterMgr::m_Spawns` 中，供副本 `ResetMap` 重刷（`Monster.cpp:509-548`）。
示例：`1/470/240/6/鸡`。
### 3.4 `drops.txt`
加载：`Server.cpp:60` → `Monster.cpp:331-370`；判定在 `RollDrops`（`Monster.cpp:379-395`）。
字段：`怪物名/物品名/概率(0-100)`

- 同一只怪可以有多行，**逐行独立判定**（不是权重表）。
- 概率会被钳到 0–100；物品名不存在时静默跳过（`Monster.cpp:352-353`）。
- 有该怪记录时，`monsters.txt` 里的单掉落不再生效。
示例：`鸡/金创药/100`、`鸡/木剑/5`。
### 3.5 `classes.txt` 与 `levels.txt`
加载：`Server.cpp:52` → `ClassTable.cpp:14-90`。
`classes.txt` 字段：`id/名称/HP成长/MP成长/最小攻击/最大攻击/最小防御/最大防御/魔法/道术/负重成长`

- 至少 11 列；不足则整行丢弃（`ClassTable.cpp:30`）。
- 成长值按“每级增量”参与 `ApplyClassStats`：`maxhp=50+hpGrow*(L-1)` 等（`HumanPlayer.cpp:318-346`）。
- 缺文件/空表时有内置战/法/道兜底（`ClassTable.cpp:47-64`）。
示例：`0/战士/60/10/2/3/1/2/0/0/10`、`1/法师/38/50/0/1/0/1/2/0/5`。

`levels.txt` 字段：`等级/升到下一级所需经验`

- 下标=等级；文件最大等级即 `MaxLevel()`（`ClassTable.cpp:104-107`）。
- `LevelExpNeed(n)` 返回从 n 升到 n+1 所需经验；满级返回 0（`ClassTable.cpp:109-116`）。
- 当前 50 行（1..50 级），第 50 级为满级。
示例：`1/110`、`50/11725400`。
### 3.6 `skills.txt`
加载：`Server.cpp:53` → `Skill.cpp:18-41`。
字段：`id/名称/职业(0战 1法 2道)/需要等级/消耗MP/冷却(ms)/威力(百分比)/类型/射程`

- 至少 9 列（`Skill.cpp:27`）。
- 实际类型比文件头注释多：1 物理、2 魔法、3 治疗、4 道术、5 范围、6 魔法盾、7 增益、8 召唤、9 施毒（`HumanPlayer.cpp:386-470`）。
- 技能 id 用 `&31` 映射到冷却槽位与已学位图（`HumanPlayer.cpp:371-375`、`HumanPlayer.h:64`）。
示例：`3/火球术/1/1/4/900/165/2/6`。
### 3.7 `shop.txt`
读取：`gsClientObj.cpp:1283`（快照）、`1403-1412`（购买）；`ServerConsole.cpp:495-533`。
字段：`物品名/售价`

- 全服只有一张货架，所有类型 1 的商人 NPC 共用（`gsClientObj.cpp:599-620`）。
- 售价为整数金币；购买数量上限 99，金币不足/背包满有校验（`gsClientObj.cpp:1399-1443`）。
- 每次请求都重新读文件，因此改价**不需要重启/热更新**。
示例：`金创药/50`、`木剑/300`。
### 3.8 `npcs.txt`
加载：`Server.cpp:64` → `Npc.cpp:76-119`。
字段：`名字/地图id/x/y/类型/目标地图/目标x/目标y/对话`

| 类型 | 含义 | 行为 |
|---|---|---|
| 1 | 商店 | `npcshop` 列出 `shop.txt`；`talk` 显示对话 |
| 2 | 传送员 | `npcteleport` 传送到目标地图/坐标 |
| 3 | 副本入口 | `npcteleport` 调 `CDungeonMgr::Enter`（`gsClientObj.cpp:822-839`） |

- 至少 8 列；第 9 列对话可省略（`Npc.cpp:90-104`）。
- NPC 进入地图时锁自己的格（`Npc.cpp:58-64`），玩家不能穿过。
- `shape` 直接取类型值（`Npc.cpp:38`），客户端用它画不同外观。
示例：`杂货商/1/477/215/1/0/0/0/欢迎光临中州杂货铺，要买点什么？`；`副本使者/1/496/215/3/8/256/256/组队挑战沃玛神殿，要进去吗？`。
### 3.9 `quests.txt`
加载：`Server.cpp:65` → `Quest.cpp:14-52`。
字段：`id/名称/发布NPC/类型(1杀怪 2收集)/目标/数量/经验/金币/奖励物品/数量/需求等级/对话/[前置任务id]/[可重复]`

- 至少 11 列；对话/前置/可重复可省略（`Quest.cpp:44-46`）。
- 类型 1 的“目标”是怪物名，类型 2 是物品名；提交时收集类会先扣物品（`HumanPlayer.cpp:590-599`）。
- 前置任务未完成/等级不足/已接/已完成会拒绝接取（`HumanPlayer.cpp` 的 `AcceptQuest`）。
- 可重复任务提交后回到“可接”（state=0），普通任务置“已完成”（state=2）（`HumanPlayer.cpp:617-626`）。
示例：`1/除害/杂货商/1/鸡/10/500/100/金创药/3/1/最近鸡太多了，帮我消灭10只鸡。`；带前置：`5/挑战沃玛教主/寺庙守卫/1/沃玛教主/1/5000/1000/魔法药/5/10/.../3/0`。
### 3.10 `dungeons.txt`
加载：`Server.cpp:74` → `Dungeon.cpp:31-89`。
字段：`id/名称/Boss/实例地图(逗号)/通关成就id/[首通称号id]`

- 至少 5 列；每个实例地图生成一个 `DUNGEONSLOT`（`Dungeon.cpp:74-86`）。
- 实例难度按“该副本内第几个槽位”决定：1=普通、2=困难、其他=地狱（`Dungeon.cpp:182-186`）。
- 进入要求已组队；同队优先复用槽位，否则找空槽；槽位满返回失败（`Dungeon.cpp:114-180`）。
- 进入时清空该地图普通怪 + Boss 并立即重刷（`Dungeon.cpp:149-158`）。
示例：`1/沃玛神殿/沃玛教主/8,9,10/4/201`。
### 3.11 `bosses.txt`
加载：`Server.cpp:72` → `Boss.cpp:21-45`；刷新在 `CBossMgr::Update`（`Boss.cpp:52-75`）。
字段：`怪物名/地图id/x/y/刷新秒数`

- 至少 5 列；刷新秒数 <10 会被钳到 10（`Boss.cpp:27`）。
- 死亡时按“地图 + 实例 id + 名字”确认是自己刷出的那只，再倒计时（`Boss.cpp:81-86`）。
- 内部 `dwInterval=0` 表示世界 Boss，不自动重刷（`Boss.cpp:60`、`188`）。
示例：`沃玛教主/3/200/200/120`。
### 3.12 `events.txt`
加载：`Server.cpp:79` → `Event.cpp:30-77`；触发在 `Event.cpp:96-134`。
两种记录：
```
doubleexp/名称/开始HH:MM/持续分钟
worldboss/名称/开始HH:MM/持续分钟/地图/x/y/怪物
```

- `doubleexp` 至少 4 列；`worldboss` 至少 8 列（`Event.cpp:44-60`）。
- 到点后按 `tm_mday` 去重，每天只触发一次（`Event.cpp:147-160`）。
- 双倍经验通过 `CEventMgr::StartDoubleExp` 设置截止时间；`AddExp` 中翻倍（`HumanPlayer.cpp:288`）。
- 世界 Boss 刷出后登记到 `CBossMgr::RegisterWorldBoss`，不自动重刷（`Event.cpp:106-120`）。
示例：`doubleexp/双倍经验/21:00/30`、`worldboss/世界Boss/22:00/10/4/280/240/沃玛教主`。
### 3.13 `achievements.txt`
加载：`Server.cpp:69` → `Achievement.cpp:16-101`。
字段：`id/名称/描述/类型/值/点数/称号加成`

- 至少 5 列；点数默认 10；第 7 段是 `HP:20,DC:2` 形式的加成（`Achievement.cpp:35-90`）。
- 类型：`level/boss/dungeon/upgrade/enchant/quest/codex`（`Achievement.cpp:147-182`）。
- 匹配规则：`boss`/`dungeon` 按字符串相等；`level/upgrade/enchant/quest/codex` 按数值 `nNumber >= nValue`（`Achievement.cpp:121-136`）。
- 加成只在佩戴该称号时通过 `GetTitleBonus` 生效（`HumanPlayer.cpp:1039-1059`）。
示例：`1/初出茅庐/达到 10 级/level/10/10/HP:20`。
### 3.14 `titles.txt`
加载：`Server.cpp:70` → `Title.cpp:14-70`。
字段：`id/名称/价格(成就点)/称号加成`

- 至少 3 列；价格 `<0` 表示不可用点数购买（`BuyTitle` 拒绝，`HumanPlayer.cpp:1150-1156`）。
- 首通称号 201–208 价格为 0，由副本首通 `GrantTitle` 授予；但 `buytitle` 对 0 价不作特殊排除，当前可被 0 点兑换（数据/代码现状，见 3.14 注意）。
- 加成格式与成就相同：`HP:5`、`DC:3`、`SC:3` 等，可逗号分隔。
示例：`101/新兵/20/HP:5`、`201/沃玛首通者/0/HP:20,DC:2`。

> 注意：`SendTitles` 会列出全部 `CTitleMgr` 称号（`gsClientObj.cpp:1594-1605`），而 `buytitle` 只检查 `cost>=0 && points>=cost`，因此 0 价首通称号可被玩家兑换。若要保持“首通专属”，应在 `BuyTitle` 排除 `cost==0` 或给首通称号用 `-1`。
### 3.15 `sets.txt`
加载：`Server.cpp:68` → `Set.cpp:32-83`；应用在 `CEquipment::ApplySetBonuses`（`Equipment.cpp:342-370`）。
字段：`套装名/成员(逗号)/触发件数/属性:值(逗号分隔)`

- 成员按**物品名**匹配（不是 id）；触发件数为 0 时按 1 处理（`Set.cpp:50-51`）。
- 每个套装独立判定、可叠加；加成写入装备属性缓存，穿脱/强化后重算。
- 属性名支持 `AC/MAC/DC/MC/SC`，不支持 `HP/MP`（`Set.cpp:14-30`）。
示例：`战士套装/重盔甲(男),骷髅头盔,裁决之杖/2/DC:5,AC:3`。
### 3.16 `dialogues.txt`
加载：`Server.cpp:78` → `Dialogue.cpp:14-39`；查询在 `GetOptions/GetOption`（`Dialogue.cpp:41-57`）。
字段：`NPC/序号/选项文本/动作/参数`

- 至少 5 列；动作：`shop`、`quests`、`teleport`、`close`（`gsClientObj.cpp:798-819`）。
- `teleport` 参数格式 `地图,x,y`；`shop/quests` 只是提示玩家用对应命令，不直接打开界面。
- 同一 NPC 的序号需唯一；`choose` 按 NPC+序号查找（`Dialogue.cpp:51-57`）。
示例：`杂货商/1/看看货架/shop/`、`传送员/1/去落霞岛/teleport/2,200,200`。

---
## 4. 地图格式
### 4.1 逻辑图 `data/maps/logic/{id}.ini`
```ini
[define]
name=中州
title=中州
blockmap=map0
type=1
areawidth=32
areaheight=32
```

| 键 | 读取 | 含义 |
|---|---|---|
| `blockmap` | `LogicMap.cpp:178` | 物理图 `[map] name`，缺失则加载失败 |
| `name` | `LogicMap.cpp:179` | 地图名，下发 `SM_SETMAP` |
| `areawidth` / `areaheight` | `LogicMap.cpp:180-181` | 每个 Area 的格数，默认 1 |
| `title` | `LogicMap.h:54` | 地图标题，缺省回退 `name` |
| `type` | **未读取** | `m_Type` 保持 `EMT_NOFIGHT`（`LogicMap.h:88`） |

地图 id = 文件名数字（`LogicMapMgr.cpp:33-42`）；`1.ini`→1、`60.ini`→60。当前 60 张逻辑图复用 27 张物理图。
### 4.2 物理图 `data/maps/physics/*.nmp`
原版为二进制，重建为文本（`PhysicsMap.h:7-16`）：
```ini
[map]
name=map0
width=512
height=512
[block]
0=111111...
218=000...11111111111000...
```

- `name` 缺省用文件名去扩展名；宽高必须在 1–4096（`PhysicsMap.cpp:22-41`）。
- `[block]` 的键是行号 `y`；值逐字符解析，`1` 或 `#` 阻挡，其他字符可走；逗号/空格/制表符跳过（`PhysicsMap.cpp:45-60`）。
- 缺失的行视为整行可走（`PhysicsMap.cpp:50`）。
- `tools/gen_sample_map.py` 可生成示例图（`tools/gen_sample_map.py:18-46`）。

---
## 5. 存档与运行态
### 5.1 `data/players/*.dat`（GSDB11）
写入：`CPlayerStore::Save`（`PlayerStore.cpp:72-158`）；读取：`Load`（`161-328`）。完整字段顺序：

| # | 字段 | 长度 | 说明 |
|---|---|---|---|
| 1 | magic | 6 B | `"GSDB11"` |
| 2 | version | 4 B | `5` |
| 3 | `CHARDBINFO` | 120 B | 基础属性/坐标/金币/经验 |
| 4 | 已学技能 | 4 B | 位图 |
| 5 | 任务数 + 数组 | 4 + N×12 B | `PLAYERQUEST` |
| 6 | 背包数 + 数组 | 4 + N×64 B | `ITEM`，最多 46 |
| 7 | 装备数 + (位置+物品) | 4 + N×65 B | `BYTE pos` + `ITEM` |
| 8 | 成就数 + int[] | 4 + N×4 B | 成就 id |
| 9 | 当前称号 | 4 B | 0 表示无 |
| 10 | 已消费点数 | 4 B | 商店兑换累计 |
| 11 | 商店称号数 + int[] | 4 + N×4 B | 已购买/首通授予 |
| 12 | 图鉴数 + 条目 | 4 + Σ(4+len) B | 物品名 |
| 13 | 图鉴奖励数 + 条目 | 4 + Σ(4+len) B | 套装名 |

- 读档对 magic/version 与各数组上限做校验（任务≤16、背包≤200、装备≤10、成就≤256、商店称号≤64、图鉴/奖励≤256），失败按新角色处理。
- 原始结构体写入，**依赖编译器 ABI（端序/padding）**；当前 `sizeof(ITEM)=64`、`sizeof(CHARDBINFO)=120`、`sizeof(HUMANPROP)=44`。
- 文件名为清洗后的角色名（保留中文/字母/数字，其余 `_`，`PlayerStore.cpp:28-61`）；删角/改名用原始名拼路径（`gsClientObj.cpp:465,483-485`）。
### 5.2 账号与角色索引
| 文件 | 格式 | 读写位置 |
|---|---|---|
| `accounts.txt` | `账号/salt/hash`；`hash=sha256(salt+password)` 小写 hex；salt 16 hex | 懒加载 `gsClientObj.cpp:119-138`；注册/改密写 `140-146,156-187` |
| `characters.txt` | `账号/角色名/职业` | 懒加载 `gsClientObj.cpp:193-212`；建/删/改名写 `214-261` |
| `gm.txt` | `角色名/等级(1-3)` | 启动 `Gm.cpp`；`/gm gmadd/gmremove` 写回 |
### 5.3 行会、Boss、副本、赛季
| 文件 | 格式 | 写入代码 | 时机 |
|---|---|---|---|
| `guilds.dat` | `name/leader/vice/level/funds/m1,m2,...` | `Guild.cpp:77-96` | 退出 |
| `guildware.dat` | `"GWH1" + u32 行会数 + 每行会[u32 名长+名+u32 物品数+ITEM[]]` | `Guild.cpp:308-332` | 退出 |
| `boss_kills.dat` | 每行 `地图id/怪物名` | `Boss.cpp:140-147` | 首杀 + 退出 |
| `dungeon_first.dat` | 每行一个副本 id | `Dungeon.cpp:400-411` | 首通 + 退出 |
| `dungeon_rank.dat` | `副本/队伍/秒数/MM-DD` | `Dungeon.cpp:368-379` | 通关 + 退出 |
| `season.dat` | 一个整数 + 换行 | `Dungeon.cpp:451-460` | 赛季结算 + 退出 |

行会仓库是 50 格（`Guild.cpp:292`）；排行每副本保留前 5（`Dungeon.cpp:86-98`、`Dungeon.h:12`）；副本实例最长 1800 秒（`Dungeon.h:11`）。

---
## 6. 热更新
`Server.cpp:109-155` 每约 1 秒 `stat` 这 10 个文件的 mtime，变化则重载并全服公告：

| 文件 | 重载动作 | 副作用 |
|---|---|---|
| `classes.txt` / `levels.txt` | `CPlayerClassMgr::Load` | 老角色不会立即重算；下次升级才用新表 |
| `skills.txt` | `CSkillMgr::Load` | 已学技能位图保留；已学但新表删掉的技能无法释放 |
| `monsters.txt` / `drops.txt` | `CMonsterMgr::LoadTemplates/LoadDrops` | 只影响之后新刷的怪；现有怪不重置 |
| `npcs.txt` | `CNpcMgr::ClearAll` + `Load` | 现有 NPC 被删重建；若玩家站在 NPC 格上可能暂时重叠 |
| `quests.txt` | `CQuestMgr::Load` | 玩家任务进度保留；被删任务无法提交 |
| `events.txt` | `CEventMgr::Load` | 排期替换；**正在进行的双倍经验计时不清零** |
| `bosses.txt` | `CBossMgr::Load` | `m_Bosses.clear()`，现有 Boss 失去跟踪；下一帧会按新表重刷（可能重复） |
| `sets.txt` | `CSetMgr::Load` | 已穿戴角色的属性缓存不会自动刷新，需再穿/脱/强化一次 |

不受热更新：`baseitem.txt`（启动时加载）、`spawns.txt`（启动/副本 ResetMap）、`shop.txt`（每次请求重读，实际立即生效）、`dungeons.txt`、`achievements.txt`、`titles.txt`、`dialogues.txt`、`gm.txt`、`accounts.txt`、`characters.txt`。

---
## 7. 新增内容的最小改动
| 目标 | 最小改动 | 生效方式 / 注意 |
|---|---|---|
| 加一件物品 | `baseitem.txt` 追加 20(+2) 列；如需售卖加 `shop.txt`，如需掉落加 `drops.txt`，如需套装加 `sets.txt` | **重启**（baseitem 不热更）；`makeIndex` 自动生成，不必手填 |
| 加一种怪 | `monsters.txt` 追加行；`spawns.txt` 加刷怪点（或 Boss 表）；可选 `drops.txt` 多掉落 | 模板/掉落热更，刷怪点需重启（副本图可用 `ResetMap`） |
| 加刷怪点 | `spawns.txt` 追加 `地图id/x/y/数量/名` | 重启；坐标 ±2 随机、数量≤50 |
| 加一条任务 | `quests.txt` 追加行；发布 NPC 必须在 `npcs.txt`；奖励物品必须在 `baseitem.txt`；前置任务 id 要存在 | 热更；任务 id 唯一，`nCount>0` |
| 加一个副本 | `dungeons.txt` 一行 + 每个实例地图 `logic/*.ini`/`physics/*.nmp` + 类型 3 的入口 NPC + 各实例地图的 `bosses.txt` 行 + 成就/称号 id | **重启**；实例地图必须是已加载的逻辑图；首通称号放 `titles.txt` |
| 加一个 Boss | `bosses.txt` 一行；怪物模板、地图必须存在 | 热更；热更会清空现有 Boss 跟踪（见第 6 节） |
| 加一张地图 | `logic/{id}.ini` + `physics/*.nmp`；`blockmap` 指向物理图 `[map] name` | **重启**；地图 id=文件名数字 |
| 加一个 NPC | `npcs.txt` 一行；类型 1/2/3；传送目标坐标必须可走 | 热更（Clear+Load） |
| 加套装 | `sets.txt` 一行；成员名必须与 `baseitem.txt` 完全一致 | 热更；已穿角色需重新触发属性重算 |

---
## 8. 数据校验
`tools/validate_data.py` 存在，用法：
```bash
python3 tools/validate_data.py                 # 全部
python3 tools/validate_data.py --strict        # warning 也失败
python3 tools/validate_data.py --only items,skills
python3 tools/validate_data.py --quiet         # 只打印错误与汇总
```

- 分组（`GROUPS`）：`maps, items, monsters, spawns, drops, classes, levels, skills, shop, npcs, quests, bosses, dungeons, achievements, titles, sets, dialogues, events, accounts, gm`（`tools/validate_data.py:788-790`）。
- 检查项：字段数、数值范围、交叉引用（物品/怪物/NPC/地图/成就/称号 id）、地图文件完整性、账号/角色/GM 表；纯只读。
- 退出码：0 通过（可含 warning），1 有 error（`--strict` 下 warning 也算失败）（`tools/validate_data.py:11-13`）。
- 当前运行结果：**2350 项检查，0 error，16 warning**；warning 主要是 `ITEMBASE.szName[16]` 只能放 15 字节，若干 17–20 字节中文物品名会被截断（`data/baseitem.txt` 的“雷霆战甲(男)”等）。

运行时回归用 `python3 tools/smoke_test.py`（会启停服务端并写入测试存档），与静态校验互补。
