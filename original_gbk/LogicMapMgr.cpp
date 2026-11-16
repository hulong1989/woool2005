#include "StdAfx.h"
#include ".\logicmapmgr.h"
#include "logicmap.h"
CLogicMapMgr * CLogicMapMgr::m_pInstance = NULL;

CLogicMapMgr::CLogicMapMgr(void):m_pLoadingMap(NULL)
{
	if( m_pInstance == NULL )
		m_pInstance = this;
}

CLogicMapMgr::~CLogicMapMgr(void)
{
}

VOID CLogicMapMgr::OnFoundFile( const char * pszFilename )
{
	if( m_pLoadingMap == NULL )
	{
		//m_pLoadingMap = new CLogicMap;
		UINT id = m_LogicMaps.New( &m_pLoadingMap );
		if( id == 0 )
			m_pLoadingMap = NULL;
		else
			m_pLoadingMap->SetId( id );
	}

	assert( m_pLoadingMap != NULL );
	//	读取地图
	if( !m_pLoadingMap->LoadMap(pszFilename))
		return;
	//	添加到地图名字哈西表
	if( !m_MapNameHash.HAdd( m_pLoadingMap->GetTitle(), (LPVOID)m_pLoadingMap ) )
		return;
	//	成功后，将指针设置成空，让下一个文件重新new一个对象。
	m_pLoadingMap = NULL;
}

VOID	CLogicMapMgr::Load( const char * pszPath )
{
	//	开始查找所有的ini文件
	StartFind( pszPath, "*.ini" );
	//	如果最后一个指针读取失败，删除最后一个指针。
	if( m_pLoadingMap != NULL )
		delete m_pLoadingMap;
	//	Ok, 读取结束
}

CVLogicMap * CLogicMapMgr::GetLogicMapByTitle( const char * pszMapTitle )
{
	return (CVLogicMap*)m_MapNameHash.HGet(pszMapTitle);
}
