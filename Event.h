// Event.h  ——  全服活动（双倍经验 / 世界 Boss）与每日排期
#pragma once

#include "public.h"
#include <vector>
#include <string>

struct EVENTSCHEDULE
{
	int		nType;			//	0 双倍经验 / 1 世界 Boss
	char	szName[32];
	int		nHour;
	int		nMinute;
	int		nDurationMin;
	int		nLastDay;

	//	世界 Boss
	UINT	nMapId;
	WORD	x;
	WORD	y;
	char	szBoss[32];
};

class CEventMgr
{
public:
	static CEventMgr * GetInstance();

	BOOL	Load( const char * pszFile );
	VOID	StartDoubleExp( DWORD dwSeconds );
	BOOL	IsDoubleExp();
	DWORD	RemainingSeconds();

	BOOL	Trigger( int nIndex, std::string & msg );
	BOOL	Update( std::string & started, BOOL & expired );

	int		GetScheduleCount();
	EVENTSCHEDULE * GetScheduleByIndex( int nIndex );

private:
	static CEventMgr * m_pInstance;
	DWORD	m_dwDoubleExpEnd;
	std::vector<EVENTSCHEDULE>	m_Schedules;
};
