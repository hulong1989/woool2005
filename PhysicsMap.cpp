// PhysicsMap.cpp  ——  重建（原快照缺失）
#include "stdafx.h"
#include "PhysicsMap.h"

CPhysicsMap::CPhysicsMap(void)
	: m_iWidth(0), m_iHeight(0), m_nRefCount(0)
{
	m_szName[0] = '\0';
}

CPhysicsMap::~CPhysicsMap(void)
{
}

BOOL CPhysicsMap::LoadMap( const char * pszFilename )
{
	if( pszFilename == NULL )return FALSE;

	CDataFile file;
	if( !file.Open( pszFilename ) )return FALSE;

	const char * pszName = file.GetString( "map", "name", NULL );
	if( pszName != NULL )
	{
		o_strncpy( m_szName, pszName, sizeof( m_szName ) - 1 );
	}
	else
	{
		//	用文件名做地图名
		const char * pszBase = pszFilename;
		for( const char * p = pszFilename; *p != '\0'; ++p )
			if( *p == '\\' || *p == '/' )pszBase = p + 1;
		o_strncpy( m_szName, pszBase, sizeof( m_szName ) - 1 );
		char * pszDot = strrchr( m_szName, '.' );
		if( pszDot != NULL )*pszDot = '\0';
	}

	m_iWidth = file.GetInteger( "map", "width", 0 );
	m_iHeight = file.GetInteger( "map", "height", 0 );
	if( m_iWidth <= 0 || m_iHeight <= 0 || m_iWidth > 4096 || m_iHeight > 4096 )
		return FALSE;

	m_Blocks.assign( (size_t)m_iWidth * (size_t)m_iHeight, (BYTE)0 );

	for( int y = 0; y < m_iHeight; ++y )
	{
		char szKey[16];
		snprintf( szKey, sizeof( szKey ), "%d", y );
		const char * pszRow = file.GetString( "block", szKey, NULL );
		if( pszRow == NULL )continue;

		int x = 0;
		for( const char * p = pszRow; *p != '\0' && x < m_iWidth; ++p )
		{
			if( *p == ',' || *p == ' ' || *p == '\t' )continue;
			if( *p == '1' || *p == '#' )m_Blocks[(size_t)y * m_iWidth + x] = 1;
			else m_Blocks[(size_t)y * m_iWidth + x] = 0;
			++x;
		}
	}
	return TRUE;
}

BOOL CPhysicsMap::IsBlocked( int x, int y )
{
	if( x < 0 || y < 0 || x >= m_iWidth || y >= m_iHeight )return TRUE;
	return m_Blocks[(size_t)y * m_iWidth + x] != 0;
}
