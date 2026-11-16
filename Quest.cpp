// Quest.cpp  ——  任务表加载
#include "stdafx.h"
#include "Quest.h"

CQuestMgr * CQuestMgr::m_pInstance = NULL;

CQuestMgr * CQuestMgr::GetInstance()
{
	if( m_pInstance == NULL )
		m_pInstance = new CQuestMgr;
	return m_pInstance;
}

BOOL CQuestMgr::Load( const char * pszFile )
{
	m_Quests.clear();

	CStringFile sf( pszFile );
	for( int i = 0; i < sf.GetLineCount(); ++i )
	{
		const char * pszLine = sf[i];
		if( pszLine == NULL || *pszLine == '\0' || *pszLine == '#' )continue;

		char	szBuffer[512];
		o_strncpy( szBuffer, pszLine, sizeof( szBuffer ) - 1 );
		char *	Params[16];
		//	 id/名称/发布NPC/类型/目标/数量/经验/金币/奖励物品/数量/需求等级/对话
		int nParam = SearchParam( szBuffer, Params, 16, '/' );
		if( nParam < 11 )continue;

		QUESTTEMPLATE q;
		memset( &q, 0, sizeof( q ) );
		q.id = atoi( Params[0] );
		o_strncpy( q.szName, Params[1], sizeof( q.szName ) - 1 );
		o_strncpy( q.szNpc, Params[2], sizeof( q.szNpc ) - 1 );
		q.nType = atoi( Params[3] );
		o_strncpy( q.szTarget, Params[4], sizeof( q.szTarget ) - 1 );
		q.nCount = atoi( Params[5] );
		q.nExp = atoi( Params[6] );
		q.nGold = atoi( Params[7] );
		o_strncpy( q.szRewardItem, Params[8], sizeof( q.szRewardItem ) - 1 );
		q.nRewardCount = atoi( Params[9] );
		q.nNeedLevel = atoi( Params[10] );
		o_strncpy( q.szDialog, nParam >= 12 ? Params[11] : "", sizeof( q.szDialog ) - 1 );
		q.nPrevQuest = ( nParam >= 13 ) ? atoi( Params[12] ) : 0;
		q.bRepeatable = ( nParam >= 14 ) ? ( atoi( Params[13] ) != 0 ) : FALSE;
		if( q.nCount <= 0 )q.nCount = 1;
		m_Quests.push_back( q );
	}
	return !m_Quests.empty();
}

const QUESTTEMPLATE * CQuestMgr::Get( int nQuestId )
{
	for( size_t i = 0; i < m_Quests.size(); ++i )
		if( m_Quests[i].id == nQuestId )return &m_Quests[i];
	return NULL;
}

const QUESTTEMPLATE * CQuestMgr::FindByName( const char * pszName )
{
	if( pszName == NULL )return NULL;
	for( size_t i = 0; i < m_Quests.size(); ++i )
		if( strcmp( m_Quests[i].szName, pszName ) == 0 )return &m_Quests[i];
	return NULL;
}

const QUESTTEMPLATE * CQuestMgr::GetByIndex( int nIndex )
{
	if( nIndex < 0 || (size_t)nIndex >= m_Quests.size() )return NULL;
	return &m_Quests[(size_t)nIndex];
}

int CQuestMgr::GetCount()
{
	return (int)m_Quests.size();
}
