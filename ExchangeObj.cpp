#include "stdafx.h"
#include "ExchangeObj.h"
#include "HumanPlayer.h"

CExchangeObj::CExchangeObj(CHumanPlayer * pPlayer):m_State(EE_FREE)
{
	m_pPlayer = pPlayer;
	m_pOtherSide = NULL;
	m_dwGold = 0;
	m_dwYuanbao = 0;
	memset( m_Items, 0, sizeof( m_Items ) );
	m_pErrorMsg = "交易成功!";
}

CExchangeObj::~CExchangeObj(void)
{
}

BOOL	CExchangeObj::ConnectOther( CHumanPlayer * pPlayer )
{
	if( m_State != EE_FREE )
	{
		return FALSE;
	}
	if( pPlayer->GetExchangeObj().m_State != EE_FREE )
	{
		m_pErrorMsg = "对方正在交易中";
		return FALSE;
	}
	m_pOtherSide = pPlayer;
	
	if( m_pOtherSide->GetExchangeObj().m_pOtherSide != m_pPlayer )
	{
		if( !m_pOtherSide->GetExchangeObj().ConnectOther(m_pPlayer))
		{
			m_pOtherSide = NULL;
			return FALSE;
		}
	}

	m_State = EE_PUTITEMS;
	m_pPlayer->SendMsg( 0, 0x2a1, 0, 0, 0, (LPVOID)pPlayer->GetName() );

	return TRUE;
}

VOID	CExchangeObj::DisconnectOther()
{
	//	disconnecting~
	CHumanPlayer * pPlayer = m_pOtherSide;
	m_pOtherSide = NULL;
	if( pPlayer->GetExchangeObj().m_pOtherSide != NULL )
		pPlayer->GetExchangeObj().DisconnectOther();
	m_State = EE_FREE;
}

BOOL	CExchangeObj::AddItem( ITEM & item )
{
	for( int i = 0;i < 10;i ++ )
	{
		if( m_Items[i].dwMakeIndex == 0 )
		{
			m_Items[i] = item;
			//	let other see this item
			m_pOtherSide->SendMsg(m_pPlayer->GetId(), 0x2aa, 0, 0, 0, &item, sizeof( item ) );
			return TRUE;
		}
	}
	m_pErrorMsg = "交易栏满了，你无法放入物品。";
	return FALSE;
}

BOOL	CExchangeObj::TakeItem( DWORD dwMakeIndex, ITEM & item )
{
	ZeroMemory( &item, sizeof( item ) );

	for( int i = 0;i < 10;i ++ )
	{
		if( m_Items[i].dwMakeIndex == dwMakeIndex )
		{
			item = m_Items[i];
			//	let other see you take it
			
			return TRUE;
		}
	}
	m_pErrorMsg = "您的交易栏没有该物品!";

	return FALSE;
}

BOOL	CExchangeObj::DoExchange()
{
	for( int i = 0;i < 10;i ++ )
	{
		if( m_Items[i].dwMakeIndex != 0 )
			m_pPlayer->GetBag().RemoveItem( m_Items[i].dwMakeIndex );
	}
	return TRUE;
}


BOOL	CExchangeObj::Cancel()
{
	m_pPlayer->SendMsg( 0, 0x2a9, 0, 0, 0, NULL );
	m_pOtherSide->SendMsg( 0, 0x2a9, 0, 0, 0, NULL );
	DisconnectOther();
	return TRUE;
}
BOOL	CExchangeObj::Complete()
{
	//	检测自己的格子和对方的格子是否够
	//	检测身上的钱是不是超出范围
	//	不够或者超出范围就取消交易
	//	否则就开始交易．
	return TRUE;
}

