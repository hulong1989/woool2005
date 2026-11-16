// ClassTable.h  ——  职业成长与等级经验表
#pragma once

#include "public.h"
#include <vector>

struct PLAYERCLASS
{
	int		id;
	char	szName[16];
	int		hpGrow;
	int		mpGrow;
	int		minDcGrow;
	int		maxDcGrow;
	int		minAcGrow;
	int		maxAcGrow;
	int		mcGrow;
	int		scGrow;
	int		weightGrow;
};

class CPlayerClassMgr
{
public:
	static CPlayerClassMgr * GetInstance();

	//	pszClassFile: data/classes.txt ; pszLevelFile: data/levels.txt
	BOOL Load( const char * pszClassFile, const char * pszLevelFile );

	const PLAYERCLASS * Get( int nClassId );
	int		GetCount();
	int		MaxLevel();
	DWORD	LevelExpNeed( int nLevel );		//	从 nLevel 升到 nLevel+1 所需经验
	const char * GetClassName( int nClassId );

private:
	static CPlayerClassMgr * m_pInstance;
	std::vector<PLAYERCLASS>	m_Classes;
	std::vector<DWORD>			m_LevelNeed;	//	下标 = 等级
};
