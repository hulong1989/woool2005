#pragma once
#include "AliveObject.h"
#include "ItemBox.h"
#include "Equipment.h"
#include "ExchangeObj.h"
#include "Quest.h"
#include <vector>
#include <set>
#include <string>
class CClientObj;

typedef struct tagCREATEHUMANDESC
{
	tagCREATEHUMANDESC()
	{
		memset( this, 0, sizeof( *this));
	}
	//tagCREATEHUMANDESC( const char * pszName, BYTE _btClass, BYTE _btSex, BYTE _btHair, BYTE _btFlag, WORD _wLevel, WORD _x, WORD _y, UINT _mapid, CClientObj * pObj )
	//{
	//	o_strncpy( szName, pszName, 18 );
	//	btClass = _btClass;
	//	btSex = _btSex;
	//	btHair = _btHair;
	//	btFlag = _btFlag;
	//	wLevel = _wLevel;
	//	x = _x;
	//	y = _y;
	//	mapid = _mapid;
	//	pClientObj = pObj;
	//}
	CHARDBINFO	dbinfo;
	CClientObj * pClientObj;
}CREATEHUMANDESC;
//	可以用另外的shell来替换自身，shell有存活时间。
class CHumanPlayer :
	public CAliveObject
{
public:
	CHumanPlayer(void);
	virtual ~CHumanPlayer(void);
	BOOL Init( CREATEHUMANDESC & desc );
	e_object_type GetType(){ return OBJ_PLAYER;}
	VOID OnAroundMsg( CMapObject * pSender, const char * pszCodedMsg, int size );
	VOID OnEnterMap( CVLogicMap * pMap, CVArea * pArea );
	DWORD GetFeather();
	DWORD GetHealth();
	DWORD GetStatus();
	//	战斗
	virtual int		GetCurHp(){ return m_Humandesc.dbinfo.hp; }
	virtual VOID	SetCurHp( int nHp );
	virtual BOOL	OnDeath( CAliveObject * pAttacker );
	VOID			AddExp( int nExp );

	//	职业 / 技能（按传奇世界：战/法/道）
	int		GetClass(){ return m_Humandesc.dbinfo.btClass; }
	VOID	ApplyClassStats();
	BOOL	CastSkill( int nSkillId, int nDir );
	BOOL	CastSkillByName( const char * pszName, int nDir );
	//	增益/减益
	VOID	AddBuff( int nType, DWORD dwMs, int nValue );
	int		GetBuffValue( int nType );
	VOID	UpdateBuffs();
	virtual VOID Update();
	BOOL	HasSkill( int nSkillId ){ return ( m_dwLearnedSkills & ( (DWORD)1 << ( nSkillId & 31 ) ) ) != 0; }
	BOOL	LearnSkill( int nSkillId );
	virtual BOOL BeAttack( CAliveObject * pAttacker );
	DWORD	GetLearnedSkills(){ return m_dwLearnedSkills; }
	VOID	SetLearnedSkills( DWORD dwMask ){ m_dwLearnedSkills = dwMask; }
	VOID	RestoreAll();

	//	成就 / 称号
	BOOL	UnlockAchievement( int nId );
	BOOL	HasAchievement( int nId );
	const std::vector<int> & GetAchievements(){ return m_Achievements; }
	VOID	RestoreAchievements( const std::vector<int> & list, int nTitle );
	BOOL	SetTitle( int nId );
	int		GetTitle(){ return m_nTitle; }
	int		GetCompletedQuestCount();
	int		GetAchievementPoints();
	int		GetAvailablePoints();
	int		GetPointsSpent(){ return m_nPointsSpent; }
	BOOL	BuyTitle( int nId, int nCost );
	BOOL	GrantTitle( int nId );
	BOOL	HasShopTitle( int nId );
	const std::vector<int> & GetShopTitles(){ return m_ShopTitles; }
	VOID	RestoreShopTitles( int nSpent, const std::vector<int> & list );
	const char * GetTitleName();

	//	图鉴
	VOID	AddCodex( const char * pszItem );
	VOID	CheckSetRewards();
	BOOL	HasCodex( const char * pszItem );
	const std::set<std::string> & GetCodex(){ return m_Codex; }
	VOID	RestoreCodex( const std::vector<std::string> & list );
	BOOL	HasCodexReward( const char * pszSet );
	VOID	RestoreCodexRewards( const std::vector<std::string> & list );
	int		GetTitleBonus( PROP_INDEX index );

	//	药品 / 金币 / 自然恢复
	BOOL	UseItem( DWORD dwMakeIndex );

	//	任务
	BOOL	AcceptQuest( int nQuestId );
	BOOL	SubmitQuest( int nQuestId );
	int		GetQuestState( int nQuestId );
	int		GetQuestProgress( int nQuestId );
	int		GetQuestCount(){ return m_nQuestCount; }
	const PLAYERQUEST * GetQuestByIndex( int nIndex );
	VOID	OnKillMonster( const char * pszMonster );
	VOID	RestoreQuests( const PLAYERQUEST * pQuests, int nCount );
	const PLAYERQUEST * GetQuestArray(){ return m_Quests; }
	DWORD	GetGold(){ return m_Humandesc.dbinfo.dwGold; }
	CClientObj * GetClientObj(){ return m_pClientObj; }
	BOOL	AddGold( int nDelta );
	VOID	RegenTick( DWORD dwNow );
	DWORD	GetSkillReady( int nSkillId ){ return m_dwSkillReady[nSkillId & 31]; }

	BOOL	GetDBInfo( CHARDBINFO & info );
	DWORD	GetDBId(){ return m_Humandesc.dbinfo.dwDBId;}
	DWORD	GetDBInfoExp(){ return m_Humandesc.dbinfo.dwCurExp;}
	const char * GetName(){ return m_Humandesc.dbinfo.szName;}

	BOOL	GetViewmsg( char * pszMsg, int & length );


	BOOL	CanRecvMsg(){
		if( m_pClientObj == NULL )return FALSE;
		return TRUE;
	}


public:
	BOOL	AddBagItem( ITEM & item );
	BOOL	DropItem( ITEM & item );
	BOOL	PickupItem();
	BOOL	DropBagItem( DWORD dwMakeIndex );

	BOOL	EquipItem( int pos, DWORD dwMakeIndex );
	BOOL	UnEquipItem( int pos, DWORD dwMakeIndex );

	BOOL	Trade();


	CItemBox & GetBag(){ return m_ItemBox;}
	//	重建时补充：控制台/调试需要查询装备位错误信息
	CEquipment & GetEquipments(){ return m_Equipments;}
	int	GetPropValue( PROP_INDEX index );
public:
	VOID	UpdateProp();
	BOOL	AddGroupMember( CHumanPlayer * pObject );
	BOOL	RemoveGroupMember( CHumanPlayer * pObject );

	xListHost<CHumanPlayer> & GetGroupList(){return m_xGroupList;}
	xListHost<CHumanPlayer>::xListNode & GetGroupListNode(){return m_GroupListNode;}

	CExchangeObj & GetExchangeObj(){return m_ExchangeObj;}
protected:
	DWORD	m_dwSkillReady[32];
	DWORD	m_dwShieldUntil;
	struct BUFFSTATE
	{
		int		nType;
		DWORD	dwUntil;
		int		nValue;
	};
	BUFFSTATE	m_Buffs[8];
	int			m_nBuffCount;
	DWORD	m_dwNextRegen;
	DWORD	m_dwLearnedSkills;	//	技能 id 位图
	PLAYERQUEST	m_Quests[QUEST_MAX_PER_PLAYER];
	std::vector<int>	m_Achievements;
	std::vector<int>	m_ShopTitles;
	std::set<std::string>	m_Codex;
	std::set<std::string>	m_CodexRewarded;
	int				m_nPointsSpent;
	int				m_nTitle;
	int		m_nQuestCount;

	xListHost<CHumanPlayer>	m_xGroupList;
	xListHost<CHumanPlayer>::xListNode m_GroupListNode;

	CClientObj * m_pClientObj;
	CREATEHUMANDESC	m_Humandesc;
	CItemBox	m_ItemBox;
	CEquipment m_Equipments;
	CExchangeObj m_ExchangeObj;
//	ITEM		m_Equipments[10];
};

