// PlayerFactory.cpp  ——  角色创建 + 读档恢复
#include "stdafx.h"
#include "PlayerFactory.h"
#include "HumanPlayer.h"
#include "HumanPlayerMgr.h"
#include "GameWorld.h"
#include "ItemBox.h"
#include "Equipment.h"
#include "PlayerStore.h"

#include <vector>
#include <utility>

CHumanPlayer * PlayerFactory_Create( const char * pszName, CClientObj * pClientObj, BOOL bTestPlayer, int nClassId )
{
	if( pszName == NULL || *pszName == '\0' )return NULL;
	if( CHumanPlayerMgr::GetInstance()->FindbyName( pszName ) != NULL )
		return NULL;		//	重名角色不允许同时在线

	CHARDBINFO info;
	memset( &info, 0, sizeof( info ) );
	o_strncpy( info.szName, pszName, sizeof( info.szName ) - 1 );
	info.wLevel = 0;		//	0 = 新角色，由 CHumanPlayer::Init 套默认属性
	info.mapid = 1;

	std::vector<ITEM>						bag;
	std::vector<std::pair<int, ITEM> >		equips;
	DWORD									dwLearned = 0;
	std::vector<PLAYERQUEST>				quests;
	std::vector<int> achievements;
	std::vector<int> shopTitles;
	std::vector<std::string> codex;
	std::vector<std::string> codexRewarded;
	int nTitle = 0;
	int nPointsSpent = 0;
	BOOL bLoaded = CPlayerStore::GetInstance()->Load( pszName, info, bag, equips, dwLearned, quests, achievements, nTitle, nPointsSpent, shopTitles, codex, codexRewarded );
	if( bLoaded )
	{
		//	存档里的名字可能已被清理过，统一用请求的名字
		o_strncpy( info.szName, pszName, sizeof( info.szName ) - 1 );
		if( info.btClass > 2 )info.btClass = 0;
	}
	else
	{
		//	新角色：职业由创建方指定
		info.btClass = ( nClassId >= 0 && nClassId <= 2 ) ? (BYTE)nClassId : 0;
	}

	CREATEHUMANDESC desc;
	desc.dbinfo = info;
	desc.pClientObj = pClientObj;

	CHumanPlayer * pPlayer = CHumanPlayerMgr::GetInstance()->NewPlayer( desc );
	if( pPlayer == NULL )return NULL;

	pPlayer->SetMapId( info.mapid != 0 ? info.mapid : 1 );

	BOOL bAdded = bTestPlayer
		? CGameWorld::GetInstance()->AddTestPlayer( pPlayer )
		: CGameWorld::GetInstance()->AddPlayer( pPlayer );
	if( !bAdded )
	{
		CHumanPlayerMgr::GetInstance()->DeletePlayer( pPlayer );
		return NULL;
	}

	for( size_t i = 0; i < bag.size(); ++i )
		pPlayer->GetBag().AddItem( bag[i] );
	for( size_t i = 0; i < equips.size(); ++i )
		pPlayer->GetEquipments().RestoreItem( equips[i].first, equips[i].second );

	if( bLoaded )
	{
		pPlayer->SetLearnedSkills( dwLearned );	//	已学技能随存档恢复
		if( !quests.empty() )
			pPlayer->RestoreQuests( &quests[0], (int)quests.size() );
		pPlayer->RestoreShopTitles( nPointsSpent, shopTitles );
		pPlayer->RestoreCodex( codex );
		pPlayer->RestoreCodexRewards( codexRewarded );
		pPlayer->RestoreAchievements( achievements, nTitle );
	}

	pPlayer->UpdateProp();
	return pPlayer;
}
