#include "stdafx.h"
#include "DownItemMgr.h"
#include "LogicMap.h"
#include "HumanPlayer.h"
CDownItemMgr * CDownItemMgr::m_pInstance = NULL;
CDownItemMgr::CDownItemMgr(void)
{
	if( m_pInstance == NULL)
		m_pInstance = this;
}

CDownItemMgr::~CDownItemMgr(void)
{
}

CDownItemObject * CDownItemMgr::NewDownItemObject( ITEM & item, WORD x, WORD y, DWORD dwOwner )
{
	CDownItemObject * pDownItem = NULL;
	UINT id = m_DownItems.New( &pDownItem );
	if( id == 0 || pDownItem == NULL )return NULL;

	pDownItem->Clean();

	pDownItem->SetId( id );
	pDownItem->SetItem( item );
	pDownItem->setXY( x, y );
	pDownItem->SetOwnerId( dwOwner );
	return pDownItem;
}

BOOL	CDownItemMgr::DeleteDownItemObject( CDownItemObject * pObject )
{
	if( pObject == NULL )return FALSE;
	return m_DownItems.Del( pObject->GetId() );
}

BOOL	CDownItemMgr::DropItem( CVLogicMap * pMap, ITEM & item, WORD x, WORD y, DWORD dwOwner )
{
	CDownItemObject * pDownItem = NewDownItemObject( item, x, y, dwOwner );
	if( pDownItem == NULL )return FALSE;
	if( !pMap->AddObject( pDownItem ) )
	{
		DeleteDownItemObject( pDownItem );
		return FALSE;
	}

	//	fixme:update item pos

	return TRUE;
}

BOOL	CDownItemMgr::PickupItem( CVLogicMap * pMap, CDownItemObject * pObject, CHumanPlayer * pActionObject )
{
	if( !pMap->RemoveObject(pObject))
		return FALSE;
	if( !pActionObject->AddBagItem( pObject->GetItem() ))
	{
		if( !pMap->AddObject(pObject))
		{
			//	fixme: destroy item
			DeleteDownItemObject(pObject);
		}
		return FALSE;
	}
	DeleteDownItemObject(pObject);
	//	fixme:update item pos
	return TRUE;
}
