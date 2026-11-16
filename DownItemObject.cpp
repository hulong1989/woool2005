#include "stdafx.h"
#include "DownItemObject.h"

CDownItemObject::CDownItemObject(void):m_Id(0)
{
}

CDownItemObject::~CDownItemObject(void)
{
}

VOID	CDownItemObject::Clean()
{
	m_Id = 0;
	memset( &m_Item, 0, sizeof( ITEM ));
}

BOOL	CDownItemObject::GetViewmsg( char * pszMsg, int & length )
{
	length = EncodeMsg( pszMsg, m_Item.dwMakeIndex, SM_DOWNITEMAPPEAR, 
									m_wX, m_wY, m_Item.baseitem.wImageIndex, 
									m_Item.baseitem.szName, m_Item.baseitem.btNameLength );
	return TRUE;
}

BOOL	CDownItemObject::GetOutViewmsg( char * pszMsg, int & length )
{
	length = EncodeMsg( pszMsg, m_Item.dwMakeIndex, SM_DOWNITEMDISAPPEAR, m_wX, m_wY, 0, NULL );
	return TRUE;
}
