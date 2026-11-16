// Guild.h  ——  行会（持久化到 data/guilds.dat）
#pragma once

#include "public.h"
#include <vector>
#include <string>

class CGuildMgr
{
public:
	struct GUILD
	{
		std::string					name;
		std::string					leader;
		std::string					vice;		//	副会长
		int							level;		//	行会等级 1-5
		long long					funds;		//	行会资金
		std::vector<std::string>	members;
		std::vector<ITEM>			warehouse;	//	行会仓库（最多 50 格）
	};

	static CGuildMgr * GetInstance();

	BOOL	Load( const char * pszFile );
	BOOL	Save( const char * pszFile );

	BOOL	Create( const char * pszGuild, const char * pszLeader );
	BOOL	Join( const char * pszGuild, const char * pszPlayer );
	BOOL	Leave( const char * pszPlayer );

	//	行会成长与权限
	BOOL	Donate( const char * pszPlayer, int nGold, int & nNewLevel );
	BOOL	Kick( const char * pszLeader, const char * pszMember );
	BOOL	Appoint( const char * pszLeader, const char * pszMember );
	static int LevelForFunds( long long funds );
	static int MemberCap( int nLevel );
	static int ExpBonusPercent( int nLevel );

	//	行会仓库
	BOOL	SaveWarehouse( const char * pszFile );
	BOOL	LoadWarehouse( const char * pszFile );
	BOOL	AddToWarehouse( const char * pszPlayer, ITEM & item );
	BOOL	TakeFromWarehouse( const char * pszPlayer, int nIndex, ITEM & item );

	GUILD * Find( const char * pszGuild );
	GUILD * FindByMember( const char * pszPlayer );
	GUILD * GetByIndex( int nIndex );
	int		GetCount();

	VOID	Broadcast( const char * pszGuild, const char * pszSender, const char * pszText );

private:
	static CGuildMgr * m_pInstance;
	std::vector<GUILD>	m_Guilds;
};
