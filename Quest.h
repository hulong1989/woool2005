// Quest.h  ——  任务模板与玩家任务状态
#pragma once

#include "public.h"
#include <vector>

//	类型：1 杀怪 / 2 收集
struct QUESTTEMPLATE
{
	int		id;
	char	szName[32];
	char	szNpc[32];
	int		nType;
	char	szTarget[32];
	int		nCount;
	int		nExp;
	int		nGold;
	char	szRewardItem[32];
	int		nRewardCount;
	int		nNeedLevel;
	char	szDialog[128];
	int		nPrevQuest;		//	前置任务 id（0 无）
	BOOL	bRepeatable;	//	可重复接取（日常）
};

//	玩家任务状态：1 进行中 / 2 已完成
#define QUEST_MAX_PER_PLAYER	16
struct PLAYERQUEST
{
	int		nId;
	int		nProgress;
	int		nState;
};

class CQuestMgr
{
public:
	static CQuestMgr * GetInstance();

	BOOL	Load( const char * pszFile );
	const QUESTTEMPLATE * Get( int nQuestId );
	const QUESTTEMPLATE * FindByName( const char * pszName );
	const QUESTTEMPLATE * GetByIndex( int nIndex );
	int		GetCount();

private:
	static CQuestMgr * m_pInstance;
	std::vector<QUESTTEMPLATE>	m_Quests;
};
