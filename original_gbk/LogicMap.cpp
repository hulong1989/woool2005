#include "StdAfx.h"
#include ".\logicmap.h"
#include "mapobject.h"
#include "area.h"
#include "physicsmapmgr.h"
#include "server.h"
CLogicMap::CLogicMap(void):m_xObjList(NULL),m_pPhysicsMap(NULL),m_RefNode(this),m_Id(0),m_Flag((e_mapflag)0)
{
	//m_pxObjList = new xListHost<CMapObject>(this);
	//m_pRefNode = new xListHost<CVLogicMap>::xListNode<CVLogicMap>(this);
}

CLogicMap::~CLogicMap(void)
{
}

BOOL CLogicMap::AddObject( CMapObject * pObject )
{
	xListHost<CMapObject>::xListNode/*<CMapObject>*/ * pNode = NULL;
	if( pObject == NULL || (pNode = pObject->GetLinkNode(LNI_MAP)) == NULL )return FALSE;
	//	已经是成员了，就不需要再加入了
	if(pNode->BelongTo( &m_xObjList ) )return FALSE;
	//	执行加入
	if( !m_xObjList.addNode(pNode) )return FALSE;
	int x = pObject->getX();
	int y = pObject->getY();
	CVArea * pArea = GetArea(x,y);
	if( !pArea->AddObject(pObject) )
	{
		m_xObjList.removeNode(pNode);
		return FALSE;
	}
	pObject->OnEnterMap( this, pArea );
	return TRUE;
}

BOOL CLogicMap::RemoveObject( CMapObject * pObject )
{
	//	如果是空指针
	xListHost<CMapObject>::xListNode/*<CMapObject>*/ * pNode = NULL;
	if( pObject == NULL || (pNode = pObject->GetLinkNode(LNI_MAP)) == NULL )return FALSE;
	//	如果不属于地图的物件
	if( !pNode->BelongTo( &m_xObjList ) )return FALSE;
	//	先离开目前所在的area, 如果在区域内的话
	CVArea * pArea = pObject->GetArea();
	if( pArea == NULL )
	{
		pArea = GetArea( pObject->getX(), pObject->getY() );
	}
	if( pArea != NULL )
		pArea->RemoveObject(pObject);

	//	执行删除
	if( !m_xObjList.removeNode(pNode) )return FALSE;

	pObject->OnLeaveMap( this, pArea );
	return TRUE;
}

BOOL CLogicMap::InitAreas()
{
	assert( m_pPhysicsMap!=NULL );
	assert( m_iWidth != 0 );
	assert( m_iHeight != 0 );
	assert( m_iAreaBlockWidth != 0 );
	assert( m_iAreaBlockHeight != 0 );


	m_iAreaWidth = (m_iWidth + m_iAreaBlockWidth -1)/m_iAreaBlockWidth;
	m_iAreaHeight = (m_iHeight + m_iAreaBlockHeight -1)/m_iAreaBlockHeight;

	int	i = 0,j = 0;
	
	m_pAreas = new CArea *[m_iAreaWidth];
	
	for( i = 0;i < m_iAreaWidth; i ++ )
	{
		m_pAreas[i] = new CArea[m_iAreaHeight];
		for( j = 0;j < m_iAreaHeight; j ++ )
		{
			m_pAreas[i][j].SetMap(this);
		}
	}
	
	for( i = 0;i < m_iAreaWidth;i ++ )
	{
		for( j = 0;j < m_iAreaHeight; j ++ )
		{
			for( int d = 0;d < 8;d ++ )
			{
				int x = i, y = j;
				GETNEXTPOS( x,y, d );
				m_pAreas[i][j].SetAroundArea(d,GetAreaByAreaPos(x,y));

			}
			m_pAreas[i][j].SetAreaInfo( j * m_iAreaWidth + i , i, j );
		}
	}

	return TRUE;
}

CVArea* CLogicMap::GetArea( int x, int y )
{
	if( !VerifyPos(x,y))return NULL;
	assert(m_iAreaBlockHeight != 0 );
	assert(m_iAreaBlockWidth != 0 );
	x /= m_iAreaBlockWidth;
	y /= m_iAreaBlockHeight;
	return &m_pAreas[x][y];
}

