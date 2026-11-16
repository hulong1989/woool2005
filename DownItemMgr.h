#pragma once
#include "DownItemObject.h"
class CHumanPlayer;

class CDownItemMgr
{
public:
	CDownItemMgr(void);
	virtual ~CDownItemMgr(void);
	CDownItemObject * NewDownItemObject( ITEM & item, WORD x, WORD y, DWORD dwOwner = 0 );
	BOOL	DeleteDownItemObject( CDownItemObject * pObject );
	BOOL	DropItem( CVLogicMap * pMap, ITEM & item, WORD x, WORD y, DWORD dwOwner = 0 );
	BOOL	PickupItem( CVLogicMap * pMap, CDownItemObject * pObject, CHumanPlayer * pActionObject );



	static CDownItemMgr * GetInstance()
	{
		if( m_pInstance == NULL )
			m_pInstance = new CDownItemMgr;
		return m_pInstance;
	}
private:
	CIndexList<CDownItemObject, 20000>	m_DownItems;
	static CDownItemMgr * m_pInstance;
};
