// Party.cpp  ——  组队实现
#include "stdafx.h"
#include "Party.h"
#include "HumanPlayer.h"
#include "HumanPlayerMgr.h"

CPartyMgr * CPartyMgr::m_pInstance = NULL;

CPartyMgr * CPartyMgr::GetInstance()
{
	if( m_pInstance == NULL )
		m_pInstance = new CPartyMgr;
	return m_pInstance;
}

CPartyMgr::CPartyMgr() : m_nNextId( 1 )
{
}

CPartyMgr::PARTY * CPartyMgr::FindParty( const char * pszName )
{
	if( pszName == NULL )return NULL;
	for( size_t i = 0; i < m_Parties.size(); ++i )
	{
		for( size_t j = 0; j < m_Parties[i].members.size(); ++j )
			if( m_Parties[i].members[j] == pszName )
				return &m_Parties[i];
	}
	return NULL;
}

int CPartyMgr::GetPartyId( const char * pszName )
{
	PARTY * p = FindParty( pszName );
	return p != NULL ? p->id : 0;
}

BOOL CPartyMgr::IsInParty( const char * pszName )
{
	return FindParty( pszName ) != NULL;
}

BOOL CPartyMgr::Invite( const char * pszInviter, const char * pszTarget )
{
	if( pszInviter == NULL || pszTarget == NULL )return FALSE;

	CHumanPlayer * pInviter = CHumanPlayerMgr::GetInstance()->FindbyName( pszInviter );
	CHumanPlayer * pTarget = CHumanPlayerMgr::GetInstance()->FindbyName( pszTarget );
	if( pInviter == NULL || pTarget == NULL )return FALSE;
	if( FindParty( pszTarget ) != NULL )return FALSE;		//	对方已有队伍

	PARTY * pParty = FindParty( pszInviter );
	if( pParty == NULL )
	{
		PARTY party;
		party.id = m_nNextId++;
		party.members.push_back( pszInviter );
		m_Parties.push_back( party );
		pParty = &m_Parties.back();
	}
	if( pParty->members.size() >= 5 )return FALSE;			//	队伍已满

	pParty->members.push_back( pszTarget );

	if( pTarget->CanRecvMsg() )
	{
		char szMsg[128];
		snprintf( szMsg, sizeof( szMsg ), "你加入了 %s 的队伍。", pszInviter );
		pTarget->SendMsg( pTarget->GetId(), SM_CHAT, 0xff00, 0, 0, (LPVOID)szMsg );
	}
	return TRUE;
}

BOOL CPartyMgr::Leave( const char * pszName )
{
	if( pszName == NULL )return FALSE;
	for( size_t i = 0; i < m_Parties.size(); ++i )
	{
		std::vector<std::string> & members = m_Parties[i].members;
		for( size_t j = 0; j < members.size(); ++j )
		{
			if( members[j] != pszName )continue;
			members.erase( members.begin() + j );
			if( members.empty() )
				m_Parties.erase( m_Parties.begin() + i );
			return TRUE;
		}
	}
	return FALSE;
}

int CPartyMgr::GetMembers( const char * pszName, std::vector<std::string> & members )
{
	members.clear();
	PARTY * pParty = FindParty( pszName );
	if( pParty == NULL )return 0;
	members = pParty->members;
	return (int)members.size();
}

VOID CPartyMgr::ShareExp( CHumanPlayer * pKiller, int nExp )
{
	if( pKiller == NULL || nExp <= 0 )return;
	PARTY * pParty = FindParty( pKiller->GetName() );
	if( pParty == NULL )return;
	if( pParty->members.size() <= 1 )return;

	int nShare = nExp / 2;		//	队友获得 50%，击杀者已拿全额
	for( size_t i = 0; i < pParty->members.size(); ++i )
	{
		if( pParty->members[i] == pKiller->GetName() )continue;
		CHumanPlayer * pMember = CHumanPlayerMgr::GetInstance()->FindbyName( pParty->members[i].c_str() );
		if( pMember == NULL )continue;
		if( pMember->GetMap() != pKiller->GetMap() )continue;

		int dx = (int)pMember->getX() - (int)pKiller->getX();
		int dy = (int)pMember->getY() - (int)pKiller->getY();
		if( dx < 0 )dx = -dx;
		if( dy < 0 )dy = -dy;
		if( dx > 12 || dy > 12 )continue;

		pMember->AddExp( nShare );
	}
}

VOID CPartyMgr::Broadcast( CHumanPlayer * pSender, const char * pszText )
{
	if( pSender == NULL || pszText == NULL )return;
	PARTY * pParty = FindParty( pSender->GetName() );
	if( pParty == NULL )return;

	char szMsg[256];
	snprintf( szMsg, sizeof( szMsg ), "【队伍】%s: %s", pSender->GetName(), pszText );

	for( size_t i = 0; i < pParty->members.size(); ++i )
	{
		CHumanPlayer * pMember = CHumanPlayerMgr::GetInstance()->FindbyName( pParty->members[i].c_str() );
		if( pMember == NULL )continue;
		if( pMember->CanRecvMsg() )
			pMember->SendMsg( pMember->GetId(), SM_CHAT, 0xff00, 0, 0, (LPVOID)szMsg );
	}
}
