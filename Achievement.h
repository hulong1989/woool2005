// Achievement.h  ——  成就与称号
#pragma once

#include "public.h"
#include <vector>

class CHumanPlayer;

struct STATBONUS
{
	int		nIndex;
	int		nValue;
	int		nPoints;		//	成就点数
	std::vector<STATBONUS>	bonuses;	//	佩戴称号时的属性加成
};

struct ACHIEVEMENT
{
	int		id;
	char	szName[32];
	char	szDesc[64];
	char	szType[16];		//	level / boss / dungeon / upgrade / enchant / quest
	char	szValue[32];
	int		nValue;
	int		nPoints;		//	成就点数
	std::vector<STATBONUS>	bonuses;	//	佩戴称号时的属性加成
};

class CAchievementMgr
{
public:
	static CAchievementMgr * GetInstance();

	BOOL	Load( const char * pszFile );
	int		GetCount();
	ACHIEVEMENT * GetByIndex( int nIndex );
	ACHIEVEMENT * Find( int nId );

	VOID	CheckLevel( CHumanPlayer * pPlayer, int nLevel );
	VOID	OnBossKill( CHumanPlayer * pPlayer, const char * pszBoss );
	VOID	OnDungeonClear( CHumanPlayer * pPlayer, int nAchId );
	VOID	OnUpgrade( CHumanPlayer * pPlayer, int nLevel );
	VOID	OnEnchant( CHumanPlayer * pPlayer, int nPower );
	VOID	OnQuestCompleted( CHumanPlayer * pPlayer, int nCompleted );
	VOID	CheckCodex( CHumanPlayer * pPlayer, int nCompleteSets );

private:
	VOID	UnlockType( CHumanPlayer * pPlayer, const char * pszType, const char * pszValue, int nNumber );
	static CAchievementMgr * m_pInstance;
	std::vector<ACHIEVEMENT>	m_Achievements;
};
