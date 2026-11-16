// ItemBox.h  ——  重建（原快照缺失）
#pragma once

#include "public.h"

#define ITEM_MAX_STACK	99

struct itembox_node
{
	itembox_node() : id(0)
	{
		memset( &item, 0, sizeof( item ) );
	}
	UINT	id;
	ITEM	item;
};

//	背包 / 物品容器
class CItemBox
{
public:
	CItemBox(void);
	virtual ~CItemBox(void);

	VOID	Clean();
	BOOL	Create( int size );

	BOOL	AddItem( ITEM & item );
	BOOL	RemoveItem( DWORD dwMakeIndex );
	ITEM *	FindItem( DWORD dwMakeIndex );
	itembox_node * FindItemNode( DWORD dwMakeIndex );

	BOOL	SetItems( ITEM * pItems, int count );
	int		GetItems( ITEM * pItems, int count );

	int		CalcWeight();
	int		GetFreeCount(){ return m_Items.GetFreeCount(); }
	//	重建时补充：按名字统计/扣除（任务收集用）
	int		CountItemByName( const char * pszName );
	int		RemoveItemsByName( const char * pszName, int nCount );
	int		RemoveAllByName( const char * pszName );
	BOOL	SplitItem( DWORD dwMakeIndex, int nCount );
	BOOL	RemoveItemStack( DWORD dwMakeIndex, ITEM & outItem );

private:
	CIndexList<itembox_node>	m_Items;
	itembox_node *				m_pCachedItem;
};
