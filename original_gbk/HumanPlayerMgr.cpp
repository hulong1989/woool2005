#include "StdAfx.h"
#include ".\humanplayermgr.h"
CHumanPlayerMgr * CHumanPlayerMgr::m_pInstance = NULL;

CHumanPlayerMgr::CHumanPlayerMgr(void)
{
	if( m_pInstance == NULL )
		m_pInstance = this;
}

CHumanPlayerMgr::~CHumanPlayerMgr(void)
{

}

CHumanPlayer * CHumanPlayerMgr::FindbyName( const char * pszName )
{
	return (CHumanPlayer*)m_PlayerNameHash.HGet(pszName);
}

CHumanPlayer * CHumanPlayerMgr::FindbyId( UINT id )
{
	if( ((id & 0xff000000)>>24) == OBJ_PLAYER )
		id &= 0xffffff;
	return m_HumanPlayers.Get(id);
}

CHumanPlayer * CHumanPlayerMgr::NewPlayer( CREATEHUMANDESC & desc )
{
	CHumanPlayer * pPlayer = NULL;
	UINT id = 0;
	id = m_HumanPlayers.New(&pPlayer);
	if( id == 0 || pPlayer == NULL )
		return NULL;
	if( !pPlayer->Init( desc ) )
	{
		m_HumanPlayers.Del( id );
		return NULL;
	}
	id |= (OBJ_PLAYER << 24);
	pPlayer->SetId( id );
	return pPlayer;
}

BOOL CHumanPlayerMgr::DeletePlayer( CHumanPlayer * pPlayer )
{
	UINT	id = (pPlayer->GetId() & 0xffffff);
	return m_HumanPlayers.Del(id);
}
