// ClassTable.cpp  ——  职业成长/经验表加载
#include "stdafx.h"
#include "ClassTable.h"

CPlayerClassMgr * CPlayerClassMgr::m_pInstance = NULL;

CPlayerClassMgr * CPlayerClassMgr::GetInstance()
{
	if( m_pInstance == NULL )
		m_pInstance = new CPlayerClassMgr;
	return m_pInstance;
}

BOOL CPlayerClassMgr::Load( const char * pszClassFile, const char * pszLevelFile )
{
	m_Classes.clear();
	m_LevelNeed.clear();

	//	职业
	CStringFile classes( pszClassFile );
	for( int i = 0; i < classes.GetLineCount(); ++i )
	{
		const char * pszLine = classes[i];
		if( pszLine == NULL || *pszLine == '\0' || *pszLine == '#' )continue;

		char	szBuffer[256];
		o_strncpy( szBuffer, pszLine, sizeof( szBuffer ) - 1 );
		char *	Params[16];
		int		nParam = SearchParam( szBuffer, Params, 16, '/' );
		if( nParam < 11 )continue;

		PLAYERCLASS pc;
		memset( &pc, 0, sizeof( pc ) );
		pc.id = atoi( Params[0] );
		o_strncpy( pc.szName, Params[1], sizeof( pc.szName ) - 1 );
		pc.hpGrow = atoi( Params[2] );
		pc.mpGrow = atoi( Params[3] );
		pc.minDcGrow = atoi( Params[4] );
		pc.maxDcGrow = atoi( Params[5] );
		pc.minAcGrow = atoi( Params[6] );
		pc.maxAcGrow = atoi( Params[7] );
		pc.mcGrow = atoi( Params[8] );
		pc.scGrow = atoi( Params[9] );
		pc.weightGrow = atoi( Params[10] );
		m_Classes.push_back( pc );
	}
	if( m_Classes.empty() )
	{
		//	内置兜底：战/法/道
		PLAYERCLASS pc;
		memset( &pc, 0, sizeof( pc ) );
		pc.id = 0; o_strncpy( pc.szName, "战士", 15 );
		pc.hpGrow = 60; pc.mpGrow = 10; pc.minDcGrow = 2; pc.maxDcGrow = 3;
		pc.minAcGrow = 1; pc.maxAcGrow = 2; pc.weightGrow = 10;
		m_Classes.push_back( pc );
		pc.id = 1; o_strncpy( pc.szName, "法师", 15 );
		pc.hpGrow = 25; pc.mpGrow = 50; pc.maxDcGrow = 1; pc.maxAcGrow = 1;
		pc.mcGrow = 2; pc.weightGrow = 5;
		m_Classes.push_back( pc );
		pc.id = 2; o_strncpy( pc.szName, "道士", 15 );
		pc.hpGrow = 35; pc.mpGrow = 30; pc.maxDcGrow = 1; pc.maxAcGrow = 2;
		pc.scGrow = 2; pc.weightGrow = 8;
		m_Classes.push_back( pc );
	}

	//	经验表
	CStringFile levels( pszLevelFile );
	int nMaxLevel = 0;
	for( int i = 0; i < levels.GetLineCount(); ++i )
	{
		const char * pszLine = levels[i];
		if( pszLine == NULL || *pszLine == '\0' || *pszLine == '#' )continue;
		char	szBuffer[256];
		o_strncpy( szBuffer, pszLine, sizeof( szBuffer ) - 1 );
		char *	Params[4];
		if( SearchParam( szBuffer, Params, 4, '/' ) < 2 )continue;
		int nLevel = atoi( Params[0] );
		if( nLevel > nMaxLevel )nMaxLevel = nLevel;
		if( nLevel >= (int)m_LevelNeed.size() )
			m_LevelNeed.resize( (size_t)nLevel + 1, 0 );
		m_LevelNeed[(size_t)nLevel] = (DWORD)strtoul( Params[1], NULL, 10 );
	}
	if( m_LevelNeed.size() < 2 )
	{
		//	兜底：1..50，need = 100 * level^2
		m_LevelNeed.assign( 51, 0 );
		for( int L = 1; L < 50; ++L )m_LevelNeed[(size_t)L] = (DWORD)( 100 * L * L );
	}
	return TRUE;
}

const PLAYERCLASS * CPlayerClassMgr::Get( int nClassId )
{
	for( size_t i = 0; i < m_Classes.size(); ++i )
		if( m_Classes[i].id == nClassId )return &m_Classes[i];
	return m_Classes.empty() ? NULL : &m_Classes[0];
}

int CPlayerClassMgr::GetCount()
{
	return (int)m_Classes.size();
}

int CPlayerClassMgr::MaxLevel()
{
	return (int)m_LevelNeed.size() - 1;
}

DWORD CPlayerClassMgr::LevelExpNeed( int nLevel )
{
	if( nLevel <= 0 )return 0;
	if( nLevel >= MaxLevel() )return 0;			//	满级
	if( (size_t)nLevel < m_LevelNeed.size() )
		return m_LevelNeed[(size_t)nLevel];
	return (DWORD)( 100 * nLevel * nLevel );
}

const char * CPlayerClassMgr::GetClassName( int nClassId )
{
	const PLAYERCLASS * pc = Get( nClassId );
	return pc != NULL ? pc->szName : "未知";
}
