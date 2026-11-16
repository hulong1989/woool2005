#include "stdafx.h"
#include "LogicMapMgr.h"
#include "LogicMap.h"
CLogicMapMgr * CLogicMapMgr::m_pInstance = NULL;

CLogicMapMgr::CLogicMapMgr(void):m_pLoadingMap(NULL),m_nLoadingId(0)
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
		m_nLoadingId = id;
		if( id == 0 )
			m_pLoadingMap = NULL;
		else
			m_pLoadingMap->SetId( id );
	}

	assert( m_pLoadingMap != NULL );
	//	读取地图
	if( !m_pLoadingMap->LoadMap(pszFilename))
		return;
	//	地图 id 取文件名中的数字（1.ini -> 1），保证与刷怪/传送配置一致
	{
		const char * pszBase = pszFilename;
		for( const char * p = pszFilename; *p != '\0'; ++p )
			if( *p == '/' || *p == '\\' )pszBase = p + 1;
		UINT nFileId = (UINT)atoi( pszBase );
		if( nFileId != 0 )
			m_pLoadingMap->SetId( nFileId );
		m_MapIdMap[m_pLoadingMap->GetId()] = m_pLoadingMap;
	}

	//	添加到地图名字哈西表
	if( !m_MapNameHash.HAdd( m_pLoadingMap->GetTitle(), (LPVOID)m_pLoadingMap ) )
		return;
	//	成功后，将指针设置成空，让下一个文件重新new一个对象。
	m_pLoadingMap = NULL;
	m_nLoadingId = 0;
}

VOID	CLogicMapMgr::Load( const char * pszPath )
{
	m_MapIdMap.clear();

	//	开始查找所有的ini文件
	StartFind( pszPath, "*.ini" );

	//	如果最后一个指针读取失败，从对象池移除（不能直接 delete，池里还持有它）
	if( m_pLoadingMap != NULL && m_nLoadingId != 0 )
	{
		m_LogicMaps.Del( m_nLoadingId );
		m_pLoadingMap = NULL;
		m_nLoadingId = 0;
	}
	//	Ok, 读取结束
}

CVLogicMap * CLogicMapMgr::GetLogicMapByTitle( const char * pszMapTitle )
{
	return (CVLogicMap*)m_MapNameHash.HGet(pszMapTitle);
}
