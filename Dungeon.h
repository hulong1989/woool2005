// Dungeon.h  ——  多副本定义 + 并行实例
#pragma once

#include "public.h"
#include <string>
#include <vector>

class CHumanPlayer;
class CNpc;

#define DUNGEON_DURATION	1800
#define DUNGEON_MAX_RECORD	5

struct DUNGEONDEF
{
	int						id;
	char					szName[32];
	char					szBoss[32];
	std::vector<UINT>		maps;		//	每个元素是一个实例地图
	int						nAchId;		//	通关成就 id
	int						nTitleId;	//	首通称号 id
};

struct DUNGEONRECORD
{
	char	szDungeon[32];
	char	szParty[32];
	DWORD	dwSeconds;
	char	szDate[16];
};

struct DUNGEONSLOT
{
	int			nDef;			//	所属副本定义下标
	UINT		nMapId;
	int			nPartyId;
	std::string	strPartyName;
	DWORD		dwStartTime;
	DWORD		dwEndTime;
};

class CDungeonMgr
{
public:
	static CDungeonMgr * GetInstance();

	BOOL	Load( const char * pszFile );
	int		GetDefCount();
	DUNGEONDEF * GetDefByIndex( int nIndex );
	DUNGEONDEF * GetDefById( int nId );

	std::string Enter( CHumanPlayer * pPlayer, CNpc * pNpc );
	VOID	OnMonsterDead( const char * pszMonster, UINT nMapId );
	VOID	Update();

	BOOL	LoadFirstClears( const char * pszFile );
	BOOL	SaveFirstClears( const char * pszFile );
	int		GetFirstClearCount();
	BOOL	IsFirstCleared( int nId );

	BOOL	LoadRecords( const char * pszFile );
	BOOL	SaveRecords( const char * pszFile );
	int		GetRecordCount();
	VOID	TrimRecords();
	VOID	ClearRecords();

	BOOL	LoadSeason( const char * pszFile );
	BOOL	SaveSeason( const char * pszFile );
	int		GetSeason();
	std::string SettleSeason();
	DUNGEONRECORD * GetRecordByIndex( int nIndex );

	int		GetSlotCount();
	DUNGEONSLOT * GetSlotByIndex( int nIndex );

private:
	static CDungeonMgr * m_pInstance;
	std::vector<DUNGEONDEF>		m_Defs;
	std::vector<DUNGEONSLOT>	m_Slots;
	std::vector<DUNGEONRECORD>	m_Records;
	std::vector<int>			m_FirstClears;
	int							m_nLastFirstDay;
	int							m_nSeason;
};
