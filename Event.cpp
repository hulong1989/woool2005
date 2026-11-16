// Event.cpp
#include "stdafx.h"
#include "Event.h"
#include "Monster.h"
#include "Boss.h"
#include <time.h>

CEventMgr * CEventMgr::m_pInstance = NULL;

CEventMgr * CEventMgr::GetInstance()
{
	if( m_pInstance == NULL )
	{
		m_pInstance = new CEventMgr;
		m_pInstance->m_dwDoubleExpEnd = 0;
	}
	return m_pInstance;
}

static void Event_LocalTime( struct tm & lt )
{
	time_t t = time( NULL );
#if defined(_WIN32)
	lt = *localtime( &t );
#else
	localtime_r( &t, &lt );
#endif
}

BOOL CEventMgr::Load( const char * pszFile )
{
	m_Schedules.clear();

	CStringFile sf( pszFile );
	for( int i = 0; i < sf.GetLineCount(); ++i )
	{
		const char * pszLine = sf[i];
		if( pszLine == NULL || *pszLine == '\0' || *pszLine == '#' )continue;

		char	szBuffer[256];
		o_strncpy( szBuffer, pszLine, sizeof( szBuffer ) - 1 );
		char *	Params[10];
		int nParam = SearchParam( szBuffer, Params, 10, '/' );
		if( nParam < 4 )continue;

		EVENTSCHEDULE e;
		memset( &e, 0, sizeof( e ) );
		e.nLastDay = -1;

		if( strcmp( Params[0], "worldboss" ) == 0 )
		{
			//	worldboss/名称/开始/持续分钟/地图/x/y/怪物
			if( nParam < 8 )continue;
			e.nType = 1;
			o_strncpy( e.szName, Params[1], sizeof( e.szName ) - 1 );
			e.nDurationMin = atoi( Params[3] );
			e.nMapId = (UINT)atoi( Params[4] );
			e.x = (WORD)atoi( Params[5] );
			e.y = (WORD)atoi( Params[6] );
			o_strncpy( e.szBoss, Params[7], sizeof( e.szBoss ) - 1 );
			if( sscanf( Params[2], "%d:%d", &e.nHour, &e.nMinute ) != 2 )continue;
		}
		else
		{
			//	doubleexp/名称/开始/持续分钟
			e.nType = 0;
			o_strncpy( e.szName, Params[1], sizeof( e.szName ) - 1 );
			e.nDurationMin = atoi( Params[3] );
			if( sscanf( Params[2], "%d:%d", &e.nHour, &e.nMinute ) != 2 )continue;
		}

		if( e.nHour < 0 || e.nHour > 23 || e.nMinute < 0 || e.nMinute > 59 )continue;
		if( e.nDurationMin <= 0 )e.nDurationMin = 30;
		m_Schedules.push_back( e );
	}
	return !m_Schedules.empty();
}

VOID CEventMgr::StartDoubleExp( DWORD dwSeconds )
{
	if( dwSeconds < 10 )dwSeconds = 10;
	m_dwDoubleExpEnd = timeGetTime() + dwSeconds * 1000;
}

BOOL CEventMgr::IsDoubleExp()
{
	return m_dwDoubleExpEnd != 0 && timeGetTime() < m_dwDoubleExpEnd;
}

DWORD CEventMgr::RemainingSeconds()
{
	if( !IsDoubleExp() )return 0;
	return ( m_dwDoubleExpEnd - timeGetTime() ) / 1000;
}

BOOL CEventMgr::Trigger( int nIndex, std::string & msg )
{
	msg.clear();
	if( nIndex < 0 || (size_t)nIndex >= m_Schedules.size() )return FALSE;

	EVENTSCHEDULE & e = m_Schedules[(size_t)nIndex];
	struct tm lt;
	Event_LocalTime( lt );
	e.nLastDay = lt.tm_mday;

	if( e.nType == 1 )
	{
		CMonster * pMonster = CMonsterMgr::GetInstance()->Spawn( e.szBoss, e.nMapId, e.x, e.y );
		if( pMonster == NULL )
		{
			msg = "世界 Boss 刷新失败";
			return FALSE;
		}
		CBossMgr::GetInstance()->RegisterWorldBoss( e.szBoss, e.nMapId, pMonster->GetId() );

		char szMsg[160];
		snprintf( szMsg, sizeof( szMsg ), "世界 Boss【%s】降临 地图 %u (%u,%u)，持续 %d 分钟",
			e.szBoss, e.nMapId, e.x, e.y, e.nDurationMin );
		msg = szMsg;
	}
	else
	{
		StartDoubleExp( (DWORD)e.nDurationMin * 60 );
		char szMsg[96];
		snprintf( szMsg, sizeof( szMsg ), "双倍经验活动开始：%s（%d 分钟）", e.szName, e.nDurationMin );
		msg = szMsg;
	}
	return TRUE;
}

BOOL CEventMgr::Update( std::string & started, BOOL & expired )
{
	BOOL bChanged = FALSE;
	started.clear();
	expired = FALSE;

	if( m_dwDoubleExpEnd != 0 && timeGetTime() >= m_dwDoubleExpEnd )
	{
		m_dwDoubleExpEnd = 0;
		expired = TRUE;
		bChanged = TRUE;
	}

	struct tm lt;
	Event_LocalTime( lt );

	for( size_t i = 0; i < m_Schedules.size(); ++i )
	{
		EVENTSCHEDULE & e = m_Schedules[i];
		if( e.nHour != lt.tm_hour || e.nMinute != lt.tm_min )continue;
		if( e.nLastDay == lt.tm_mday )continue;

		std::string msg;
		if( Trigger( (int)i, msg ) )
		{
			started = msg;
			bChanged = TRUE;
		}
	}
	return bChanged;
}

int CEventMgr::GetScheduleCount()
{
	return (int)m_Schedules.size();
}

EVENTSCHEDULE * CEventMgr::GetScheduleByIndex( int nIndex )
{
	if( nIndex < 0 || (size_t)nIndex >= m_Schedules.size() )return NULL;
	return &m_Schedules[(size_t)nIndex];
}
