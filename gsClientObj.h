#pragma once
enum	e_sendmsg_rage
{
	SMR_AREA,
	SMR_AROUND,
	SMR_MAP,
	SMR_SERVER,
	SMR_WORLD,
	SMR_GROUP,
	SMR_GUILD,
};

enum	e_gm_command
{
	GC_SHOW,
	GC_MAKEITEM,
	GC_DROPITEM,
	GC_MOVE,
	GC_KICK,
	GC_CLEANTESTPLAYER,
	GC_ALLOWSPECIALPOWER,
	GC_DROPITEMTEST,
	GC_CLEANALLITEM,
	GC_SETITEMCLASS,
	GC_SENDAROUNDMSG,
	GC_SENDSINGLEMSG,
	GC_SENDMSG,
	GC_SENDALLMSG,
	GC_CTRL_MAX,
};

class CHumanPlayer;

class CClientObj :
	public CClientObject
{
public:
	CClientObj(void);
	virtual ~CClientObj(void);
	VOID	Clean();
	VOID	Update();
	VOID	OnUnCodeMsg( const char * pszMsg, int size );
	VOID	OnCodedMsg( PMIRMSG	pMsg, int datasize );
	VOID	OnDBMsg( PMIRMSG pMsg, int datasize );
	VOID	OnVerifyString( const char * pszString );
	VOID	OnDisconnect();
public:
	VOID SendActionResult( int x, int y, BOOL bSuccess );
	VOID	SendAddItem( ITEM & item );
	VOID	SendEquipItemResult( BOOL bSuccess, int pos, DWORD dwMakeIndex );
	VOID	SendUnEquipItemResult( BOOL bSuccess, int pos, DWORD dwMakeIndex );
	//	重建时补充：背包/装备/商店结构化快照
	VOID	SendBag();
	VOID	SendEquipment();
	VOID	SendShop();
	VOID	SendSkills();
	VOID	SendQuests();
	VOID	SendMapData();
	VOID	SendTitles();
	VOID	SendCodex();
	VOID	HandleGmCommand( const std::string & args );
	VOID	OnBagCommand( const char * pszCmd );
private:
	VOID ProcClientMsg( PMIRMSG pMsg, int datasize );
	VOID	OnCommand( const char * pszCommand );
	VOID	SendFirstDlg( const char * pszString );
	VOID	SendDialog( const char * pszNpc, const char * pszText );
	VOID	SendAuth( const char * pszToken );
	VOID	SendCharList();
	VOID	SendParty();
	VOID	SendGuild();
	VOID	SendDungeons();
	e_gm_command GetCmCommand( const char * pszCmd );
	VOID	CreateTestItem( const char * pszName, BYTE btStdMode, BYTE btShape, WORD wImage, BYTE btFlag );
	//	VOID	SendRangeMsg( e_sendmsg_rage range, DWORD dwFlag, WORD wCmd, WORD w1, WORD w2, WORD w3, LPVOID lpData, int datasize = -1);
	ENTERGAMESERVER	m_EnterInfo;
	gameserverusermode	m_State;
	CHumanPlayer * m_pPlayer;
	char	m_szToken[33];
	char	m_szAccount[32];
	DWORD	m_dwTokenExpire;
};
