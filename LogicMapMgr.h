#pragma once
#include "FindFile.h"
#include "LogicMap.h"
#define MAX_LOGIC_MAP	1024

class CLogicMapMgr :
	public CFindFile
{
public:
	CLogicMapMgr(void);
	virtual ~CLogicMapMgr(void);

	VOID Load( const char * pszPath );
	CVLogicMap * GetLogicMapByTitle( const char * pszTitleName );
	//	重建时补充：地图 id 由文件名数字决定（1.ini -> 1），避免目录枚举顺序不确定
	CVLogicMap * GetLogicMapById( UINT id )
	{
		std::map<UINT, CVLogicMap *>::iterator it = m_MapIdMap.find( id );
		return it == m_MapIdMap.end() ? NULL : it->second;
	}
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
	std::map<UINT, CVLogicMap *>	m_MapIdMap;
	CLogicMap * m_pLoadingMap;
	UINT		m_nLoadingId;
	CIndexList<CLogicMap,MAX_LOGIC_MAP>	m_LogicMaps;
};
