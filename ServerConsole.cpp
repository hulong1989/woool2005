// ServerConsole.cpp  ——  控制台命令实现（原快照中该部分应在 Server.cpp，重建为独立文件）
#include "stdafx.h"
#include "Server.h"
#include "GameWorld.h"
#include "HumanPlayer.h"
#include "HumanPlayerMgr.h"
#include "ItemManager.h"
#include "ItemBox.h"
#include "Equipment.h"
#include "DownItemMgr.h"
#include "PlayerFactory.h"
#include "PlayerStore.h"
#include "Monster.h"
#include "ClassTable.h"
#include "Skill.h"
#include "LogicMapMgr.h"
#include "Npc.h"
#include "GameActions.h"
#include "Quest.h"
#include "Party.h"
#include "Guild.h"
#include "Boss.h"
#include "Dialogue.h"
#include "Event.h"
#include "Set.h"
#include "Dungeon.h"
#include "Achievement.h"
#include "Gm.h"
#include "Title.h"

#include <string>

static VOID server_Help( CBaseServer * pServer )
{
	pServer->Log( RGB( 0, 255, 255 ),
		"可用命令:\n"
		"  help                         显示本帮助\n"
		"  status                       服务器状态\n"
		"  addbot [名字] [职业]         创建（或读档恢复）测试角色，职业 0战/1法/2道\n"
		"  players                      在线测试角色数量\n"
		"  online                       列出全部在线玩家（含客户端）\n"
		"  gmadd <角色> <等级>          设置 GM（1基础 2进阶 3管理）\n"
		"  gmremove <角色>              移除 GM\n"
		"  gmlist                       查看 GM 名单\n"
		"  spawn <怪物> <地图> <x> <y> [数量]  刷怪\n"
		"  heal <角色>                  恢复 HP/MP\n"
		"  give <角色> <物品名>         按模板发一件物品到背包\n"
		"  bag <角色>                   列出背包\n"
		"  equip <角色> <位置> <编号>   穿上背包里的装备\n"
		"  unequip <角色> <位置> <编号> 脱下装备\n"
		"  drop <角色> <编号>           丢弃背包物品（随机落点）\n"
		"  droptile <角色> <编号>       把物品放在脚下，便于测试拾取\n"
		"  pickup <角色>                拾取脚下的物品\n"
		"  info <角色>                  查看职业/等级/经验/属性\n"
		"  addexp <角色> <经验>         增加经验并结算升级\n"
		"  cast <角色> <技能> [方向]    施放技能（方向缺省=当前朝向）\n"
		"  skills <角色>                查看该职业技能表（已学/可学/未达）\n"
		"  learn <角色> <技能>          [调试] 直接学习技能\n"
		"  testskill <角色> <技能> [怪] [调试] 强制施放技能\n"
		"  restore <角色>               回满 HP/MP\n"
		"  hurt <角色> <伤害>           [调试] 对角色造成伤害\n"
		"  use <角色> <编号>            使用背包里的药品\n"
		"  shop                         查看商店在售物品\n"
		"  buy <角色> <物品> [数量]     从商店购买\n"
		"  sell <角色> <编号>           把背包物品卖给商店\n"
		"  teleport <角色> <地图id> <x> <y>  传送（1=中州 2=落霞岛）\n"
		"  npcs                         列出所有 NPC\n"
		"  talk <角色> <NPC>            与 NPC 对话（需 3 格内）\n"
		"  npcshop <角色> <NPC>         查看商人货架\n"
		"  npcteleport <角色> <NPC>     使用传送员\n"
		"  quests <角色>                查看任务状态\n"
		"  accept <角色> <任务>         接取任务（需靠近 NPC）\n"
		"  submit <角色> <任务>         提交任务（杀怪/收集达标）\n"
		"  equipinfo <角色>             查看装备与强化等级\n"
		"  upgrade <角色> <位置> <编号> 强化装备（+9 上限）\n"
		"  repair <角色> <位置> <编号>  修理装备\n"
		"  split <角色> <编号> <数量>   拆分堆叠\n"
		"  sellall <角色> <物品名>      批量卖出同类物品\n"
		"  party <角色>                 查看队伍\n"
		"  partyinvite <角色> <目标>    邀请组队\n"
		"  partyleave <角色>            离开队伍\n"
		"  psay <角色> <内容>           队伍频道\n"
		"  guild create <角色> <名字>   创建行会（1000 金币）\n"
		"  guild join <角色> <行会>     加入行会\n"
		"  guild leave <角色>           退出行会\n"
		"  guild info <行会>            行会信息\n"
		"  guild list                   行会列表\n"
		"  gsay <角色> <内容>           行会频道\n"
		"  boss                         查看 Boss 刷新状态\n"
		"  mapmonsters <地图id>         查看地图怪物数量\n"
		"  dungeon                      查看副本占用状态\n"
		"  dungeonrank [副本]           查看副本排行榜\n"
		"  season status|settle         查看/结算赛季\n"
		"  firstkills                   查看首杀记录\n"
		"  bosskill <序号> <角色>       [调试] 直接击杀 Boss\n"
		"  choose <角色> <NPC> <序号>   选择对话选项（对话树）\n"
		"  guild donate <角色> <金币>   向行会捐献\n"
		"  guild kick <会长> <成员>     踢出成员\n"
		"  guild appoint <会长> <成员>  任命副会长\n"
		"  guild store <角色> <编号>    存入行会仓库\n"
		"  guild ware <角色>            查看行会仓库\n"
		"  guild take <角色> <序号>     从仓库取出\n"
		"  addgold <角色> <金币>        [调试] 增加金币\n"
		"  sets <角色>                  查看套装激活状态\n"
		"  achievements <角色>          查看成就\n"
		"  title <角色> <称号id>        佩戴称号（成就/兑换）\n"
		"  pointshop <角色>             成就点数商店\n"
		"  pointbuy <角色> <称号id>     用成就点数兑换称号\n"
		"  titles <角色>                查看已拥有称号\n"
		"  codex <角色>                 查看套装图鉴收集进度\n"
		"  enchant <角色> <位置> <编号> <元素>  附魔（1火 2冰 3雷 4毒）\n"
		"  refine <角色> <位置> <编号>  洗炼元素\n"
		"  drops <怪物>                 查看多掉落表\n"
		"  event doubleexp <秒>         开启双倍经验活动\n"
		"  event status                 活动状态\n"
		"  event list                   查看活动排期\n"
		"  event trigger <序号>         立即触发排期\n"
		"  announce <内容>              全服公告\n"
		"  spawn <角色> <怪物>          在角色东侧刷一只怪\n"
		"  attack <角色> <方向 0-7>     让角色攻击指定方向\n"
		"  monsters                     当前怪物数量\n"
		"  testcombat <角色> <怪物>     [调试] 刷怪并强制攻击一次（验证战斗/经验）\n"
		"  kill <角色>                  [调试] 直接杀死角色（测试死亡复活）\n"
		"  save                         立即保存全部角色存档\n"
		"  kick <id>                    断开指定客户端\n"
		"  kickname <角色>              按角色名踢下线（管理面板用）\n"
		"  clear                        清屏\n"
		"  exit / quit                  退出服务器\n"
		"  提示: 装备位置 0=武器 1=衣服 2=头盔 3=项链 4/5=戒指 6/7=护腕 8=腰带 9=鞋\n" );
}

static CHumanPlayer * server_FindPlayer( CBaseServer * pServer, const char * pszName )
{
	CHumanPlayer * pPlayer = CHumanPlayerMgr::GetInstance()->FindbyName( pszName );
	if( pPlayer == NULL )
		pServer->Log( RGB( 255, 0, 0 ), "找不到角色: %s\n", pszName ? pszName : "(空)" );
	return pPlayer;
}

static int server_ParseClass( const char * pszClass )
{
	if( pszClass == NULL || *pszClass == '\0' )return 0;
	if( strcmp( pszClass, "战士" ) == 0 )return 0;
	if( strcmp( pszClass, "法师" ) == 0 )return 1;
	if( strcmp( pszClass, "道士" ) == 0 )return 2;
	int n = atoi( pszClass );
	return ( n >= 0 && n <= 2 ) ? n : 0;
}

static VOID server_AddBot( CBaseServer * pServer, const char * pszName, const char * pszClass )
{
	const char * pszRealName = ( pszName && *pszName ) ? pszName : "测试玩家";

	CHumanPlayer * pPlayer = PlayerFactory_Create( pszRealName, NULL, TRUE, server_ParseClass( pszClass ) );
	if( pPlayer == NULL )
	{
		pServer->Log( RGB( 255, 0, 0 ), "创建角色失败（角色表已满 / 同名在线 / 地图未加载）\n" );
		return;
	}

	pServer->Log( RGB( 0, 255, 0 ), "测试角色 %s（%s，%d 级）已进入世界（当前 %d 个）\n",
		pPlayer->GetName(), CPlayerClassMgr::GetInstance()->GetClassName( pPlayer->GetClass() ),
		pPlayer->GetPropValue( PI_LEVEL ), CGameWorld::GetInstance()->GetTestPlayerCount() );
}

static VOID server_Give( CBaseServer * pServer, const char * pszPlayer, const char * pszItem )
{
	CHumanPlayer * pPlayer = server_FindPlayer( pServer, pszPlayer );
	if( pPlayer == NULL )return;

	ITEM item;
	if( !CItemManager::GetInstance()->MakeupTemplateItem( pszItem, item ) )
	{
		pServer->Log( RGB( 255, 0, 0 ), "物品模板不存在: %s（检查 data/baseitem.txt）\n", pszItem );
		return;
	}

	item.dwMakeIndex = CItemManager::NewMakeIndex();
	if( !pPlayer->AddBagItem( item ) )
	{
		pServer->Log( RGB( 255, 0, 0 ), "背包已满，无法放入 %s\n", item.baseitem.szName );
		return;
	}
	pServer->Log( RGB( 0, 255, 0 ), "%s 获得 %s（编号 %u，重量 %u）\n",
		pPlayer->GetName(), item.baseitem.szName, item.dwMakeIndex, item.baseitem.btWeight );
}

static VOID server_Bag( CBaseServer * pServer, const char * pszPlayer )
{
	CHumanPlayer * pPlayer = server_FindPlayer( pServer, pszPlayer );
	if( pPlayer == NULL )return;

	ITEM items[46];
	int nCount = pPlayer->GetBag().GetItems( items, 46 );
	pServer->Log( RGB( 0, 255, 255 ), "%s 的背包（%d 件，总重 %d）:\n",
		pPlayer->GetName(), nCount, pPlayer->GetBag().CalcWeight() );
	for( int i = 0; i < nCount; ++i )
	{
		pServer->Log( RGB( 160, 160, 160 ), "  [%u] %s%s x%u  stdmode=%u 耐久=%u/%u\n",
			items[i].dwMakeIndex, items[i].baseitem.szName,
			items[i].btUpgradeLevel > 0 ? " +强化" : "", items[i].wCount,
			items[i].baseitem.btStdMode, items[i].wCurDura, items[i].wMaxDura );
	}
}

static VOID server_Equip( CBaseServer * pServer, const char * pszPlayer, int nPos, DWORD dwMakeIndex )
{
	CHumanPlayer * pPlayer = server_FindPlayer( pServer, pszPlayer );
	if( pPlayer == NULL )return;

	if( pPlayer->EquipItem( nPos, dwMakeIndex ) )
		pServer->Log( RGB( 0, 255, 0 ), "%s 装备了 %u（位置 %d）\n", pPlayer->GetName(), dwMakeIndex, nPos );
	else
		pServer->Log( RGB( 255, 0, 0 ), "装备失败：%s\n", pPlayer->GetEquipments().GetErrorMsg() );
}

static VOID server_Unequip( CBaseServer * pServer, const char * pszPlayer, int nPos, DWORD dwMakeIndex )
{
	CHumanPlayer * pPlayer = server_FindPlayer( pServer, pszPlayer );
	if( pPlayer == NULL )return;

	if( pPlayer->UnEquipItem( nPos, dwMakeIndex ) )
		pServer->Log( RGB( 0, 255, 0 ), "%s 脱下 %u（位置 %d）\n", pPlayer->GetName(), dwMakeIndex, nPos );
	else
		pServer->Log( RGB( 255, 0, 0 ), "脱下失败（位置或编号不匹配）\n" );
}

static VOID server_Drop( CBaseServer * pServer, const char * pszPlayer, DWORD dwMakeIndex )
{
	CHumanPlayer * pPlayer = server_FindPlayer( pServer, pszPlayer );
	if( pPlayer == NULL )return;

	if( pPlayer->DropBagItem( dwMakeIndex ) )
		pServer->Log( RGB( 0, 255, 0 ), "%s 丢下了 %u，坐标 (%u,%u)\n",
			pPlayer->GetName(), dwMakeIndex, pPlayer->getX(), pPlayer->getY() );
	else
		pServer->Log( RGB( 255, 0, 0 ), "丢弃失败（背包里没有 %u，或地图未加载）\n", dwMakeIndex );
}

