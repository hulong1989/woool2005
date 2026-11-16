// LogicMap.h  ——  重建（原快照缺失）
#pragma once

#include "public.h"
#include "vMap.h"

class CVPhysicsMap;
class CArea;

//	逻辑地图：物理地图 + 区域(Area)划分 + 对象列表
class CLogicMap :
	public CVLogicMap
{
public:
	CLogicMap(void);
	virtual ~CLogicMap(void);

	//	CVLogicMap
	virtual BOOL		LoadMap( const char * pszFilename );
	virtual e_maptype	GetType() { return m_Type; }
	virtual BOOL		TestFlag( e_mapflag flag ) { return (m_Flag & flag) != 0; }
	virtual BOOL		IsBlocked( int x, int y );
	virtual int			GetWidth() { return m_iWidth; }
	virtual int			GetHeight() { return m_iHeight; }
	virtual BOOL		SendMsg( CMapObject * pSender, const char * pszCodedMsg, int size );
	virtual CVPhysicsMap * GetPhysicsMap() { return m_pPhysicsMap; }
	virtual VOID		SetPhysicsMap( CVPhysicsMap * pPhysicsMap );
	virtual CVArea *	GetArea( int x, int y );
	virtual CVArea *	GetAreaByAreaPos( int x, int y );
	virtual BOOL		ChangeArea( CMapObject * pObject, CVArea * pAreaTo );

	virtual BOOL		LockPos( int x, int y )
	{
		if( m_pLockLayer == NULL )return TRUE;
		if( !VerifyPos( x, y ) )return FALSE;
		int idx = y * m_iWidth + x;
		DWORD mask = (DWORD)1 << ( idx & 31 );
		if( m_pLockLayer[idx >> 5] & mask )return FALSE;
		m_pLockLayer[idx >> 5] |= mask;
		return TRUE;
	}
	virtual BOOL		UnLockPos( int x, int y )
	{
		if( m_pLockLayer == NULL )return TRUE;
		if( !VerifyPos( x, y ) )return FALSE;
		int idx = y * m_iWidth + x;
		m_pLockLayer[idx >> 5] &= ~( (DWORD)1 << ( idx & 31 ) );
		return TRUE;
	}

	virtual const char * GetName() { return m_pName ? m_pName : "NoName"; }
	virtual const char * GetTitle()
	{
		const char * pszTitle = m_DataFile.GetString( "define", "title", NULL );
		if( pszTitle != NULL )return pszTitle;
		return GetName();
	}

	virtual BOOL		AddObject( CMapObject * pObject );
	virtual BOOL		RemoveObject( CMapObject * pObject );
	virtual CMapObject * FindObject( int x, int y, e_object_type type );

	UINT	GetId() { return m_Id; }
	VOID	SetId( UINT id ) { m_Id = id; }

private:
	BOOL	InitAreas();
	BOOL	VerifyPos( int x, int y ) const
	{
		return x >= 0 && y >= 0 && x < m_iWidth && y < m_iHeight;
	}
	BOOL	IsLocked( int x, int y ) const
	{
		if( m_pLockLayer == NULL )return FALSE;
		if( !VerifyPos( x, y ) )return TRUE;
		int idx = y * m_iWidth + x;
		return ( m_pLockLayer[idx >> 5] & ( (DWORD)1 << ( idx & 31 ) ) ) != 0;
	}

	xListHost<CMapObject>					m_xObjList;
	xListHost<CVLogicMap>::xListNode		m_RefNode;
	CDataFile								m_DataFile;
	CVPhysicsMap *							m_pPhysicsMap = NULL;
	CArea **								m_pAreas = NULL;
	DWORD *									m_pLockLayer = NULL;
	UINT									m_Id = 0;
	e_mapflag								m_Flag = (e_mapflag)0;
	e_maptype								m_Type = EMT_NOFIGHT;
	int										m_iWidth = 0;
	int										m_iHeight = 0;
	int										m_iAreaWidth = 0;
	int										m_iAreaHeight = 0;
	int										m_iAreaBlockWidth = 0;
	int										m_iAreaBlockHeight = 0;
	int										m_iMaxBlockElements = 0;
	const char *							m_pName = NULL;
};
