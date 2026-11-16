#include "StdAfx.h"
#include ".\mapobject.h"
#include "vmap.h"

CMapObject::CMapObject(void):m_pMap(NULL),m_pArea(NULL)
{
	int i = 0;
	for( i = 0;i < LNI_MAX;i ++ )
	{
		m_DLinkNodes[i] = new xListHost<CMapObject>::xListNode/*<CMapObject>*/(this);
	}
}

CMapObject::~CMapObject(void)
{
}

//BOOL CMapObject::EnterMap( CVLogicMap * pMap )
//{
//	return pMap->AddObject(this);
//}
//
//BOOL CMapObject::LeaveMap()
//{
//	return GetLinkNode(LNI_MAP)->Leave();
//}
//
//BOOL CMapObject::EnterArea( CVArea * pArea )
//{
//	return pArea->AddObject( this );
//}
//
//BOOL CMapObject::LeaveArea()
//{
//	return GetLinkNode(LNI_AREA)->Leave();
//}

VOID CMapObject::Clean()
{
	m_wX = 0,m_wY = 0;
	for( int i = 0;i < LNI_MAX;i ++ )
		m_DLinkNodes[i]->Leave();
	m_pMap = NULL;
	m_pArea = NULL;
}

VOID	CMapObject::setXY(WORD x, WORD y )
{
	//OnSetPos( m_wX, m_wY, x, y );
	m_wX = x, m_wY = y;

	CVLogicMap * pMap = GetMap();
	if( pMap == NULL )return;
	CVArea * pArea = pMap->GetArea( x, y );
	//	»»µØÍ¼
	if( pArea != GetArea() )
	{
		pMap->ChangeArea( this, pArea );
	}
}

int CMapObject::EncodeMsg( char * buffer, DWORD dwFlag, WORD wCmd, WORD w1, WORD w2, WORD w3, LPVOID lpdata, int datasize )
{
	MIRMSGHEADER	header( dwFlag, wCmd, w1, w2, w3 );
	int	codedsize = 1;
	buffer[0] = '#';
	codedsize += _CodeGameCode( (BYTE*)&header, sizeof( header ), (BYTE*)buffer+codedsize );
	if( lpdata != NULL )
	{
		if( datasize < 0 )
			datasize = strlen( (char*)lpdata );
		codedsize += _CodeGameCode( (BYTE*)lpdata, datasize, (BYTE*)buffer+codedsize );
	}
	buffer[codedsize++] = '!';
	buffer[codedsize] = 0;
	return codedsize;
}

VOID CMapObject::OnEnterMap( CVLogicMap * pMap, CVArea * pArea ) 
{
	m_pMap = pMap;
	m_pArea = pArea;
	if( pArea == NULL )return;

	CHAR	szMsg[1024];
	int size = 1024;
	if( GetViewmsg( szMsg, size ) )
	{
		pArea->SendAroundMsg( this, szMsg, size );
	}
}
VOID CMapObject::OnLeaveMap( CVLogicMap * pMap, CVArea * pArea ) 
{
	m_pMap = NULL;
	m_pArea = NULL;

	if( pArea == NULL )return ;
	char	szMsg[1024];
	int size = 1024;

	if( GetOutViewmsg( szMsg, size ) )
	{
		pArea->SendAroundMsg( this, szMsg, size );
	}

}

