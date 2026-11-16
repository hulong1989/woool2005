#include "StdAfx.h"
#include ".\area.h"
#include "mapobject.h"
CArea::CArea():m_xObjList(NULL),m_pMap(NULL)
{
	memset( m_pAroundArea, 0, sizeof( m_pAroundArea ));
}

CArea::~CArea(void)
{
}

BOOL CArea::SendMsg( CMapObject * pSender, const char * pszCodedMsg, int size )
{
	xListHost<CMapObject>::xListNode/*<CMapObject>*/ * pNode = NULL;
	
	for( pNode = m_xObjList.getHead(); pNode != NULL; pNode = pNode->getNext())
	{
		CMapObject * pObj = pNode->getObject();
		if( pObj->CanRecvMsg() && pObj != pSender )
			pObj->OnAroundMsg( pSender, pszCodedMsg, size );
	}
	return TRUE;
}

BOOL CArea::SendAroundMsg( CMapObject * pSender, const char * pszCodedMsg, int size )
{
	int i = 0;
	SendMsg( pSender, pszCodedMsg, size );
	for( i = 0;i < 8;i ++ )
	{
		if( m_pAroundArea[i] )
			m_pAroundArea[i]->SendMsg(pSender,pszCodedMsg,size);
	}
	return TRUE;
}

BOOL CArea::AddObject( CMapObject * pObject )
{
	xListHost<CMapObject>::xListNode/*<CMapObject>*/ * pNode = NULL;
	if( pObject == NULL || (pNode = pObject->GetLinkNode(LNI_AREA)) == NULL )return FALSE;
	//	已经是成员了，就不需要再加入了
	if(pNode->BelongTo( &m_xObjList ) )return FALSE;
	//	执行加入
	return m_xObjList.addNode(pNode);
}

BOOL CArea::RemoveObject( CMapObject * pObject )
{
	xListHost<CMapObject>::xListNode/*<CMapObject>*/ * pNode = NULL;
	if( pObject == NULL || (pNode = pObject->GetLinkNode(LNI_AREA)) == NULL )return FALSE;
	if( !pNode->BelongTo( &m_xObjList ) )return FALSE;
	return m_xObjList.removeNode( pNode );
}

//VOID	CArea::OnAddObject( CMapObject * pObject )
//{
//	//char	szMsg[1024];
//	//int length = 1024;
//	//if( pObject->GetViewmsg( szMsg, length ) )
//	//	SendAroundMsg( pObject, szMsg, length );
//}
//VOID	CArea::OnRemoveObject( CMapObject * pObject )
//{
//	//char szMsg[1024];
//	//int length = 1024;
//	//if( pObject->GetOutViewmsg( szMsg, length ) )
//	//	SendAroundMsg( pObject, szMsg, length );
//}

VOID CArea::AppearFor( CMapObject * pObject )
{
	xListHost<CMapObject>::xListNode/*<CMapObject>*/ * pNode = NULL;
	char	szMsg[1024];
	int size = 1024;

	for( pNode = m_xObjList.getHead(); pNode != NULL; pNode = pNode->getNext())
	{
		CMapObject * pObj = pNode->getObject();
		size = 1024;
		if( pObj != pObject && pObj->GetViewmsg(szMsg, size))
		{
			pObject->OnAroundMsg( pObj, szMsg, size );
		}
	}
}

VOID	CArea::DisappearFor( CMapObject * pObject )
{
	xListHost<CMapObject>::xListNode/*<CMapObject>*/ * pNode = NULL;
	char	szMsg[1024];
	int size = 1024;

	for( pNode = m_xObjList.getHead(); pNode != NULL; pNode = pNode->getNext())
	{
		CMapObject * pObj = pNode->getObject();
		size = 1024;
		if( pObj != pObject && pObj->GetOutViewmsg(szMsg, size))
		{
			pObject->OnAroundMsg( pObj, szMsg, size );
		}
	}
}

CMapObject *CArea::FindObject( WORD x, WORD y, e_object_type type )
{

	xListHost<CMapObject>::xListNode/*<CMapObject>*/ * pNode = NULL;
	for( pNode = m_xObjList.getHead(); pNode != NULL; pNode = pNode->getNext())
	{
		CMapObject * pObj = pNode->getObject();
		if( pObj != NULL && pObj->GetType() == type && pObj->getX() == x && pObj->getY() == y )
		{
			return pObj;
		}
	}
	return NULL;
}