CVArea* CLogicMap::GetAreaByAreaPos( int x, int y )
{
	if( x < 0 || x >= m_iAreaWidth )return NULL;
	if( y < 0 || y >= m_iAreaHeight )return NULL;
	return &m_pAreas[x][y];
}


//VOID CLogicMap::OnAddObject( CMapObject * pObject )
//{
//	if( !pObject->CanRecvMsg() )return;
//	//	首先,发送地图信息 地图障碍名,坐标,方向,地图标题
//	
//	//	然后,发送给其他人,自己的视觉信息	LetAroundView
//	//	接着,收集周围其他人的视觉信息		SeeArea();
//	//	然后,收集地面物品							SeeAreaItems();
//}
//
//VOID CLogicMap::OnRemoveObject( CMapObject * pObject )
//{
//	
//}

BOOL CLogicMap::SendMsg( CMapObject * pSender, const char * pszCodedMsg, int size )
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

BOOL CLogicMap::IsBlocked( int x, int y )
{
	if( !VerifyPos(x,y))return TRUE;
	if( IsLocked(x,y))return TRUE;

	return m_pPhysicsMap->IsBlocked(x,y);
}

BOOL CLogicMap::LoadMap( const char * pszFilename )
{
	m_DataFile.Open( pszFilename );
	//	read physics map name
	char * pszPhysicsMapName = (char*)m_DataFile.GetString("define", "blockmap", NULL );
	m_pName = (char*)m_DataFile.GetString( "define", "name", "NoName" );
	m_iAreaBlockWidth = m_DataFile.GetInteger( "define", "areawidth", MAP_AREA_WIDTH );
	m_iAreaBlockHeight = m_DataFile.GetInteger( "define", "areaheight", MAP_AREA_HEIGHT );


	if( pszPhysicsMapName == NULL )
	{
		m_DataFile.Close();
		return FALSE;
	}
	//	find physics map from game world 's  physicsmap manager
	CVPhysicsMap * pPhysicsMap = CPhysicsMapMgr::GetInstance()->GetPhysicsMapByName( pszPhysicsMapName );
	//	if ! found return FALSE
	if( pPhysicsMap == NULL )
	{
		m_DataFile.Close();
		return FALSE;
	}
	//	SetPhysicsMap( physicsmap )
	SetPhysicsMap( pPhysicsMap );
	//	Init areas
	if( !InitAreas() )return FALSE;
	//	ok~ let's successed return TRUE
	return TRUE;
}

