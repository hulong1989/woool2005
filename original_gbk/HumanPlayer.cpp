#include "StdAfx.h"
#include ".\humanplayer.h"
#include "gsclientobj.h"
#include "logicmap.h"
#include "server.h"
#include "downitemmgr.h"
#include "equipment.h"
#define MAKEFEATHER(a,hair,b,c)  (((a&0xff) << 24) | ((hair&0xff) << 16 )|( (b&0xff) << 8 )|(c&0xff))

CHumanPlayer::CHumanPlayer(void):m_pClientObj(NULL),
	m_Equipments(this),m_xGroupList(NULL),
	m_GroupListNode(this),m_ExchangeObj(this)
{
	m_ItemBox.Create(46);
}

CHumanPlayer::~CHumanPlayer(void)
{
}

BOOL CHumanPlayer::Init( CREATEHUMANDESC & desc )
{
	//	对刚出生的角色进行初始化
	if( desc.dbinfo.wLevel == 0 )
	{
		desc.dbinfo.wLevel = 50;
		desc.dbinfo.x = 477;
		desc.dbinfo.y = 222;
		desc.dbinfo.mapid = 1;
		desc.dbinfo.hp = 10000;
		desc.dbinfo.mp = 10000;
		desc.dbinfo.maxhp = 10000;
		desc.dbinfo.maxmp = 10000;
		desc.dbinfo.dwGold = 1000000;
		desc.dbinfo.dwCurExp = 50000000;
		desc.dbinfo.handweight = 100;
		desc.dbinfo.weight = 1000;
		desc.dbinfo.bodyweight = 100;
		desc.dbinfo.maxac = 50;
		desc.dbinfo.minac = 50;
		desc.dbinfo.maxmac = 40;
		desc.dbinfo.minmac = 40;
		desc.dbinfo.maxmc = 50;
		desc.dbinfo.minmc = 50;
		desc.dbinfo.maxsc = 50;
		desc.dbinfo.minsc = 50;
		desc.dbinfo.maxdc = 50;
		desc.dbinfo.mindc = 50;
	}
	m_Humandesc = desc;
	setXY( desc.dbinfo.x, desc.dbinfo.y );
	SetMapId( desc.dbinfo.mapid );
	m_pClientObj = desc.pClientObj;
	//ZeroMemory( m_Equipments, 0, sizeof( m_Equipments ));
	m_ItemBox.Clean();
	m_Equipments.Clean();
	//m_wX = desc.x;
	//m_wY = desc.y;
	//m_Mapid = desc.mapid;
	//m_pClientObj = desc.pClientObj;
	return TRUE;
}

VOID CHumanPlayer::OnAroundMsg( CMapObject * pSender, const char * pszCodedMsg, int size )
{
	assert( m_pClientObj != NULL );
	m_pClientObj->PostMsg( pszCodedMsg, size );
	MIRMSG	header;
	char	szHeader[20];
	strncpy( szHeader, pszCodedMsg+1, 16 ); 
	szHeader[16] = 0;
	_UnGameCode( szHeader, (BYTE*)&header );
	//if( header.wCmd == SM_APPEAR )
	//{
	//	CServer::GetInstance()->GetIoConsole()->OutPut( 0xff00, "id = %d 向 id = %d 发送视觉信息!\n", 
	//																	pSender->GetId(), GetId() );

	//}
	//else if( header.wCmd == SM_DISAPPEAR )
	//{
	//	CServer::GetInstance()->GetIoConsole()->OutPut( 0xff00, "id = %d 向 id = %d 发送消失信息!\n", 
	//		pSender->GetId(), GetId() );
	//}

}

