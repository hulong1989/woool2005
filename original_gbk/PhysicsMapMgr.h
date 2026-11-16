#pragma once
#include "findfile.h"
class CVPhysicsMap;

class CPhysicsMapMgr :
	public CFindFile
{
public:
	CPhysicsMapMgr(void);
	virtual ~CPhysicsMapMgr(void);

	VOID	Load( const char * pszPath );

	CVPhysicsMap * GetPhysicsMapByName( const char * pszMapName );

	static CPhysicsMapMgr * GetInstance()
	{
		if( m_pInstance == NULL )
			m_pInstance = new CPhysicsMapMgr;
		return m_pInstance;
	}

protected:
	VOID	OnFoundFile( const char * pszFilename );

private:
	static CPhysicsMapMgr * m_pInstance;
	CNameHash	m_MapNameHash;
	CVPhysicsMap * m_pLoadingMap;
};
