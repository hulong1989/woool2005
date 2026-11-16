#include "stdafx.h"
#include "GameWorld.h"
#include "PhysicsMapMgr.h"
#include "LogicMapMgr.h"
#include "HumanPlayerMgr.h"
#include "ItemManager.h"
#include "Server.h"
CGameWorld * CGameWorld::m_pInstance = NULL;

CGameWorld::CGameWorld(void):m_pPhysicsMapMgr(NULL),m_pLogicMapMgr(NULL),m_pHumanPlayerMgr(NULL),m_xTestPlayerList(NULL)
{
	if( m_pInstance == NULL )
		m_pInstance = this;
}

CGameWorld::~CGameWorld(void)
{
}

BOOL	CGameWorld::Init()
{
	m_pPhysicsMapMgr = CPhysicsMapMgr::GetInstance();
	m_pPhysicsMapMgr->Load( ".\\data\\maps\\physics" );
	m_pLogicMapMgr = CLogicMapMgr::GetInstance();
	m_pLogicMapMgr->Load( ".\\data\\maps\\logic" );
	m_pHumanPlayerMgr = CHumanPlayerMgr::GetInstance();
	m_pItemManager = CItemManager::GetInstance();
	CStringFile	sf( ".\\data\\baseitem.txt" );
	for( int i = 0;i < sf.GetLineCount();i ++ )
	{
		if( *sf[i] != '#' )
		{
			if( !m_pItemManager->AddItemClassString( sf[i] ) )
			{
				CServer::GetInstance()->GetIoConsole()->OutPut( 255, "无法解析的物品类信息: %s\n错误: %s\n", sf[i], m_pItemManager->GetErrorMsg() );
			}
		}
	}
	//m_pItemManager->AddItemClassString( "赤明天帝/ 5/ 57/738/0/0/ 0/0/0/22/28/0/0/0/0/60/30/2000000/0/48" );
	//m_pItemManager->AddItemClassString( "金鹏宝甲/10/192/293/0/8/14/2/3/ 0/ 1/0/3/0/2/20/30/2000000/0/ 0" );
	//m_pItemManager->AddItemClassString( "金鹏宝甲/11/192/294/0/8/14/2/3/ 0/ 1/0/3/0/2/20/30/2000000/0/ 0" );

	return TRUE;
}

VOID CGameWorld::Update()
{
	xListHost<CMapObject>::xListNode/*<CMapObject>*/ * pNode = NULL;
	for( pNode = m_xTestPlayerList.getHead();pNode != NULL;pNode = pNode->getNext() )
	{
		CAliveObject * p = (CAliveObject*)pNode->getObject();
		p->Update();
		if( p->GetActionType() == AT_STAND )
			p->Walk( Getrand( 8 ), TRUE );
	}

	//	真实玩家也必须更新动作，否则 walk/attack 的动作永远不结束
	CIndexList<CHumanPlayer, MAX_HUMANPLAYER> & players = CHumanPlayerMgr::GetInstance()->GetPlayerList();
	for( CHumanPlayer * pPlayer = players.First(); pPlayer != NULL; pPlayer = players.Next() )
		pPlayer->Update();
}

BOOL CGameWorld::AddPlayer( CHumanPlayer * pPlayer )
{
	UINT	id = pPlayer->GetMapId();
	CVLogicMap * pMap = m_pLogicMapMgr->GetLogicMapById( id );
	if( pMap == NULL )return FALSE;
	if( !pMap->AddObject(pPlayer))
		return FALSE;
	return TRUE;
}

BOOL CGameWorld::RemovePlayer( CHumanPlayer * pPlayer )
{
	CVLogicMap * pMap = pPlayer->GetMap();
	if( pMap != NULL )
		pMap->RemoveObject( pPlayer );	//	where on remove let around see u are disappeared
	//	
	return TRUE;
}

BOOL	CGameWorld::AddTestPlayer( CHumanPlayer * pPlayer )
{
	if( !m_xTestPlayerList.addNode(pPlayer->GetLinkNode(LNI_WORLD)))return FALSE;
	return AddPlayer( pPlayer );
}

BOOL	CGameWorld::RemoveTestPlayer( CHumanPlayer * pPlayer )
{
	if( !RemovePlayer( pPlayer ) )return FALSE;
	return m_xTestPlayerList.removeNode( pPlayer->GetLinkNode(LNI_WORLD));
}