static VOID server_DropTile( CBaseServer * pServer, const char * pszPlayer, DWORD dwMakeIndex )
{
	CHumanPlayer * pPlayer = server_FindPlayer( pServer, pszPlayer );
	if( pPlayer == NULL )return;

	ITEM * pItem = pPlayer->GetBag().FindItem( dwMakeIndex );
	if( pItem == NULL )
	{
		pServer->Log( RGB( 255, 0, 0 ), "背包里没有编号 %u 的物品\n", dwMakeIndex );
		return;
	}
	if( !CDownItemMgr::GetInstance()->DropItem( pPlayer->GetMap(), *pItem,
			pPlayer->getX(), pPlayer->getY(), pPlayer->GetId() ) )
	{
		pServer->Log( RGB( 255, 0, 0 ), "掉落到脚下失败（地图未加载）\n" );
		return;
	}
	pPlayer->GetBag().RemoveItem( dwMakeIndex );
	pServer->Log( RGB( 0, 255, 0 ), "%s 把 %u 放在脚下 (%u,%u)\n",
		pPlayer->GetName(), dwMakeIndex, pPlayer->getX(), pPlayer->getY() );
}

static VOID server_Pickup( CBaseServer * pServer, const char * pszPlayer )
{
	CHumanPlayer * pPlayer = server_FindPlayer( pServer, pszPlayer );
	if( pPlayer == NULL )return;

	if( pPlayer->PickupItem() )
		pServer->Log( RGB( 0, 255, 0 ), "%s 拾取了脚下的物品\n", pPlayer->GetName() );
	else
		pServer->Log( RGB( 255, 255, 0 ), "脚下没有可拾取的物品\n" );
}

static VOID server_Info( CBaseServer * pServer, const char * pszPlayer )
{
	CHumanPlayer * pPlayer = server_FindPlayer( pServer, pszPlayer );
	if( pPlayer == NULL )return;

	if( pPlayer->GetTitle() > 0 )
		pServer->Log( RGB( 255, 215, 0 ), "  称号：【%s】\n", pPlayer->GetTitleName() );

	pServer->Log( RGB( 0, 255, 255 ),
		"%s（%s）  等级=%d  经验=%u/%u  HP=%d/%d  MP=%d/%d\n"
		"  攻击 %d-%d  道术 %d-%d  魔法 %d-%d  防御 %d-%d  魔防 %d-%d\n"
		"  负重 背包=%d/%d  腕力=%d/%d  身体=%d/%d  地图=%u(%u,%u)\n",
		pPlayer->GetName(),
		CPlayerClassMgr::GetInstance()->GetClassName( pPlayer->GetClass() ),
		pPlayer->GetPropValue( PI_LEVEL ),
		(unsigned)pPlayer->GetDBInfoExp(),
		(unsigned)CPlayerClassMgr::GetInstance()->LevelExpNeed( pPlayer->GetPropValue( PI_LEVEL ) ),
		pPlayer->GetPropValue( PI_CURHP ), pPlayer->GetPropValue( PI_MAXHP ),
		pPlayer->GetPropValue( PI_CURMP ), pPlayer->GetPropValue( PI_MAXMP ),
		pPlayer->GetPropValue( PI_MINDC ), pPlayer->GetPropValue( PI_MAXDC ),
		pPlayer->GetPropValue( PI_MINSC ), pPlayer->GetPropValue( PI_MAXSC ),
		pPlayer->GetPropValue( PI_MINMC ), pPlayer->GetPropValue( PI_MAXMC ),
		pPlayer->GetPropValue( PI_MINAC ), pPlayer->GetPropValue( PI_MAXAC ),
		pPlayer->GetPropValue( PI_MINMAC ), pPlayer->GetPropValue( PI_MAXMAC ),
		pPlayer->GetPropValue( PI_CURBAGWEIGHT ), pPlayer->GetPropValue( PI_MAXBAGWEIGHT ),
		pPlayer->GetPropValue( PI_CURHANDWEIGHT ), pPlayer->GetPropValue( PI_MAXHANDWEIGHT ),
		pPlayer->GetPropValue( PI_CURBODYWEIGHT ), pPlayer->GetPropValue( PI_MAXBODYWEIGHT ),
		pPlayer->GetMapId(), pPlayer->getX(), pPlayer->getY() );
}

static VOID server_Spawn( CBaseServer * pServer, const char * pszPlayer, const char * pszMonster )
{
	CHumanPlayer * pPlayer = server_FindPlayer( pServer, pszPlayer );
	if( pPlayer == NULL )return;

	CMonster * pMonster = CMonsterMgr::GetInstance()->Spawn(
		pszMonster, pPlayer->GetMapId(), (WORD)( pPlayer->getX() + 1 ), pPlayer->getY() );
	if( pMonster == NULL )
	{
		pServer->Log( RGB( 255, 0, 0 ), "刷怪失败：模板不存在或地图未加载（检查 data/monsters.txt）\n" );
		return;
	}
	pServer->Log( RGB( 0, 255, 0 ), "在 %s 东侧 (%u,%u) 刷出 %s（HP %d，等级 %d）\n",
		pPlayer->GetName(), pMonster->getX(), pMonster->getY(),
		pMonster->GetName(), pMonster->GetCurHp(), pMonster->GetPropValue( PI_LEVEL ) );
}

static VOID server_Attack( CBaseServer * pServer, const char * pszPlayer, int nDir )
{
	CHumanPlayer * pPlayer = server_FindPlayer( pServer, pszPlayer );
	if( pPlayer == NULL )return;

	BOOL bOk = pPlayer->Attack( nDir & 7 );
	pServer->Log( bOk ? RGB( 0, 255, 0 ) : RGB( 255, 255, 0 ),
		"%s 向方向 %d 攻击%s\n", pPlayer->GetName(), nDir & 7, bOk ? "" : "（动作未完成）" );
}

static VOID server_KillPlayer( CBaseServer * pServer, const char * pszPlayer )
{
	CHumanPlayer * pPlayer = server_FindPlayer( pServer, pszPlayer );
	if( pPlayer == NULL )return;

	pPlayer->SetCurHp( 0 );
	pPlayer->OnDeath( NULL );
	pServer->Log( RGB( 255, 160, 0 ), "[调试] %s 已被杀死并复活（HP %d）\n",
		pPlayer->GetName(), pPlayer->GetCurHp() );
}

static VOID server_Save( CBaseServer * pServer )
{
	int nSaved = CPlayerStore::GetInstance()->SaveAll();
	pServer->Log( RGB( 0, 255, 0 ), "已保存 %d 个角色存档到 data/players/\n", nSaved );
}

static VOID server_TestCombat( CBaseServer * pServer, const char * pszPlayer, const char * pszMonster )
{
	CHumanPlayer * pPlayer = server_FindPlayer( pServer, pszPlayer );
	if( pPlayer == NULL )return;

	CMonster * pMonster = CMonsterMgr::GetInstance()->Spawn(
		pszMonster, pPlayer->GetMapId(), (WORD)( pPlayer->getX() + 1 ), pPlayer->getY() );
	if( pMonster == NULL )
	{
		pServer->Log( RGB( 255, 0, 0 ), "刷怪失败：模板不存在或地图未加载\n" );
		return;
	}

	//	[调试] 反复强制普攻直到怪物死亡，保证低等级角色也能完成击杀
	int nHits = 0;
	for( int i = 0; i < 100 && !pMonster->IsDead(); ++i )
	{
		pPlayer->SetAction( AT_STAND, ED_RIGHT, pPlayer->getX(), pPlayer->getY(), 0 );
		if( !pPlayer->Attack( ED_RIGHT ) )break;
		++nHits;
	}

	pServer->Log( pMonster->IsDead() ? RGB( 0, 255, 0 ) : RGB( 255, 0, 0 ),
		"%s 攻击 %s：命中 %d 次，怪物 HP %d%s，角色经验 %u\n",
		pPlayer->GetName(), pMonster->GetName(), nHits, pMonster->GetCurHp(),
		pMonster->IsDead() ? "（已击杀）" : "",
		(unsigned)pPlayer->GetDBInfoExp() );
}

static VOID server_Monsters( CBaseServer * pServer )
{
	pServer->Log( RGB( 0, 255, 255 ), "当前怪物数量: %d\n", CMonsterMgr::GetInstance()->GetCount() );
}

static VOID server_AddExp( CBaseServer * pServer, const char * pszPlayer, int nExp )
{
	CHumanPlayer * pPlayer = server_FindPlayer( pServer, pszPlayer );
	if( pPlayer == NULL )return;

	int nOld = pPlayer->GetPropValue( PI_LEVEL );
	pPlayer->AddExp( nExp );
	pServer->Log( RGB( 0, 255, 0 ), "%s 获得 %d 经验：%d 级 -> %d 级，%u/%u\n",
		pPlayer->GetName(), nExp, nOld, pPlayer->GetPropValue( PI_LEVEL ),
		(unsigned)pPlayer->GetDBInfoExp(),
		(unsigned)CPlayerClassMgr::GetInstance()->LevelExpNeed( pPlayer->GetPropValue( PI_LEVEL ) ) );
}

static VOID server_Cast( CBaseServer * pServer, const char * pszPlayer, const char * pszSkill, int nDir )
{
	CHumanPlayer * pPlayer = server_FindPlayer( pServer, pszPlayer );
	if( pPlayer == NULL )return;

	if( nDir < 0 )nDir = (int)pPlayer->GetDirection();
	BOOL bOk = pPlayer->CastSkillByName( pszSkill, nDir );
	pServer->Log( bOk ? RGB( 0, 255, 0 ) : RGB( 255, 0, 0 ),
		"%s 施放 %s：%s（MP %d/%d）\n", pPlayer->GetName(), pszSkill,
		bOk ? "成功" : "失败（职业/等级/MP/冷却/动作）",
		pPlayer->GetPropValue( PI_CURMP ), pPlayer->GetPropValue( PI_MAXMP ) );
}

static VOID server_TestSkill( CBaseServer * pServer, const char * pszPlayer, const char * pszSkill, const char * pszMonster )
{
	CHumanPlayer * pPlayer = server_FindPlayer( pServer, pszPlayer );
	if( pPlayer == NULL )return;

	const SKILLTEMPLATE * pSkill = CSkillMgr::GetInstance()->FindByName( pszSkill );
	if( pSkill == NULL )
	{
		pServer->Log( RGB( 255, 0, 0 ), "技能不存在: %s\n", pszSkill );
		return;
	}

	CMonster * pMonster = NULL;
	if( pSkill->nType != 3 )
	{
		pMonster = CMonsterMgr::GetInstance()->Spawn(
			pszMonster != NULL ? pszMonster : "鸡",
			pPlayer->GetMapId(), (WORD)( pPlayer->getX() + 1 ), pPlayer->getY() );
		if( pMonster == NULL )
		{
			pServer->Log( RGB( 255, 0, 0 ), "刷怪失败：模板或地图不存在\n" );
			return;
		}
	}

	pPlayer->SetAction( AT_STAND, ED_RIGHT, pPlayer->getX(), pPlayer->getY(), 0 );
	BOOL bOk = pPlayer->CastSkill( pSkill->id, ED_RIGHT );

	pServer->Log( bOk ? RGB( 0, 255, 0 ) : RGB( 255, 0, 0 ),
		"%s 施放 %s：%s，MP %d/%d%s\n",
		pPlayer->GetName(), pSkill->szName, bOk ? "成功" : "失败（职业/等级/MP/冷却）",
		pPlayer->GetPropValue( PI_CURMP ), pPlayer->GetPropValue( PI_MAXMP ),
		pMonster != NULL
			? ( pMonster->IsDead() ? "，目标已击杀" : "，目标残血" )
			: "" );
}

static VOID server_Skills( CBaseServer * pServer, const char * pszPlayer )
{
	CHumanPlayer * pPlayer = server_FindPlayer( pServer, pszPlayer );
	if( pPlayer == NULL )return;

	pServer->Log( RGB( 0, 255, 255 ), "%s（%s，%d 级）可用技能：\n",
		pPlayer->GetName(), CPlayerClassMgr::GetInstance()->GetClassName( pPlayer->GetClass() ),
		pPlayer->GetPropValue( PI_LEVEL ) );

	for( int i = 0; i < CSkillMgr::GetInstance()->GetCount(); ++i )
	{
		const SKILLTEMPLATE * pSkill = CSkillMgr::GetInstance()->GetByIndex( i );
		if( pSkill == NULL || pSkill->nClass != pPlayer->GetClass() )continue;
		BOOL bLearned = pPlayer->HasSkill( pSkill->id );
		BOOL bLevelOk = pPlayer->GetPropValue( PI_LEVEL ) >= pSkill->nNeedLevel;
		const char * pszState = bLearned ? "已学" : ( bLevelOk ? "可学" : "未达" );
		pServer->Log( bLearned ? RGB( 0, 255, 0 ) : ( bLevelOk ? RGB( 255, 255, 0 ) : RGB( 120, 120, 120 ) ),
			"  [%s] %s  需要 %d 级  MP %d  威力 %d  射程 %d\n",
			pszState, pSkill->szName, pSkill->nNeedLevel,
			pSkill->nMp, pSkill->nPower, pSkill->nRange );
	}
}

static VOID server_Learn( CBaseServer * pServer, const char * pszPlayer, const char * pszSkill )
{
	CHumanPlayer * pPlayer = server_FindPlayer( pServer, pszPlayer );
	if( pPlayer == NULL )return;

	const SKILLTEMPLATE * pSkill = CSkillMgr::GetInstance()->FindByName( pszSkill );
	if( pSkill == NULL )
	{
		pServer->Log( RGB( 255, 0, 0 ), "技能不存在: %s\n", pszSkill );
		return;
	}

	if( pPlayer->LearnSkill( pSkill->id ) )
		pServer->Log( RGB( 0, 255, 0 ), "%s 学会了 %s\n", pPlayer->GetName(), pSkill->szName );
	else
		pServer->Log( RGB( 255, 0, 0 ), "学习失败（职业不符/等级不足/已学会）\n" );
}

