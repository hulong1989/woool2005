// Npc.h  ——  NPC 实体（商店/传送）
#pragma once

#include "public.h"
#include "MapObject.h"

//	类型：1 商店 / 2 传送
class CNpc :
	public CMapObject
{
public:
	CNpc();
	virtual ~CNpc();

	BOOL Init( const char * pszName, int nType, int nDestMap, int nDestX, int nDestY,
			   const char * pszDialog, WORD x, WORD y );

	virtual e_object_type GetType(){ return OBJ_NPC; }
	virtual UINT GetId(){ return m_Id; }
	VOID SetId( UINT id ){ m_Id = id; }
	const char * GetName(){ return m_szName; }
	const char * GetDialog(){ return m_szDialog; }
	int GetNpcType(){ return m_nType; }
	int GetDestMap(){ return m_nDestMap; }
	int GetDestX(){ return m_nDestX; }
	int GetDestY(){ return m_nDestY; }

	virtual BOOL GetViewmsg( char * pszMsg, int & length );
	virtual BOOL GetOutViewmsg( char * pszMsg, int & length );
	virtual VOID OnEnterMap( CVLogicMap * pMap, CVArea * pArea );
	virtual VOID OnLeaveMap( CVLogicMap * pMap, CVArea * pArea );

private:
	UINT	m_Id;
	int		m_nType;
	int		m_nDestMap;
	int		m_nDestX;
	int		m_nDestY;
	WORD	m_wShape;
	char	m_szName[32];
	char	m_szDialog[128];
};

class CNpcMgr
{
public:
	static CNpcMgr * GetInstance();

	BOOL	Load( const char * pszFile );
	CNpc *	Find( const char * pszName );
	CNpc *	GetByIndex( int nIndex );
	int		GetCount();
	VOID	ClearAll();

private:
	static CNpcMgr * m_pInstance;
	CIndexList<CNpc, 256>	m_Npcs;
};
