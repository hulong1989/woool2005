// Title.cpp
#include "stdafx.h"
#include "Title.h"

CTitleMgr * CTitleMgr::m_pInstance = NULL;

CTitleMgr * CTitleMgr::GetInstance()
{
	if( m_pInstance == NULL )
		m_pInstance = new CTitleMgr;
	return m_pInstance;
}

BOOL CTitleMgr::Load( const char * pszFile )
{
	m_Titles.clear();

	CStringFile sf( pszFile );
	for( int i = 0; i < sf.GetLineCount(); ++i )
	{
		const char * pszLine = sf[i];
		if( pszLine == NULL || *pszLine == '\0' || *pszLine == '#' )continue;

		char	szBuffer[256];
		o_strncpy( szBuffer, pszLine, sizeof( szBuffer ) - 1 );
		char *	Params[6];
		//	id/名称/价格/加成
		int nParam = SearchParam( szBuffer, Params, 6, '/' );
		if( nParam < 3 )continue;

		TITLEDEF t;
		t.id = atoi( Params[0] );
		o_strncpy( t.szName, Params[1], sizeof( t.szName ) - 1 );
		t.nCost = atoi( Params[2] );
		t.bonuses.clear();

		if( nParam >= 4 )
		{
			std::string list = Params[3];
			size_t start = 0;
			while( start <= list.size() )
			{
				size_t comma = list.find( ',', start );
				std::string stat = list.substr( start, comma == std::string::npos ? std::string::npos : comma - start );
				char szName[16] = { 0 };
				int nVal = 0;
				if( sscanf( stat.c_str(), "%15[^:]:%d", szName, &nVal ) == 2 )
				{
					TITLEBONUS b;
					b.nValue = nVal;
					if( strcmp( szName, "HP" ) == 0 ){ b.nIndex = PI_MAXHP; t.bonuses.push_back( b ); }
					else if( strcmp( szName, "MP" ) == 0 ){ b.nIndex = PI_MAXMP; t.bonuses.push_back( b ); }
					else if( strcmp( szName, "AC" ) == 0 )
					{
						b.nIndex = PI_MINAC; t.bonuses.push_back( b );
						b.nIndex = PI_MAXAC; t.bonuses.push_back( b );
					}
					else if( strcmp( szName, "DC" ) == 0 )
					{
						b.nIndex = PI_MINDC; t.bonuses.push_back( b );
						b.nIndex = PI_MAXDC; t.bonuses.push_back( b );
					}
					else if( strcmp( szName, "MC" ) == 0 )
					{
						b.nIndex = PI_MINMC; t.bonuses.push_back( b );
						b.nIndex = PI_MAXMC; t.bonuses.push_back( b );
					}
					else if( strcmp( szName, "SC" ) == 0 )
					{
						b.nIndex = PI_MINSC; t.bonuses.push_back( b );
						b.nIndex = PI_MAXSC; t.bonuses.push_back( b );
					}
				}
				if( comma == std::string::npos )break;
				start = comma + 1;
			}
		}
		m_Titles.push_back( t );
	}
	return !m_Titles.empty();
}

int CTitleMgr::GetCount()
{
	return (int)m_Titles.size();
}

TITLEDEF * CTitleMgr::GetByIndex( int nIndex )
{
	if( nIndex < 0 || (size_t)nIndex >= m_Titles.size() )return NULL;
	return &m_Titles[(size_t)nIndex];
}

TITLEDEF * CTitleMgr::Find( int nId )
{
	for( size_t i = 0; i < m_Titles.size(); ++i )
		if( m_Titles[i].id == nId )return &m_Titles[i];
	return NULL;
}
