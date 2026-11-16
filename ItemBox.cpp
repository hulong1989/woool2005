#include "stdafx.h"
#include "ItemBox.h"
#include "ItemManager.h"

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
	itembox_node * pnode = FindItemNode( dwMakeIndex );
	if( pnode == NULL )return FALSE;

	//	堆叠物品一次只扣一个，扣完才删除槽位
	if( pnode->item.wCount > 1 )
	{
		pnode->item.wCount --;
		return TRUE;
	}

	m_pCachedItem = NULL;
	return m_Items.Del( pnode->id );
}

//	可堆叠：药品/技能书（有使用类型）
static BOOL ItemBox_IsStackable( const ITEM & item )
{
	return item.baseitem.btUseType != 0;
}

BOOL	CItemBox::AddItem( ITEM & item )
{
	//	重建时补充：编号为 0 的 ITEM 代表空槽，不允许进入背包
	if( item.dwMakeIndex == 0 )return FALSE;
	if( item.wCount == 0 )item.wCount = 1;

	int nRemaining = (int)item.wCount;

	//	先并入已有同类堆叠
	if( ItemBox_IsStackable( item ) )
	{
		for( itembox_node * pExist = m_Items.First(); pExist != NULL; pExist = m_Items.Next() )
		{
			if( !ItemBox_IsStackable( pExist->item ) )continue;
			if( pExist->item.baseitem.btUseType != item.baseitem.btUseType )continue;
			if( strcmp( pExist->item.baseitem.szName, item.baseitem.szName ) != 0 )continue;

			int nSpace = ITEM_MAX_STACK - (int)pExist->item.wCount;
			if( nSpace <= 0 )continue;

			int nAdd = nRemaining < nSpace ? nRemaining : nSpace;
			pExist->item.wCount = (WORD)( pExist->item.wCount + nAdd );
			nRemaining -= nAdd;
			if( nRemaining <= 0 )return TRUE;
		}
	}

	if( nRemaining > ITEM_MAX_STACK )nRemaining = ITEM_MAX_STACK;

	itembox_node * pnode = FindItemNode( item.dwMakeIndex );
	if( pnode != NULL )return FALSE;		//	同一实例不能重复入包

	UINT	id = m_Items.New( &pnode );
	if( id == 0 || pnode == NULL )return FALSE;
	pnode->id = id;
	pnode->item = item;
	pnode->item.wCount = (WORD)nRemaining;
	//	多出来的堆叠需要新的实例编号
	if( nRemaining != (int)item.wCount && ItemBox_IsStackable( item ) )
		pnode->item.dwMakeIndex = CItemManager::NewMakeIndex();
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

//	背包总重量（原快照的 ItemBox.cpp 缺少该实现，按 GetPropValue 的用法补齐）
int CItemBox::CalcWeight()
{
	int weight = 0;
	for( itembox_node * pnode = m_Items.First();pnode != NULL;pnode = m_Items.Next() )
	{
		weight += pnode->item.baseitem.btWeight * (int)pnode->item.wCount;
	}
	return weight;
}


//	按名字统计数量（含堆叠）
int CItemBox::CountItemByName( const char * pszName )
{
	if( pszName == NULL )return 0;
	int nTotal = 0;
	for( itembox_node * pnode = m_Items.First(); pnode != NULL; pnode = m_Items.Next() )
	{
		if( strcmp( pnode->item.baseitem.szName, pszName ) == 0 )
			nTotal += (int)pnode->item.wCount;
	}
	return nTotal;
}

//	按名字扣除数量，返回实际扣除数
int CItemBox::RemoveItemsByName( const char * pszName, int nCount )
{
	if( pszName == NULL || nCount <= 0 )return 0;

	int nRemoved = 0;
	while( nRemoved < nCount )
	{
		itembox_node * pFound = NULL;
		for( itembox_node * pnode = m_Items.First(); pnode != NULL; pnode = m_Items.Next() )
		{
			if( strcmp( pnode->item.baseitem.szName, pszName ) == 0 )
			{
				pFound = pnode;
				break;
			}
		}
		if( pFound == NULL )break;

		if( (int)pFound->item.wCount <= ( nCount - nRemoved ) )
		{
			nRemoved += (int)pFound->item.wCount;
			m_Items.Del( pFound->id );
			m_pCachedItem = NULL;
		}
		else
		{
			pFound->item.wCount = (WORD)( pFound->item.wCount - ( nCount - nRemoved ) );
			nRemoved = nCount;
		}
	}
	return nRemoved;
}

//	从堆叠中拆出 nCount 个到新格
BOOL CItemBox::SplitItem( DWORD dwMakeIndex, int nCount )
{
	itembox_node * pnode = FindItemNode( dwMakeIndex );
	if( pnode == NULL || nCount <= 0 || nCount >= (int)pnode->item.wCount )
		return FALSE;

	itembox_node * pNew = NULL;
	UINT id = m_Items.New( &pNew );
	if( id == 0 || pNew == NULL )return FALSE;

	pNew->id = id;
	pNew->item = pnode->item;
	pNew->item.wCount = (WORD)nCount;
	pNew->item.dwMakeIndex = CItemManager::NewMakeIndex();
	pnode->item.wCount = (WORD)( pnode->item.wCount - nCount );
	return TRUE;
}

//	按名字删除全部，返回删除数量
int CItemBox::RemoveAllByName( const char * pszName )
{
	if( pszName == NULL )return 0;
	int nRemoved = 0;
	for( ;; )
	{
		itembox_node * pFound = NULL;
		for( itembox_node * pnode = m_Items.First(); pnode != NULL; pnode = m_Items.Next() )
		{
			if( strcmp( pnode->item.baseitem.szName, pszName ) == 0 )
			{
				pFound = pnode;
				break;
			}
		}
		if( pFound == NULL )break;
		nRemoved += (int)pFound->item.wCount;
		m_Items.Del( pFound->id );
		m_pCachedItem = NULL;
	}
	return nRemoved;
}

//	取出整个堆叠（返回物品并删除该格）
BOOL CItemBox::RemoveItemStack( DWORD dwMakeIndex, ITEM & outItem )
{
	itembox_node * pnode = FindItemNode( dwMakeIndex );
	if( pnode == NULL )return FALSE;
	outItem = pnode->item;
	m_pCachedItem = NULL;
	return m_Items.Del( pnode->id );
}