static VOID server_Restore( CBaseServer * pServer, const char * pszPlayer )
{
	CHumanPlayer * pPlayer = server_FindPlayer( pServer, pszPlayer );
	if( pPlayer == NULL )return;
	pPlayer->RestoreAll();
	pServer->Log( RGB( 0, 255, 0 ), "%s 已恢复满血满蓝（HP %d，MP %d）\n",
		pPlayer->GetName(), pPlayer->GetPropValue( PI_CURHP ), pPlayer->GetPropValue( PI_CURMP ) );
}

//	读取商店价格（每次读文件，方便热改；未上架返回 -1）
static int server_GetShopPrice( const char * pszItem )
{
	CStringFile shop( "data/shop.txt" );
	for( int i = 0; i < shop.GetLineCount(); ++i )
	{
		const char * pszLine = shop[i];
		if( pszLine == NULL || *pszLine == '\0' || *pszLine == '#' )continue;

		char	szBuffer[256];
		o_strncpy( szBuffer, pszLine, sizeof( szBuffer ) - 1 );
		char *	Params[4];
		if( SearchParam( szBuffer, Params, 4, '/' ) < 2 )continue;
		if( strcmp( Params[0], pszItem ) == 0 )
			return atoi( Params[1] );
	}
	return -1;
}

static VOID server_Shop( CBaseServer * pServer )
{
	CStringFile shop( "data/shop.txt" );
	if( shop.GetLineCount() <= 0 )
	{
		pServer->Log( RGB( 255, 0, 0 ), "未找到 data/shop.txt\n" );
		return;
	}

	pServer->Log( RGB( 0, 255, 255 ), "商店在售：\n" );
	for( int i = 0; i < shop.GetLineCount(); ++i )
	{
		const char * pszLine = shop[i];
		if( pszLine == NULL || *pszLine == '\0' || *pszLine == '#' )continue;
		char	szBuffer[256];
		o_strncpy( szBuffer, pszLine, sizeof( szBuffer ) - 1 );
		char *	Params[4];
		if( SearchParam( szBuffer, Params, 4, '/' ) < 2 )continue;
		pServer->Log( RGB( 160, 160, 160 ), "  %s  %d 金币\n", Params[0], atoi( Params[1] ) );
	}
}

static VOID server_Buy( CBaseServer * pServer, const char * pszPlayer, const char * pszItem, int nCount )
{
	CHumanPlayer * pPlayer = server_FindPlayer( pServer, pszPlayer );
	if( pPlayer == NULL )return;

	if( nCount <= 0 )nCount = 1;
	if( nCount > 50 )nCount = 50;

	int nPrice = server_GetShopPrice( pszItem );
	if( nPrice < 0 )
	{
		pServer->Log( RGB( 255, 0, 0 ), "商店不出售 %s\n", pszItem );
		return;
	}

	ITEM templateItem;
	if( !CItemManager::GetInstance()->MakeupTemplateItem( pszItem, templateItem ) )
	{
		pServer->Log( RGB( 255, 0, 0 ), "物品模板不存在: %s\n", pszItem );
		return;
	}

	int nTotal = nPrice * nCount;
	if( !pPlayer->AddGold( -nTotal ) )
	{
		pServer->Log( RGB( 255, 0, 0 ), "金币不足：需要 %d，当前 %u\n", nTotal, (unsigned)pPlayer->GetGold() );
		return;
	}

	int nBought = 0;

	if( templateItem.baseitem.btUseType != 0 )
	{
		//	可堆叠物品一次放入，避免生成多余实例编号
		ITEM item = templateItem;
		item.dwMakeIndex = CItemManager::NewMakeIndex();
		item.wCount = (WORD)nCount;
		if( pPlayer->AddBagItem( item ) )
			nBought = nCount;
		else
			pPlayer->AddGold( nPrice * nCount );
	}
	else
	{
		for( int i = 0; i < nCount; ++i )
		{
			ITEM item = templateItem;
			item.dwMakeIndex = CItemManager::NewMakeIndex();
			if( pPlayer->AddBagItem( item ) )
				++nBought;
			else
				pPlayer->AddGold( nPrice );		//	放入失败则退款
		}
	}

	pServer->Log( nBought > 0 ? RGB( 0, 255, 0 ) : RGB( 255, 0, 0 ),
		"%s 购买 %s x%d（成功 %d），剩余 %u 金币\n",
		pPlayer->GetName(), pszItem, nCount, nBought, (unsigned)pPlayer->GetGold() );
}

static VOID server_Sell( CBaseServer * pServer, const char * pszPlayer, DWORD dwMakeIndex )
{
	CHumanPlayer * pPlayer = server_FindPlayer( pServer, pszPlayer );
	if( pPlayer == NULL )return;

	ITEM * pItem = pPlayer->GetBag().FindItem( dwMakeIndex );
	if( pItem == NULL )
	{
		pServer->Log( RGB( 255, 0, 0 ), "背包里没有编号 %u\n", dwMakeIndex );
		return;
	}

	int nPrice = pItem->baseitem.nPrice / 2;
	if( nPrice < 1 )nPrice = 1;
	char szName[32];
	o_strncpy( szName, pItem->baseitem.szName, sizeof( szName ) - 1 );

	pPlayer->GetBag().RemoveItem( dwMakeIndex );
	pPlayer->AddGold( nPrice );

	pServer->Log( RGB( 0, 255, 0 ), "%s 卖出 %s，获得 %d 金币，剩余 %u\n",
		pPlayer->GetName(), szName, nPrice, (unsigned)pPlayer->GetGold() );
}

static VOID server_Use( CBaseServer * pServer, const char * pszPlayer, DWORD dwMakeIndex )
{
	CHumanPlayer * pPlayer = server_FindPlayer( pServer, pszPlayer );
	if( pPlayer == NULL )return;

	if( pPlayer->UseItem( dwMakeIndex ) )
		pServer->Log( RGB( 0, 255, 0 ), "%s 使用了 %u：HP %d/%d  MP %d/%d\n",
			pPlayer->GetName(), dwMakeIndex,
			pPlayer->GetPropValue( PI_CURHP ), pPlayer->GetPropValue( PI_MAXHP ),
			pPlayer->GetPropValue( PI_CURMP ), pPlayer->GetPropValue( PI_MAXMP ) );
	else
		pServer->Log( RGB( 255, 0, 0 ), "无法使用 %u（不是药品或编号不存在）\n", dwMakeIndex );
}

static VOID server_Hurt( CBaseServer * pServer, const char * pszPlayer, int nDamage )
{
	CHumanPlayer * pPlayer = server_FindPlayer( pServer, pszPlayer );
	if( pPlayer == NULL )return;
	if( nDamage <= 0 )nDamage = 1;
	pPlayer->ApplyDamage( nDamage, NULL );
	pServer->Log( RGB( 255, 160, 0 ), "[调试] %s 受到 %d 伤害：HP %d/%d\n",
		pPlayer->GetName(), nDamage, pPlayer->GetPropValue( PI_CURHP ), pPlayer->GetPropValue( PI_MAXHP ) );
}

static VOID server_Teleport( CBaseServer * pServer, const char * pszPlayer, UINT nMapId, int x, int y )
{
	CHumanPlayer * pPlayer = server_FindPlayer( pServer, pszPlayer );
	if( pPlayer == NULL )return;

	CVLogicMap * pMap = CLogicMapMgr::GetInstance()->GetLogicMapById( nMapId );
	if( pMap == NULL )
	{
		pServer->Log( RGB( 255, 0, 0 ), "地图 %u 未加载\n", nMapId );
		return;
	}

	if( !Game_TeleportPlayer( pPlayer, nMapId, x, y ) )
	{
		pServer->Log( RGB( 255, 0, 0 ), "传送失败：坐标 (%d,%d) 不可通行或超出范围\n", x, y );
		return;
	}

	pServer->Log( RGB( 0, 255, 0 ), "%s 传送到 %s (%d,%d)\n",
		pPlayer->GetName(), pMap->GetTitle(), x, y );
}

//	玩家与 NPC 的距离（同地图、3 格内）
static BOOL server_IsNearNpc( CHumanPlayer * pPlayer, CNpc * pNpc )
{
	if( pPlayer == NULL || pNpc == NULL )return FALSE;
	if( pPlayer->GetMap() != pNpc->GetMap() )return FALSE;
	int dx = (int)pPlayer->getX() - (int)pNpc->getX();
	int dy = (int)pPlayer->getY() - (int)pNpc->getY();
	if( dx < 0 )dx = -dx;
	if( dy < 0 )dy = -dy;
	return dx <= 3 && dy <= 3;
}

static VOID server_Npcs( CBaseServer * pServer )
{
	int nCount = CNpcMgr::GetInstance()->GetCount();
	pServer->Log( RGB( 0, 255, 255 ), "当前 NPC: %d 个\n", nCount );
	for( int i = 0; i < nCount; ++i )
	{
		CNpc * pNpc = CNpcMgr::GetInstance()->GetByIndex( i );
		if( pNpc == NULL )continue;
		pServer->Log( RGB( 160, 160, 160 ), "  %s  %s (%u,%u)  %s\n",
			pNpc->GetName(), pNpc->GetMap()->GetTitle(), pNpc->getX(), pNpc->getY(),
			pNpc->GetNpcType() == 1 ? "商店" : ( pNpc->GetNpcType() == 2 ? "传送" : "其他" ) );
	}
}

static VOID server_Talk( CBaseServer * pServer, const char * pszPlayer, const char * pszNpc )
{
	CHumanPlayer * pPlayer = server_FindPlayer( pServer, pszPlayer );
	if( pPlayer == NULL )return;

	CNpc * pNpc = CNpcMgr::GetInstance()->Find( pszNpc );
	if( pNpc == NULL )
	{
		pServer->Log( RGB( 255, 0, 0 ), "找不到 NPC: %s\n", pszNpc );
		return;
	}
	if( !server_IsNearNpc( pPlayer, pNpc ) )
	{
		pServer->Log( RGB( 255, 0, 0 ), "%s 离 %s 太远了\n", pPlayer->GetName(), pNpc->GetName() );
		return;
	}

	pServer->Log( RGB( 0, 255, 255 ), "%s：%s\n", pNpc->GetName(), pNpc->GetDialog() );
	if( pPlayer->CanRecvMsg() )
		pPlayer->SendMsg( pPlayer->GetId(), SM_CHAT, 0xff00, 0, 0, (LPVOID)pNpc->GetDialog() );

	std::vector<DIALOGOPTION> options;
	int nOptions = CDialogueMgr::GetInstance()->GetOptions( pNpc->GetName(), options );
	for( int i = 0; i < nOptions; ++i )
	{
		pServer->Log( RGB( 160, 160, 160 ), "  %d. %s\n", options[i].nIndex, options[i].szText );
		if( pPlayer->CanRecvMsg() )
		{
			char szMsg[128];
			snprintf( szMsg, sizeof( szMsg ), "%d. %s", options[i].nIndex, options[i].szText );
			pPlayer->SendMsg( pPlayer->GetId(), SM_CHAT, 0xff00, 0, 0, (LPVOID)szMsg );
		}
	}
}

static VOID server_NpcShop( CBaseServer * pServer, const char * pszPlayer, const char * pszNpc )
{
	CHumanPlayer * pPlayer = server_FindPlayer( pServer, pszPlayer );
	if( pPlayer == NULL )return;

	CNpc * pNpc = CNpcMgr::GetInstance()->Find( pszNpc );
	if( pNpc == NULL )
	{
		pServer->Log( RGB( 255, 0, 0 ), "找不到 NPC: %s\n", pszNpc );
		return;
	}
	if( pNpc->GetNpcType() != 1 )
	{
		pServer->Log( RGB( 255, 0, 0 ), "%s 不是商人\n", pNpc->GetName() );
		return;
	}
	if( !server_IsNearNpc( pPlayer, pNpc ) )
	{
		pServer->Log( RGB( 255, 0, 0 ), "%s 离 %s 太远了\n", pPlayer->GetName(), pNpc->GetName() );
		return;
	}

	pServer->Log( RGB( 0, 255, 255 ), "%s 的货架：\n", pNpc->GetName() );
	server_Shop( pServer );
}

static VOID server_NpcTeleport( CBaseServer * pServer, const char * pszPlayer, const char * pszNpc )
{
	CHumanPlayer * pPlayer = server_FindPlayer( pServer, pszPlayer );
	if( pPlayer == NULL )return;

	CNpc * pNpc = CNpcMgr::GetInstance()->Find( pszNpc );
	if( pNpc == NULL )
	{
		pServer->Log( RGB( 255, 0, 0 ), "找不到 NPC: %s\n", pszNpc );
		return;
	}
	if( pNpc->GetNpcType() == 3 )
	{
		std::string msg = CDungeonMgr::GetInstance()->Enter( pPlayer, pNpc );
		pServer->Log( RGB( 0, 255, 0 ), "%s\n", msg.c_str() );
		return;
	}
	if( pNpc->GetNpcType() != 2 )
	{
		pServer->Log( RGB( 255, 0, 0 ), "%s 不是传送员\n", pNpc->GetName() );
		return;
	}
	if( !server_IsNearNpc( pPlayer, pNpc ) )
	{
		pServer->Log( RGB( 255, 0, 0 ), "%s 离 %s 太远了\n", pPlayer->GetName(), pNpc->GetName() );
		return;
	}

	if( !Game_TeleportPlayer( pPlayer, (UINT)pNpc->GetDestMap(), pNpc->GetDestX(), pNpc->GetDestY() ) )
	{
		pServer->Log( RGB( 255, 0, 0 ), "传送失败：目标地图或坐标不可用\n" );
		return;
	}

	pServer->Log( RGB( 0, 255, 0 ), "%s 通过 %s 传送到地图 %d (%d,%d)\n",
		pPlayer->GetName(), pNpc->GetName(), pNpc->GetDestMap(), pNpc->GetDestX(), pNpc->GetDestY() );
}

