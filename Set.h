// Set.h  ——  装备套装
#pragma once

#include "public.h"
#include <vector>
#include <string>

struct SETBONUS
{
	int		nMinProp;		//	对应属性（最小）
	int		nMaxProp;		//	对应属性（最大）
	int		nValue;
};

struct SETDEF
{
	char					szName[32];
	std::vector<std::string>	items;
	int						nNeed;		//	触发件数
	std::vector<SETBONUS>	bonuses;
};

class CSetMgr
{
public:
	static CSetMgr * GetInstance();
	BOOL	Load( const char * pszFile );
	int		GetCount();
	SETDEF * GetByIndex( int nIndex );

private:
	static CSetMgr * m_pInstance;
	std::vector<SETDEF>	m_Sets;
};
