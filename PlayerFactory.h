// PlayerFactory.h  ——  统一的角色创建入口（含读档）
#pragma once

#include "public.h"

class CClientObj;
class CHumanPlayer;

//  创建一个角色：
//    pClientObj  为 NULL 时创建“测试角色”（加入世界的测试列表）
//    nClassId    0 战士 / 1 法师 / 2 道士（仅新角色生效）
//    名字存在存档时自动读取等级/属性/背包/装备
CHumanPlayer * PlayerFactory_Create( const char * pszName, CClientObj * pClientObj, BOOL bTestPlayer, int nClassId = 0 );
