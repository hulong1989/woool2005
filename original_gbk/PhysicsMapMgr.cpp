#include "StdAfx.h"
#include ".\physicsmapmgr.h"
#include "physicsmap.h"
CPhysicsMapMgr * CPhysicsMapMgr::m_pInstance = NULL;

CPhysicsMapMgr::CPhysicsMapMgr(void):m_pLoadingMap(NULL)
{
	if( m_pInstance == NULL )
		m_pInstance = this;
}

CPhysicsMapMgr::~CPhysicsMapMgr(void)
{
}

VOID CPhysicsMapMgr::OnFoundFile( const char * pszFilename )
{
	if( m_pLoadingMap == NULL )
		m_pLoadingMap = new CPhysicsMap;
	assert( m_pLoadingMap != NULL );
	//	读取地图
	if( !m_pLoadingMap->LoadMap(pszFilename))
		return;
	//	添加到地图名字哈西表
	if( !m_MapNameHash.HAdd( m_pLoadingMap->GetName(), (LPVOID)m_pLoadingMap ) )
		return;
	//	成功后，将指针设置成空，让下一个文件重新new一个对象。
	m_pLoadingMap = NULL;
}

VOID	CPhysicsMapMgr::Load( const char * pszPath )
{
	//	开始查找所有的nmp文件
	StartFind( pszPath, "*.nmp" );
	//	如果最后一个指针读取失败，删除最后一个指针。
	if( m_pLoadingMap != NULL )
		delete m_pLoadingMap;
	//	Ok, 读取结束
}

CVPhysicsMap * CPhysicsMapMgr::GetPhysicsMapByName( const char * pszMapName )
{
	return (CVPhysicsMap*)m_MapNameHash.HGet(pszMapName);
}