static const char * server_QuestStateName( int nState )
{
	if( nState == 2 )return "已完成";
	if( nState == 1 )return "进行中";
	return "可接";
}

static VOID server_Quests( CBaseServer * pServer, const char * pszPlayer )
{
	CHumanPlayer * pPlayer = server_FindPlayer( pServer, pszPlayer );
	if( pPlayer == NULL )return;

	CQuestMgr * pMgr = CQuestMgr::GetInstance();
	pServer->Log( RGB( 0, 255, 255 ), "%s 的任务（%d 个模板）：\n", pPlayer->GetName(), pMgr->GetCount() );
	for( int i = 0; i < pMgr->GetCount(); ++i )
	{
		const QUESTTEMPLATE * q = pMgr->GetByIndex( i );
		if( q == NULL )continue;
		int nState = pPlayer->GetQuestState( q->id );
		int nProgress = pPlayer->GetQuestProgress( q->id );
		const char * pszState = server_QuestStateName( nState );
		if( nState == 0 && q->nPrevQuest != 0 && pPlayer->GetQuestState( q->nPrevQuest ) != 2 )
			pszState = "未达前置";
		pServer->Log( nState == 1 ? RGB( 255, 255, 0 ) : ( nState == 2 ? RGB( 0, 255, 0 ) : RGB( 160, 160, 160 ) ),
			"  [%s] %s%s  目标 %s x%d  进度 %d/%d  奖励 %d 经验 / %d 金币\n",
			pszState, q->szName, q->bRepeatable ? "（日常）" : "", q->szTarget, q->nCount,
			nProgress, q->nCount, q->nExp, q->nGold );
	}
}

static VOID server_AcceptQuest( CBaseServer * pServer, const char * pszPlayer, const char * pszQuest )
{
	CHumanPlayer * pPlayer = server_FindPlayer( pServer, pszPlayer );
	if( pPlayer == NULL )return;

	const QUESTTEMPLATE * q = CQuestMgr::GetInstance()->FindByName( pszQuest );
	if( q == NULL )
	{
		pServer->Log( RGB( 255, 0, 0 ), "任务不存在: %s\n", pszQuest );
		return;
	}

	CNpc * pNpc = CNpcMgr::GetInstance()->Find( q->szNpc );
	if( pNpc == NULL || !server_IsNearNpc( pPlayer, pNpc ) )
	{
		pServer->Log( RGB( 255, 0, 0 ), "需要靠近发布任务的 NPC：%s\n", q->szNpc );
		return;
	}

	if( pPlayer->AcceptQuest( q->id ) )
		pServer->Log( RGB( 0, 255, 0 ), "%s 接受任务 [%s]：%s（目标 %s x%d）\n",
			pPlayer->GetName(), q->szName, q->szDialog, q->szTarget, q->nCount );
	else
		pServer->Log( RGB( 255, 0, 0 ), "接受失败（等级不足/已接/已完成/任务栏已满）\n" );
}

static VOID server_SubmitQuest( CBaseServer * pServer, const char * pszPlayer, const char * pszQuest )
{
	CHumanPlayer * pPlayer = server_FindPlayer( pServer, pszPlayer );
	if( pPlayer == NULL )return;

	const QUESTTEMPLATE * q = CQuestMgr::GetInstance()->FindByName( pszQuest );
	if( q == NULL )
	{
		pServer->Log( RGB( 255, 0, 0 ), "任务不存在: %s\n", pszQuest );
		return;
	}

	CNpc * pNpc = CNpcMgr::GetInstance()->Find( q->szNpc );
	if( pNpc == NULL || !server_IsNearNpc( pPlayer, pNpc ) )
	{
		pServer->Log( RGB( 255, 0, 0 ), "需要靠近发布任务的 NPC：%s\n", q->szNpc );
		return;
	}

	if( pPlayer->SubmitQuest( q->id ) )
		pServer->Log( RGB( 0, 255, 0 ), "%s 提交任务 [%s]：经验 +%d，金币 +%d，奖励 %s x%d\n",
			pPlayer->GetName(), q->szName, q->nExp, q->nGold, q->szRewardItem, q->nRewardCount );
	else
		pServer->Log( RGB( 255, 0, 0 ), "提交失败（未接取/条件未达成/背包里材料不足）\n" );
}

//	装备强化/修理/查看
static VOID server_EquipList( CBaseServer * pServer, const char * pszPlayer )
{
	CHumanPlayer * pPlayer = server_FindPlayer( pServer, pszPlayer );
	if( pPlayer == NULL )return;

	pServer->Log( RGB( 0, 255, 255 ), "%s 的装备：\n", pPlayer->GetName() );
	for( int i = 0; i < _U_MAX; ++i )
	{
		ITEM * pItem = pPlayer->GetEquipments().GetEquipment( i );
		if( pItem == NULL )continue;
		char szElem[32] = "";
		if( pItem->btElement > 0 && pItem->btElementPower > 0 )
			snprintf( szElem, sizeof( szElem ), " [元素%d+%d]", pItem->btElement, pItem->btElementPower );
		pServer->Log( RGB( 160, 160, 160 ), "  [%d] [%u] %s +%d%s  耐久 %u/%u\n",
			i, pItem->dwMakeIndex, pItem->baseitem.szName, pItem->btUpgradeLevel, szElem,
			pItem->wCurDura, pItem->wMaxDura );
	}
}

static VOID server_Upgrade( CBaseServer * pServer, const char * pszPlayer, int nPos, DWORD dwMakeIndex )
{
	CHumanPlayer * pPlayer = server_FindPlayer( pServer, pszPlayer );
	if( pPlayer == NULL )return;

	ITEM * pItem = pPlayer->GetEquipments().GetEquipment( nPos );
	if( pItem == NULL || pItem->dwMakeIndex != dwMakeIndex )
	{
		pServer->Log( RGB( 255, 0, 0 ), "该装备位没有编号 %u\n", dwMakeIndex );
		return;
	}
	if( pItem->btUpgradeLevel >= 9 )
	{
		pServer->Log( RGB( 255, 0, 0 ), "已经 +9，无法继续强化\n" );
		return;
	}

	int nCost = 300 * ( pItem->btUpgradeLevel + 1 );
	if( !pPlayer->AddGold( -nCost ) )
	{
		pServer->Log( RGB( 255, 0, 0 ), "金币不足：强化需要 %d，当前 %u\n", nCost, (unsigned)pPlayer->GetGold() );
		return;
	}

	//	武器加攻击、衣服加防御，其余加攻击
	BYTE stdMode = pItem->baseitem.btStdMode;
	if( stdMode == 5 || stdMode == 6 )
	{
		++ pItem->baseitem.btMinAtk;
		++ pItem->baseitem.btMaxAtk;
	}
	else if( stdMode == 10 || stdMode == 11 )
	{
		++ pItem->baseitem.btMinDef;
		++ pItem->baseitem.btMaxDef;
	}
	else
	{
		++ pItem->baseitem.btMinAtk;
		++ pItem->baseitem.btMaxAtk;
	}
	++ pItem->btUpgradeLevel;

	pPlayer->GetEquipments().ResetPropCache();
	pPlayer->UpdateProp();

	pServer->Log( RGB( 0, 255, 0 ), "%s 的 %s 强化到 +%d，花费 %d 金币\n",
		pPlayer->GetName(), pItem->baseitem.szName, pItem->btUpgradeLevel, nCost );
	CAchievementMgr::GetInstance()->OnUpgrade( pPlayer, pItem->btUpgradeLevel );
}

static VOID server_Repair( CBaseServer * pServer, const char * pszPlayer, int nPos, DWORD dwMakeIndex )
{
	CHumanPlayer * pPlayer = server_FindPlayer( pServer, pszPlayer );
	if( pPlayer == NULL )return;

	ITEM * pItem = pPlayer->GetEquipments().GetEquipment( nPos );
	if( pItem == NULL || pItem->dwMakeIndex != dwMakeIndex )
	{
		pServer->Log( RGB( 255, 0, 0 ), "该装备位没有编号 %u\n", dwMakeIndex );
		return;
	}
	if( pItem->wCurDura >= pItem->wMaxDura )
	{
		pServer->Log( RGB( 255, 255, 0 ), "%s 耐久已满\n", pItem->baseitem.szName );
		return;
	}

	int nCost = (int)( pItem->wMaxDura - pItem->wCurDura ) * 2;
	if( !pPlayer->AddGold( -nCost ) )
	{
		pServer->Log( RGB( 255, 0, 0 ), "金币不足：修理需要 %d\n", nCost );
		return;
	}
	pItem->wCurDura = pItem->wMaxDura;
	pServer->Log( RGB( 0, 255, 0 ), "%s 修理 %s，花费 %d 金币\n",
		pPlayer->GetName(), pItem->baseitem.szName, nCost );
}

static VOID server_Split( CBaseServer * pServer, const char * pszPlayer, DWORD dwMakeIndex, int nCount )
{
	CHumanPlayer * pPlayer = server_FindPlayer( pServer, pszPlayer );
	if( pPlayer == NULL )return;

	if( pPlayer->GetBag().SplitItem( dwMakeIndex, nCount ) )
		pServer->Log( RGB( 0, 255, 0 ), "%s 从 %u 中拆出 %d 个\n", pPlayer->GetName(), dwMakeIndex, nCount );
	else
		pServer->Log( RGB( 255, 0, 0 ), "拆分失败（编号不存在/数量不合法/背包满）\n" );
}

static VOID server_SellAll( CBaseServer * pServer, const char * pszPlayer, const char * pszItem )
{
	CHumanPlayer * pPlayer = server_FindPlayer( pServer, pszPlayer );
	if( pPlayer == NULL )return;

	ITEMCLASS * pClass = CItemManager::GetInstance()->GetItemClassByName( pszItem );
	if( pClass == NULL )
	{
		pServer->Log( RGB( 255, 0, 0 ), "物品模板不存在: %s\n", pszItem );
		return;
	}
	int nCount = pPlayer->GetBag().CountItemByName( pszItem );
	if( nCount <= 0 )
	{
		pServer->Log( RGB( 255, 0, 0 ), "背包里没有 %s\n", pszItem );
		return;
	}

	int nPrice = pClass->nPrice / 2;
	if( nPrice < 1 )nPrice = 1;
	pPlayer->GetBag().RemoveAllByName( pszItem );
	pPlayer->AddGold( nPrice * nCount );

	pServer->Log( RGB( 0, 255, 0 ), "%s 批量卖出 %s x%d，获得 %d 金币\n",
		pPlayer->GetName(), pszItem, nCount, nPrice * nCount );
}

static VOID server_PartyInfo( CBaseServer * pServer, const char * pszPlayer )
{
	CHumanPlayer * pPlayer = server_FindPlayer( pServer, pszPlayer );
	if( pPlayer == NULL )return;

	std::vector<std::string> members;
	int nCount = CPartyMgr::GetInstance()->GetMembers( pPlayer->GetName(), members );
	if( nCount == 0 )
	{
		pServer->Log( RGB( 255, 255, 0 ), "%s 还没有队伍\n", pPlayer->GetName() );
		return;
	}

	pServer->Log( RGB( 0, 255, 255 ), "%s 的队伍（%d 人）：\n", pPlayer->GetName(), nCount );
	for( int i = 0; i < nCount; ++i )
	{
		CHumanPlayer * pMember = CHumanPlayerMgr::GetInstance()->FindbyName( members[i].c_str() );
		pServer->Log( RGB( 160, 160, 160 ), "  %s  %s\n", members[i].c_str(),
			pMember != NULL ? "在线" : "离线" );
	}
}

static VOID server_PartyInvite( CBaseServer * pServer, const char * pszInviter, const char * pszTarget )
{
	if( CPartyMgr::GetInstance()->Invite( pszInviter, pszTarget ) )
		pServer->Log( RGB( 0, 255, 0 ), "%s 邀请 %s 加入队伍\n", pszInviter, pszTarget );
	else
		pServer->Log( RGB( 255, 0, 0 ), "组队失败（角色不存在/对方已有队伍/队伍已满）\n" );
}

static VOID server_PartyLeave( CBaseServer * pServer, const char * pszPlayer )
{
	if( CPartyMgr::GetInstance()->Leave( pszPlayer ) )
		pServer->Log( RGB( 0, 255, 0 ), "%s 离开了队伍\n", pszPlayer );
	else
		pServer->Log( RGB( 255, 255, 0 ), "%s 没有队伍\n", pszPlayer );
}

static VOID server_PSay( CBaseServer * pServer, const char * pszPlayer, const char * pszText )
{
	CHumanPlayer * pPlayer = server_FindPlayer( pServer, pszPlayer );
	if( pPlayer == NULL )return;
	if( !CPartyMgr::GetInstance()->IsInParty( pszPlayer ) )
	{
		pServer->Log( RGB( 255, 0, 0 ), "没有队伍，无法发送队伍消息\n" );
		return;
	}
	CPartyMgr::GetInstance()->Broadcast( pPlayer, pszText );
	pServer->Log( RGB( 0, 255, 255 ), "【队伍】%s: %s\n", pszPlayer, pszText );
}

