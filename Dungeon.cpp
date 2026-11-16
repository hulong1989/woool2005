// Dungeon.cpp
#include "stdafx.h"
#include "Dungeon.h"
#include "HumanPlayer.h"
#include "HumanPlayerMgr.h"
#include "Party.h"
#include "Npc.h"
#include "GameActions.h"
#include "LogicMap.h"
#include "Boss.h"
#include "Monster.h"
#include "Achievement.h"
#include "Title.h"
#include <fstream>
#include <algorithm>
#include <time.h>

CDungeonMgr * CDungeonMgr::m_pInstance = NULL;

CDungeonMgr * CDungeonMgr::GetInstance()
{
	if( m_pInstance == NULL )
	{
		m_pInstance = new CDungeonMgr;
		m_pInstance->m_nLastFirstDay = -1;
		m_pInstance->m_nSeason = 1;
	}
	return m_pInstance;
}

BOOL CDungeonMgr::Load( const char * pszFile )
{
	m_Defs.clear();
	m_Slots.clear();

	CStringFile sf( pszFile );
	for( int i = 0; i < sf.GetLineCount(); ++i )
	{
		const char * pszLine = sf[i];
		if( pszLine == NULL || *pszLine == '\0' || *pszLine == '#' )continue;

		char	szBuffer[256];
		o_strncpy( szBuffer, pszLine, sizeof( szBuffer ) - 1 );
		char *	Params[8];
		//	id/名称/Boss/实例地图/成就id
		int nParam = SearchParam( szBuffer, Params, 8, '/' );
		if( nParam < 5 )continue;

		DUNGEONDEF def;
		def.id = 0;
		def.szName[0] = '\0';
		def.szBoss[0] = '\0';
		def.maps.clear();
		def.nAchId = 0;
		def.nTitleId = 0;
		def.id = atoi( Params[0] );
		o_strncpy( def.szName, Params[1], sizeof( def.szName ) - 1 );
		o_strncpy( def.szBoss, Params[2], sizeof( def.szBoss ) - 1 );
		def.nAchId = atoi( Params[4] );
		def.nTitleId = ( nParam >= 6 ) ? atoi( Params[5] ) : 0;

		std::string maps = Params[3];
		size_t start = 0;
		while( start <= maps.size() )
		{
			size_t comma = maps.find( ',', start );
			std::string m = maps.substr( start, comma == std::string::npos ? std::string::npos : comma - start );
			if( !m.empty() )def.maps.push_back( (UINT)atoi( m.c_str() ) );
			if( comma == std::string::npos )break;
			start = comma + 1;
		}
		if( def.maps.empty() )continue;

		int nDefIndex = (int)m_Defs.size();
		m_Defs.push_back( def );

		for( size_t k = 0; k < def.maps.size(); ++k )
		{
			DUNGEONSLOT slot;
			memset( &slot.nPartyId, 0, sizeof( slot.nPartyId ) );
			slot.nDef = nDefIndex;
			slot.nMapId = def.maps[k];
			slot.dwStartTime = 0;
			slot.dwEndTime = 0;
			m_Slots.push_back( slot );
		}
	}
	return !m_Defs.empty();
}

int CDungeonMgr::GetDefCount(){ return (int)m_Defs.size(); }

DUNGEONDEF * CDungeonMgr::GetDefByIndex( int nIndex )
{
	if( nIndex < 0 || (size_t)nIndex >= m_Defs.size() )return NULL;
	return &m_Defs[(size_t)nIndex];
}

DUNGEONDEF * CDungeonMgr::GetDefById( int nId )
{
	for( size_t i = 0; i < m_Defs.size(); ++i )
		if( m_Defs[i].id == nId )return &m_Defs[i];
	return NULL;
}

int CDungeonMgr::GetSlotCount(){ return (int)m_Slots.size(); }

DUNGEONSLOT * CDungeonMgr::GetSlotByIndex( int nIndex )
{
	if( nIndex < 0 || (size_t)nIndex >= m_Slots.size() )return NULL;
	return &m_Slots[(size_t)nIndex];
}

