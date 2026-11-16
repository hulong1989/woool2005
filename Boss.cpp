// Boss.cpp  ——  Boss 刷新实现
#include "stdafx.h"
#include "Boss.h"
#include "Monster.h"
#include "HumanPlayer.h"
#include "HumanPlayerMgr.h"
#include "GameActions.h"
#include "Achievement.h"
#include <cstdio>
#include <fstream>

CBossMgr * CBossMgr::m_pInstance = NULL;

CBossMgr * CBossMgr::GetInstance()
{
	if( m_pInstance == NULL )
		m_pInstance = new CBossMgr;
	return m_pInstance;
}

BOOL CBossMgr::Load( const char * pszFile )
{
	m_Bosses.clear();

	CStringFile sf( pszFile );
	for( int i = 0; i < sf.GetLineCount(); ++i )
	{
		const char * pszLine = sf[i];
		if( pszLine == NULL || *pszLine == '\0' || *pszLine == '#' )continue;

		char	szBuffer[256];
		o_strncpy( szBuffer, pszLine, sizeof( szBuffer ) - 1 );
		char *	Params[8];
		//	怪物名/地图id/x/y/刷新秒数
		if( SearchParam( szBuffer, Params, 8, '/' ) < 5 )continue;

		BOSSENTRY entry;
		memset( &entry, 0, sizeof( entry ) );
		o_strncpy( entry.szMonster, Params[0], sizeof( entry.szMonster ) - 1 );
		entry.nMapId = (UINT)atoi( Params[1] );
		entry.x = (WORD)atoi( Params[2] );
		entry.y = (WORD)atoi( Params[3] );
		entry.dwInterval = (DWORD)atoi( Params[4] );
		if( entry.dwInterval < 10 )entry.dwInterval = 10;
		entry.dwNextTime = 0;		//	启动后立即刷第一只
		entry.bAlive = FALSE;
		m_Bosses.push_back( entry );
	}
	return TRUE;
}

VOID CBossMgr::Update()
{
	DWORD dwNow = timeGetTime();

	for( size_t i = 0; i < m_Bosses.size(); ++i )
	{
		BOSSENTRY & b = m_Bosses[i];
		if( b.bAlive )continue;
		if( b.dwInterval == 0 )continue;		//	世界 Boss 不自动重刷
		if( dwNow < b.dwNextTime )continue;

		CMonster * pMonster = CMonsterMgr::GetInstance()->Spawn( b.szMonster, b.nMapId, b.x, b.y );
		if( pMonster != NULL )
		{
			b.bAlive = TRUE;
			b.nMonsterId = pMonster->GetId();
		}
		else
		{
			b.dwNextTime = dwNow + 5000;		//	刷失败 5 秒后重试
		}
	}
}

VOID CBossMgr::OnMonsterDead( const char * pszMonster, UINT nMapId, UINT nMonsterId, CAliveObject * pAttacker )
{
	if( pszMonster == NULL )return;
	DWORD dwNow = timeGetTime();

	for( size_t i = 0; i < m_Bosses.size(); ++i )
	{
		BOSSENTRY & b = m_Bosses[i];
		if( b.nMapId != nMapId )continue;
		//	只认自己刷出来的那只，避免同名普通怪误触发
		if( b.nMonsterId != nMonsterId )continue;
		if( strcmp( b.szMonster, pszMonster ) != 0 )continue;
		b.bAlive = FALSE;
		b.nMonsterId = 0;
		b.dwNextTime = dwNow + b.dwInterval * 1000;

		//	首杀：全服公告 + 击杀者奖励
		char szKey[128];
		snprintf( szKey, sizeof( szKey ), "%u/%s", nMapId, pszMonster );
		BOOL bFirst = TRUE;
		for( size_t k = 0; k < m_FirstKills.size(); ++k )
			if( m_FirstKills[k] == szKey ){ bFirst = FALSE; break; }

		CHumanPlayer * pKiller = NULL;
		if( pAttacker != NULL && pAttacker->GetType() == OBJ_PLAYER )
			pKiller = CHumanPlayerMgr::GetInstance()->FindbyId( pAttacker->GetId() );

		if( bFirst )
		{
			m_FirstKills.push_back( szKey );
			SaveFirstKills( "data/boss_kills.dat" );

			if( pKiller != NULL )
			{
				pKiller->AddGold( 3000 );
				pKiller->AddExp( 3000 );
			}

			char szMsg[192];
			snprintf( szMsg, sizeof( szMsg ), "%s 首杀 %s（地图 %u），奖励 3000 金币 / 3000 经验",
				pKiller != NULL ? pKiller->GetName() : "某位勇士", pszMonster, nMapId );
			Game_Announce( szMsg );
		}

		//	击杀任意 Boss 都计入成就（首杀额外奖励）
		if( pKiller != NULL )
			CAchievementMgr::GetInstance()->OnBossKill( pKiller, pszMonster );
	}
}

