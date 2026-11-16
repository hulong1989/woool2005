#include "stdafx.h"
#include "HumanPlayerMgr.h"
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
	//	重建时补充：原实现没有把角色登记到名字表，FindbyName 永远查不到
	m_PlayerNameHash.HAdd( pPlayer->GetName(), pPlayer );
	return pPlayer;
}

BOOL CHumanPlayerMgr::DeletePlayer( CHumanPlayer * pPlayer )
{
	if( pPlayer == NULL )return FALSE;
	m_PlayerNameHash.HDel( pPlayer->GetName() );
	UINT	id = (pPlayer->GetId() & 0xffffff);
	return m_HumanPlayers.Del(id);
}
