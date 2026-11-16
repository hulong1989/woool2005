#include "stdafx.h"
#include "Server.h"
#include "GameWorld.h"
#include "Monster.h"
#include "PlayerStore.h"
#include "ClassTable.h"
#include "Skill.h"
#include "HumanPlayerMgr.h"
#include "HumanPlayer.h"
#include "Npc.h"
#include "Quest.h"
#include "Guild.h"
#include "Boss.h"
#include "Dialogue.h"
#include "Event.h"
#include "Set.h"
#include "Dungeon.h"
#include "Achievement.h"
#include "Title.h"
#include "Gm.h"
#include "GameActions.h"
#include <sys/stat.h>
CServer *CServer::m_pInstance = NULL;

CServer::CServer(void)
{
	m_pInstance = this;
}

CServer::~CServer(void)
{
}

CClientObject * CServer::NewClientObject()
{
	CClientObj * pObj;
	UINT id = m_ClientObjects.New(&pObj);
	if( id == 0 || pObj == NULL )return NULL;
	pObj->Clean();
	pObj->SetId(id);
	return pObj;
}

VOID CServer::DeleteClientObject( CClientObject * pObject )
{
	UINT id = pObject->GetId();
	m_ClientObjects.Del(id);
}

BOOL CServer::InitServer(CSettingFile&s)
{
	CPlayerClassMgr::GetInstance()->Load( "data/classes.txt", "data/levels.txt" );
	CSkillMgr::GetInstance()->Load( "data/skills.txt" );

	m_pGameWorld = CGameWorld::GetInstance();
	BOOL bRet = m_pGameWorld->Init();

	//	重建时补充：怪物模板与地图刷怪（原快照没有怪物模块）
	BOOL bTpl = CMonsterMgr::GetInstance()->LoadTemplates( "data/monsters.txt" );
	CMonsterMgr::GetInstance()->LoadDrops( "data/drops.txt" );
	int nSpawned = CMonsterMgr::GetInstance()->SpawnFromFile( "data/spawns.txt" );

	//	重建时补充：NPC（商人/传送员）与任务
	CNpcMgr::GetInstance()->Load( "data/npcs.txt" );
	CQuestMgr::GetInstance()->Load( "data/quests.txt" );
	CGuildMgr::GetInstance()->Load( "data/guilds.dat" );
	CGuildMgr::GetInstance()->LoadWarehouse( "data/guildware.dat" );
	CSetMgr::GetInstance()->Load( "data/sets.txt" );
	CAchievementMgr::GetInstance()->Load( "data/achievements.txt" );
	CTitleMgr::GetInstance()->Load( "data/titles.txt" );
	CGmMgr::GetInstance()->Load( "data/gm.txt" );
	CBossMgr::GetInstance()->Load( "data/bosses.txt" );
	CBossMgr::GetInstance()->LoadFirstKills( "data/boss_kills.dat" );
	CDungeonMgr::GetInstance()->Load( "data/dungeons.txt" );
	CDungeonMgr::GetInstance()->LoadSeason( "data/season.dat" );
	CDungeonMgr::GetInstance()->LoadFirstClears( "data/dungeon_first.dat" );
	CDungeonMgr::GetInstance()->LoadRecords( "data/dungeon_rank.dat" );
	CDialogueMgr::GetInstance()->Load( "data/dialogues.txt" );
	CEventMgr::GetInstance()->Load( "data/events.txt" );
	if( m_pIoConsole != NULL )
	{
		m_pIoConsole->OutPut( RGB( 160, 160, 160 ), "职业: %d 种，技能: %d 个，最高等级: %d\n",
			CPlayerClassMgr::GetInstance()->GetCount(), CSkillMgr::GetInstance()->GetCount(),
			CPlayerClassMgr::GetInstance()->MaxLevel() );
		m_pIoConsole->OutPut( RGB( 160, 160, 160 ), "怪物模板: %s，刷出怪物: %d，NPC: %d，任务: %d，行会: %d，Boss: %d\n",
			bTpl ? "已加载" : "未找到 data/monsters.txt", nSpawned,
			CNpcMgr::GetInstance()->GetCount(), CQuestMgr::GetInstance()->GetCount(),
			CGuildMgr::GetInstance()->GetCount(), CBossMgr::GetInstance()->GetCount() );
	}
	return bRet;
}

