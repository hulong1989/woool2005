#pragma once
#include "humanplayer.h"
#define MAX_HUMANPLAYER	128





class CHumanPlayerMgr
{
public:
	CHumanPlayerMgr(void);
	virtual ~CHumanPlayerMgr(void);


	CHumanPlayer * FindbyName( const char * pszName );
	CHumanPlayer * FindbyId( UINT id );

	CHumanPlayer * NewPlayer( CREATEHUMANDESC & desc );
	BOOL DeletePlayer( CHumanPlayer * pPlayer );



	static CHumanPlayerMgr * GetInstance()
	{
		if( m_pInstance == NULL )
			m_pInstance = new CHumanPlayerMgr;
		return m_pInstance;
	}
private:
	static CHumanPlayerMgr * m_pInstance;
	CIndexList<CHumanPlayer, MAX_HUMANPLAYER>	m_HumanPlayers;
	CNameHash m_PlayerNameHash;
};
