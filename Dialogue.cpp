// Dialogue.cpp
#include "stdafx.h"
#include "Dialogue.h"

CDialogueMgr * CDialogueMgr::m_pInstance = NULL;

CDialogueMgr * CDialogueMgr::GetInstance()
{
	if( m_pInstance == NULL )
		m_pInstance = new CDialogueMgr;
	return m_pInstance;
}

BOOL CDialogueMgr::Load( const char * pszFile )
{
	m_Options.clear();

	CStringFile sf( pszFile );
	for( int i = 0; i < sf.GetLineCount(); ++i )
	{
		const char * pszLine = sf[i];
		if( pszLine == NULL || *pszLine == '\0' || *pszLine == '#' )continue;

		char	szBuffer[256];
		o_strncpy( szBuffer, pszLine, sizeof( szBuffer ) - 1 );
		char *	Params[8];
		if( SearchParam( szBuffer, Params, 8, '/' ) < 5 )continue;

		DIALOGOPTION opt;
		memset( &opt, 0, sizeof( opt ) );
		o_strncpy( opt.szNpc, Params[0], sizeof( opt.szNpc ) - 1 );
		opt.nIndex = atoi( Params[1] );
		o_strncpy( opt.szText, Params[2], sizeof( opt.szText ) - 1 );
		o_strncpy( opt.szAction, Params[3], sizeof( opt.szAction ) - 1 );
		o_strncpy( opt.szParam, Params[4], sizeof( opt.szParam ) - 1 );
		m_Options.push_back( opt );
	}
	return !m_Options.empty();
}

int CDialogueMgr::GetOptions( const char * pszNpc, std::vector<DIALOGOPTION> & options )
{
	options.clear();
	if( pszNpc == NULL )return 0;
	for( size_t i = 0; i < m_Options.size(); ++i )
		if( strcmp( m_Options[i].szNpc, pszNpc ) == 0 )
			options.push_back( m_Options[i] );
	return (int)options.size();
}

const DIALOGOPTION * CDialogueMgr::GetOption( const char * pszNpc, int nIndex )
{
	for( size_t i = 0; i < m_Options.size(); ++i )
		if( strcmp( m_Options[i].szNpc, pszNpc ) == 0 && m_Options[i].nIndex == nIndex )
			return &m_Options[i];
	return NULL;
}