static VOID server_GuildCreate( CBaseServer * pServer, const char * pszPlayer, const char * pszGuild )
{
	CHumanPlayer * pPlayer = server_FindPlayer( pServer, pszPlayer );
	if( pPlayer == NULL )return;

	if( CGuildMgr::GetInstance()->FindByMember( pszPlayer ) != NULL )
	{
		pServer->Log( RGB( 255, 0, 0 ), "你已经在一个行会里了\n" );
		return;
	}
	if( CGuildMgr::GetInstance()->Find( pszGuild ) != NULL )
	{
		pServer->Log( RGB( 255, 0, 0 ), "行会名已存在\n" );
		return;
	}
	if( !pPlayer->AddGold( -1000 ) )
	{
		pServer->Log( RGB( 255, 0, 0 ), "建立行会需要 1000 金币\n" );
		return;
	}
	if( !CGuildMgr::GetInstance()->Create( pszGuild, pszPlayer ) )
	{
		pPlayer->AddGold( 1000 );
		pServer->Log( RGB( 255, 0, 0 ), "创建行会失败\n" );
		return;
	}
	CGuildMgr::GetInstance()->Save( "data/guilds.dat" );
	pServer->Log( RGB( 0, 255, 0 ), "%s 创建了行会 [%s]\n", pszPlayer, pszGuild );
}

static VOID server_GuildJoin( CBaseServer * pServer, const char * pszPlayer, const char * pszGuild )
{
	CHumanPlayer * pPlayer = server_FindPlayer( pServer, pszPlayer );
	if( pPlayer == NULL )return;

	if( CGuildMgr::GetInstance()->Join( pszGuild, pszPlayer ) )
	{
		CGuildMgr::GetInstance()->Save( "data/guilds.dat" );
		pServer->Log( RGB( 0, 255, 0 ), "%s 加入了行会 [%s]\n", pszPlayer, pszGuild );
	}
	else
	{
		pServer->Log( RGB( 255, 0, 0 ), "加入失败（行会不存在/已在他会/人数已满）\n" );
	}
}

static VOID server_GuildLeave( CBaseServer * pServer, const char * pszPlayer )
{
	if( CGuildMgr::GetInstance()->Leave( pszPlayer ) )
	{
		CGuildMgr::GetInstance()->Save( "data/guilds.dat" );
		pServer->Log( RGB( 0, 255, 0 ), "%s 退出了行会\n", pszPlayer );
	}
	else
	{
		pServer->Log( RGB( 255, 0, 0 ), "%s 没有行会\n", pszPlayer );
	}
}

static VOID server_GuildInfo( CBaseServer * pServer, const char * pszGuild )
{
	CGuildMgr::GUILD * pGuild = CGuildMgr::GetInstance()->Find( pszGuild );
	if( pGuild == NULL )return;

	pServer->Log( RGB( 0, 255, 255 ), "行会 [%s]  等级 %d  资金 %lld  会长 %s%s%s  成员 %d/%d  经验加成 +%d%%  仓库 %d/50\n",
		pGuild->name.c_str(), pGuild->level, pGuild->funds, pGuild->leader.c_str(),
		pGuild->vice.empty() ? "" : "  副会长 ", pGuild->vice.c_str(),
		(int)pGuild->members.size(), CGuildMgr::MemberCap( pGuild->level ),
		CGuildMgr::ExpBonusPercent( pGuild->level ), (int)pGuild->warehouse.size() );
	for( size_t i = 0; i < pGuild->members.size(); ++i )
		pServer->Log( RGB( 160, 160, 160 ), "  %s\n", pGuild->members[i].c_str() );
}

static VOID server_GuildList( CBaseServer * pServer )
{
	int nCount = CGuildMgr::GetInstance()->GetCount();
	pServer->Log( RGB( 0, 255, 255 ), "当前行会: %d 个\n", nCount );
	for( int i = 0; i < nCount; ++i )
	{
		CGuildMgr::GUILD * pGuild = CGuildMgr::GetInstance()->GetByIndex( i );
		if( pGuild == NULL )continue;
		pServer->Log( RGB( 160, 160, 160 ), "  [%s] 会长 %s  %d 人\n",
			pGuild->name.c_str(), pGuild->leader.c_str(), (int)pGuild->members.size() );
	}
}

static VOID server_GSay( CBaseServer * pServer, const char * pszPlayer, const char * pszText )
{
	CHumanPlayer * pPlayer = server_FindPlayer( pServer, pszPlayer );
	if( pPlayer == NULL )return;

	CGuildMgr::GUILD * pGuild = CGuildMgr::GetInstance()->FindByMember( pszPlayer );
	if( pGuild == NULL )
	{
		pServer->Log( RGB( 255, 0, 0 ), "没有行会，无法发送行会消息\n" );
		return;
	}
	CGuildMgr::GetInstance()->Broadcast( pGuild->name.c_str(), pszPlayer, pszText );
	pServer->Log( RGB( 0, 255, 255 ), "【行会】%s: %s\n", pszPlayer, pszText );
}

static VOID server_Bosses( CBaseServer * pServer )
{
	int nCount = CBossMgr::GetInstance()->GetCount();
	pServer->Log( RGB( 0, 255, 255 ), "Boss 刷新点: %d 个\n", nCount );
	DWORD dwNow = timeGetTime();
	for( int i = 0; i < nCount; ++i )
	{
		BOSSENTRY * pBoss = CBossMgr::GetInstance()->GetByIndex( i );
		if( pBoss == NULL )continue;
		if( pBoss->bAlive )
			pServer->Log( RGB( 0, 255, 0 ), "  %s  地图 %u (%u,%u)  存活\n",
				pBoss->szMonster, pBoss->nMapId, pBoss->x, pBoss->y );
		else if( pBoss->dwInterval == 0 )
			pServer->Log( RGB( 160, 160, 160 ), "  %s  地图 %u (%u,%u)  已击杀（世界Boss，不重刷）\n",
				pBoss->szMonster, pBoss->nMapId, pBoss->x, pBoss->y );
		else
			pServer->Log( RGB( 255, 255, 0 ), "  %s  地图 %u (%u,%u)  刷新倒计时 %u 秒\n",
				pBoss->szMonster, pBoss->nMapId, pBoss->x, pBoss->y,
				( pBoss->dwNextTime > dwNow ? ( pBoss->dwNextTime - dwNow ) / 1000 : 0 ) );
	}
}

//	选择对话选项（对话树）
static VOID server_Choose( CBaseServer * pServer, const char * pszPlayer, const char * pszNpc, int nIndex )
{
	CHumanPlayer * pPlayer = server_FindPlayer( pServer, pszPlayer );
	if( pPlayer == NULL )return;

	CNpc * pNpc = CNpcMgr::GetInstance()->Find( pszNpc );
	if( pNpc == NULL )
	{
		pServer->Log( RGB( 255, 0, 0 ), "找不到 NPC: %s\n", pszNpc );
		return;
	}
	if( !server_IsNearNpc( pPlayer, pNpc ) )
	{
		pServer->Log( RGB( 255, 0, 0 ), "%s 离 %s 太远了\n", pPlayer->GetName(), pNpc->GetName() );
		return;
	}

	const DIALOGOPTION * pOption = CDialogueMgr::GetInstance()->GetOption( pNpc->GetName(), nIndex );
	if( pOption == NULL )
	{
		pServer->Log( RGB( 255, 0, 0 ), "没有这个选项\n" );
		return;
	}

	if( strcmp( pOption->szAction, "shop" ) == 0 )
	{
		server_Shop( pServer );
	}
	else if( strcmp( pOption->szAction, "quests" ) == 0 )
	{
		server_Quests( pServer, pszPlayer );
	}
	else if( strcmp( pOption->szAction, "teleport" ) == 0 )
	{
		UINT nMapId = 0;
		int x = 0, y = 0;
		if( sscanf( pOption->szParam, "%u,%d,%d", &nMapId, &x, &y ) == 3 &&
			Game_TeleportPlayer( pPlayer, nMapId, x, y ) )
		{
			pServer->Log( RGB( 0, 255, 0 ), "%s 通过对话传送到地图 %u (%d,%d)\n",
				pPlayer->GetName(), nMapId, x, y );
		}
		else
		{
			pServer->Log( RGB( 255, 0, 0 ), "传送失败\n" );
		}
	}
	else if( strcmp( pOption->szAction, "close" ) == 0 )
	{
		pServer->Log( RGB( 160, 160, 160 ), "%s：再见。\n", pNpc->GetName() );
	}
	else
	{
		pServer->Log( RGB( 255, 255, 0 ), "未知对话动作: %s\n", pOption->szAction );
	}
}

static VOID server_GuildDonate( CBaseServer * pServer, const char * pszPlayer, int nGold )
{
	CHumanPlayer * pPlayer = server_FindPlayer( pServer, pszPlayer );
	if( pPlayer == NULL )return;

	if( CGuildMgr::GetInstance()->FindByMember( pszPlayer ) == NULL )
	{
		pServer->Log( RGB( 255, 0, 0 ), "没有行会\n" );
		return;
	}
	if( nGold <= 0 || !pPlayer->AddGold( -nGold ) )
	{
		pServer->Log( RGB( 255, 0, 0 ), "金币不足\n" );
		return;
	}

	int nNewLevel = 0;
	CGuildMgr::GetInstance()->Donate( pszPlayer, nGold, nNewLevel );
	CGuildMgr::GetInstance()->Save( "data/guilds.dat" );

	CGuildMgr::GUILD * pGuild = CGuildMgr::GetInstance()->FindByMember( pszPlayer );
	pServer->Log( RGB( 0, 255, 0 ), "%s 为行会捐献 %d 金币（资金 %lld）%s\n",
		pszPlayer, nGold, pGuild != NULL ? pGuild->funds : 0LL,
		nNewLevel > 0 ? "，行会升级！" : "" );
	if( nNewLevel > 0 )
		pServer->Log( RGB( 255, 255, 0 ), "行会 [%s] 升到 %d 级\n", pGuild->name.c_str(), nNewLevel );
}

static VOID server_GuildKick( CBaseServer * pServer, const char * pszLeader, const char * pszMember )
{
	if( CGuildMgr::GetInstance()->Kick( pszLeader, pszMember ) )
	{
		CGuildMgr::GetInstance()->Save( "data/guilds.dat" );
		pServer->Log( RGB( 0, 255, 0 ), "%s 把 %s 踢出了行会\n", pszLeader, pszMember );
	}
	else
	{
		pServer->Log( RGB( 255, 0, 0 ), "踢人失败（只有会长可以踢人/成员不存在）\n" );
	}
}

static VOID server_GuildAppoint( CBaseServer * pServer, const char * pszLeader, const char * pszMember )
{
	if( CGuildMgr::GetInstance()->Appoint( pszLeader, pszMember ) )
	{
		CGuildMgr::GetInstance()->Save( "data/guilds.dat" );
		pServer->Log( RGB( 0, 255, 0 ), "%s 任命 %s 为副会长\n", pszLeader, pszMember );
	}
	else
	{
		pServer->Log( RGB( 255, 0, 0 ), "任命失败（只有会长可以任命/成员不存在）\n" );
	}
}

static VOID server_AddGold( CBaseServer * pServer, const char * pszPlayer, int nGold )
{
	CHumanPlayer * pPlayer = server_FindPlayer( pServer, pszPlayer );
	if( pPlayer == NULL )return;
	pPlayer->AddGold( nGold );
	pServer->Log( RGB( 255, 160, 0 ), "[调试] %s 金币 %+d，当前 %u\n",
		pPlayer->GetName(), nGold, (unsigned)pPlayer->GetGold() );
}

static VOID server_GuildStore( CBaseServer * pServer, const char * pszPlayer, DWORD dwMakeIndex )
{
	CHumanPlayer * pPlayer = server_FindPlayer( pServer, pszPlayer );
	if( pPlayer == NULL )return;

	CGuildMgr::GUILD * pGuild = CGuildMgr::GetInstance()->FindByMember( pszPlayer );
	if( pGuild == NULL )
	{
		pServer->Log( RGB( 255, 0, 0 ), "没有行会\n" );
		return;
	}
	if( pGuild->warehouse.size() >= 50 )
	{
		pServer->Log( RGB( 255, 0, 0 ), "行会仓库已满（50 格）\n" );
		return;
	}

	ITEM item;
	if( !pPlayer->GetBag().RemoveItemStack( dwMakeIndex, item ) )
	{
		pServer->Log( RGB( 255, 0, 0 ), "背包里没有编号 %u\n", dwMakeIndex );
		return;
	}
	if( !CGuildMgr::GetInstance()->AddToWarehouse( pszPlayer, item ) )
	{
		pPlayer->GetBag().AddItem( item );		//	放不进去就退回
		pServer->Log( RGB( 255, 0, 0 ), "存入行会仓库失败\n" );
		return;
	}
	CGuildMgr::GetInstance()->SaveWarehouse( "data/guildware.dat" );
	pServer->Log( RGB( 0, 255, 0 ), "%s 把 %s 存入行会仓库（%d/50）\n",
		pszPlayer, item.baseitem.szName, (int)pGuild->warehouse.size() );
}

static VOID server_GuildWare( CBaseServer * pServer, const char * pszPlayer )
{
	CGuildMgr::GUILD * pGuild = CGuildMgr::GetInstance()->FindByMember( pszPlayer );
	if( pGuild == NULL )
	{
		pServer->Log( RGB( 255, 0, 0 ), "没有行会\n" );
		return;
	}

	pServer->Log( RGB( 0, 255, 255 ), "行会 [%s] 仓库（%d 件）：\n",
		pGuild->name.c_str(), (int)pGuild->warehouse.size() );
	for( size_t i = 0; i < pGuild->warehouse.size(); ++i )
	{
		ITEM & item = pGuild->warehouse[i];
		pServer->Log( RGB( 160, 160, 160 ), "  [%d] [%u] %s%s x%u  耐久 %u/%u\n",
			(int)i, item.dwMakeIndex, item.baseitem.szName,
			item.btUpgradeLevel > 0 ? " +强化" : "", item.wCount,
			item.wCurDura, item.wMaxDura );
	}
}