VOID CServer::CleanServer()
{
	//	退出前保存全部角色
	int nSaved = CPlayerStore::GetInstance()->SaveAll();
	if( m_pIoConsole != NULL && nSaved > 0 )
		m_pIoConsole->OutPut( RGB( 160, 160, 160 ), "已保存 %d 个角色存档\n", nSaved );
	CGuildMgr::GetInstance()->Save( "data/guilds.dat" );
	CGuildMgr::GetInstance()->SaveWarehouse( "data/guildware.dat" );
	CBossMgr::GetInstance()->SaveFirstKills( "data/boss_kills.dat" );
	CDungeonMgr::GetInstance()->SaveRecords( "data/dungeon_rank.dat" );
	CDungeonMgr::GetInstance()->SaveFirstClears( "data/dungeon_first.dat" );
	CDungeonMgr::GetInstance()->SaveSeason( "data/season.dat" );
	CMonsterMgr::GetInstance()->ClearAll();
	CNpcMgr::GetInstance()->ClearAll();
}

static void HotReloadData()
{
	static bool s_bInit = false;
	static time_t s_mt[10] = {0};
	static const char * s_files[10] = {
		"data/classes.txt", "data/levels.txt", "data/skills.txt",
		"data/monsters.txt", "data/drops.txt", "data/npcs.txt",
		"data/quests.txt", "data/events.txt", "data/bosses.txt", "data/sets.txt"
	};
	time_t now[10];
	for( int i = 0; i < 10; ++i )
	{
		struct stat st;
		now[i] = ( stat( s_files[i], &st ) == 0 ) ? st.st_mtime : 0;
	}
	if( !s_bInit ){ for( int i = 0; i < 10; ++i )s_mt[i] = now[i]; s_bInit = true; return; }

	bool bClass = now[0] != s_mt[0] || now[1] != s_mt[1];
	bool bSkill = now[2] != s_mt[2];
	bool bMon = now[3] != s_mt[3] || now[4] != s_mt[4];
	bool bNpc = now[5] != s_mt[5];
	bool bQuest = now[6] != s_mt[6];
	bool bEvent = now[7] != s_mt[7];
	bool bBoss = now[8] != s_mt[8];
	bool bSet = now[9] != s_mt[9];
	for( int i = 0; i < 10; ++i )s_mt[i] = now[i];
	if( !( bClass || bSkill || bMon || bNpc || bQuest || bEvent || bBoss || bSet ) )return;

	if( bClass ) CPlayerClassMgr::GetInstance()->Load( "data/classes.txt", "data/levels.txt" );
	if( bSkill ) CSkillMgr::GetInstance()->Load( "data/skills.txt" );
	if( bMon )
	{
		CMonsterMgr::GetInstance()->LoadTemplates( "data/monsters.txt" );
		CMonsterMgr::GetInstance()->LoadDrops( "data/drops.txt" );
	}
	if( bNpc )
	{
		CNpcMgr::GetInstance()->ClearAll();
		CNpcMgr::GetInstance()->Load( "data/npcs.txt" );
	}
	if( bQuest ) CQuestMgr::GetInstance()->Load( "data/quests.txt" );
	if( bEvent ) CEventMgr::GetInstance()->Load( "data/events.txt" );
	if( bBoss ) CBossMgr::GetInstance()->Load( "data/bosses.txt" );
	if( bSet ) CSetMgr::GetInstance()->Load( "data/sets.txt" );
	Game_Announce( "数据表已热更新（无需重启）" );
	CServer::GetInstance()->Log( RGB( 120, 220, 160 ), "数据表已热更新（无需重启）\n" );
}

VOID CServer::Update()
{
	CClientObj * pObject = m_ClientObjects.First();
	while( pObject )
	{
		pObject->Update();
		pObject = m_ClientObjects.Next();
	}
	
	UpdateSCServer();
	UpdateDBServer();

	if( m_pGameWorld != NULL )
		m_pGameWorld->Update();

	//	重建时补充：怪物 AI、Boss 刷新与定时存档
	CMonsterMgr::GetInstance()->Update();
	CBossMgr::GetInstance()->Update();
	CDungeonMgr::GetInstance()->Update();
	{
		static DWORD s_dwLastHot = 0;
		DWORD dwHot = timeGetTime();
		if( dwHot - s_dwLastHot > 1000 )
		{
			s_dwLastHot = dwHot;
			HotReloadData();
		}
	}

	std::string startedEvent;
	BOOL bExpired = FALSE;
	if( CEventMgr::GetInstance()->Update( startedEvent, bExpired ) )
	{
		if( bExpired )
			Game_Announce( "双倍经验活动已结束" );
		if( !startedEvent.empty() )
			Game_Announce( startedEvent.c_str() );
	}

	static DWORD s_dwLastAutoSave = 0;
	DWORD dwNow = timeGetTime();

	//	重建时补充：全服角色自然恢复（每 3 秒 5%）
	CIndexList<CHumanPlayer, MAX_HUMANPLAYER> & players = CHumanPlayerMgr::GetInstance()->GetPlayerList();
	for( CHumanPlayer * pPlayer = players.First(); pPlayer != NULL; pPlayer = players.Next() )
		pPlayer->RegenTick( dwNow );

	if( s_dwLastAutoSave == 0 )s_dwLastAutoSave = dwNow;
	if( dwNow - s_dwLastAutoSave >= 60000 )
	{
		s_dwLastAutoSave = dwNow;
		int nSaved = CPlayerStore::GetInstance()->SaveAll();
		if( m_pIoConsole != NULL && nSaved > 0 )
			m_pIoConsole->OutPut( RGB( 160, 160, 160 ), "自动存档: %d 个角色\n", nSaved );
	}
}

