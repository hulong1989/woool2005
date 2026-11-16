#pragma once

enum	e_maptype
{
	EMT_NOFIGHT,
	EMT_FIGHT,
};
enum	e_mapflag
{
	EMF_PKMAP = 1,
	EMF_NOPK = 2,
	EMF_NOTELEPORT = 4,
	EMF_NODROPITEM = 8,
	EMF_NORIDE = 16,
	EMF_UNDERGROUND = 32,
	EMF_SKY = 64,
	EMF_CANWALKTHROUGH = 128,
};

enum	e_object_type
{
	OBJ_UNKNOWN,
	OBJ_GROUNDMAGIC,
	OBJ_DOWNITEM,
	OBJ_MONSTER,
	OBJ_NPC,
	OBJ_PLAYER,
};

#define MAP_AREA_WIDTH		1
#define MAP_AREA_HEIGHT	1

class CMapObject;
class CVLogicMap;
class CVPhysicsMap;
class CVArea;


class CVArea
{
public:
	//	重建时补充：接口基类必须有虚析构，delete CVArea* 才是良定义行为
	virtual ~CVArea() {}
	virtual CVArea *	GetAroundArea( int dir ) = 0;
	virtual void		SetAroundArea( int dir, CVArea * pArea ) = 0;

	virtual BOOL		SendMsg( CMapObject * pSender, const char * pszCodedMsg, int size ) = 0;
	virtual BOOL		SendAroundMsg( CMapObject * pSender, const char * pszCodedMsg, int size ) = 0;
	virtual BOOL		AddObject( CMapObject * pObject ) = 0;
	virtual BOOL		RemoveObject( CMapObject * pObject ) = 0;
	virtual VOID		SetMap( CVLogicMap * pMap ) = 0;
	virtual CVLogicMap * GetMap() = 0;
	virtual VOID		AppearFor( CMapObject * pObject ) = 0;
	virtual VOID		DisappearFor( CMapObject * pObject ) = 0;

	virtual UINT	GetAreaIndex() = 0;
	virtual WORD	GetAreaX() = 0;
	virtual WORD	GetAreaY() = 0;
	virtual CMapObject * FindObject( WORD x, WORD y, e_object_type type ) = 0;

};

class CVPhysicsMap
{
public:
	virtual ~CVPhysicsMap() {}
	virtual BOOL	LoadMap( const char * pszFilename ) = 0;
	virtual BOOL	IsBlocked( int x, int y ) = 0;
	virtual int		GetHeight() = 0;
	virtual int		GetWidth() = 0;
	virtual const char * GetName() = 0;
	virtual BOOL	AddRefMap( const CVLogicMap *) = 0;
};

class CVLogicMap
{
public:
	virtual ~CVLogicMap() {}
	virtual BOOL		LoadMap( const char * pszFilename ) = 0;
	virtual e_maptype	GetType() = 0;
	virtual BOOL		TestFlag( e_mapflag flag ) = 0;
	virtual BOOL		IsBlocked( int x, int y ) = 0;
	virtual int			GetWidth() = 0;
	virtual int			GetHeight() = 0;
	virtual BOOL		SendMsg( CMapObject * pSender, const char * pszCodedMsg, int size ) = 0;
	virtual CVPhysicsMap * GetPhysicsMap() = 0;
	virtual VOID		SetPhysicsMap( CVPhysicsMap * pPhysicsMap ) = 0;
	virtual CVArea	*	GetArea( int x, int y ) = 0;
	virtual CVArea	*	GetAreaByAreaPos( int x, int y ) = 0;

	virtual BOOL ChangeArea( CMapObject * pObject,  CVArea *  pAreaTo ) = 0;

	virtual BOOL	LockPos( int x, int y ) = 0;
	virtual BOOL	UnLockPos( int x, int y ) = 0;

	virtual const char * GetName() = 0;
	virtual const char * GetTitle() = 0;

	virtual BOOL	AddObject( CMapObject * pObject ) = 0;
	virtual BOOL	RemoveObject( CMapObject * pObject ) = 0;
	virtual CMapObject * FindObject( int x, int y, e_object_type type ) = 0;

};