VOID CHumanPlayer::OnEnterMap( CVLogicMap * pMap, CVArea * pArea )
{
	BOOL	bSeeArea = CanRecvMsg();
	if( bSeeArea )
	{
		m_pClientObj->SendMsg( GetId(), SM_SETMAP, m_wX, m_wY, 0, (LPVOID)pMap->GetName() );
		DWORD	dwParam[4] = { MAKEFEATHER( 0, m_Humandesc.dbinfo.btHair, 0, 0 ), 0, 0, 0 };
		m_pClientObj->SendMsg( GetId(), SM_SETPLAYER, m_wX, m_wY, 0x100, (LPVOID)dwParam, sizeof( DWORD ) * 4 );
		m_pClientObj->SendMsg( GetId(), SM_SETPLAYERNAME, 0xff, 0, 0, (LPVOID)GetName() );
		m_pClientObj->SendMsg( 1, 0x2c4, 0, 0, 0, NULL );	//	设置地图的属性 dwflag = 1 战斗， dwflag = 0 非战斗
		m_pClientObj->SendMsg( 0, SM_SETMAPNAME, 0, 0, 0, (LPVOID)pMap->GetTitle() );
		m_pClientObj->SendMsg( 0xf2d505d7, SM_READY, 0, 0, 0, NULL );
		UpdateProp();
		pArea->AppearFor( this );
		for( int i = 0;i < 8;i ++ )
		{
			if( pArea->GetAroundArea(i) != NULL )
			{
				if( bSeeArea )pArea->GetAroundArea(i)->AppearFor( this );
			}
		}
	}
	CAliveObject::OnEnterMap( pMap, pArea );
}
BOOL	CHumanPlayer::GetViewmsg( char * pszMsg, int & length )
{
	//	00 24 41 5a 00 00 00 00 63 00 64 00
	char	szData[200];
	DWORD	* pdwData = (DWORD*)szData;
	pdwData[0] = MAKEFEATHER( 0, m_Humandesc.dbinfo.btHair, 0, 0 );
	pdwData[1] = 0;
	pdwData[2] = 0x00640064;
	sprintf( szData+12, "%s/%u", GetName(), 255 );
	length = EncodeMsg( pszMsg, GetId(), SM_APPEAR, m_wX, m_wY, m_Direction, (LPVOID)szData, 12 + strlen( szData + 12 ));
	return TRUE;
}

DWORD CHumanPlayer::GetFeather()
{
	return MAKEFEATHER( 
		m_Equipments[_U_DRESS].baseitem.btShape,
		m_Humandesc.dbinfo.btHair, 
		m_Equipments[_U_WEAPON].baseitem.btShape,
		0
		);
}
DWORD CHumanPlayer::GetHealth()
{
	return 0x00640064;
}
DWORD CHumanPlayer::GetStatus()
{
	return 0;
}

BOOL	CHumanPlayer::GetDBInfo( CHARDBINFO & info )
{
	info = m_Humandesc.dbinfo;
	info.x = m_wX;
	info.y = m_wY;
	info.mapid = m_Mapid;
	return TRUE;
}

BOOL	CHumanPlayer::AddBagItem( ITEM & item )
{
	//	fixme:
	if( m_ItemBox.AddItem(item) )
	{
		if( m_pClientObj != NULL )
			m_pClientObj->SendAddItem( item );
		return TRUE;
	}
	return FALSE;
}

BOOL	CHumanPlayer::DropItem( ITEM & item )
{
	return CDownItemMgr::GetInstance()->DropItem( m_pMap, item, m_wX + Getrand( 4 ) - 2, m_wY + Getrand( 4 ) - 2, GetId());
}

BOOL	CHumanPlayer::PickupItem()
{
	CDownItemObject * pObj = (CDownItemObject *)m_pArea->FindObject( m_wX, m_wY, OBJ_DOWNITEM );
	if( pObj == NULL )return FALSE;
	return CDownItemMgr::GetInstance()->PickupItem( m_pMap, pObj, this );
}

BOOL	CHumanPlayer::DropBagItem( DWORD dwMakeIndex )
{
	ITEM * pItem = m_ItemBox.FindItem(dwMakeIndex );
	if( pItem == NULL )return FALSE;
	if( !DropItem(*pItem))return FALSE;
	m_ItemBox.RemoveItem(dwMakeIndex);
	return TRUE;
}

