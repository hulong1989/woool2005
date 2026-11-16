#include "stdafx.h"
#include "ItemManager.h"
#include "Server.h"
#include "localsupport.h"
static char	g_szParseBuffer[2048];

CItemManager*CItemManager::m_pInstance = NULL;
CItemManager::CItemManager(void):m_xItemClassList(NULL)
{
	if( m_pInstance == NULL )
		m_pInstance = this;
}

CItemManager::~CItemManager(void)
{
}

BOOL CItemManager::CreateItem( const char * pszName, DWORD dwId, DWORD dwKey, DWORD dwOwner, WORD wPos, BYTE btFlag )
{
	CDBClientObj * pObj = CServer::GetInstance()->GetDBConnection(0);
	ITEM item;
	if( pObj != NULL )
	{
		if( !MakeupTemplateItem( pszName, item ) )return FALSE;
		pObj->SendCreateItem( dwId, dwKey, dwOwner, btFlag, wPos, &item );
		return TRUE;
	}
	return FALSE;
}

BOOL CItemManager::AddItemClassString( const char * pszItemClassDesc )
{
	o_strncpy(g_szParseBuffer, pszItemClassDesc, 2047 );
	char * Params[200];
	int nParam = SearchParam( g_szParseBuffer, Params, 200, '/');
	//	name/stdmode/shape/image/specialpower/ac1/ac2/mac1/mac2/dc1/dc2/mc1/mc2/sc1/sc2/weight/maxdura/price/needtype/needlevel
	//	1   /2      /3    /4    /5           /6  /7  /8   /9   /10 /11 /12 /13 /14 /15 /16    /17     /18
	if( nParam < 20 )
	{
		m_pErrorMsg = "参数数量不够!";
		return FALSE;
	}

	ITEMCLASS	*	pClass = new ITEMCLASS;
	o_strncpy( pClass->szName, Params[0], 20 );
	pClass->stdMode = (BYTE)atoi( Params[1] );
	pClass->btShape = (BYTE)atoi( Params[2] );
	pClass->wImage = (WORD)atoi( Params[3] );
	pClass->specialpower = (char)atoi( Params[4] );
	pClass->ac[0] = (BYTE)atoi( Params[5] );
	pClass->ac[1] = (BYTE)atoi( Params[6] );
	pClass->mac[0] = (BYTE)atoi( Params[7] );
	pClass->mac[1] = (BYTE)atoi( Params[8] );
	pClass->dc[0] = (BYTE)atoi( Params[9] );
	pClass->dc[1] = (BYTE)atoi( Params[10] );
	pClass->mc[0] = (BYTE)atoi( Params[11] );
	pClass->mc[1] = (BYTE)atoi( Params[12] );
	pClass->sc[0] = (BYTE)atoi( Params[13] );
	pClass->sc[1] = (BYTE)atoi( Params[14] );
	pClass->btWeight = (BYTE)atoi( Params[15] );
	pClass->maxdura = (BYTE)atoi( Params[16] );
	pClass->nPrice = atoi( Params[17] );
	pClass->needtype = (BYTE)atoi( Params[18] );
	pClass->needlevel = (BYTE)atoi( Params[19] );
	//	可选：使用类型/使用数值（药品）
	pClass->useType = ( nParam >= 21 ) ? (BYTE)atoi( Params[20] ) : 0;
	pClass->useValue = ( nParam >= 22 ) ? (WORD)atoi( Params[21] ) : 0;

	if( !AddItemClass(pClass) )
	{
		delete pClass;
		return FALSE;
	}
	return TRUE;
}

BOOL CItemManager::AddItemClass( ITEMCLASS * pItemclass )
{
	xListHost<ITEMCLASS>::xListNode * pNode = new xListHost<ITEMCLASS>::xListNode(pItemclass);
	if( pNode == NULL )
	{
		m_pErrorMsg = "分配node失败!";
		return FALSE;
	}
	if( !m_xItemClassList.addNode(pNode ) )
	{
		m_pErrorMsg = "添加node失败!";
		delete pNode;
		return FALSE;
	}
	char	szName[32];
	//	重建时修正：原实现按 14 字节截断，会把 UTF-8 中文切在半个字符上
	o_strncpy( szName, pItemclass->szName, sizeof( szName ) - 1 );
	if( pItemclass->stdMode == 10 )
	{
		if( strstr( szName, "男" ) == 0 )
			strcat( szName, "男" );
	}
	else if( pItemclass->stdMode == 11 )
	{
		if( strstr( szName, "女" ) == 0 )
			strcat( szName, "女" );
	}
	if( !m_ItemClassHash.HAdd( szName, (LPVOID)pNode ) )
	{
		m_pErrorMsg = "加入到名字表中失败!";
		m_xItemClassList.removeNode(pNode);
		delete pNode;
		return FALSE;
	}
	return TRUE;
}

ITEMCLASS *	CItemManager::GetItemClassByName( const char * pszName )
{
	xListHost<ITEMCLASS>::xListNode * pNode = (xListHost<ITEMCLASS>::xListNode *)m_ItemClassHash.HGet(pszName);
	if( pNode != NULL )
		return pNode->getObject();
	return NULL;
}

BOOL CItemManager::MakeupTemplateItem( const char * pszName, ITEM & item )
{
	ITEMCLASS * pClass = GetItemClassByName(pszName );
	if( pClass == NULL )return FALSE;


	ZeroMemory( &item, sizeof(item));

	o_strncpy( item.baseitem.szName, pClass->szName, sizeof( item.baseitem.szName ) - 1 );
	item.baseitem.btNameLength = strlen( item.baseitem.szName );

	item.baseitem.btStdMode = pClass->stdMode;
	item.baseitem.btShape = pClass->btShape;
	item.baseitem.wImageIndex = pClass->wImage;
	item.baseitem.btWeight = pClass->btWeight;
	item.baseitem.wMaxDura = 1000 * pClass->maxdura;
	
	item.baseitem.needtype = pClass->needtype;
	item.baseitem.needvalue = pClass->needlevel;


	item.baseitem.btSpecialpower = (BYTE)pClass->specialpower;


	item.baseitem.btMinDef = pClass->ac[0];
	item.baseitem.btMaxDef = pClass->ac[1];

	item.baseitem.btMinMagDef = pClass->mac[0];
	item.baseitem.btMaxMagDef = pClass->mac[1];

	item.baseitem.btMinAtk = pClass->dc[0];
	item.baseitem.btMaxAtk = pClass->dc[1];

	item.baseitem.btMinMagAtk = pClass->mc[0];
	item.baseitem.btMaxMagAtk = pClass->mc[1];

	item.baseitem.btMinSouAtk = pClass->sc[0];
	item.baseitem.btMaxSouAtk = pClass->sc[1];

	item.baseitem.nPrice = pClass->nPrice;
	item.baseitem.btUseType = pClass->useType;
	item.baseitem.wUseValue = pClass->useValue;
	
	item.wCurDura = item.wMaxDura = item.baseitem.wMaxDura;
	item.wCount = 1;
	item.btUpgradeLevel = 0;
	return TRUE;
}

//	重建时补充：全局唯一的物品实例编号
DWORD CItemManager::NewMakeIndex()
{
	static DWORD s_dwNextMakeIndex = 1000;
	return ++ s_dwNextMakeIndex;
}