VOID CServer::OnDBMsg( PMIRMSG pMsg, int datasize )
{

}

VOID CServer::OnMASMsg( WORD wCmd, WORD wType, WORD wIndex, const char * pszData, int datasize )
{
	switch( wCmd )
	{
	case	MAS_ENTERGAMESERVER:
		{
			ENTERGAMESERVER * pEnterInfo = (ENTERGAMESERVER*)pszData;
			UINT id = pEnterInfo->nClientId;

			pEnterInfo->result = AddEnterAccount( pEnterInfo->nLoginId, pEnterInfo->nSelCharId, pEnterInfo->szAccount );
			m_SCClientObj.SendMsgAcrossServer( id, MAS_ENTERGAMESERVER, MST_SINGLE, wIndex, pszData, datasize );
		}
		break;
	}
}

VOID CServer::OnSCMsg(PMIRMSG pMsg, int datasize )
{
	switch( pMsg->wCmd )
	{
	case	SCM_FINDSERVER:
		{
			if( pMsg->wParam[0] == SE_OK )
			{
				SERVERADDR * pAddr = (SERVERADDR*)pMsg->data;
				m_pIoConsole->OutPut( RGB(255,0,0), "找到了服务器地址 %s:%d\n", pAddr->addr, pAddr->nPort );
			}
			else
				m_pIoConsole->OutPut( RGB(255,0,0), "没有找到服务器 %s 的地址!\n", pMsg->data );
		}
		break;
	default:
		CBaseServer::OnSCMsg(pMsg, datasize );
		break;
	}
}

VOID CServer::OnSCServerReady()
{
	//CSCClientObj * pObj = (CSCClientObj *)GetSCConnection();
	//if( pObj )
	//{
	//	pObj->FindServer( 0, ST_SELCHARSERVER, "淡抹夕阳" );
	//}
}

SERVER_ERROR CServer::AddEnterAccount( UINT nLoginId, UINT nSelCharId, const char * pszAccount )
{
	ENTERGAMESERVER	*	pEnterInfo = NULL;
	UINT id = m_EnterObjects.New( &pEnterInfo);
	if( id == 0 || pEnterInfo == NULL )return SE_SERVERFULL;
	if( !m_Inthash.HAdd( nLoginId, id ) )
	{
		m_EnterObjects.Del(id);
		return SE_SERVERFULL;
	}
	pEnterInfo->nClientId = id;
	pEnterInfo->nLoginId = nLoginId;
	pEnterInfo->nSelCharId = nSelCharId;
	pEnterInfo->dwEnterTime = timeGetTime();
	strncpy( pEnterInfo->szAccount, pszAccount, 10 );
	pEnterInfo->szAccount[10] = 0;
	return SE_OK;
}

BOOL CServer::GetEnterInfo( UINT nLoginId, UINT nSelCharId, const char * pszAccount, ENTERGAMESERVER & enterinfo )
{
	UINT id = (UINT)m_Inthash.HGet(nLoginId);
	if( id == 0 )return FALSE;
	ENTERGAMESERVER * pEnterInfo = m_EnterObjects.Get(id);
	if( pEnterInfo == NULL )return FALSE;
	if( pEnterInfo->nLoginId != nLoginId || pEnterInfo->nSelCharId != nSelCharId )return FALSE;
	enterinfo = *pEnterInfo;
	m_EnterObjects.Del(id);
	m_Inthash.HDel(nLoginId);
	return TRUE;
}

