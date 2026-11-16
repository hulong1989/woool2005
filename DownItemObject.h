#pragma once
#include "MapObject.h"

class CDownItemObject :
	public CMapObject
{
public:
	CDownItemObject(void);
	virtual ~CDownItemObject(void);
	BOOL	GetViewmsg( char * pszMsg, int & length );
	BOOL	GetOutViewmsg( char * pszMsg, int & length );
	ITEM & GetItem(){return m_Item;}
	VOID SetItem( ITEM & item ){ m_Item = item;}
	UINT	GetId(){ return m_Id;}
	VOID SetId( UINT id ){ m_Id = id;}
	VOID	Clean();
	BOOL	CanRecvMsg(){ return FALSE;}
	UINT	GetOwnerId(){ return m_OwnerId;}
	VOID	SetOwnerId( UINT id ){ m_OwnerId = id;}
	e_object_type GetType(){ return OBJ_DOWNITEM;}
private:
	UINT	m_OwnerId;
	ITEM	m_Item;
	UINT	m_Id;
};