static VOID server_GuildTake( CBaseServer * pServer, const char * pszPlayer, int nIndex )
{
	CHumanPlayer * pPlayer = server_FindPlayer( pServer, pszPlayer );
	if( pPlayer == NULL )return;

	CGuildMgr::GUILD * pGuild = CGuildMgr::GetInstance()->FindByMember( pszPlayer );
	if( pGuild == NULL )
	{
		pServer->Log( RGB( 255, 0, 0 ), "没有行会\n" );
		return;
	}
	//	重建时补充：只有会长/副会长可以从仓库取出
	if( pGuild->leader != pszPlayer && pGuild->vice != pszPlayer )
	{
		pServer->Log( RGB( 255, 0, 0 ), "只有会长/副会长可以从行会仓库取出物品\n" );
		return;
	}

	ITEM item;
	if( !CGuildMgr::GetInstance()->TakeFromWarehouse( pszPlayer, nIndex, item ) )
	{
		pServer->Log( RGB( 255, 0, 0 ), "仓库没有第 %d 格\n", nIndex );
		return;
	}
	if( !pPlayer->AddBagItem( item ) )
	{
		CGuildMgr::GetInstance()->AddToWarehouse( pszPlayer, item );	//	背包满则退回
		pServer->Log( RGB( 255, 0, 0 ), "背包已满\n" );
		return;
	}
	CGuildMgr::GetInstance()->SaveWarehouse( "data/guildware.dat" );
	pServer->Log( RGB( 0, 255, 0 ), "%s 从行会仓库取出 %s\n", pszPlayer, item.baseitem.szName );
}

static VOID server_Drops( CBaseServer * pServer, const char * pszMonster )
{
	int nCount = CMonsterMgr::GetInstance()->GetDropCount( pszMonster );
	if( nCount <= 0 )
	{
		pServer->Log( RGB( 255, 255, 0 ), "%s 没有多掉落表（使用模板单掉落）\n", pszMonster );
		return;
	}
	pServer->Log( RGB( 0, 255, 255 ), "%s 的掉落表（%d 条）：\n", pszMonster, nCount );
	for( int i = 0; i < nCount; ++i )
	{
		const MONSTERDROP * pDrop = CMonsterMgr::GetInstance()->GetDropByIndex( pszMonster, i );
		if( pDrop == NULL )continue;
		pServer->Log( RGB( 160, 160, 160 ), "  %s  %d%%\n", pDrop->szItem, pDrop->nRate );
	}
}

static VOID server_Sets( CBaseServer * pServer, const char * pszPlayer )
{
	CHumanPlayer * pPlayer = server_FindPlayer( pServer, pszPlayer );
	if( pPlayer == NULL )return;

	pServer->Log( RGB( 0, 255, 255 ), "%s 的套装状态：\n", pPlayer->GetName() );
	for( int s = 0; s < CSetMgr::GetInstance()->GetCount(); ++s )
	{
		SETDEF * pDef = CSetMgr::GetInstance()->GetByIndex( s );
		if( pDef == NULL )continue;

		int nEquipped = 0;
		for( size_t j = 0; j < pDef->items.size(); ++j )
			for( int i = 0; i < _U_MAX; ++i )
			{
				ITEM * pItem = pPlayer->GetEquipments().GetEquipment( i );
				if( pItem != NULL && strcmp( pItem->baseitem.szName, pDef->items[j].c_str() ) == 0 )
				{
					++nEquipped;
					break;
				}
			}

		BOOL bActive = nEquipped >= pDef->nNeed;
		pServer->Log( bActive ? RGB( 0, 255, 0 ) : RGB( 160, 160, 160 ),
			"  [%s] %s  已穿 %d 件 / 需要 %d 件\n",
			bActive ? "已激活" : "未激活", pDef->szName, nEquipped, pDef->nNeed );
	}
}

static VOID server_FirstKills( CBaseServer * pServer )
{
	int nCount = CBossMgr::GetInstance()->GetFirstKillCount();
	pServer->Log( RGB( 0, 255, 255 ), "首杀记录: %d 条\n", nCount );
	if( nCount == 0 )return;

	std::vector<std::string> shown;
	for( int i = 0; i < CBossMgr::GetInstance()->GetCount(); ++i )
	{
		BOSSENTRY * pBoss = CBossMgr::GetInstance()->GetByIndex( i );
		if( pBoss == NULL )continue;
		if( !CBossMgr::GetInstance()->IsFirstKilled( pBoss->szMonster, pBoss->nMapId ) )continue;

		char szKey[128];
		snprintf( szKey, sizeof( szKey ), "%u/%s", pBoss->nMapId, pBoss->szMonster );
		BOOL bShown = FALSE;
		for( size_t k = 0; k < shown.size(); ++k )
			if( shown[k] == szKey ){ bShown = TRUE; break; }
		if( bShown )continue;
		shown.push_back( szKey );
		pServer->Log( RGB( 0, 255, 0 ), "  [已首杀] %s  地图 %u\n", pBoss->szMonster, pBoss->nMapId );
	}
}

static VOID server_BossKill( CBaseServer * pServer, int nIndex, const char * pszPlayer )
{
	CHumanPlayer * pPlayer = server_FindPlayer( pServer, pszPlayer );
	if( pPlayer == NULL )return;

	BOSSENTRY * pBoss = CBossMgr::GetInstance()->GetByIndex( nIndex );
	if( pBoss == NULL || !pBoss->bAlive )
	{
		pServer->Log( RGB( 255, 0, 0 ), "该 Boss 不存在或未存活\n" );
		return;
	}

	CMonster * pMonster = CMonsterMgr::GetInstance()->FindById( pBoss->nMonsterId );
	if( pMonster == NULL )
	{
		pServer->Log( RGB( 255, 0, 0 ), "找不到 Boss 实例\n" );
		return;
	}
	pMonster->ApplyDamage( 9999999, pPlayer );
	pServer->Log( RGB( 255, 160, 0 ), "[调试] %s 击杀了 %s\n", pszPlayer, pBoss->szMonster );
}

static const char * server_ElementName( int nElement )
{
	switch( nElement )
	{
	case 1: return "火";
	case 2: return "冰";
	case 3: return "雷";
	case 4: return "毒";
	default: return "无";
	}
}

static VOID server_Enchant( CBaseServer * pServer, const char * pszPlayer, int nPos, DWORD dwId, int nElement )
{
	CHumanPlayer * pPlayer = server_FindPlayer( pServer, pszPlayer );
	if( pPlayer == NULL )return;
	if( nPos < 0 || nPos >= _U_MAX ){ pServer->Log( RGB( 255, 0, 0 ), "装备位置无效\n" ); return; }
	if( nElement < 1 || nElement > 4 ){ pServer->Log( RGB( 255, 0, 0 ), "元素只能是 1 火 / 2 冰 / 3 雷 / 4 毒\n" ); return; }

	ITEM * pItem = pPlayer->GetEquipments().GetEquipment( nPos );
	if( pItem == NULL || pItem->dwMakeIndex != dwId )
	{
		pServer->Log( RGB( 255, 0, 0 ), "该位置没有编号 %u 的装备\n", dwId );
		return;
	}
	if( pItem->btElementPower >= 5 )
	{
		pServer->Log( RGB( 255, 0, 0 ), "附魔已满（+5）\n" );
		return;
	}

	int nCost = 1000 * ( pItem->btElementPower + 1 );
	if( !pPlayer->AddGold( -nCost ) )
	{
		pServer->Log( RGB( 255, 0, 0 ), "金币不足，需要 %d\n", nCost );
		return;
	}

	pItem->btElement = (BYTE)nElement;
	pItem->btElementPower += 1;
	pPlayer->GetEquipments().ResetPropCache();
	pPlayer->UpdateProp();
	pServer->Log( RGB( 0, 255, 0 ), "%s 的 %s 附魔 %s+%d（花费 %d 金币）\n",
		pPlayer->GetName(), pItem->baseitem.szName, server_ElementName( nElement ),
		pItem->btElementPower, nCost );
	CAchievementMgr::GetInstance()->OnEnchant( pPlayer, pItem->btElementPower );
}

static VOID server_Refine( CBaseServer * pServer, const char * pszPlayer, int nPos, DWORD dwId )
{
	CHumanPlayer * pPlayer = server_FindPlayer( pServer, pszPlayer );
	if( pPlayer == NULL )return;
	if( nPos < 0 || nPos >= _U_MAX ){ pServer->Log( RGB( 255, 0, 0 ), "装备位置无效\n" ); return; }

	ITEM * pItem = pPlayer->GetEquipments().GetEquipment( nPos );
	if( pItem == NULL || pItem->dwMakeIndex != dwId )
	{
		pServer->Log( RGB( 255, 0, 0 ), "该位置没有编号 %u 的装备\n", dwId );
		return;
	}
	if( !pPlayer->AddGold( -2000 ) )
	{
		pServer->Log( RGB( 255, 0, 0 ), "金币不足，需要 2000\n" );
		return;
	}

	int nElement = Getrand( 4 ) + 1;
	int nPower = Getrand( 5 ) + 1;
	pItem->btElement = (BYTE)nElement;
	pItem->btElementPower = (BYTE)nPower;
	pPlayer->GetEquipments().ResetPropCache();
	pPlayer->UpdateProp();
	pServer->Log( RGB( 0, 255, 0 ), "%s 的 %s 洗炼为 %s+%d（花费 2000 金币）\n",
		pPlayer->GetName(), pItem->baseitem.szName, server_ElementName( nElement ), nPower );
}

static VOID server_Dungeon( CBaseServer * pServer )
{
	for( int d = 0; d < CDungeonMgr::GetInstance()->GetDefCount(); ++d )
	{
		DUNGEONDEF * pDef = CDungeonMgr::GetInstance()->GetDefByIndex( d );
		if( pDef == NULL )continue;

		int nSlotCount = 0;
		for( int i = 0; i < CDungeonMgr::GetInstance()->GetSlotCount(); ++i )
		{
			DUNGEONSLOT * pSlot = CDungeonMgr::GetInstance()->GetSlotByIndex( i );
			if( pSlot != NULL && pSlot->nDef == d )++nSlotCount;
		}
		pServer->Log( RGB( 0, 255, 255 ), "%s（%d 个实例）：\n", pDef->szName, nSlotCount );

		int nOrdinal = 0;
		for( int i = 0; i < CDungeonMgr::GetInstance()->GetSlotCount(); ++i )
		{
			DUNGEONSLOT * pSlot = CDungeonMgr::GetInstance()->GetSlotByIndex( i );
			if( pSlot == NULL || pSlot->nDef != d )continue;
			++nOrdinal;
			const char * pszDiff = nOrdinal == 1 ? "普通" : ( nOrdinal == 2 ? "困难" : "地狱" );
			if( pSlot->nPartyId == 0 )
			{
				pServer->Log( RGB( 160, 160, 160 ), "  %d（%s）：空闲（地图 %u）\n",
					nOrdinal, pszDiff, pSlot->nMapId );
			}
			else
			{
				DWORD dwRemain = pSlot->dwEndTime > timeGetTime() ? ( pSlot->dwEndTime - timeGetTime() ) / 1000 : 0;
				pServer->Log( RGB( 0, 255, 0 ), "  %d（%s）：队伍 %s（地图 %u），剩余 %u 秒\n",
					nOrdinal, pszDiff, pSlot->strPartyName.c_str(), pSlot->nMapId, (unsigned)dwRemain );
			}
		}
	}
}

static VOID server_DungeonRank( CBaseServer * pServer, const char * pszFilter )
{
	if( pszFilter != NULL && pszFilter[0] != '\0' )
	{
		pServer->Log( RGB( 0, 255, 255 ), "%s 排行：\n", pszFilter );
		int nShown = 0;
		for( int i = 0; i < CDungeonMgr::GetInstance()->GetRecordCount() && nShown < 5; ++i )
		{
			DUNGEONRECORD * pRec = CDungeonMgr::GetInstance()->GetRecordByIndex( i );
			if( pRec == NULL || strcmp( pRec->szDungeon, pszFilter ) != 0 )continue;
			++nShown;
			pServer->Log( nShown == 1 ? RGB( 255, 215, 0 ) : RGB( 160, 160, 160 ),
				"  %d. %s  用时 %u 秒  %s\n", nShown, pRec->szParty,
				(unsigned)pRec->dwSeconds, pRec->szDate );
		}
		if( nShown == 0 )
			pServer->Log( RGB( 160, 160, 160 ), "  暂无记录\n" );
		return;
	}

	for( int d = 0; d < CDungeonMgr::GetInstance()->GetDefCount(); ++d )
	{
		DUNGEONDEF * pDef = CDungeonMgr::GetInstance()->GetDefByIndex( d );
		if( pDef == NULL )continue;
		pServer->Log( RGB( 0, 255, 255 ), "%s：\n", pDef->szName );

		int nShown = 0;
		for( int i = 0; i < CDungeonMgr::GetInstance()->GetRecordCount() && nShown < 3; ++i )
		{
			DUNGEONRECORD * pRec = CDungeonMgr::GetInstance()->GetRecordByIndex( i );
			if( pRec == NULL || strcmp( pRec->szDungeon, pDef->szName ) != 0 )continue;
			++nShown;
			pServer->Log( nShown == 1 ? RGB( 255, 215, 0 ) : RGB( 160, 160, 160 ),
				"  %d. %s  用时 %u 秒  %s\n", nShown, pRec->szParty,
				(unsigned)pRec->dwSeconds, pRec->szDate );
		}
		if( nShown == 0 )
			pServer->Log( RGB( 160, 160, 160 ), "  暂无记录\n" );
	}
}

