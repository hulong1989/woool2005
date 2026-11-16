#pragma once

class CPhysicsMapMgr;
class CLogicMapMgr;
class CHumanPlayerMgr;
class CHumanPlayer;
class CMapObject;
class CItemManager;

class CGameWorld
{
public:
	CGameWorld(void);
	virtual ~CGameWorld(void);
	BOOL	Init();

	VOID	Update();

	BOOL	AddPlayer( CHumanPlayer * pPlayer );
	BOOL	RemovePlayer( CHumanPlayer * pPlayer );
	
	BOOL	AddTestPlayer( CHumanPlayer * pPlayer );
	BOOL	RemoveTestPlayer( CHumanPlayer * pPlayer );
	
	int	GetTestPlayerCount(){ return m_xTestPlayerList.getCount();}

	static CGameWorld * GetInstance()
	{
		if( m_pInstance == NULL )
			m_pInstance = new CGameWorld;
		return m_pInstance;
	}
private:
	static CGameWorld * m_pInstance;
	CPhysicsMapMgr * m_pPhysicsMapMgr;
	CLogicMapMgr * m_pLogicMapMgr;
	CHumanPlayerMgr * m_pHumanPlayerMgr;
	CItemManager * m_pItemManager;
	xListHost<CMapObject>	m_xTestPlayerList;
};