BOOL	CHumanPlayer::EquipItem( int pos, DWORD dwMakeIndex )
{
	ITEM * pItem = m_ItemBox.FindItem(dwMakeIndex);
	if( pItem == NULL )return FALSE;
	//	todo: add more condition
	//m_ItemBox.RemoveItem(dwMakeIndex);
	ITEM	itemout;
	if( m_Equipments.EquipItem( pos, *pItem, itemout ) )
	{
		m_ItemBox.RemoveItem(dwMakeIndex);
		if( itemout.dwMakeIndex != 0 )AddBagItem(itemout);
		return TRUE;
	}
	return FALSE;
}

BOOL CHumanPlayer::UnEquipItem( int pos, DWORD dwMakeIndex )
{
	if( dwMakeIndex == 0 || m_Equipments[pos].dwMakeIndex != dwMakeIndex )
		return FALSE;
	ITEM	item;
	if( !m_ItemBox.AddItem(m_Equipments[pos]))
		return FALSE;
	if( !m_Equipments.UnEquipItem(pos, item ))
	{
		m_ItemBox.RemoveItem(dwMakeIndex);
		return FALSE;
	}
	if( m_pClientObj != NULL )
		m_pClientObj->SendAddItem( item );

	return TRUE;
}

BOOL	CHumanPlayer::AddGroupMember( CHumanPlayer * pObject )
{
	return TRUE;
}

BOOL	CHumanPlayer::RemoveGroupMember( CHumanPlayer * pObject )
{
	return TRUE;
}


int CHumanPlayer::GetPropValue( PROP_INDEX index )
{
	int	sRet = 0;
	switch( index )
	{
	case PI_MINAC:	sRet = m_Equipments.GetProp(index) + m_Humandesc.dbinfo.minac;break;
	case PI_MAXAC:	sRet = m_Equipments.GetProp(index) + m_Humandesc.dbinfo.maxac;break;

	case PI_MINMAC:	sRet = m_Equipments.GetProp(index) + m_Humandesc.dbinfo.minmac;break;
	case PI_MAXMAC:	sRet = m_Equipments.GetProp(index) + m_Humandesc.dbinfo.maxmac;break;

	case PI_MINDC:	sRet = m_Equipments.GetProp(index) + m_Humandesc.dbinfo.mindc;break;
	case PI_MAXDC:	sRet = m_Equipments.GetProp(index) + m_Humandesc.dbinfo.maxdc;break;

	case PI_MINMC:	sRet = m_Equipments.GetProp(index) + m_Humandesc.dbinfo.minmc;break;
	case PI_MAXMC:	sRet = m_Equipments.GetProp(index) + m_Humandesc.dbinfo.maxmc;break;

	case PI_MINSC:	sRet = m_Equipments.GetProp(index) + m_Humandesc.dbinfo.minsc;break;
	case PI_MAXSC:	sRet = m_Equipments.GetProp(index) + m_Humandesc.dbinfo.maxsc;break;

	case PI_HITRATE:		sRet = m_Equipments.GetProp(index);break;
	case PI_ESCAPE:			sRet = m_Equipments.GetProp(index);break;
	case PI_MAGESCAPE:		sRet = m_Equipments.GetProp(index);break;
	case PI_POISONESCAPE:	sRet = m_Equipments.GetProp(index);break;

	case PI_ATTACKSPEED:	sRet = m_Equipments.GetProp(index);break;
	case PI_LUCKY:			sRet = m_Equipments.GetProp(index);break;
	case PI_DAWN:			sRet = m_Equipments.GetProp(index);break;

	case PI_HPRECOVER:		sRet = m_Equipments.GetProp(index);break;
	case PI_MPRECOVER:		sRet = m_Equipments.GetProp(index);break;
	case PI_POISONRECOVER:	sRet = m_Equipments.GetProp(index);break;

	case PI_HARD:			sRet = m_Equipments.GetProp(index);break;
	case PI_HOLLY:			sRet = m_Equipments.GetProp(index);break;

	case PI_LEVEL:			sRet = m_Humandesc.dbinfo.wLevel;break;

	case PI_CURBAGWEIGHT:	sRet = m_ItemBox.CalcWeight();break;
	case PI_MAXBAGWEIGHT:	sRet = m_Humandesc.dbinfo.weight;break;

	case PI_CURHANDWEIGHT:	sRet = m_Equipments[_U_WEAPON].baseitem.btWeight;break;
	case PI_MAXHANDWEIGHT:	sRet = m_Humandesc.dbinfo.handweight;break;

	case PI_CURBODYWEIGHT:	sRet = m_Equipments.CalcEquipmentsWeight(-1);break;
	case PI_MAXBODYWEIGHT:	sRet = m_Humandesc.dbinfo.bodyweight;break;

	case PI_CURHP:			sRet = m_Humandesc.dbinfo.hp; break;
	case PI_CURMP:			sRet = m_Humandesc.dbinfo.mp; break;
	case PI_MAXHP:			sRet = m_Humandesc.dbinfo.maxhp; break;
	case PI_MAXMP:			sRet = m_Humandesc.dbinfo.maxmp; break;
	}
	return sRet;
}