static VOID server_Codex( CBaseServer * pServer, const char * pszPlayer )
{
	CHumanPlayer * pPlayer = server_FindPlayer( pServer, pszPlayer );
	if( pPlayer == NULL )return;

	pServer->Log( RGB( 0, 255, 255 ), "%s 的图鉴（物品 %d 件，称号 %d 个，成就 %d 条）：\n",
		pPlayer->GetName(), (int)pPlayer->GetCodex().size(),
		(int)pPlayer->GetShopTitles().size(), (int)pPlayer->GetAchievements().size() );

	for( int s = 0; s < CSetMgr::GetInstance()->GetCount(); ++s )
	{
		SETDEF * pDef = CSetMgr::GetInstance()->GetByIndex( s );
		if( pDef == NULL )continue;

		int nFound = 0;
		for( size_t j = 0; j < pDef->items.size(); ++j )
			if( pPlayer->HasCodex( pDef->items[j].c_str() ) )++nFound;

		BOOL bDone = nFound >= (int)pDef->items.size();
		const char * pszState = !bDone ? "收集中" : ( pPlayer->HasCodexReward( pDef->szName ) ? "已奖励" : "已集齐" );
		pServer->Log( bDone ? RGB( 255, 215, 0 ) : RGB( 160, 160, 160 ),
			"  [%s] %s  %d/%d\n", pszState,
			pDef->szName, nFound, (int)pDef->items.size() );
	}
}

static VOID server_Title( CBaseServer * pServer, const char * pszPlayer, int nId )
{
	CHumanPlayer * pPlayer = server_FindPlayer( pServer, pszPlayer );
	if( pPlayer == NULL )return;
	if( !pPlayer->SetTitle( nId ) )
	{
		pServer->Log( RGB( 255, 0, 0 ), "尚未获得该称号，无法佩戴\n" );
		return;
	}
	pServer->Log( RGB( 255, 215, 0 ), "%s 佩戴称号【%s】\n",
		pPlayer->GetName(), pPlayer->GetTitleName() );
}

static VOID server_MapMonsters( CBaseServer * pServer, UINT nMapId )
{
	int nCount = CMonsterMgr::GetInstance()->CountMap( nMapId );
	pServer->Log( RGB( 0, 255, 255 ), "地图 %u 当前怪物: %d\n", nMapId, nCount );
}

static VOID server_Achievements( CBaseServer * pServer, const char * pszPlayer )
{
	CHumanPlayer * pPlayer = server_FindPlayer( pServer, pszPlayer );
	if( pPlayer == NULL )return;

	pServer->Log( RGB( 0, 255, 255 ), "%s 的成就（%d/%d，%d 点）：\n",
		pPlayer->GetName(), (int)pPlayer->GetAchievements().size(),
		CAchievementMgr::GetInstance()->GetCount(), pPlayer->GetAchievementPoints() );
	for( int i = 0; i < CAchievementMgr::GetInstance()->GetCount(); ++i )
	{
		ACHIEVEMENT * pAch = CAchievementMgr::GetInstance()->GetByIndex( i );
		if( pAch == NULL )continue;
		BOOL bDone = pPlayer->HasAchievement( pAch->id );
		pServer->Log( bDone ? RGB( 0, 255, 0 ) : RGB( 160, 160, 160 ),
			"  [%s] %d. %s  %s\n", bDone ? "已完成" : "未完成", pAch->id, pAch->szName, pAch->szDesc );
	}
}

static VOID server_PointShop( CBaseServer * pServer, const char * pszPlayer )
{
	CHumanPlayer * pPlayer = server_FindPlayer( pServer, pszPlayer );
	if( pPlayer == NULL )return;

	pServer->Log( RGB( 0, 255, 255 ), "成就点数商店（可用 %d 点）：\n", pPlayer->GetAvailablePoints() );
	for( int i = 0; i < CTitleMgr::GetInstance()->GetCount(); ++i )
	{
		TITLEDEF * pTitle = CTitleMgr::GetInstance()->GetByIndex( i );
		if( pTitle == NULL || pTitle->nCost <= 0 )continue;
		BOOL bOwned = pPlayer->HasShopTitle( pTitle->id );
		pServer->Log( bOwned ? RGB( 0, 255, 0 ) : RGB( 160, 160, 160 ),
			"  [%s] %d. %s  %d 点\n", bOwned ? "已兑换" : "可兑换",
			pTitle->id, pTitle->szName, pTitle->nCost );
	}
}

static VOID server_PointBuy( CBaseServer * pServer, const char * pszPlayer, int nId )
{
	CHumanPlayer * pPlayer = server_FindPlayer( pServer, pszPlayer );
	if( pPlayer == NULL )return;

	TITLEDEF * pTitle = CTitleMgr::GetInstance()->Find( nId );
	if( pTitle == NULL || pTitle->nCost <= 0 )
	{
		pServer->Log( RGB( 255, 0, 0 ), "没有这个可兑换称号\n" );
		return;
	}
	if( !pPlayer->BuyTitle( pTitle->id, pTitle->nCost ) )
	{
		pServer->Log( RGB( 255, 0, 0 ), "兑换失败（点数不足或已拥有）\n" );
		return;
	}
	pServer->Log( RGB( 0, 255, 0 ), "%s 消耗 %d 点兑换称号【%s】，剩余 %d 点\n",
		pPlayer->GetName(), pTitle->nCost, pTitle->szName, pPlayer->GetAvailablePoints() );
}

static VOID server_Titles( CBaseServer * pServer, const char * pszPlayer )
{
	CHumanPlayer * pPlayer = server_FindPlayer( pServer, pszPlayer );
	if( pPlayer == NULL )return;

	pServer->Log( RGB( 0, 255, 255 ), "%s 的称号（可用 %d 点，当前【%s】）：\n",
		pPlayer->GetName(), pPlayer->GetAvailablePoints(), pPlayer->GetTitleName() );

	for( int i = 0; i < CAchievementMgr::GetInstance()->GetCount(); ++i )
	{
		ACHIEVEMENT * pAch = CAchievementMgr::GetInstance()->GetByIndex( i );
		if( pAch == NULL || !pPlayer->HasAchievement( pAch->id ) )continue;
		pServer->Log( RGB( 0, 255, 0 ), "  [成就] %d. %s\n", pAch->id, pAch->szName );
	}
	for( size_t i = 0; i < pPlayer->GetShopTitles().size(); ++i )
	{
		TITLEDEF * pTitle = CTitleMgr::GetInstance()->Find( pPlayer->GetShopTitles()[i] );
		if( pTitle != NULL )
			pServer->Log( RGB( 255, 215, 0 ), "  [称号] %d. %s\n", pTitle->id, pTitle->szName );
	}
}

static VOID server_Season( CBaseServer * pServer, const char * pszAction )
{
	if( pszAction != NULL && strcmp( pszAction, "settle" ) == 0 )
	{
		std::string report = CDungeonMgr::GetInstance()->SettleSeason();
		pServer->Log( RGB( 255, 215, 0 ), "第 %d 赛季结算完成：\n%s",
			CDungeonMgr::GetInstance()->GetSeason() - 1,
			report.empty() ? "  本赛季没有有效记录\n" : report.c_str() );
		return;
	}

	pServer->Log( RGB( 0, 255, 255 ), "当前第 %d 赛季，排行记录 %d 条\n",
		CDungeonMgr::GetInstance()->GetSeason(),
		CDungeonMgr::GetInstance()->GetRecordCount() );
	pServer->Log( RGB( 160, 160, 160 ), "用法: season status | season settle\n" );
}

static VOID server_Online( CBaseServer * pServer )
{
	CIndexList<CHumanPlayer, MAX_HUMANPLAYER> & list = CHumanPlayerMgr::GetInstance()->GetPlayerList();
	pServer->Log( RGB( 0, 255, 255 ), "在线玩家：\n" );
	int nCount = 0;
	for( CHumanPlayer * p = list.First(); p != NULL; p = list.Next() )
	{
		++nCount;
		UINT nMap = ( p->GetMap() != NULL ) ? ( (CLogicMap*)p->GetMap() )->GetId() : 0;
		pServer->Log( RGB( 0, 255, 0 ), "  %s  Lv.%d  地图 %u (%u,%u)%s\n",
			p->GetName(), p->GetPropValue( PI_LEVEL ), nMap,
			p->getX(), p->getY(), p->GetClientObj() != NULL ? "  [客户端]" : "  [测试]" );
	}
	if( nCount == 0 )
		pServer->Log( RGB( 160, 160, 160 ), "  （无）\n" );
}

static VOID server_GmList( CBaseServer * pServer )
{
	pServer->Log( RGB( 0, 255, 255 ), "GM 名单：\n" );
	for( int i = 0; i < CGmMgr::GetInstance()->GetCount(); ++i )
	{
		GMACCOUNT * pAcc = CGmMgr::GetInstance()->GetByIndex( i );
		if( pAcc != NULL )
			pServer->Log( RGB( 0, 255, 0 ), "  %s  Lv.%d\n", pAcc->name.c_str(), pAcc->level );
	}
}

static VOID server_GmAdd( CBaseServer * pServer, const char * pszPlayer, int nLevel )
{
	CGmMgr::GetInstance()->Add( pszPlayer, nLevel );
	CGmMgr::GetInstance()->Save( "data/gm.txt" );
	pServer->Log( RGB( 0, 255, 0 ), "%s 已设为 GM %d 级\n", pszPlayer, nLevel );
}

static VOID server_GmRemove( CBaseServer * pServer, const char * pszPlayer )
{
	if( CGmMgr::GetInstance()->Remove( pszPlayer ) )
	{
		CGmMgr::GetInstance()->Save( "data/gm.txt" );
		pServer->Log( RGB( 0, 255, 0 ), "已移除 GM：%s\n", pszPlayer );
	}
	else pServer->Log( RGB( 255, 0, 0 ), "没有这个 GM\n" );
}

static VOID server_Spawn( CBaseServer * pServer, const char * pszMonster, int nMap, int x, int y, int nCount )
{
	if( nCount <= 0 )nCount = 1;
	if( nCount > 50 )nCount = 50;
	int nOk = 0;
	for( int i = 0; i < nCount; ++i )
	{
		WORD sx = (WORD)( x + Getrand( 5 ) - 2 );
		WORD sy = (WORD)( y + Getrand( 5 ) - 2 );
		if( CMonsterMgr::GetInstance()->Spawn( pszMonster, (UINT)nMap, sx, sy ) != NULL )++nOk;
	}
	pServer->Log( RGB( 0, 255, 0 ), "刷出 %s x%d（地图 %d）\n", pszMonster, nOk, nMap );
}

static VOID server_Heal( CBaseServer * pServer, const char * pszPlayer )
{
	CHumanPlayer * pPlayer = server_FindPlayer( pServer, pszPlayer );
	if( pPlayer == NULL )return;
	pPlayer->RestoreAll();
	pServer->Log( RGB( 0, 255, 0 ), "%s 已恢复 HP/MP\n", pszPlayer );
}

