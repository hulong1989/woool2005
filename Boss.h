// Boss.h  ——  Boss 定时刷新
#pragma once

#include "public.h"
#include <vector>

class CAliveObject;

struct BOSSENTRY
{
	char	szMonster[32];
	UINT	nMapId;
	WORD	x;
	WORD	y;
	DWORD	dwInterval;		//	秒
	DWORD	dwNextTime;		//	毫秒时间戳
	UINT	nMonsterId;		//	当前存活实例 id
	BOOL	bAlive;
};

class CBossMgr
{
public:
	static CBossMgr * GetInstance();

	BOOL	Load( const char * pszFile );
	VOID	Update();
	VOID	OnMonsterDead( const char * pszMonster, UINT nMapId, UINT nMonsterId, CAliveObject * pAttacker );

	//	首杀
	BOOL	LoadFirstKills( const char * pszFile );
	BOOL	SaveFirstKills( const char * pszFile );
	BOOL	IsFirstKilled( const char * pszMonster, UINT nMapId );
	BOOL	RegisterWorldBoss( const char * pszMonster, UINT nMapId, UINT nMonsterId );
	BOOL	ForceRespawn( UINT nMapId );
	VOID	ClearMap( UINT nMapId );
	int		GetFirstKillCount();
	int		GetCount();
	BOSSENTRY * GetByIndex( int nIndex );

private:
	static CBossMgr * m_pInstance;
	std::vector<BOSSENTRY>	m_Bosses;
	std::vector<std::string>	m_FirstKills;
};