VOID CHumanPlayer::UpdateProp()
{
	HUMANPROP	prop;
	prop.wLevel = m_Humandesc.dbinfo.wLevel;
	prop.dwCurexp = m_Humandesc.dbinfo.dwCurExp;
	prop.dwMaxexp = m_Humandesc.dbinfo.dwCurExp * 2;	//	fixme: 经验值
	prop.wCurBagWeight = GetPropValue( PI_CURBAGWEIGHT );											//	fixme: 计算背包重量
	prop.wCurHp = GetPropValue( PI_CURHP );
	prop.wCurMp = GetPropValue( PI_CURMP );
	prop.wMaxBagWeight = GetPropValue( PI_MAXBAGWEIGHT );
	prop.wMaxHp = GetPropValue( PI_MAXHP );
	prop.wMaxMp = GetPropValue( PI_MAXMP );
	prop.btCurBodyWeight = GetPropValue( PI_CURBODYWEIGHT );
	prop.btCurHandWeight = GetPropValue( PI_CURHANDWEIGHT );

	prop.btMaxAtk = GetPropValue( PI_MAXDC );
	prop.btMinAtk = GetPropValue( PI_MINDC );
	prop.btMaxDef = GetPropValue( PI_MAXAC );
	prop.btMinDef = GetPropValue( PI_MINAC );

	prop.btMaxMagAtk = GetPropValue( PI_MAXMC );
	prop.btMinMagAtk = GetPropValue( PI_MINMC );
	prop.btMaxMagicDef = GetPropValue( PI_MAXMAC );
	prop.btMinMagicDef = GetPropValue( PI_MINMAC );

	prop.btMaxSprAtk = GetPropValue( PI_MAXSC );
	prop.btMinSprAtk = GetPropValue( PI_MINSC );

	prop.btMaxBodyWeight = GetPropValue( PI_MAXBODYWEIGHT );
	prop.btMaxHandWeight = GetPropValue( PI_MAXHANDWEIGHT );
	prop.dwUnknown1 = 0xf12233f4;
	//prop.wP1 = 1200;
	//prop.wP2 = 2000;
	m_pClientObj->SendMsg( m_Humandesc.dbinfo.dwGold, SM_UPDATEPROP, 
		m_Humandesc.dbinfo.btHair<<8|m_Humandesc.dbinfo.btClass, 
		m_Humandesc.dbinfo.btSex, 0, &prop, sizeof( prop ));
}

BOOL	CHumanPlayer::Trade()
{
	if( m_ExchangeObj.GetState() != EE_FREE )
	{
		Say( "您正在和别人交易，不能再次交易!" );
		return FALSE;
	}
	int x = getX();
	int y = getY();
	GETNEXTPOS( x, y, GetDirection() );
	CHumanPlayer * pPlayer = (CHumanPlayer*)m_pMap->FindObject(x,y,OBJ_PLAYER);
	if( pPlayer != NULL )
	{
		
		if( !m_ExchangeObj.ConnectOther(pPlayer) )
		{
			Say( m_ExchangeObj.GetErrorMsg());
			return FALSE;
		}
	}
	return TRUE;
}

