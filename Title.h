// Title.h  ——  成就点数兑换的称号
#pragma once

#include "public.h"
#include <vector>
#include <string>

struct TITLEBONUS
{
	int		nIndex;
	int		nValue;
};

struct TITLEDEF
{
	int		id;
	char	szName[32];
	int		nCost;		//	成就点数
	std::vector<TITLEBONUS>	bonuses;
};

class CTitleMgr
{
public:
	static CTitleMgr * GetInstance();
	BOOL	Load( const char * pszFile );
	int		GetCount();
	TITLEDEF * GetByIndex( int nIndex );
	TITLEDEF * Find( int nId );

private:
	static CTitleMgr * m_pInstance;
	std::vector<TITLEDEF>	m_Titles;
};