std::string CDungeonMgr::Enter( CHumanPlayer * pPlayer, CNpc * pNpc )
{
	if( pPlayer == NULL || pNpc == NULL )return "参数错误";
	if( pNpc->GetNpcType() != 3 )return "对方不是副本入口";

	//	通过入口坐标找到副本
	int nDef = -1;
	for( size_t d = 0; d < m_Defs.size(); ++d )
		for( size_t k = 0; k < m_Defs[d].maps.size(); ++k )
			if( (int)m_Defs[d].maps[k] == pNpc->GetDestMap() ){ nDef = (int)d; break; }
	if( nDef < 0 )return "该入口没有对应副本";

	int nPartyId = CPartyMgr::GetInstance()->GetPartyId( pPlayer->GetName() );
	if( nPartyId == 0 )return "需要先组队才能进入副本";

	int nSlot = -1;
	for( size_t i = 0; i < m_Slots.size(); ++i )
	{
		if( m_Slots[i].nDef != nDef )continue;
		if( m_Slots[i].nPartyId == nPartyId ){ nSlot = (int)i; break; }
	}
	if( nSlot < 0 )
	{
		for( size_t i = 0; i < m_Slots.size(); ++i )
		{
			if( m_Slots[i].nDef != nDef )continue;
			if( m_Slots[i].nPartyId == 0 ){ nSlot = (int)i; break; }
		}
	}
	if( nSlot < 0 )return "该副本实例已满，请稍后再来";

	DUNGEONDEF & def = m_Defs[(size_t)nDef];
	DUNGEONSLOT & slot = m_Slots[(size_t)nSlot];

	if( slot.nPartyId == 0 )
	{
		slot.nPartyId = nPartyId;
		slot.strPartyName = pPlayer->GetName();
		slot.dwStartTime = timeGetTime();
		slot.dwEndTime = timeGetTime() + DUNGEON_DURATION * 1000;

		CBossMgr::GetInstance()->ClearMap( slot.nMapId );
		CMonsterMgr::GetInstance()->ResetMap( slot.nMapId );
		CBossMgr::GetInstance()->ForceRespawn( slot.nMapId );
	}

	std::vector<std::string> members;
	CPartyMgr::GetInstance()->GetMembers( pPlayer->GetName(), members );

	int nEntered = 0;
	for( size_t i = 0; i < members.size(); ++i )
	{
		CHumanPlayer * pMember = CHumanPlayerMgr::GetInstance()->FindbyName( members[i].c_str() );
		if( pMember == NULL )continue;

		BOOL bOk = FALSE;
		for( int nTry = 0; nTry < 20 && !bOk; ++nTry )
		{
			int dx = (int)( i % 5 ) + nTry / 5;
			int dy = (int)( i / 5 ) + nTry % 3;
			bOk = Game_TeleportPlayer( pMember, slot.nMapId,
				pNpc->GetDestX() + dx, pNpc->GetDestY() + dy );
		}
		if( bOk )++nEntered;
	}
	if( nEntered == 0 )return "传送失败";

	//	难度 = 该副本内第几个实例
	int nOrdinal = 0;
	for( size_t i = 0; i <= (size_t)nSlot; ++i )
		if( m_Slots[i].nDef == nDef )++nOrdinal;
	const char * pszDiff = nOrdinal == 1 ? "普通" : ( nOrdinal == 2 ? "困难" : "地狱" );

	char szMsg[192];
	snprintf( szMsg, sizeof( szMsg ), "队伍进入【%s】%d 号实例（%s，地图 %u，%d 人）",
		def.szName, nOrdinal, pszDiff, slot.nMapId, nEntered );
	return szMsg;
}

