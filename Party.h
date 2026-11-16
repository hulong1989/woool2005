// Party.h  ——  组队（运行时状态，不落库）
#pragma once

#include "public.h"
#include <vector>
#include <string>

class CHumanPlayer;

class CPartyMgr
{
public:
	static CPartyMgr * GetInstance();

	CPartyMgr();

	//	a 邀请 b；a 未组队则先建队
	BOOL Invite( const char * pszInviter, const char * pszTarget );
	BOOL Leave( const char * pszName );
	BOOL IsInParty( const char * pszName );
	int  GetPartyId( const char * pszName );
	int  GetMembers( const char * pszName, std::vector<std::string> & members );
	//	组队经验分享：同地图 12 格内的队友获得 50% 经验
	VOID ShareExp( CHumanPlayer * pKiller, int nExp );
	//	队伍频道
	VOID Broadcast( CHumanPlayer * pSender, const char * pszText );

private:
	struct PARTY
	{
		int							id;
		std::vector<std::string>	members;
	};
	PARTY * FindParty( const char * pszName );

	static CPartyMgr * m_pInstance;
	std::vector<PARTY>	m_Parties;
	int					m_nNextId;
};
