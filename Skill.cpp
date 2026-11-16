// Skill.cpp  ——  技能表加载
#include "stdafx.h"
#include "Skill.h"

CSkillMgr * CSkillMgr::m_pInstance = NULL;

CSkillMgr * CSkillMgr::GetInstance()
{
	if( m_pInstance == NULL )
		m_pInstance = new CSkillMgr;
	return m_pInstance;
}

BOOL CSkillMgr::Load( const char * pszFile )
{
	m_Skills.clear();

	CStringFile sf( pszFile );
	for( int i = 0; i < sf.GetLineCount(); ++i )
	{
		const char * pszLine = sf[i];
		if( pszLine == NULL || *pszLine == '\0' || *pszLine == '#' )continue;

		char	szBuffer[256];
		o_strncpy( szBuffer, pszLine, sizeof( szBuffer ) - 1 );
		char *	Params[16];
		if( SearchParam( szBuffer, Params, 16, '/' ) < 9 )continue;

		SKILLTEMPLATE sk;
		memset( &sk, 0, sizeof( sk ) );
		sk.id = atoi( Params[0] );
		o_strncpy( sk.szName, Params[1], sizeof( sk.szName ) - 1 );
		sk.nClass = atoi( Params[2] );
		sk.nNeedLevel = atoi( Params[3] );
		sk.nMp = atoi( Params[4] );
		sk.dwCooldown = (DWORD)strtoul( Params[5], NULL, 10 );
		sk.nPower = atoi( Params[6] );
		sk.nType = atoi( Params[7] );
		sk.nRange = atoi( Params[8] );
		m_Skills.push_back( sk );
	}
	return !m_Skills.empty();
}

const SKILLTEMPLATE * CSkillMgr::Get( int nSkillId )
{
	for( size_t i = 0; i < m_Skills.size(); ++i )
		if( m_Skills[i].id == nSkillId )return &m_Skills[i];
	return NULL;
}

const SKILLTEMPLATE * CSkillMgr::FindByName( const char * pszName )
{
	if( pszName == NULL )return NULL;
	for( size_t i = 0; i < m_Skills.size(); ++i )
		if( strcmp( m_Skills[i].szName, pszName ) == 0 )return &m_Skills[i];
	return NULL;
}

const SKILLTEMPLATE * CSkillMgr::GetByIndex( int nIndex )
{
	if( nIndex < 0 || (size_t)nIndex >= m_Skills.size() )return NULL;
	return &m_Skills[(size_t)nIndex];
}

int CSkillMgr::GetCount()
{
	return (int)m_Skills.size();
}