VOID CDungeonMgr::OnMonsterDead( const char * pszMonster, UINT nMapId )
{
	if( pszMonster == NULL )return;

	int nSlot = -1;
	for( size_t i = 0; i < m_Slots.size(); ++i )
		if( m_Slots[i].nMapId == nMapId && m_Slots[i].nPartyId != 0 ){ nSlot = (int)i; break; }
	if( nSlot < 0 )return;

	DUNGEONSLOT & slot = m_Slots[(size_t)nSlot];
	DUNGEONDEF & def = m_Defs[(size_t)slot.nDef];
	if( strncmp( pszMonster, def.szBoss, strlen( def.szBoss ) ) != 0 )return;

	DWORD dwSeconds = ( slot.dwStartTime != 0 ) ? ( timeGetTime() - slot.dwStartTime ) / 1000 : 0;

	time_t t = time( NULL );
	struct tm lt;
#if defined(_WIN32)
	lt = *localtime( &t );
#else
	localtime_r( &t, &lt );
#endif
	int nToday = lt.tm_mday;
	if( m_nLastFirstDay != nToday )
	{
		m_nLastFirstDay = nToday;
		std::vector<std::string> members;
		CPartyMgr::GetInstance()->GetMembers( slot.strPartyName.c_str(), members );
		int nReward = 0;
		for( size_t i = 0; i < members.size(); ++i )
		{
			CHumanPlayer * pMember = CHumanPlayerMgr::GetInstance()->FindbyName( members[i].c_str() );
			if( pMember == NULL )continue;
			pMember->AddGold( 2000 );
			pMember->AddExp( 2000 );
			++nReward;
		}
		char szReward[128];
		snprintf( szReward, sizeof( szReward ), "每日首通奖励：%d 名队员各获 2000 金币 / 2000 经验", nReward );
		Game_Announce( szReward );
	}

	{
		std::vector<std::string> members;
		CPartyMgr::GetInstance()->GetMembers( slot.strPartyName.c_str(), members );
		for( size_t i = 0; i < members.size(); ++i )
		{
			CHumanPlayer * pMember = CHumanPlayerMgr::GetInstance()->FindbyName( members[i].c_str() );
			if( pMember != NULL )
				CAchievementMgr::GetInstance()->OnDungeonClear( pMember, def.nAchId );
		}
	}

	//	首通称号：每个副本只授予第一次通关的队伍
	BOOL bFirstClear = TRUE;
	for( size_t i = 0; i < m_FirstClears.size(); ++i )
		if( m_FirstClears[i] == def.id ){ bFirstClear = FALSE; break; }

	if( bFirstClear )
	{
		m_FirstClears.push_back( def.id );
		SaveFirstClears( "data/dungeon_first.dat" );

		TITLEDEF * pTitle = CTitleMgr::GetInstance()->Find( def.nTitleId );
		std::vector<std::string> members;
		CPartyMgr::GetInstance()->GetMembers( slot.strPartyName.c_str(), members );
		for( size_t i = 0; i < members.size(); ++i )
		{
			CHumanPlayer * pMember = CHumanPlayerMgr::GetInstance()->FindbyName( members[i].c_str() );
			if( pMember != NULL && def.nTitleId > 0 )
				pMember->GrantTitle( def.nTitleId );
		}

		char szFirst[192];
		snprintf( szFirst, sizeof( szFirst ), "队伍【%s】首通%s，全员获得称号【%s】！",
			slot.strPartyName.c_str(), def.szName,
			pTitle != NULL ? pTitle->szName : "首通者" );
		Game_Announce( szFirst );
	}

	DUNGEONRECORD rec;
	memset( &rec, 0, sizeof( rec ) );
	o_strncpy( rec.szDungeon, def.szName, sizeof( rec.szDungeon ) - 1 );
	o_strncpy( rec.szParty, slot.strPartyName.c_str(), sizeof( rec.szParty ) - 1 );
	rec.dwSeconds = dwSeconds;
	snprintf( rec.szDate, sizeof( rec.szDate ), "%02d-%02d", lt.tm_mon + 1, lt.tm_mday );

	m_Records.push_back( rec );
	std::sort( m_Records.begin(), m_Records.end(),
		[]( const DUNGEONRECORD & a, const DUNGEONRECORD & b ){ return a.dwSeconds < b.dwSeconds; } );
	TrimRecords();
	SaveRecords( "data/dungeon_rank.dat" );

	BOOL bRecord = ( !m_Records.empty() && strcmp( m_Records[0].szParty, rec.szParty ) == 0 &&
		m_Records[0].dwSeconds == dwSeconds );
	char szMsg[192];
	snprintf( szMsg, sizeof( szMsg ), "队伍【%s】通关%s，用时 %u 秒%s",
		rec.szParty, def.szName, (unsigned)dwSeconds, bRecord ? "，刷新最快纪录！" : "" );
	Game_Announce( szMsg );

	slot.nPartyId = 0;
	slot.dwStartTime = 0;
	slot.dwEndTime = 0;
	slot.strPartyName.clear();
}

