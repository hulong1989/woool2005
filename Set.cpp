// Set.cpp
#include "stdafx.h"
#include "Set.h"

CSetMgr * CSetMgr::m_pInstance = NULL;

CSetMgr * CSetMgr::GetInstance()
{
	if( m_pInstance == NULL )
		m_pInstance = new CSetMgr;
	return m_pInstance;
}

static BOOL Set_ParseStat( const char * pszStat, SETBONUS & bonus )
{
	//	DC/AC/MC/SC/MAC : value
	char szName[16] = { 0 };
	int nValue = 0;
	if( sscanf( pszStat, "%15[^:]:%d", szName, &nValue ) != 2 )return FALSE;

	if( strcmp( szName, "DC" ) == 0 ){ bonus.nMinProp = PI_MINDC; bonus.nMaxProp = PI_MAXDC; }
	else if( strcmp( szName, "AC" ) == 0 ){ bonus.nMinProp = PI_MINAC; bonus.nMaxProp = PI_MAXAC; }
	else if( strcmp( szName, "MC" ) == 0 ){ bonus.nMinProp = PI_MINMC; bonus.nMaxProp = PI_MAXMC; }
	else if( strcmp( szName, "SC" ) == 0 ){ bonus.nMinProp = PI_MINSC; bonus.nMaxProp = PI_MAXSC; }
	else if( strcmp( szName, "MAC" ) == 0 ){ bonus.nMinProp = PI_MINMAC; bonus.nMaxProp = PI_MAXMAC; }
	else return FALSE;

	bonus.nValue = nValue;
	return TRUE;
}

BOOL CSetMgr::Load( const char * pszFile )
{
	m_Sets.clear();

	CStringFile sf( pszFile );
	for( int i = 0; i < sf.GetLineCount(); ++i )
	{
		const char * pszLine = sf[i];
		if( pszLine == NULL || *pszLine == '\0' || *pszLine == '#' )continue;

		char	szBuffer[512];
		o_strncpy( szBuffer, pszLine, sizeof( szBuffer ) - 1 );
		char *	Params[8];
		//	套装名/成员(逗号)/件数/属性:值,属性:值
		if( SearchParam( szBuffer, Params, 8, '/' ) < 4 )continue;

		SETDEF def;
		memset( &def.szName, 0, sizeof( def.szName ) );
		o_strncpy( def.szName, Params[0], sizeof( def.szName ) - 1 );
		def.nNeed = atoi( Params[2] );
		if( def.nNeed <= 0 )def.nNeed = 1;

		std::string members = Params[1];
		size_t start = 0;
		while( start <= members.size() )
		{
			size_t comma = members.find( ',', start );
			std::string item = members.substr( start, comma == std::string::npos ? std::string::npos : comma - start );
			if( !item.empty() )def.items.push_back( item );
			if( comma == std::string::npos )break;
			start = comma + 1;
		}

		std::string bonusList = Params[3];
		start = 0;
		while( start <= bonusList.size() )
		{
			size_t comma = bonusList.find( ',', start );
			std::string stat = bonusList.substr( start, comma == std::string::npos ? std::string::npos : comma - start );
			SETBONUS bonus;
			memset( &bonus, 0, sizeof( bonus ) );
			if( Set_ParseStat( stat.c_str(), bonus ) )def.bonuses.push_back( bonus );
			if( comma == std::string::npos )break;
			start = comma + 1;
		}

		if( !def.items.empty() && !def.bonuses.empty() )
			m_Sets.push_back( def );
	}
	return !m_Sets.empty();
}

int CSetMgr::GetCount()
{
	return (int)m_Sets.size();
}

SETDEF * CSetMgr::GetByIndex( int nIndex )
{
	if( nIndex < 0 || (size_t)nIndex >= m_Sets.size() )return NULL;
	return &m_Sets[(size_t)nIndex];
}
