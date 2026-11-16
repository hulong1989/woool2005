// GameActions.cpp
#include "stdafx.h"
#include "GameActions.h"
#include "HumanPlayer.h"
#include "LogicMapMgr.h"
#include "Npc.h"
#include "HumanPlayerMgr.h"
#include "Server.h"

BOOL Game_TeleportPlayer( CHumanPlayer * pPlayer, UINT nMapId, int x, int y )
{
	if( pPlayer == NULL )return FALSE;

	CVLogicMap * pMap = CLogicMapMgr::GetInstance()->GetLogicMapById( nMapId );
	if( pMap == NULL )return FALSE;
	if( x < 0 || y < 0 || x >= pMap->GetWidth() || y >= pMap->GetHeight() )
		return FALSE;
	if( pMap->IsBlocked( x, y ) )
		return FALSE;

	if( pPlayer->GetMap() != NULL )
		pPlayer->GetMap()->RemoveObject( pPlayer );

	pPlayer->SetMapId( nMapId );
	pPlayer->setXY( (WORD)x, (WORD)y );

	if( !pMap->AddObject( pPlayer ) )
		return FALSE;

	pPlayer->UpdateProp();
	return TRUE;
}

BOOL Game_IsNearNpc( CHumanPlayer * pPlayer, CNpc * pNpc )
{
	if( pPlayer == NULL || pNpc == NULL )return FALSE;
	if( pPlayer->GetMap() != pNpc->GetMap() )return FALSE;

	int dx = (int)pPlayer->getX() - (int)pNpc->getX();
	int dy = (int)pPlayer->getY() - (int)pNpc->getY();
	if( dx < 0 )dx = -dx;
	if( dy < 0 )dy = -dy;
	return dx <= 3 && dy <= 3;
}

VOID Game_Announce( const char * pszText )
{
	if( pszText == NULL )return;

	if( CServer::GetInstance()->GetIoConsole() != NULL )
		CServer::GetInstance()->GetIoConsole()->OutPut( RGB( 255, 255, 0 ), "【公告】%s\n", pszText );

	CIndexList<CHumanPlayer, MAX_HUMANPLAYER> & players = CHumanPlayerMgr::GetInstance()->GetPlayerList();
	char szMsg[256];
	snprintf( szMsg, sizeof( szMsg ), "【公告】%s", pszText );
	for( CHumanPlayer * p = players.First(); p != NULL; p = players.Next() )
	{
		if( p->CanRecvMsg() )
			p->SendMsg( p->GetId(), SM_CHAT, 0xff00, 0, 0, (LPVOID)szMsg );
	}
}
