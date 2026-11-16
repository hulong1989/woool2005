# 来源与授权说明（NOTICE）

本文件说明本仓库各部分的来源与授权状态。代码与素材的**权利归属不同**，使用前请分别确认。

## 1. 代码来源

| 部分 | 来源 | 说明 |
|---|---|---|
| 根目录 2005 年原始逻辑层（40 个 `.cpp/.h`，GBK，VC++ 7.1） | 2005 年 4 月服务端快照（原工程 `chuanqiServer`，`D:/chuanqiServer`） | 完整备份在 [`original_gbk/`](original_gbk/)，未改动 |
| `public/`、`Server.h`、`GameServer.h`、`LogicMap.h`、`Area.h`、`PhysicsMap.*`、`ItemBox.h`、`gsClientObj.cpp` 等 | 本仓库重建 | 依据是原代码的调用点与 2005-05-26 的 Doxygen 文档（[`bangzhu/`](bangzhu/)）；逐条记录见 [docs/RECONSTRUCTION.md](docs/RECONSTRUCTION.md) |
| `Monster`、`Npc`、`Quest`、`Party`、`Guild`、`Boss`、`Dungeon`、`Achievement`、`Title`、`Set`、`Trade`、`Event`、`PlayerStore`、`PlayerFactory`、`ClassTable` 等玩法模块 | 本仓库新增 | 原始快照中没有这些系统 |
| `client/`、`tools/`、`server996/`、`data/`、`docs/` | 本仓库新增 | 客户端、测试/管理工具、数据表与文档 |
| 协议实现（消息号、加密、分帧） | 本仓库重建 | **不是** 2005 年官方协议，不能与真实客户端互通 |

## 2. 代码授权状态

- 本仓库**没有附加 LICENSE 文件**，因此未授予任何明示或暗示的使用许可。
- 2005 年原始代码的著作权归原权利人所有；本仓库对其做的是技术研究与重建实践，不构成商业使用授权。
- 新增代码的授权方式由仓库所有者决定。在仓库所有者在 `LICENSE` 中明确声明前，默认保留所有权利。

## 3. 第三方素材

桌面客户端与文档中使用的第三方美术/音效素材及许可（均为公开授权素材）：

| 素材 | 作者 | 许可 | 用途 |
|---|---|---|---|
| FANTASY GUI | MELLE（OpenGameArt） | CC0 1.0 | 桌面客户端面板/边框/按钮 |
| Isometric Hero and Creatures | Clint Bellanger（OpenGameArt） | CC-BY 3.0 | 玩家角色（裁剪/缩放/着色） |
| Dungeon Crawl Stone Soup tiles | DCSS 社区 | CC0 1.0 | 怪物/NPC/物品/地图贴图 |
| Stone Soup isometric floors/walls | Stone Soup 社区 | CC0 1.0 | 等距地砖/墙体 |

完整清单与来源链接见 [`client/assets/CREDITS.txt`](client/assets/CREDITS.txt)。CC-BY 3.0 素材要求保留署名，修改版本已在此与 CREDITS 中注明。

**未包含**：996 引擎、传奇世界（传世）、爱传世等商业产品的原版切图、地图、音效或数据。

## 4. 其它

- `README-BUILD.md` 是开发过程记录，其中部分"已知限制"写于早期，可能落后于当前代码，以实际代码与 `docs/` 为准。
- 仓库中的账号/角色/行会等 `data/*.txt`、`data/*.dat` 为演示数据，不是生产数据；测试账号密码以加盐 SHA-256 存储，不对应任何真实账号。
