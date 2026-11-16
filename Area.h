// Area.h  ——  重建（原快照缺失）
#pragma once

#include "public.h"
#include "vMap.h"

//	逻辑地图中的一个区域(Area)，维护九宫格视野
class CArea :
	public CVArea
{
public:
	CArea();
	virtual ~CArea();

	virtual CVArea *	GetAroundArea( int dir )
	{
		if( dir < 0 || dir >= 8 )return NULL;
		return m_pAroundArea[dir];
	}
	virtual void		SetAroundArea( int dir, CVArea * pArea )
	{
		if( dir < 0 || dir >= 8 )return;
		m_pAroundArea[dir] = pArea;
	}

	virtual BOOL		SendMsg( CMapObject * pSender, const char * pszCodedMsg, int size );
	virtual BOOL		SendAroundMsg( CMapObject * pSender, const char * pszCodedMsg, int size );
	virtual BOOL		AddObject( CMapObject * pObject );
	virtual BOOL		RemoveObject( CMapObject * pObject );

	virtual VOID		SetMap( CVLogicMap * pMap ) { m_pMap = pMap; }
	virtual CVLogicMap * GetMap() { return m_pMap; }

	virtual VOID		AppearFor( CMapObject * pObject );
	virtual VOID		DisappearFor( CMapObject * pObject );

	virtual UINT		GetAreaIndex() { return m_dwAreaIndex; }
	virtual WORD		GetAreaX() { return m_wAreaX; }
	virtual WORD		GetAreaY() { return m_wAreaY; }
	virtual CMapObject * FindObject( WORD x, WORD y, e_object_type type );

	VOID				SetAreaInfo( UINT index, WORD x, WORD y )
	{
		m_dwAreaIndex = index;
		m_wAreaX = x;
		m_wAreaY = y;
	}

private:
	xListHost<CMapObject>	m_xObjList;
	CVArea *				m_pAroundArea[8];
	CVLogicMap *			m_pMap = NULL;
	UINT					m_dwAreaIndex = 0;
	WORD					m_wAreaX = 0;
	WORD					m_wAreaY = 0;
};
