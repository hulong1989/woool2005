// Gm.cpp
#include "stdafx.h"
#include "Gm.h"
#include <fstream>

CGmMgr * CGmMgr::m_pInstance = NULL;

CGmMgr * CGmMgr::GetInstance()
{
	if( m_pInstance == NULL )
		m_pInstance = new CGmMgr;
	return m_pInstance;
}

BOOL CGmMgr::Load( const char * pszFile )
{
	m_Accounts.clear();
	std::ifstream in( pszFile, std::ios::binary );
	if( !in.is_open() )return FALSE;

	std::string line;
	while( std::getline( in, line ) )
	{
		if( line.empty() || line[0] == '#' )continue;
		size_t slash = line.find( '/' );
		if( slash == std::string::npos )continue;
		GMACCOUNT acc;
		acc.name = line.substr( 0, slash );
		acc.level = atoi( line.substr( slash + 1 ).c_str() );
		if( acc.level < 1 || acc.level > 3 )acc.level = 1;
		if( !acc.name.empty() )m_Accounts.push_back( acc );
	}
	return TRUE;
}

BOOL CGmMgr::Save( const char * pszFile )
{
	std::ofstream out( pszFile, std::ios::binary | std::ios::trunc );
	if( !out.is_open() )return FALSE;
	out << "# GM 账号：名字/等级(1基础 2进阶 3管理)\n";
	for( size_t i = 0; i < m_Accounts.size(); ++i )
		out << m_Accounts[i].name << "/" << m_Accounts[i].level << "\n";
	return TRUE;
}

BOOL CGmMgr::IsGm( const char * pszName )
{
	return GetLevel( pszName ) > 0;
}

int CGmMgr::GetLevel( const char * pszName )
{
	if( pszName == NULL )return 0;
	for( size_t i = 0; i < m_Accounts.size(); ++i )
		if( m_Accounts[i].name == pszName )return m_Accounts[i].level;
	return 0;
}

BOOL CGmMgr::Add( const char * pszName, int nLevel )
{
	if( pszName == NULL || pszName[0] == '\0' )return FALSE;
	if( nLevel < 1 )nLevel = 1;
	if( nLevel > 3 )nLevel = 3;
	for( size_t i = 0; i < m_Accounts.size(); ++i )
	{
		if( m_Accounts[i].name == pszName )
		{
			m_Accounts[i].level = nLevel;
			return TRUE;
		}
	}
	GMACCOUNT acc;
	acc.name = pszName;
	acc.level = nLevel;
	m_Accounts.push_back( acc );
	return TRUE;
}

BOOL CGmMgr::Remove( const char * pszName )
{
	if( pszName == NULL )return FALSE;
	for( size_t i = 0; i < m_Accounts.size(); ++i )
	{
		if( m_Accounts[i].name == pszName )
		{
			m_Accounts.erase( m_Accounts.begin() + i );
			return TRUE;
		}
	}
	return FALSE;
}

int CGmMgr::GetCount()
{
	return (int)m_Accounts.size();
}

GMACCOUNT * CGmMgr::GetByIndex( int nIndex )
{
	if( nIndex < 0 || (size_t)nIndex >= m_Accounts.size() )return NULL;
	return &m_Accounts[(size_t)nIndex];
}