BOOL CBossMgr::LoadFirstKills( const char * pszFile )
{
	m_FirstKills.clear();
	std::ifstream in( pszFile, std::ios::binary );
	if( !in.is_open() )return FALSE;
	std::string line;
	while( std::getline( in, line ) )
	{
		if( line.empty() || line[0] == '#' )continue;
		m_FirstKills.push_back( line );
	}
	return TRUE;
}

BOOL CBossMgr::SaveFirstKills( const char * pszFile )
{
	std::ofstream out( pszFile, std::ios::binary | std::ios::trunc );
	if( !out.is_open() )return FALSE;
	for( size_t i = 0; i < m_FirstKills.size(); ++i )
		out << m_FirstKills[i] << "\n";
	return TRUE;
}

BOOL CBossMgr::IsFirstKilled( const char * pszMonster, UINT nMapId )
{
	char szKey[128];
	snprintf( szKey, sizeof( szKey ), "%u/%s", nMapId, pszMonster );
	for( size_t i = 0; i < m_FirstKills.size(); ++i )
		if( m_FirstKills[i] == szKey )return TRUE;
	return FALSE;
}

int CBossMgr::GetFirstKillCount()
{
	return (int)m_FirstKills.size();
}

int CBossMgr::GetCount()
{
	return (int)m_Bosses.size();
}

BOSSENTRY * CBossMgr::GetByIndex( int nIndex )
{
	if( nIndex < 0 || (size_t)nIndex >= m_Bosses.size() )return NULL;
	return &m_Bosses[nIndex];
}

//	事件刷出的世界 Boss：登记后不自动重刷
BOOL CBossMgr::RegisterWorldBoss( const char * pszMonster, UINT nMapId, UINT nMonsterId )
{
	if( pszMonster == NULL )return FALSE;
	for( size_t i = 0; i < m_Bosses.size(); ++i )
	{
		if( m_Bosses[i].nMonsterId == nMonsterId && m_Bosses[i].bAlive )
			return TRUE;		//	已登记
	}

	BOSSENTRY entry;
	memset( &entry, 0, sizeof( entry ) );
	o_strncpy( entry.szMonster, pszMonster, sizeof( entry.szMonster ) - 1 );
	entry.nMapId = nMapId;
	entry.dwInterval = 0;		//	0 = 世界 Boss，不重刷
	entry.dwNextTime = 0;
	entry.nMonsterId = nMonsterId;
	entry.bAlive = TRUE;

	//	位置从怪物实例读取
	CMonster * pMonster = CMonsterMgr::GetInstance()->FindById( nMonsterId );
	if( pMonster != NULL )
	{
		entry.x = (WORD)pMonster->getX();
		entry.y = (WORD)pMonster->getY();
	}
	m_Bosses.push_back( entry );
	return TRUE;
}

//	清空某地图上的 Boss 存活标记（配合地图重置）
VOID CBossMgr::ClearMap( UINT nMapId )
{
	for( size_t i = 0; i < m_Bosses.size(); ++i )
	{
		BOSSENTRY & b = m_Bosses[i];
		if( b.nMapId != nMapId )continue;
		b.bAlive = FALSE;
		b.nMonsterId = 0;
		b.dwNextTime = 0;
	}
}

//	立即刷新某地图上的 Boss（副本进入时重置）
BOOL CBossMgr::ForceRespawn( UINT nMapId )
{
	for( size_t i = 0; i < m_Bosses.size(); ++i )
	{
		BOSSENTRY & b = m_Bosses[i];
		if( b.nMapId != nMapId )continue;
		if( b.dwInterval == 0 )continue;		//	世界 Boss 不重置
		if( b.bAlive )continue;

		CMonster * pMonster = CMonsterMgr::GetInstance()->Spawn( b.szMonster, b.nMapId, b.x, b.y );
		if( pMonster != NULL )
		{
			b.bAlive = TRUE;
			b.nMonsterId = pMonster->GetId();
			return TRUE;
		}
	}
	return FALSE;
}
