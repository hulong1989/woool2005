// Npc.cpp  ——  NPC 加载、放置、视野消息
#include "stdafx.h"
#include "Npc.h"
#include "LogicMapMgr.h"

CNpcMgr * CNpcMgr::m_pInstance = NULL;

CNpcMgr * CNpcMgr::GetInstance()
{
	if( m_pInstance == NULL )
		m_pInstance = new CNpcMgr;
	return m_pInstance;
}

// ---------------------------------------------------------------------------
// CNpc
// ---------------------------------------------------------------------------
CNpc::CNpc()
	: m_Id( 0 ), m_nType( 0 ), m_nDestMap( 0 ), m_nDestX( 0 ), m_nDestY( 0 ), m_wShape( 0 )
{
	m_szName[0] = '\0';
	m_szDialog[0] = '\0';
}

CNpc::~CNpc()
{
}

BOOL CNpc::Init( const char * pszName, int nType, int nDestMap, int nDestX, int nDestY,
				 const char * pszDialog, WORD x, WORD y )
{
	o_strncpy( m_szName, pszName, sizeof( m_szName ) - 1 );
	o_strncpy( m_szDialog, pszDialog, sizeof( m_szDialog ) - 1 );
	m_nType = nType;
	m_nDestMap = nDestMap;
	m_nDestX = nDestX;
	m_nDestY = nDestY;
	m_wShape = (WORD)nType;
	setXY( x, y );
	return TRUE;
}

BOOL CNpc::GetViewmsg( char * pszMsg, int & length )
{
	char szData[80];
	snprintf( szData, sizeof( szData ), "%s/%d", m_szName, m_nType );
	length = EncodeMsg( pszMsg, GetId(), SM_APPEAR, m_wX, m_wY, m_wShape,
						(LPVOID)szData, (int)strlen( szData ) );
	return TRUE;
}

BOOL CNpc::GetOutViewmsg( char * pszMsg, int & length )
{
	length = EncodeMsg( pszMsg, GetId(), SM_DISAPPEAR, m_wX, m_wY, 0, NULL );
	return TRUE;
}

VOID CNpc::OnEnterMap( CVLogicMap * pMap, CVArea * pArea )
{
	CMapObject::OnEnterMap( pMap, pArea );
	//	占住自己的格子，玩家不能穿过去
	if( pMap != NULL )
		pMap->LockPos( getX(), getY() );
}

VOID CNpc::OnLeaveMap( CVLogicMap * pMap, CVArea * pArea )
{
	if( pMap != NULL )
		pMap->UnLockPos( getX(), getY() );
	CMapObject::OnLeaveMap( pMap, pArea );
}

// ---------------------------------------------------------------------------
// CNpcMgr
// ---------------------------------------------------------------------------
BOOL CNpcMgr::Load( const char * pszFile )
{
	CStringFile sf( pszFile );
	if( sf.GetLineCount() <= 0 )return FALSE;

	for( int i = 0; i < sf.GetLineCount(); ++i )
	{
		const char * pszLine = sf[i];
		if( pszLine == NULL || *pszLine == '\0' || *pszLine == '#' )continue;

		char	szBuffer[512];
		o_strncpy( szBuffer, pszLine, sizeof( szBuffer ) - 1 );
		char *	Params[16];
		//	名字/地图id/x/y/类型/目标地图/目标x/目标y/对话
		int nParam = SearchParam( szBuffer, Params, 16, '/' );
		if( nParam < 8 )continue;

		UINT nMapId = (UINT)atoi( Params[1] );
		CVLogicMap * pMap = CLogicMapMgr::GetInstance()->GetLogicMapById( nMapId );
		if( pMap == NULL )continue;

		CNpc *	pNpc = NULL;
		UINT	id = m_Npcs.New( &pNpc );
		if( id == 0 || pNpc == NULL )continue;

		if( !pNpc->Init( Params[0], atoi( Params[4] ), atoi( Params[5] ),
						 atoi( Params[6] ), atoi( Params[7] ),
						 nParam >= 9 ? Params[8] : "",
						 (WORD)atoi( Params[2] ), (WORD)atoi( Params[3] ) ) )
		{
			m_Npcs.Del( id );
			continue;
		}

		pNpc->SetId( id | ( (UINT)OBJ_NPC << 24 ) );

		if( !pMap->AddObject( pNpc ) )
		{
			m_Npcs.Del( id );
			continue;
		}
	}
	return TRUE;
}

CNpc * CNpcMgr::Find( const char * pszName )
{
	if( pszName == NULL )return NULL;
	for( CNpc * pNpc = m_Npcs.First(); pNpc != NULL; pNpc = m_Npcs.Next() )
	{
		if( strcmp( pNpc->GetName(), pszName ) == 0 )
			return pNpc;
	}
	return NULL;
}

CNpc * CNpcMgr::GetByIndex( int nIndex )
{
	if( nIndex < 0 )return NULL;
	int n = 0;
	for( CNpc * pNpc = m_Npcs.First(); pNpc != NULL; pNpc = m_Npcs.Next() )
	{
		if( n++ == nIndex )return pNpc;
	}
	return NULL;
}

int CNpcMgr::GetCount()
{
	return m_Npcs.getCount();
}

VOID CNpcMgr::ClearAll()
{
	for( CNpc * pNpc = m_Npcs.First(); pNpc != NULL; pNpc = m_Npcs.Next() )
	{
		if( pNpc->GetMap() != NULL )
			pNpc->GetMap()->RemoveObject( pNpc );
	}
	m_Npcs.Clean();
}
