// Achievement.cpp
#include "stdafx.h"
#include "Achievement.h"
#include "HumanPlayer.h"
#include "GameActions.h"

CAchievementMgr * CAchievementMgr::m_pInstance = NULL;

CAchievementMgr * CAchievementMgr::GetInstance()
{
	if( m_pInstance == NULL )
		m_pInstance = new CAchievementMgr;
	return m_pInstance;
}

BOOL CAchievementMgr::Load( const char * pszFile )
{
	m_Achievements.clear();

	CStringFile sf( pszFile );
	for( int i = 0; i < sf.GetLineCount(); ++i )
	{
		const char * pszLine = sf[i];
		if( pszLine == NULL || *pszLine == '\0' || *pszLine == '#' )continue;

		char	szBuffer[256];
		o_strncpy( szBuffer, pszLine, sizeof( szBuffer ) - 1 );
		char *	Params[8];
		//	id/名称/描述/类型/值/点数/加成
		int nParam = SearchParam( szBuffer, Params, 8, '/' );
		if( nParam < 5 )continue;

		ACHIEVEMENT a;
		a.id = 0;
		a.szName[0] = '\0';
		a.szDesc[0] = '\0';
		a.szType[0] = '\0';
		a.szValue[0] = '\0';
		a.nValue = 0;
		a.nPoints = 10;
		a.bonuses.clear();
		a.id = atoi( Params[0] );
		o_strncpy( a.szName, Params[1], sizeof( a.szName ) - 1 );
		o_strncpy( a.szDesc, Params[2], sizeof( a.szDesc ) - 1 );
		o_strncpy( a.szType, Params[3], sizeof( a.szType ) - 1 );
		o_strncpy( a.szValue, Params[4], sizeof( a.szValue ) - 1 );
		a.nValue = atoi( Params[4] );
		a.nPoints = ( nParam >= 6 ) ? atoi( Params[5] ) : 10;

		//	第 7 段：HP:20,DC:2,AC:1
		if( nParam >= 7 )
		{
			std::string bonusList = Params[6];
			size_t start = 0;
			while( start <= bonusList.size() )
			{
				size_t comma = bonusList.find( ',', start );
				std::string stat = bonusList.substr( start, comma == std::string::npos ? std::string::npos : comma - start );
				char szName[16] = { 0 };
				int nVal = 0;
				if( sscanf( stat.c_str(), "%15[^:]:%d", szName, &nVal ) == 2 )
				{
					STATBONUS b;
					b.nValue = nVal;
					if( strcmp( szName, "HP" ) == 0 ){ b.nIndex = PI_MAXHP; a.bonuses.push_back( b ); }
					else if( strcmp( szName, "MP" ) == 0 ){ b.nIndex = PI_MAXMP; a.bonuses.push_back( b ); }
					else if( strcmp( szName, "AC" ) == 0 )
					{
						b.nIndex = PI_MINAC; a.bonuses.push_back( b );
						b.nIndex = PI_MAXAC; a.bonuses.push_back( b );
					}
					else if( strcmp( szName, "MAC" ) == 0 )
					{
						b.nIndex = PI_MINMAC; a.bonuses.push_back( b );
						b.nIndex = PI_MAXMAC; a.bonuses.push_back( b );
					}
					else if( strcmp( szName, "DC" ) == 0 )
					{
						b.nIndex = PI_MINDC; a.bonuses.push_back( b );
						b.nIndex = PI_MAXDC; a.bonuses.push_back( b );
					}
					else if( strcmp( szName, "MC" ) == 0 )
					{
						b.nIndex = PI_MINMC; a.bonuses.push_back( b );
						b.nIndex = PI_MAXMC; a.bonuses.push_back( b );
					}
					else if( strcmp( szName, "SC" ) == 0 )
					{
						b.nIndex = PI_MINSC; a.bonuses.push_back( b );
						b.nIndex = PI_MAXSC; a.bonuses.push_back( b );
					}
				}
				if( comma == std::string::npos )break;
				start = comma + 1;
			}
		}

		m_Achievements.push_back( a );
	}
	return !m_Achievements.empty();
}

int CAchievementMgr::GetCount()
{
	return (int)m_Achievements.size();
}

ACHIEVEMENT * CAchievementMgr::GetByIndex( int nIndex )
{
	if( nIndex < 0 || (size_t)nIndex >= m_Achievements.size() )return NULL;
	return &m_Achievements[(size_t)nIndex];
}

ACHIEVEMENT * CAchievementMgr::Find( int nId )
{
	for( size_t i = 0; i < m_Achievements.size(); ++i )
		if( m_Achievements[i].id == nId )return &m_Achievements[i];
	return NULL;
}

VOID CAchievementMgr::UnlockType( CHumanPlayer * pPlayer, const char * pszType, const char * pszValue, int nNumber )
{
	if( pPlayer == NULL || pszType == NULL )return;

	for( size_t i = 0; i < m_Achievements.size(); ++i )
	{
		ACHIEVEMENT & a = m_Achievements[i];
		if( strcmp( a.szType, pszType ) != 0 )continue;

		BOOL bMatch = FALSE;
		if( pszValue != NULL && a.szValue[0] != '\0' && strcmp( a.szValue, pszValue ) == 0 )
			bMatch = TRUE;
		if( nNumber > 0 && a.nValue > 0 && nNumber >= a.nValue )
			bMatch = TRUE;

		if( !bMatch )continue;
		if( pPlayer->UnlockAchievement( a.id ) )
		{
			char szMsg[160];
			snprintf( szMsg, sizeof( szMsg ), "%s 获得成就【%s】：%s",
				pPlayer->GetName(), a.szName, a.szDesc );
			Game_Announce( szMsg );
		}
	}
}

VOID CAchievementMgr::CheckLevel( CHumanPlayer * pPlayer, int nLevel )
{
	UnlockType( pPlayer, "level", NULL, nLevel );
}

VOID CAchievementMgr::OnBossKill( CHumanPlayer * pPlayer, const char * pszBoss )
{
	UnlockType( pPlayer, "boss", pszBoss, 0 );
}

VOID CAchievementMgr::OnDungeonClear( CHumanPlayer * pPlayer, int nAchId )
{
	char szValue[16];
	snprintf( szValue, sizeof( szValue ), "%d", nAchId );
	UnlockType( pPlayer, "dungeon", szValue, 0 );
}

VOID CAchievementMgr::OnUpgrade( CHumanPlayer * pPlayer, int nLevel )
{
	UnlockType( pPlayer, "upgrade", NULL, nLevel );
}

VOID CAchievementMgr::OnEnchant( CHumanPlayer * pPlayer, int nPower )
{
	UnlockType( pPlayer, "enchant", NULL, nPower );
}

VOID CAchievementMgr::OnQuestCompleted( CHumanPlayer * pPlayer, int nCompleted )
{
	UnlockType( pPlayer, "quest", NULL, nCompleted );
}

VOID CAchievementMgr::CheckCodex( CHumanPlayer * pPlayer, int nCompleteSets )
{
	UnlockType( pPlayer, "codex", NULL, nCompleteSets );
}
