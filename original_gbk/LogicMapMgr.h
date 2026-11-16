#pragma once
#include "findfile.h"
#include "logicmap.h"
#define MAX_LOGIC_MAP	1024

class CLogicMapMgr :
	public CFindFile
{
public:
	CLogicMapMgr(void);
	virtual ~CLogicMapMgr(void);

	VOID Load( const char * pszPath );
	CVLogicMap * GetLogicMapByTitle( const char * pszTitleName );
	CVLogicMap * GetLogicMapById( UINT id ){ return m_LogicMaps.Get(id);}
	static CLogicMapMgr * GetInstance()
	{
		if( m_pInstance == NULL )
			m_pInstance = new CLogicMapMgr;
		return m_pInstance;
	}
protected:
	VOID	OnFoundFile( const char * pszFilename );

private:
	static CLogicMapMgr * m_pInstance;
	CNameHash	m_MapNameHash;
	CLogicMap * m_pLoadingMap;
	CIndexList<CLogicMap,MAX_LOGIC_MAP>	m_LogicMaps;
};