VOID CDungeonMgr::Update()
{
	for( size_t i = 0; i < m_Slots.size(); ++i )
	{
		DUNGEONSLOT & slot = m_Slots[i];
		if( slot.nPartyId == 0 )continue;

		if( timeGetTime() >= slot.dwEndTime )
		{
			DUNGEONDEF & def = m_Defs[(size_t)slot.nDef];
			char szMsg[128];
			snprintf( szMsg, sizeof( szMsg ), "%s实例超时，已自动重置", def.szName );
			Game_Announce( szMsg );
			slot.nPartyId = 0;
			slot.dwStartTime = 0;
			continue;
		}

		BOOL bAnyInside = FALSE;
		CIndexList<CHumanPlayer, MAX_HUMANPLAYER> & players = CHumanPlayerMgr::GetInstance()->GetPlayerList();
		for( CHumanPlayer * p = players.First(); p != NULL; p = players.Next() )
		{
			if( p->GetMap() != NULL && ( (CLogicMap*)p->GetMap() )->GetId() == slot.nMapId )
			{
				bAnyInside = TRUE;
				break;
			}
		}
		if( !bAnyInside )
		{
			slot.nPartyId = 0;
			slot.dwStartTime = 0;
			slot.dwEndTime = 0;
			slot.strPartyName.clear();
		}
	}
}

BOOL CDungeonMgr::LoadRecords( const char * pszFile )
{
	m_Records.clear();
	std::ifstream in( pszFile, std::ios::binary );
	if( !in.is_open() )return FALSE;
	std::string line;
	while( std::getline( in, line ) )
	{
		if( line.empty() || line[0] == '#' )continue;
		DUNGEONRECORD rec;
		memset( &rec, 0, sizeof( rec ) );
		char szDungeon[32] = { 0 }, szParty[32] = { 0 }, szDate[16] = { 0 };
		int n = sscanf( line.c_str(), "%31[^/]/%31[^/]/%u/%15s", szDungeon, szParty, &rec.dwSeconds, szDate );
		if( n == 4 )
		{
			o_strncpy( rec.szDungeon, szDungeon, sizeof( rec.szDungeon ) - 1 );
			o_strncpy( rec.szParty, szParty, sizeof( rec.szParty ) - 1 );
			o_strncpy( rec.szDate, szDate, sizeof( rec.szDate ) - 1 );
		}
		else if( n == 3 )	//	旧格式：party/seconds/date
		{
			o_strncpy( rec.szDungeon, "沃玛神殿", sizeof( rec.szDungeon ) - 1 );
			o_strncpy( rec.szParty, szDungeon, sizeof( rec.szParty ) - 1 );
			o_strncpy( rec.szDate, szParty, sizeof( rec.szDate ) - 1 );
		}
		else continue;
		m_Records.push_back( rec );
	}
	return !m_Records.empty();
}

BOOL CDungeonMgr::SaveRecords( const char * pszFile )
{
	std::ofstream out( pszFile, std::ios::binary | std::ios::trunc );
	if( !out.is_open() )return FALSE;
	for( size_t i = 0; i < m_Records.size(); ++i )
		out << m_Records[i].szDungeon << "/" << m_Records[i].szParty << "/"
			<< m_Records[i].dwSeconds << "/" << m_Records[i].szDate << "\n";
	return TRUE;
}

int CDungeonMgr::GetRecordCount(){ return (int)m_Records.size(); }

DUNGEONRECORD * CDungeonMgr::GetRecordByIndex( int nIndex )
{
	if( nIndex < 0 || (size_t)nIndex >= m_Records.size() )return NULL;
	return &m_Records[(size_t)nIndex];
}

BOOL CDungeonMgr::LoadFirstClears( const char * pszFile )
{
	m_FirstClears.clear();
	std::ifstream in( pszFile, std::ios::binary );
	if( !in.is_open() )return FALSE;
	std::string line;
	while( std::getline( in, line ) )
	{
		if( line.empty() || line[0] == '#' )continue;
		m_FirstClears.push_back( atoi( line.c_str() ) );
	}
	return TRUE;
}

