// GameActions.h  ——  控制台与客户端共用的游戏动作
#pragma once

#include "public.h"

class CHumanPlayer;
class CNpc;

//	跨地图传送：校验地图/坐标/阻挡，摘除旧地图、加入新地图并刷新属性
BOOL Game_TeleportPlayer( CHumanPlayer * pPlayer, UINT nMapId, int x, int y );

//	玩家与 NPC 是否在 3 格内（同地图）
BOOL Game_IsNearNpc( CHumanPlayer * pPlayer, CNpc * pNpc );

//	全服公告（控制台 + 所有在线客户端）
VOID Game_Announce( const char * pszText );
