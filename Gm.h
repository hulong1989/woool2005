// Gm.h  ——  GM 账号与权限
#pragma once

#include "public.h"
#include <vector>
#include <string>

struct GMACCOUNT
{
	std::string	name;
	int			level;		//	1 基础 / 2 进阶 / 3 管理
};

class CGmMgr
{
public:
	static CGmMgr * GetInstance();

	BOOL	Load( const char * pszFile );
	BOOL	Save( const char * pszFile );
	BOOL	IsGm( const char * pszName );
	int		GetLevel( const char * pszName );
	BOOL	Add( const char * pszName, int nLevel );
	BOOL	Remove( const char * pszName );
	int		GetCount();
	GMACCOUNT * GetByIndex( int nIndex );

private:
	static CGmMgr * m_pInstance;
	std::vector<GMACCOUNT>	m_Accounts;
};