VOID CServer::OnInputCommand( const char * pszCommand )
{
	if( pszCommand == NULL || *pszCommand == '\0' )return;

	char	szBuffer[512];
	o_strncpy( szBuffer, pszCommand, sizeof( szBuffer ) - 1 );

	char *	Params[16];
	int		nParam = SearchParam( szBuffer, Params, 16, ' ' );
	if( nParam <= 0 )return;

	std::string cmd = Params[0];

	if( cmd == "help" || cmd == "?" )
	{
		server_Help( this );
	}
	else if( cmd == "status" )
	{
		Log( RGB( 0, 255, 255 ), "服务器: %s | 运行: %s | 客户端: %d | 测试角色: %d\n",
			GetServerName(), IsRunning() ? "是" : "否",
			GetClientCount(), CGameWorld::GetInstance()->GetTestPlayerCount() );
	}
	else if( cmd == "addbot" )
	{
		server_AddBot( this, nParam >= 2 ? Params[1] : NULL, nParam >= 3 ? Params[2] : NULL );
	}
	else if( cmd == "online" )
	{
		server_Online( this );
	}
	else if( cmd == "gmadd" && nParam >= 3 )
	{
		server_GmAdd( this, Params[1], atoi( Params[2] ) );
	}
	else if( cmd == "gmremove" && nParam >= 2 )
	{
		server_GmRemove( this, Params[1] );
	}
	else if( cmd == "gmlist" )
	{
		server_GmList( this );
	}
	else if( cmd == "spawn" && nParam >= 5 )
	{
		server_Spawn( this, Params[1], atoi( Params[2] ), atoi( Params[3] ), atoi( Params[4] ),
			nParam >= 6 ? atoi( Params[5] ) : 1 );
	}
	else if( cmd == "heal" && nParam >= 2 )
	{
		server_Heal( this, Params[1] );
	}
	else if( cmd == "players" )
	{
		Log( RGB( 0, 255, 255 ), "当前在线测试角色: %d\n", CGameWorld::GetInstance()->GetTestPlayerCount() );
	}
	else if( cmd == "give" && nParam >= 3 )
	{
		server_Give( this, Params[1], Params[2] );
	}
	else if( cmd == "bag" && nParam >= 2 )
	{
		server_Bag( this, Params[1] );
	}
	else if( cmd == "equip" && nParam >= 4 )
	{
		server_Equip( this, Params[1], atoi( Params[2] ), (DWORD)strtoul( Params[3], NULL, 10 ) );
	}
	else if( cmd == "unequip" && nParam >= 4 )
	{
		server_Unequip( this, Params[1], atoi( Params[2] ), (DWORD)strtoul( Params[3], NULL, 10 ) );
	}
	else if( cmd == "drop" && nParam >= 3 )
	{
		server_Drop( this, Params[1], (DWORD)strtoul( Params[2], NULL, 10 ) );
	}
	else if( cmd == "droptile" && nParam >= 3 )
	{
		server_DropTile( this, Params[1], (DWORD)strtoul( Params[2], NULL, 10 ) );
	}
	else if( cmd == "pickup" && nParam >= 2 )
	{
		server_Pickup( this, Params[1] );
	}
	else if( cmd == "info" && nParam >= 2 )
	{
		server_Info( this, Params[1] );
	}
	else if( cmd == "addexp" && nParam >= 3 )
	{
		server_AddExp( this, Params[1], atoi( Params[2] ) );
	}
	else if( cmd == "cast" && nParam >= 3 )
	{
		server_Cast( this, Params[1], Params[2], nParam >= 4 ? atoi( Params[3] ) : -1 );
	}
	else if( cmd == "testskill" && nParam >= 3 )
	{
		server_TestSkill( this, Params[1], Params[2], nParam >= 4 ? Params[3] : NULL );
	}
	else if( cmd == "learn" && nParam >= 3 )
	{
		server_Learn( this, Params[1], Params[2] );
	}
	else if( cmd == "skills" && nParam >= 2 )
	{
		server_Skills( this, Params[1] );
	}
	else if( cmd == "restore" && nParam >= 2 )
	{
		server_Restore( this, Params[1] );
	}
	else if( cmd == "hurt" && nParam >= 3 )
	{
		server_Hurt( this, Params[1], atoi( Params[2] ) );
	}
	else if( cmd == "use" && nParam >= 3 )
	{
		server_Use( this, Params[1], (DWORD)strtoul( Params[2], NULL, 10 ) );
	}
	else if( cmd == "shop" )
	{
		server_Shop( this );
	}
	else if( cmd == "buy" && nParam >= 3 )
	{
		server_Buy( this, Params[1], Params[2], nParam >= 4 ? atoi( Params[3] ) : 1 );
	}
	else if( cmd == "sell" && nParam >= 3 )
	{
		server_Sell( this, Params[1], (DWORD)strtoul( Params[2], NULL, 10 ) );
	}
	else if( cmd == "teleport" && nParam >= 5 )
	{
		server_Teleport( this, Params[1], (UINT)atoi( Params[2] ), atoi( Params[3] ), atoi( Params[4] ) );
	}
	else if( cmd == "npcs" )
	{
		server_Npcs( this );
	}
	else if( cmd == "talk" && nParam >= 3 )
	{
		server_Talk( this, Params[1], Params[2] );
	}
	else if( cmd == "npcshop" && nParam >= 3 )
	{
		server_NpcShop( this, Params[1], Params[2] );
	}
	else if( cmd == "npcteleport" && nParam >= 3 )
	{
		server_NpcTeleport( this, Params[1], Params[2] );
	}
	else if( cmd == "quests" && nParam >= 2 )
	{
		server_Quests( this, Params[1] );
	}
	else if( cmd == "accept" && nParam >= 3 )
	{
		server_AcceptQuest( this, Params[1], Params[2] );
	}
	else if( cmd == "submit" && nParam >= 3 )
	{
		server_SubmitQuest( this, Params[1], Params[2] );
	}
	else if( cmd == "equipinfo" && nParam >= 2 )
	{
		server_EquipList( this, Params[1] );
	}
	else if( cmd == "upgrade" && nParam >= 4 )
	{
		server_Upgrade( this, Params[1], atoi( Params[2] ), (DWORD)strtoul( Params[3], NULL, 10 ) );
	}
	else if( cmd == "repair" && nParam >= 4 )
	{
		server_Repair( this, Params[1], atoi( Params[2] ), (DWORD)strtoul( Params[3], NULL, 10 ) );
	}
	else if( cmd == "split" && nParam >= 4 )
	{
		server_Split( this, Params[1], (DWORD)strtoul( Params[2], NULL, 10 ), atoi( Params[3] ) );
	}
	else if( cmd == "sellall" && nParam >= 3 )
	{
		server_SellAll( this, Params[1], Params[2] );
	}
	else if( cmd == "party" && nParam >= 2 )
	{
		server_PartyInfo( this, Params[1] );
	}
	else if( cmd == "partyinvite" && nParam >= 3 )
	{
		server_PartyInvite( this, Params[1], Params[2] );
	}
	else if( cmd == "partyleave" && nParam >= 2 )
	{
		server_PartyLeave( this, Params[1] );
	}
	else if( cmd == "guild" && nParam >= 2 )
	{
		std::string sub = Params[1];
		if( sub == "create" && nParam >= 4 )server_GuildCreate( this, Params[2], Params[3] );
		else if( sub == "join" && nParam >= 4 )server_GuildJoin( this, Params[2], Params[3] );
		else if( sub == "leave" && nParam >= 3 )server_GuildLeave( this, Params[2] );
		else if( sub == "info" && nParam >= 3 )server_GuildInfo( this, Params[2] );
		else if( sub == "list" )server_GuildList( this );
		else if( sub == "donate" && nParam >= 4 )server_GuildDonate( this, Params[2], atoi( Params[3] ) );
		else if( sub == "kick" && nParam >= 4 )server_GuildKick( this, Params[2], Params[3] );
		else if( sub == "appoint" && nParam >= 4 )server_GuildAppoint( this, Params[2], Params[3] );
		else if( sub == "store" && nParam >= 4 )server_GuildStore( this, Params[2], (DWORD)strtoul( Params[3], NULL, 10 ) );
		else if( sub == "ware" && nParam >= 3 )server_GuildWare( this, Params[2] );
		else if( sub == "take" && nParam >= 4 )server_GuildTake( this, Params[2], atoi( Params[3] ) );
		else Log( RGB( 255, 255, 0 ), "用法: guild create/join/leave/info/list\n" );
	}
	else if( cmd == "drops" && nParam >= 2 )
	{
		server_Drops( this, Params[1] );
	}
	else if( cmd == "event" && nParam >= 2 )
	{
		if( std::string( Params[1] ) == "doubleexp" && nParam >= 3 )
		{
			DWORD dwSeconds = (DWORD)atoi( Params[2] );
			CEventMgr::GetInstance()->StartDoubleExp( dwSeconds );
			char szMsg[128];
			snprintf( szMsg, sizeof( szMsg ), "双倍经验活动开始，持续 %u 秒", (unsigned)dwSeconds );
			Game_Announce( szMsg );
		}
		else if( std::string( Params[1] ) == "list" )
		{
			int nCount = CEventMgr::GetInstance()->GetScheduleCount();
			Log( RGB( 0, 255, 255 ), "活动排期: %d 条\n", nCount );
			for( int i = 0; i < nCount; ++i )
			{
				EVENTSCHEDULE * e = CEventMgr::GetInstance()->GetScheduleByIndex( i );
				if( e == NULL )continue;
				if( e->nType == 1 )
					Log( RGB( 160, 160, 160 ), "  [%d] 世界Boss %s  %02d:%02d  %d 分钟  地图 %u (%u,%u)\n",
						i, e->szBoss, e->nHour, e->nMinute, e->nDurationMin, e->nMapId, e->x, e->y );
				else
					Log( RGB( 160, 160, 160 ), "  [%d] 双倍经验 %s  %02d:%02d  %d 分钟\n",
						i, e->szName, e->nHour, e->nMinute, e->nDurationMin );
			}
		}
		else if( std::string( Params[1] ) == "trigger" && nParam >= 3 )
		{
			int nIndex = atoi( Params[2] );
			std::string msg;
			if( CEventMgr::GetInstance()->Trigger( nIndex, msg ) )
				Game_Announce( msg.c_str() );
			else
				Log( RGB( 255, 0, 0 ), "没有这个排期或触发失败\n" );
		}
		else if( std::string( Params[1] ) == "status" )
		{
			Log( RGB( 0, 255, 255 ), "双倍经验：%s（剩余 %u 秒）\n",
				CEventMgr::GetInstance()->IsDoubleExp() ? "进行中" : "未开启",
				(unsigned)CEventMgr::GetInstance()->RemainingSeconds() );
		}
		else
		{
			Log( RGB( 255, 255, 0 ), "用法: event doubleexp <秒> | event status\n" );
		}
	}
	else if( cmd == "announce" && nParam >= 2 )
	{
		std::string text = Params[1];
		for( int i = 2; i < nParam; ++i ){ text += " "; text += Params[i]; }
		Game_Announce( text.c_str() );
	}
	else if( cmd == "enchant" && nParam >= 5 )
	{
		server_Enchant( this, Params[1], atoi( Params[2] ), (DWORD)strtoul( Params[3], NULL, 10 ), atoi( Params[4] ) );
	}
	else if( cmd == "refine" && nParam >= 4 )
	{
		server_Refine( this, Params[1], atoi( Params[2] ), (DWORD)strtoul( Params[3], NULL, 10 ) );
	}
	else if( cmd == "achievements" && nParam >= 2 )
	{
		server_Achievements( this, Params[1] );
	}
	else if( cmd == "pointshop" && nParam >= 2 )
	{
		server_PointShop( this, Params[1] );
	}
	else if( cmd == "pointbuy" && nParam >= 3 )
	{
		server_PointBuy( this, Params[1], atoi( Params[2] ) );
	}
	else if( cmd == "codex" && nParam >= 2 )
	{
		server_Codex( this, Params[1] );
	}
	else if( cmd == "titles" && nParam >= 2 )
	{
		server_Titles( this, Params[1] );
	}
	else if( cmd == "title" && nParam >= 3 )
	{
		server_Title( this, Params[1], atoi( Params[2] ) );
	}
	else if( cmd == "sets" && nParam >= 2 )
	{
		server_Sets( this, Params[1] );
	}
	else if( cmd == "addgold" && nParam >= 3 )
	{
		server_AddGold( this, Params[1], atoi( Params[2] ) );
	}
	else if( cmd == "choose" && nParam >= 4 )
	{
		server_Choose( this, Params[1], Params[2], atoi( Params[3] ) );
	}
	else if( cmd == "boss" )
	{
		server_Bosses( this );
	}
	else if( cmd == "mapmonsters" && nParam >= 2 )
	{
		server_MapMonsters( this, (UINT)atoi( Params[1] ) );
	}
	else if( cmd == "dungeon" )
	{
		server_Dungeon( this );
	}
	else if( cmd == "season" && nParam >= 2 )
	{
		server_Season( this, Params[1] );
	}
	else if( cmd == "dungeonrank" )
	{
		server_DungeonRank( this, nParam >= 2 ? Params[1] : NULL );
	}
	else if( cmd == "firstkills" )
	{
		server_FirstKills( this );
	}
	else if( cmd == "bosskill" && nParam >= 3 )
	{
		server_BossKill( this, atoi( Params[1] ), Params[2] );
	}
	else if( cmd == "gsay" && nParam >= 3 )
	{
		std::string text = Params[2];
		for( int i = 3; i < nParam; ++i ){ text += " "; text += Params[i]; }
		server_GSay( this, Params[1], text.c_str() );
	}
	else if( cmd == "psay" && nParam >= 3 )
	{
		std::string text = Params[2];
		for( int i = 3; i < nParam; ++i ){ text += " "; text += Params[i]; }
		server_PSay( this, Params[1], text.c_str() );
	}
	else if( cmd == "spawn" && nParam >= 3 )
	{
		server_Spawn( this, Params[1], Params[2] );
	}
	else if( cmd == "attack" && nParam >= 3 )
	{
		server_Attack( this, Params[1], atoi( Params[2] ) );
	}
	else if( cmd == "monsters" )
	{
		server_Monsters( this );
	}
	else if( cmd == "testcombat" && nParam >= 3 )
	{
		server_TestCombat( this, Params[1], Params[2] );
	}
	else if( cmd == "kill" && nParam >= 2 )
	{
		server_KillPlayer( this, Params[1] );
	}
	else if( cmd == "save" )
	{
		server_Save( this );
	}
	else if( cmd == "kickname" && nParam >= 2 )
	{
		CHumanPlayer * pPlayer = server_FindPlayer( this, Params[1] );
		if( pPlayer == NULL || pPlayer->GetClientObj() == NULL )
			Log( RGB( 255, 0, 0 ), "没有这个在线客户端角色\n" );
		else
		{
			UINT nId = pPlayer->GetClientObj()->GetId();
			KickClient( nId );
			Log( RGB( 255, 255, 0 ), "已把 %s 踢下线（客户端 %u）\n", Params[1], nId );
		}
	}
	else if( cmd == "kick" )
	{
		if( nParam >= 2 )
		{
			UINT id = (UINT)atoi( Params[1] );
			KickClient( id );
			Log( RGB( 255, 255, 0 ), "已请求断开客户端 %u\n", id );
		}
		else
		{
			Log( RGB( 255, 255, 0 ), "用法: kick <客户端 id>\n" );
		}
	}
	else if( cmd == "clear" )
	{
		if( GetIoConsole() != NULL )GetIoConsole()->Clear();
	}
	else
	{
		Log( RGB( 255, 255, 0 ), "未知命令: %s（输入 help 查看）\n", cmd.c_str() );
	}
}