VOID CLogicMap::SetPhysicsMap( CVPhysicsMap * pPhysicsMap )
{ 
	m_pPhysicsMap = pPhysicsMap;
	m_iWidth = m_pPhysicsMap->GetWidth();
	m_iHeight = m_pPhysicsMap->GetHeight();
	m_iMaxBlockElements = 0;
	//	测试是不是允许穿人
	if( TestFlag(EMF_CANWALKTHROUGH) )
		m_pLockLayer = NULL;
	else
	{
		m_iMaxBlockElements = (m_iWidth * m_iHeight + 31)/32;
		m_pLockLayer = new DWORD[m_iMaxBlockElements];
		ZeroMemory( m_pLockLayer, sizeof( DWORD ) * m_iMaxBlockElements );
	}
	assert( pPhysicsMap->AddRefMap(this));
}
int g_outviewarea[8][6] = 
{
	{ 3, 3, 4, 5, 0, 0 }, 
	{ 5, 3, 4, 5, 6, 7 },
	{ 3, 5, 6, 7, 0, 0 },
	{ 5, 5, 6, 7, 0, 1 },
	{ 3, 7, 0, 1, 0, 0 },
	{ 5, 7, 0, 1, 2, 3 },
	{ 3, 1, 2, 3, 0, 0 },
	{ 5, 1, 2, 3, 4, 5 },
};
BOOL CLogicMap::ChangeArea( CMapObject * pObject, CVArea * pAreaTo )
{
	if( pAreaTo == NULL )return FALSE;
	CVArea * pCurArea = pObject->GetArea();
	CVArea * pTempArea = NULL;
	int		itoDir = -1;
	char	szMsg[1024];
	int msgsize = 1024;
	int i = 0;
	BOOL	bSeeAround = pObject->CanRecvMsg();
	//CServer::GetInstance()->GetIoConsole()->OutPut(255,"change area\n" );
	if( pCurArea != NULL )
	{
		//CServer::GetInstance()->GetIoConsole()->OutPut( 0xff00, "离开 index = %u x = %u y = %u 的区域!\n",
		//											pCurArea->GetAreaIndex(), pCurArea->GetAreaX(), pCurArea->GetAreaY() );
		//	让周围的人看到自己消失，也让自己看到他们消失
		for( i = 0;i < 8;i ++ )
		{
			if( pAreaTo == pCurArea->GetAroundArea(i))
			{
				itoDir = i;
				break;
			}
		}
		if( pObject->GetOutViewmsg( szMsg, msgsize ) )
		{
			if( itoDir == -1 )	//	不是附近的区域
			{
				//	让全部的人都看到自己消失，也看到周围的人都消失
				pCurArea->SendMsg( pObject, szMsg, msgsize );
				if( bSeeAround )pCurArea->DisappearFor( pObject );
				for( i = 0;i < 8;i ++ )
				{
					if( (pTempArea = pCurArea->GetAroundArea(i)) != NULL )
					{
						pTempArea->SendMsg( pObject, szMsg, msgsize );
						if( bSeeAround )pTempArea->DisappearFor( pObject );
					}
				}
			}
			else
			{
				//	是附近的区域
				for( i = 0;i < g_outviewarea[itoDir][0];i ++ )
				{
					if( (pTempArea = pCurArea->GetAroundArea(g_outviewarea[itoDir][i+1])) != NULL )
					{
						pTempArea->SendMsg( pObject, szMsg, msgsize );
						if( bSeeAround )pTempArea->DisappearFor( pObject );
					}
				}
			}
		}

		pObject->GetArea()->RemoveObject(pObject);
	}

	//CServer::GetInstance()->GetIoConsole()->OutPut( 0xff00, "进入 index = %u x = %u y = %u 的区域!\n",
	//	pAreaTo->GetAreaIndex(), pAreaTo->GetAreaX(), pAreaTo->GetAreaY() );

	msgsize = 1024;
	if( pObject->GetViewmsg( szMsg, msgsize ))
	{
		if( itoDir == -1 )
		{
			//	让周围全部看见自己，让自己看见周围所有
			if( bSeeAround )pAreaTo->AppearFor( pObject );
			pAreaTo->SendMsg( pObject, szMsg, msgsize );
			for( i = 0;i < 8;i ++ )
			{
				if( (pTempArea = pAreaTo->GetAroundArea(i)) != NULL )
				{
					if( bSeeAround )pTempArea->AppearFor( pObject );
					pTempArea->SendMsg( pObject, szMsg, msgsize );
				}
			}
		}
		else
		{
			itoDir = (itoDir + 4)%8;
			//	让新出现的区域看见自己，让自己看见新出现的区域
			for( i = 0;i < g_outviewarea[itoDir][0];i ++ )
			{
				if( (pTempArea = pAreaTo->GetAroundArea(g_outviewarea[itoDir][i+1])) != NULL )
				{
					pTempArea->SendMsg( pObject, szMsg, msgsize );
					if( bSeeAround )pTempArea->AppearFor( pObject );
				}
			}
		}
	}
	pAreaTo->AddObject(pObject);
	pObject->SetArea( pAreaTo );


	//	如果是邻近的Area
	//	如果不是~~，那么就让周围的人都看不见我，
	//	然后离开原来的area，
	//	然后进入新的area，
	//	然后让周围的人都看到我
	return TRUE;
}

CMapObject * CLogicMap::FindObject( int x, int y, e_object_type type )
{
	if( this == NULL )return NULL;
	CVArea * pArea = GetArea( x, y );
	if( pArea == NULL )return NULL;
	return pArea->FindObject(x,y,type);
}

