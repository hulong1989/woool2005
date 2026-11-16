// Monster.h  ——  怪物（重建：原快照完全没有怪物模块）
#pragma once

#include "public.h"
#include "AliveObject.h"

#include <vector>

struct MONSTERTEMPLATE
{
	char	szName[32];
	int		nMaxHp;
	int		nMinAtk;
	int		nMaxAtk;
	int		nDef;
	int		nExp;
	char	szDropItem[32];
	int		nDropRate;		//	0-100
	int		nLevel;
	int		nShape;
};

class CMonster :
	public CAliveObject
{
public:
	CMonster();
	virtual ~CMonster();

	BOOL	Init( const MONSTERTEMPLATE & tpl, WORD x, WORD y );

	virtual e_object_type GetType(){ return OBJ_MONSTER; }
	virtual const char * GetName(){ return m_szName; }

	virtual int		GetPropValue( PROP_INDEX index );
	virtual int		GetCurHp(){ return m_nHp; }
	virtual VOID	SetCurHp( int nHp );
	virtual BOOL	BeAttack( CAliveObject * pAttacker );
	virtual BOOL	OnDeath( CAliveObject * pAttacker );
	virtual VOID	Update();
	virtual BOOL	GetViewmsg( char * pszMsg, int & length );
	virtual BOOL	GetOutViewmsg( char * pszMsg, int & length );

	BOOL	IsDead() const { return m_bDead; }
	BOOL	IsPet() const { return m_bPet; }
	VOID	SetPet( BOOL b ){ m_bPet = b; }
	VOID	SetOwner( UINT id ){ m_nOwnerId = id; }
	UINT	GetOwner() const { return m_nOwnerId; }
	UINT	GetListId() const { return m_nListId; }
	VOID	SetListId( UINT id ) { m_nListId = id; }

private:
	VOID	Think();
	int		DirTo( int dx, int dy );

	char	m_szName[32];
	int		m_nHp;
	int		m_nMaxHp;
	int		m_nMinAtk;
	int		m_nMaxAtk;
	int		m_nDef;
	int		m_nExp;
	int		m_nLevel;
	int		m_nShape;
	char	m_szDropItem[32];
	int		m_nDropRate;
	UINT	m_dwTargetId;
	DWORD	m_dwNextThink;
	BOOL	m_bDead;
	UINT	m_nListId;
	BOOL	m_bPet;
	UINT	m_nOwnerId;
};

struct MONSTERDROP
{
	char	szItem[32];
	int		nRate;		//	0-100
};

class CMonsterMgr
{
public:
	static CMonsterMgr * GetInstance();

	BOOL		LoadTemplates( const char * pszFile );
	BOOL		LoadDrops( const char * pszFile );
	BOOL		HasDrops( const char * pszMonster );
	int			GetDropCount( const char * pszMonster );
	const MONSTERDROP * GetDropByIndex( const char * pszMonster, int nIndex );
	VOID		RollDrops( const char * pszMonster, CVLogicMap * pMap, WORD x, WORD y );
	CMonster *	Spawn( const char * pszName, UINT mapid, WORD x, WORD y );
	VOID		ResetMap( UINT mapid );		//	清空并重刷某地图的普通怪
	int			CountMap( UINT mapid );
	int			SpawnFromFile( const char * pszFile );
	VOID		Update();
	VOID		ClearAll();
	int			GetCount();
	CMonster *	FindById( UINT nObjectId );

private:
	static CMonsterMgr * m_pInstance;
	std::vector<MONSTERTEMPLATE>	m_Templates;
	std::map<std::string, std::vector<MONSTERDROP> >	m_Drops;
	struct SPAWNDEF
	{
		UINT	nMapId;
		WORD	x;
		WORD	y;
		int		nCount;
		char	szName[32];
	};
	std::vector<SPAWNDEF>		m_Spawns;
	CIndexList<CMonster, 4096>		m_Monsters;
};