BOOL CDungeonMgr::SaveFirstClears( const char * pszFile )
{
	std::ofstream out( pszFile, std::ios::binary | std::ios::trunc );
	if( !out.is_open() )return FALSE;
	for( size_t i = 0; i < m_FirstClears.size(); ++i )
		out << m_FirstClears[i] << "\n";
	return TRUE;
}

BOOL CDungeonMgr::IsFirstCleared( int nId )
{
	for( size_t i = 0; i < m_FirstClears.size(); ++i )
		if( m_FirstClears[i] == nId )return TRUE;
	return FALSE;
}

int CDungeonMgr::GetFirstClearCount()
{
	return (int)m_FirstClears.size();
}

//	每个副本各保留最快 DUNGEON_MAX_RECORD 条
VOID CDungeonMgr::TrimRecords()
{
	std::vector<DUNGEONRECORD> kept;
	for( size_t i = 0; i < m_Records.size(); ++i )
	{
		int nSame = 0;
		for( size_t k = 0; k < kept.size(); ++k )
			if( strcmp( kept[k].szDungeon, m_Records[i].szDungeon ) == 0 )++nSame;
		if( nSame < DUNGEON_MAX_RECORD )
			kept.push_back( m_Records[i] );
	}
	m_Records.swap( kept );
}

VOID CDungeonMgr::ClearRecords()
{
	m_Records.clear();
	SaveRecords( "data/dungeon_rank.dat" );
}

BOOL CDungeonMgr::LoadSeason( const char * pszFile )
{
	std::ifstream in( pszFile, std::ios::binary );
	if( !in.is_open() )return FALSE;
	in >> m_nSeason;
	if( m_nSeason < 1 )m_nSeason = 1;
	return TRUE;
}

BOOL CDungeonMgr::SaveSeason( const char * pszFile )
{
	std::ofstream out( pszFile, std::ios::binary | std::ios::trunc );
	if( !out.is_open() )return FALSE;
	out << m_nSeason << "\n";
	return TRUE;
}

int CDungeonMgr::GetSeason()
{
	return m_nSeason;
}

//	结算当前赛季：每个副本第一名发奖励 + 赛季冠军称号，然后清空排行
std::string CDungeonMgr::SettleSeason()
{
	std::string report;
	char szLine[256];

	for( int d = 0; d < (int)m_Defs.size(); ++d )
	{
		DUNGEONDEF & def = m_Defs[(size_t)d];

		DUNGEONRECORD * pBest = NULL;
		for( size_t i = 0; i < m_Records.size(); ++i )
			if( strcmp( m_Records[i].szDungeon, def.szName ) == 0 &&
				( pBest == NULL || m_Records[i].dwSeconds < pBest->dwSeconds ) )
				pBest = &m_Records[i];

		if( pBest == NULL )continue;

		snprintf( szLine, sizeof( szLine ), "%s 冠军：%s（%u 秒）\n",
			def.szName, pBest->szParty, (unsigned)pBest->dwSeconds );
		report += szLine;

		std::vector<std::string> members;
		CPartyMgr::GetInstance()->GetMembers( pBest->szParty, members );
		if( members.empty() )members.push_back( pBest->szParty );

		for( size_t i = 0; i < members.size(); ++i )
		{
			CHumanPlayer * pMember = CHumanPlayerMgr::GetInstance()->FindbyName( members[i].c_str() );
			if( pMember == NULL )continue;
			pMember->AddGold( 5000 );
			pMember->AddExp( 5000 );
			pMember->GrantTitle( 401 );
		}

		char szMsg[192];
		snprintf( szMsg, sizeof( szMsg ), "第 %d 赛季·%s 冠军：队伍【%s】，奖励 5000 金币/经验与称号【赛季冠军】",
			m_nSeason, def.szName, pBest->szParty );
		Game_Announce( szMsg );
	}

	m_nSeason += 1;
	SaveSeason( "data/season.dat" );
	ClearRecords();
	return report;
}
