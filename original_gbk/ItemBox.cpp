#include "StdAfx.h"
#include ".\itembox.h"

CItemBox::CItemBox(void)
{
	m_pCachedItem = NULL;
}

CItemBox::~CItemBox(void)
{
}

VOID	CItemBox::Clean()
{
	m_Items.Clean();
}

BOOL	CItemBox::Create( int size )
{
	return m_Items.Create( size );
}

ITEM * CItemBox::FindItem( DWORD dwMakeIndex )
{
	itembox_node * pnode = FindItemNode( dwMakeIndex );
	if( pnode == NULL )return NULL;
	return &pnode->item;
}

BOOL	CItemBox::RemoveItem( DWORD dwMakeIndex )
{
	if( m_pCachedItem != NULL && m_pCachedItem->item.dwMakeIndex == dwMakeIndex )
	{
		if( !m_Items.Del( m_pCachedItem->id ) )return FALSE;
		m_pCachedItem = NULL;
		return TRUE;
	}
	itembox_node * pnode = FindItemNode( dwMakeIndex );
	if( pnode == NULL )return FALSE;
	pnode = m_pCachedItem;
	m_pCachedItem = NULL;
	return m_Items.Del(pnode->id);
}

BOOL	CItemBox::AddItem( ITEM & item )
{
	itembox_node * pnode = FindItemNode( item.dwMakeIndex );
	if( pnode != NULL )return FALSE;
	UINT	id = m_Items.New( &pnode );
	if( id == 0 || pnode == NULL )return FALSE;
	pnode->id = id;
	pnode->item = item;
	return TRUE;
}

itembox_node * 	CItemBox::FindItemNode( DWORD dwMakeIndex )
{
	itembox_node * pnode = NULL;

	if( m_pCachedItem != NULL && m_pCachedItem->item.dwMakeIndex == dwMakeIndex )
		return m_pCachedItem;

	for( pnode = m_Items.First();pnode != NULL;pnode = m_Items.Next() )
	{
		if( pnode->item.dwMakeIndex == dwMakeIndex )
		{
			m_pCachedItem = pnode;
			return pnode;
		}
	}
	return NULL;
}

BOOL	CItemBox::SetItems( ITEM * pItems, int count )
{
	int i = 0;
	if( count > m_Items.GetFreeCount() )
		return FALSE;
	for( i = 0;i < count;i ++ )
	{
		AddItem( pItems[i] );
	}
	return TRUE;
}

int CItemBox::GetItems( ITEM * pItems, int count )
{
	int rcount = 0;

	itembox_node * pnode = NULL;

	for( pnode = m_Items.First();pnode != NULL;pnode = m_Items.Next() )
	{
		pItems[rcount++] = pnode->item;
		if( rcount >= count )
			return count;
	}
	return rcount;
}

