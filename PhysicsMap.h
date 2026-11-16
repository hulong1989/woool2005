// PhysicsMap.h  ——  重建（原快照缺失）
#pragma once

#include "public.h"
#include "vMap.h"

//	物理地图（阻挡图）。
//	原版 *.nmp 是二进制格式，快照中没有格式说明，这里重建为等价的文本格式：
//		[map]
//		name=map0
//		width=64
//		height=64
//		[block]
//		0=000000001111...
//		1=...
//	每行由 0/1 组成（允许逗号或空格分隔），1 表示不可通行。
class CPhysicsMap :
	public CVPhysicsMap
{
public:
	CPhysicsMap();
	virtual ~CPhysicsMap();

	virtual BOOL		LoadMap( const char * pszFilename );
	virtual BOOL		IsBlocked( int x, int y );
	virtual int			GetHeight() { return m_iHeight; }
	virtual int			GetWidth() { return m_iWidth; }
	virtual const char * GetName() { return m_szName; }
	virtual BOOL		AddRefMap( const CVLogicMap * pMap ) { (void)pMap; m_nRefCount++; return TRUE; }

private:
	int					m_iWidth;
	int					m_iHeight;
	int					m_nRefCount;
	char				m_szName[64];
	std::vector<BYTE>	m_Blocks;		// 0 可走，1 阻挡
};
