#pragma once
#include "aliveobject.h"
#include "itembox.h"
#include "equipment.h"
#include "exchangeobj.h"
class CClientObj;

typedef struct tagCREATEHUMANDESC
{
	tagCREATEHUMANDESC()
	{
		memset( this, 0, sizeof( *this));
	}
	//tagCREATEHUMANDESC( const char * pszName, BYTE _btClass, BYTE _btSex, BYTE _btHair, BYTE _btFlag, WORD _wLevel, WORD _x, WORD _y, UINT _mapid, CClientObj * pObj )
	//{
	//	o_strncpy( szName, pszName, 18 );
	//	btClass = _btClass;
	//	btSex = _btSex;
	//	btHair = _btHair;
	//	btFlag = _btFlag;
	//	wLevel = _wLevel;
	//	x = _x;
	//	y = _y;
	//	mapid = _mapid;
	//	pClientObj = pObj;
	//}
	CHARDBINFO	dbinfo;
	CClientObj * pClientObj;
}CREATEHUMANDESC;
//	可以用另外的shell来替换自身，shell有存活时间。
class CHumanPlayer :
	public CAliveObject
{
public:
	CHumanPlayer(void);
	virtual ~CHumanPlayer(void);
	BOOL Init( CREATEHUMANDESC & desc );
	e_object_type GetType(){ return OBJ_PLAYER;}
	VOID OnAroundMsg( CMapObject * pSender, const char * pszCodedMsg, int size );
	VOID OnEnterMap( CVLogicMap * pMap, CVArea * pArea );
	DWORD GetFeather();
	DWORD GetHealth();
	DWORD GetStatus();

	BOOL	GetDBInfo( CHARDBINFO & info );
	DWORD	GetDBId(){ return m_Humandesc.dbinfo.dwDBId;}
	const char * GetName(){ return m_Humandesc.dbinfo.szName;}

	BOOL	GetViewmsg( char * pszMsg, int & length );


	BOOL	CanRecvMsg(){
		if( m_pClientObj == NULL )return FALSE;
		return TRUE;
	}


public:
	BOOL	AddBagItem( ITEM & item );
	BOOL	DropItem( ITEM & item );
	BOOL	PickupItem();
	BOOL	DropBagItem( DWORD dwMakeIndex );

	BOOL	EquipItem( int pos, DWORD dwMakeIndex );
	BOOL	UnEquipItem( int pos, DWORD dwMakeIndex );

	BOOL	Trade();


	CItemBox & GetBag(){ return m_ItemBox;}
	int	GetPropValue( PROP_INDEX index );
public:
	VOID	UpdateProp();
	BOOL	AddGroupMember( CHumanPlayer * pObject );
	BOOL	RemoveGroupMember( CHumanPlayer * pObject );

	xListHost<CHumanPlayer> & GetGroupList(){return m_xGroupList;}
	xListHost<CHumanPlayer>::xListNode & GetGroupListNode(){return m_GroupListNode;}

	CExchangeObj & GetExchangeObj(){return m_ExchangeObj;}
protected:
	xListHost<CHumanPlayer>	m_xGroupList;
	xListHost<CHumanPlayer>::xListNode m_GroupListNode;

	CClientObj * m_pClientObj;
	CREATEHUMANDESC	m_Humandesc;
	CItemBox	m_ItemBox;
	CEquipment m_Equipments;
	CExchangeObj m_ExchangeObj;
//	ITEM		m_Equipments[10];
};

