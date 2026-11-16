// PlayerStore.h  ——  角色存档（本地文件版 DB）
// 原工程的物品/角色落库请求发给 DB 服务器；快照中没有 DB 协议，
// 这里提供一个本地文件兜底实现，保证重启不丢档。
#pragma once

#include "public.h"
#include "Quest.h"
#include <vector>
#include <utility>

class CHumanPlayer;

class CPlayerStore
{
public:
	static CPlayerStore * GetInstance();

	//	保存/读取单个角色（角色信息 + 背包 + 装备）
	BOOL Save( CHumanPlayer * pPlayer );
	BOOL Load( const char * pszName, CHARDBINFO & info,
			   std::vector<ITEM> & bag,
			   std::vector<std::pair<int, ITEM> > & equips,
			   DWORD & dwLearnedSkills,
			   std::vector<PLAYERQUEST> & quests,
			   std::vector<int> & achievements,
			   int & nTitle,
			   int & nPointsSpent,
			   std::vector<int> & shopTitles,
			   std::vector<std::string> & codex,
			   std::vector<std::string> & codexRewarded );
	BOOL Exists( const char * pszName );

	//	保存当前全部角色（退出/定时/控制台 save）
	int SaveAll();

private:
	std::string PathFor( const char * pszName ) const;
	void EnsureDir();

	static CPlayerStore * m_pInstance;
};
