// gsClientObj.cpp  ——  重建（原快照缺失）
// 单个客户端连接的业务实现：登录、进入游戏、GM 命令、消息分发。
#include "stdafx.h"
#include "gsClientObj.h"
#include "HumanPlayer.h"
#include "HumanPlayerMgr.h"
#include "GameWorld.h"
#include "Server.h"
#include "ItemManager.h"
#include "PlayerFactory.h"
#include "PlayerStore.h"
#include "Npc.h"
#include "GameActions.h"
#include "Quest.h"
#include "Skill.h"
#include "Dialogue.h"
#include "Dungeon.h"
#include "Achievement.h"
#include "Title.h"
#include "Set.h"
#include "Gm.h"
#include "Monster.h"
#include "LogicMap.h"
#include <sstream>
#include "Party.h"
#include "Guild.h"
#include "Trade.h"
#include "Auth.h"
#include <fstream>
#include <sstream>

#include <string>

CClientObj::CClientObj(void)
	: m_pPlayer(NULL), m_State(GUM_NONE)
{
	memset( &m_EnterInfo, 0, sizeof( m_EnterInfo ) );
	m_szToken[0] = '\0';
	m_szAccount[0] = '\0';
	m_dwTokenExpire = 0;
	m_dwTokenExpire = 0;
}

CClientObj::~CClientObj(void)
{
}

VOID CClientObj::Clean()
{
	CClientObject::Clean();
	m_pPlayer = NULL;
	m_State = GUM_NONE;
	memset( &m_EnterInfo, 0, sizeof( m_EnterInfo ) );
	m_szToken[0] = '\0';
}

VOID CClientObj::Update()
{
	CClientObject::Update();
}

//	把 "战士/法师/道士" 或 "0/1/2" 解析成职业 id
static int gs_ParseClass( const char * pszClass )
{
	if( pszClass == NULL || *pszClass == '\0' )return 0;
	if( strcmp( pszClass, "战士" ) == 0 )return 0;
	if( strcmp( pszClass, "法师" ) == 0 )return 1;
	if( strcmp( pszClass, "道士" ) == 0 )return 2;
	int n = atoi( pszClass );
	return ( n >= 0 && n <= 2 ) ? n : 0;
}

//	新角色起步装备：空背包 + 空装备时发放一次（测试机器人不走这里）
static void gs_GiveStarterKit( CHumanPlayer * pPlayer )
{
	if( pPlayer == NULL )return;
	if( pPlayer->GetBag().CalcWeight() > 0 )return;
	for( int i = 0; i < _U_MAX; ++i )
		if( pPlayer->GetEquipments()[i].dwMakeIndex != 0 )return;

	CItemManager * pMgr = CItemManager::GetInstance();
	if( pMgr == NULL )return;

	ITEM item;
	ITEM out;
	if( pMgr->MakeupTemplateItem( "木剑", item ) )
	{
		item.dwMakeIndex = CItemManager::NewMakeIndex();
		ZeroMemory( &out, sizeof( out ) );
		pPlayer->GetEquipments().EquipItem( _U_WEAPON, item, out );
	}
	if( pMgr->MakeupTemplateItem( "布衣(男)", item ) )
	{
		item.dwMakeIndex = CItemManager::NewMakeIndex();
		ZeroMemory( &out, sizeof( out ) );
		pPlayer->GetEquipments().EquipItem( _U_DRESS, item, out );
	}

	const char * pszPotion[] = { "金创药", "魔法药" };
	const int nCount[] = { 10, 5 };
	for( int k = 0; k < 2; ++k )
	{
		for( int i = 0; i < nCount[k]; ++i )
		{
			if( !pMgr->MakeupTemplateItem( pszPotion[k], item ) )break;
			item.dwMakeIndex = CItemManager::NewMakeIndex();
			pPlayer->AddBagItem( item );
		}
	}
	pPlayer->UpdateProp();
	printf( "[新角色] %s 已发放木剑/布衣/金创药x10/魔法药x5\n", pPlayer->GetName() );
}

//	账号库：data/accounts.txt（name/salt/hash）
struct GS_ACCOUNT { std::string name, salt, hash; };
static std::vector<GS_ACCOUNT> g_accounts;
static bool g_accounts_loaded = false;

static void gs_AccountLoad()
{
	if( g_accounts_loaded )return;
	g_accounts_loaded = true;
	g_accounts.clear();
	std::ifstream f( "data/accounts.txt" );
	std::string line;
	while( std::getline( f, line ) )
	{
		if( line.empty() || line[0] == '#' )continue;
		size_t a = line.find( '/' );
		size_t b = line.find( '/', a == std::string::npos ? 0 : a + 1 );
		if( a == std::string::npos || b == std::string::npos )continue;
		GS_ACCOUNT acc;
		acc.name = line.substr( 0, a );
		acc.salt = line.substr( a + 1, b - a - 1 );
		acc.hash = line.substr( b + 1 );
		g_accounts.push_back( acc );
	}
}

static void gs_AccountSave()
{
	std::ofstream f( "data/accounts.txt" );
	f << "# 账号: name/salt/hash\n";
	for( size_t i = 0; i < g_accounts.size(); ++i )
		f << g_accounts[i].name << '/' << g_accounts[i].salt << '/' << g_accounts[i].hash << '\n';
}

static GS_ACCOUNT * gs_AccountFind( const std::string & name )
{
	gs_AccountLoad();
	for( size_t i = 0; i < g_accounts.size(); ++i )
		if( g_accounts[i].name == name )return &g_accounts[i];
	return NULL;
}

static bool gs_AccountRegister( const std::string & name, const std::string & pass )
{
	gs_AccountLoad();
	if( name.empty() || pass.empty() )return false;
	if( gs_AccountFind( name ) != NULL )return false;
	GS_ACCOUNT acc;
	acc.name = name;
	acc.salt = Auth_RandomHex( 8 );
	acc.hash = Auth_PasswordHash( acc.salt, pass );
	g_accounts.push_back( acc );
	gs_AccountSave();
	return true;
}

static bool gs_AccountVerify( const std::string & name, const std::string & pass )
{
	GS_ACCOUNT * acc = gs_AccountFind( name );
	if( acc == NULL )return false;
	return Auth_PasswordHash( acc->salt, pass ) == acc->hash;
}

static bool gs_AccountChangePass( const std::string & name, const std::string & oldPass, const std::string & newPass )
{
	GS_ACCOUNT * acc = gs_AccountFind( name );
	if( acc == NULL )return false;
	if( Auth_PasswordHash( acc->salt, oldPass ) != acc->hash )return false;
	if( newPass.size() < 4 )return false;
	acc->salt = Auth_RandomHex( 8 );
	acc->hash = Auth_PasswordHash( acc->salt, newPass );
	gs_AccountSave();
	return true;
}

struct GS_CHAR { std::string account, name; int cls; };
static std::vector<GS_CHAR> g_chars;
static bool g_chars_loaded = false;

static void gs_CharLoad()
{
	if( g_chars_loaded )return;
	g_chars_loaded = true;
	g_chars.clear();
	std::ifstream f( "data/characters.txt" );
	std::string line;
	while( std::getline( f, line ) )
	{
		if( line.empty() || line[0] == '#' )continue;
		size_t a = line.find( '/' );
		size_t b = line.find( '/', a == std::string::npos ? 0 : a + 1 );
		if( a == std::string::npos || b == std::string::npos )continue;
		GS_CHAR c;
		c.account = line.substr( 0, a );
		c.name = line.substr( a + 1, b - a - 1 );
		c.cls = atoi( line.substr( b + 1 ).c_str() );
		g_chars.push_back( c );
	}
}

static void gs_CharSave()
{
	std::ofstream f( "data/characters.txt" );
	f << "# account/character/class\n";
	for( size_t i = 0; i < g_chars.size(); ++i )
		f << g_chars[i].account << '/' << g_chars[i].name << '/' << g_chars[i].cls << '\n';
}

static bool gs_CharNameExists( const std::string & name )
{
	gs_CharLoad();
	for( size_t i = 0; i < g_chars.size(); ++i )
		if( g_chars[i].name == name )return true;
	return CPlayerStore::GetInstance()->Exists( name.c_str() ) != FALSE;
}

static bool gs_CharAdd( const std::string & account, const std::string & name, int cls )
{
	gs_CharLoad();
	for( size_t i = 0; i < g_chars.size(); ++i )
		if( g_chars[i].name == name )return false;
	GS_CHAR c; c.account = account; c.name = name; c.cls = cls;
	g_chars.push_back( c ); gs_CharSave();
	return true;
}

static GS_CHAR * gs_CharFind( const std::string & account, const std::string & name )
{
	gs_CharLoad();
	for( size_t i = 0; i < g_chars.size(); ++i )
		if( g_chars[i].account == account && g_chars[i].name == name )return &g_chars[i];
	return NULL;
}

static bool gs_CharRemove( const std::string & account, const std::string & name )
{
	gs_CharLoad();
	for( size_t i = 0; i < g_chars.size(); ++i )
	{
		if( g_chars[i].account == account && g_chars[i].name == name )
		{
			g_chars.erase( g_chars.begin() + i );
			gs_CharSave();
			return true;
		}
	}
	return false;
}

static void gs_CharEnsureMigration( const std::string & account )
{
	gs_CharLoad();
	for( size_t i = 0; i < g_chars.size(); ++i )
		if( g_chars[i].account == account )return;
	if( !CPlayerStore::GetInstance()->Exists( account.c_str() ) )return;
	CHARDBINFO info; std::vector<ITEM> bag; std::vector<std::pair<int, ITEM> > eq;
	DWORD sk = 0; std::vector<PLAYERQUEST> qs; std::vector<int> ach; int title = 0, pts = 0;
	std::vector<int> shop; std::vector<std::string> codex, rewarded;
	if( CPlayerStore::GetInstance()->Load( account.c_str(), info, bag, eq, sk, qs, ach, title, pts, shop, codex, rewarded ) )
	{
		GS_CHAR c; c.account = account; c.name = account; c.cls = info.btClass;
		g_chars.push_back( c ); gs_CharSave();
	}
}

static int gs_HexVal( char c )
{
	if( c >= '0' && c <= '9' )return c - '0';
	if( c >= 'a' && c <= 'f' )return c - 'a' + 10;
	if( c >= 'A' && c <= 'F' )return c - 'A' + 10;
	return -1;
}

static std::string gs_DecryptCommand( const char * token, const std::string & body )
{
	size_t sp = body.find( ' ' );
	if( sp == std::string::npos )return "";
	unsigned long long counter = strtoull( body.substr( 0, sp ).c_str(), NULL, 16 );
	std::string hex = body.substr( sp + 1 );
	std::string seed( token );
	for( int i = 0; i < 8; ++i )
		seed.push_back( (char)( ( counter >> ( i * 8 ) ) & 0xFF ) );
	std::string ks = Auth_Sha256Hex( seed );
	std::string out;
	for( size_t i = 0; i + 1 < hex.size(); i += 2 )
	{
		int hi = gs_HexVal( hex[i] ), lo = gs_HexVal( hex[i + 1] );
		if( hi < 0 || lo < 0 )break;
		out.push_back( (char)( ( ( hi << 4 ) | lo ) ^ (unsigned char)ks[( i / 2 ) % ks.size()] ) );
	}
	return out;
}

//	进入游戏：创建（或从存档恢复）一个 CHumanPlayer 并放入世界
static CHumanPlayer * gs_CreatePlayer( CClientObj * pClient, const char * pszName, int nClassId )
{
	CHumanPlayer * pPlayer = PlayerFactory_Create( pszName, pClient, FALSE, nClassId );
	gs_GiveStarterKit( pPlayer );
	return pPlayer;
}

static BOOL gs_IsQuestNpcNear( CHumanPlayer * pPlayer, const QUESTTEMPLATE * q )
{
	if( pPlayer == NULL || q == NULL )return FALSE;
	CNpc * pNpc = CNpcMgr::GetInstance()->Find( q->szNpc );
	return pNpc != NULL && Game_IsNearNpc( pPlayer, pNpc );
}

VOID CClientObj::OnUnCodeMsg( const char * pszMsg, int size )
{
	if( pszMsg == NULL || size <= 0 )return;

	std::string cmd( pszMsg, (size_t)size );
	while( !cmd.empty() && ( cmd[cmd.size() - 1] == '\r' || cmd[cmd.size() - 1] == '\n' ) )
		cmd.erase( cmd.size() - 1 );

	//	登录后命令可走加密通道：enc <counterHex> <cipherHex>
	if( cmd.compare( 0, 4, "enc " ) == 0 && m_szToken[0] != 0 )
	{
		std::string plain = gs_DecryptCommand( m_szToken, cmd.substr( 4 ) );
		if( !plain.empty() )cmd = plain;
	}

	//	会话 token 30 分钟有效；过期后拒绝业务命令
	if( m_szToken[0] != 0 && timeGetTime() > m_dwTokenExpire )
	{
		m_szToken[0] = '\0';
		m_dwTokenExpire = 0;
		SendFirstDlg( "会话已过期，请重新登录。" );
		return;
	}
	//	有效命令刷新有效期
	if( m_szToken[0] != 0 )m_dwTokenExpire = timeGetTime() + 30 * 60 * 1000;

	//	调试用文本协议（真实客户端使用编码协议，见 OnCodedMsg）
	if( cmd.compare( 0, 6, "enter " ) == 0 )
	{
		if( m_pPlayer != NULL )
		{
			SendFirstDlg( "你已经在游戏中了。" );
			return;
		}
		std::string args = cmd.substr( 6 );
		while( !args.empty() && args[0] == ' ' )args.erase( 0, 1 );

		std::string name = args;
		std::string cls;
		size_t sp = args.find( ' ' );
		if( sp != std::string::npos )
		{
			name = args.substr( 0, sp );
			cls = args.substr( sp + 1 );
			while( !cls.empty() && cls[0] == ' ' )cls.erase( 0, 1 );
		}
		if( name.empty() )name = "测试玩家";

		m_pPlayer = gs_CreatePlayer( this, name.c_str(), gs_ParseClass( cls.c_str() ) );
		if( m_pPlayer == NULL )
		{
			SendFirstDlg( "进入游戏失败：地图未加载或服务器已满。" );
			return;
		}
		m_State = GUM_PLAYING;
		SendFirstDlg( "欢迎进入游戏。" );
		return;
	}

	if( cmd.compare( 0, 9, "register " ) == 0 || cmd.compare( 0, 6, "login " ) == 0 )
	{
		bool bReg = ( cmd.compare( 0, 9, "register " ) == 0 );
		std::istringstream iss( cmd.substr( bReg ? 9 : 6 ) );
		std::string name, pass, cls;
		iss >> name >> pass >> cls;
		if( name.empty() || pass.empty() ){ SendFirstDlg( "账号或密码为空。" ); return; }
		if( bReg )
		{
			SendFirstDlg( gs_AccountRegister( name, pass ) ? "注册成功，请登录。" : "注册失败（账号已存在）。" );
			return;
		}
		if( !gs_AccountVerify( name, pass ) ){ SendFirstDlg( "账号或密码错误。" ); return; }
		if( m_pPlayer != NULL ){ SendFirstDlg( "你已经在游戏中了。" ); return; }
		o_strncpy( m_szAccount, name.c_str(), sizeof( m_szAccount ) - 1 );
		o_strncpy( m_szToken, Auth_RandomHex( 16 ).c_str(), sizeof( m_szToken ) - 1 );
		m_dwTokenExpire = timeGetTime() + 30 * 60 * 1000;
		SendAuth( m_szToken );
		SendFirstDlg( "登录成功，请选择角色。" );
		return;
	}

	if( cmd.compare( 0, 11, "changepass " ) == 0 )
	{
		std::istringstream iss( cmd.substr( 11 ) );
		std::string oldPass, newPass;
		iss >> oldPass >> newPass;
		if( m_pPlayer == NULL ){ SendFirstDlg( "请先登录。" ); return; }
		SendFirstDlg( gs_AccountChangePass( m_pPlayer->GetName(), oldPass, newPass )
			? "密码修改成功。" : "密码修改失败（旧密码错误或新密码过短）。" );
		return;
	}

	if( cmd == "charlist" )
	{
		SendCharList();
		return;
	}
	if( cmd.compare( 0, 11, "createchar " ) == 0 )
	{
		if( m_szAccount[0] == 0 ){ SendFirstDlg( "请先登录账号。" ); return; }
		std::istringstream iss( cmd.substr( 11 ) );
		std::string name, clsStr;
		iss >> name >> clsStr;
		if( name.size() < 2 || name.size() > 20 || name.find( '/' ) != std::string::npos )
		{
			SendFirstDlg( "角色名长度需在 2-20 字节。" );
			return;
		}
		if( gs_CharNameExists( name ) ){ SendFirstDlg( "角色名已存在。" ); return; }
		int cls = gs_ParseClass( clsStr.c_str() );
		CHumanPlayer * p = PlayerFactory_Create( name.c_str(), NULL, FALSE, cls );
		if( p == NULL ){ SendFirstDlg( "创建失败（地图未加载或角色已满）。" ); return; }
		gs_GiveStarterKit( p );
		CPlayerStore::GetInstance()->Save( p );
		CGameWorld::GetInstance()->RemovePlayer( p );
		CHumanPlayerMgr::GetInstance()->DeletePlayer( p );
		gs_CharAdd( m_szAccount, name, cls );
		SendFirstDlg( "角色创建成功。" );
		SendCharList();
		return;
	}
	if( cmd.compare( 0, 11, "selectchar " ) == 0 )
	{
		if( m_szAccount[0] == 0 ){ SendFirstDlg( "请先登录账号。" ); return; }
		std::string name = cmd.substr( 11 );
		while( !name.empty() && name[0] == ' ' )name.erase( 0, 1 );
		GS_CHAR * c = gs_CharFind( m_szAccount, name );
		if( c == NULL ){ SendFirstDlg( "没有这个角色。" ); return; }
		if( m_pPlayer != NULL ){ SendFirstDlg( "已经在游戏中了。" ); return; }
		m_pPlayer = PlayerFactory_Create( name.c_str(), this, FALSE, c->cls );
		if( m_pPlayer == NULL ){ SendFirstDlg( "进入游戏失败。" ); return; }
		m_State = GUM_PLAYING;
		SendFirstDlg( "欢迎进入游戏。" );
		return;
	}

	if( cmd.compare( 0, 11, "deletechar " ) == 0 )
	{
		if( m_szAccount[0] == 0 ){ SendFirstDlg( "请先登录账号。" ); return; }
		std::string name = cmd.substr( 11 );
		while( !name.empty() && name[0] == ' ' )name.erase( 0, 1 );
		if( m_pPlayer != NULL && name == m_pPlayer->GetName() ){ SendFirstDlg( "角色在线，不能删除。" ); return; }
		if( !gs_CharRemove( m_szAccount, name ) ){ SendFirstDlg( "没有这个角色。" ); return; }
		std::string path = "data/players/" + name + ".dat";
		std::remove( path.c_str() );
		SendFirstDlg( "角色已删除。" );
		SendCharList();
		return;
	}
	if( cmd.compare( 0, 11, "renamechar " ) == 0 )
	{
		if( m_szAccount[0] == 0 ){ SendFirstDlg( "请先登录账号。" ); return; }
		std::istringstream iss( cmd.substr( 11 ) );
		std::string oldName, newName;
		iss >> oldName >> newName;
		if( newName.size() < 2 || newName.size() > 20 || newName.find( '/' ) != std::string::npos )
		{ SendFirstDlg( "新角色名长度需在 2-20 字节。" ); return; }
		if( gs_CharNameExists( newName ) ){ SendFirstDlg( "新角色名已存在。" ); return; }
		GS_CHAR * c = gs_CharFind( m_szAccount, oldName );
		if( c == NULL ){ SendFirstDlg( "没有这个角色。" ); return; }
		if( m_pPlayer != NULL && oldName == m_pPlayer->GetName() ){ SendFirstDlg( "角色在线，不能改名。" ); return; }
		std::string oldPath = "data/players/" + oldName + ".dat";
		std::string newPath = "data/players/" + newName + ".dat";
		if( std::rename( oldPath.c_str(), newPath.c_str() ) != 0 )
		{ SendFirstDlg( "改名失败（存档不存在）。" ); return; }
		c->name = newName;
		gs_CharSave();
		SendFirstDlg( "角色改名成功。" );
		SendCharList();
		return;
	}

	if( cmd == "whoami" )
	{
		char szBuffer[128];
		snprintf( szBuffer, sizeof( szBuffer ), "client=%u state=%d player=%s",
			GetId(), (int)m_State, m_pPlayer ? m_pPlayer->GetName() : "(无)" );
		SendRaw( szBuffer, (int)strlen( szBuffer ) );
		return;
	}

	if( m_pPlayer != NULL )
	{
		if( cmd == "gmhelp" )
		{
			HandleGmCommand( "help" );
			return;
		}
		if( cmd.compare( 0, 3, "gm " ) == 0 )
		{
			HandleGmCommand( cmd.substr( 3 ) );
			return;
		}
		if( cmd.compare( 0, 4, "say " ) == 0 )
		{
			m_pPlayer->Say( "%s", cmd.c_str() + 4 );
			return;
		}
		if( cmd == "bag" ){ SendBag(); return; }
		if( cmd == "equipment" ){ SendEquipment(); return; }
		if( cmd == "shop" ){ SendShop(); return; }
		if( cmd == "skills" ){ SendSkills(); return; }
		if( cmd == "questlog" ){ SendQuests(); return; }
		if( cmd == "mapdata" ){ SendMapData(); return; }
		if( cmd == "titlelist" ){ SendTitles(); return; }
		if( cmd == "codex" ){ SendCodex(); return; }
		if( cmd.compare( 0, 9, "usetitle " ) == 0 )
		{
			m_pPlayer->SetTitle( atoi( cmd.c_str() + 9 ) );
			SendTitles();
			return;
		}
		if( cmd.compare( 0, 9, "buytitle " ) == 0 )
		{
			int nId = atoi( cmd.c_str() + 9 );
			TITLEDEF * pTitle = CTitleMgr::GetInstance()->Find( nId );
			if( pTitle != NULL && m_pPlayer->BuyTitle( pTitle->id, pTitle->nCost ) )
			{
				char szMsg[96];
				snprintf( szMsg, sizeof( szMsg ), "兑换称号【%s】成功", pTitle->szName );
				SendFirstDlg( szMsg );
			}
			else
			{
				SendFirstDlg( "兑换失败（点数不足或已拥有）" );
			}
			SendTitles();
			return;
		}
		if( cmd.compare( 0, 4, "use " ) == 0 ||
			cmd.compare( 0, 10, "equipauto " ) == 0 ||
			cmd.compare( 0, 8, "unequip " ) == 0 ||
			cmd.compare( 0, 9, "sellitem " ) == 0 ||
			cmd.compare( 0, 4, "buy " ) == 0 )
		{
			OnBagCommand( cmd.c_str() );
			return;
		}
		if( cmd.compare( 0, 7, "attack " ) == 0 )
		{
			m_pPlayer->Attack( atoi( cmd.c_str() + 7 ) & 7 );
			return;
		}
		if( cmd == "pickup" )
		{
			m_pPlayer->PickupItem();
			return;
		}
		if( cmd.compare( 0, 5, "cast " ) == 0 )
		{
			m_pPlayer->CastSkillByName( cmd.c_str() + 5, (int)m_pPlayer->GetDirection() );
			return;
		}
		if( cmd == "restore" )
		{
			m_pPlayer->RestoreAll();
			SendFirstDlg( "已恢复满血满蓝。" );
			return;
		}
		if( cmd.compare( 0, 5, "talk " ) == 0 )
		{
			CNpc * pNpc = CNpcMgr::GetInstance()->Find( cmd.c_str() + 5 );
			if( pNpc == NULL ){ SendFirstDlg( "找不到这个 NPC。" ); return; }
			if( !Game_IsNearNpc( m_pPlayer, pNpc ) ){ SendFirstDlg( "你离 NPC 太远了。" ); return; }
			SendFirstDlg( pNpc->GetDialog() );

			std::vector<DIALOGOPTION> options;
			int nOptions = CDialogueMgr::GetInstance()->GetOptions( pNpc->GetName(), options );
			for( int i = 0; i < nOptions; ++i )
			{
				char szMsg[128];
				snprintf( szMsg, sizeof( szMsg ), "%d. %s", options[i].nIndex, options[i].szText );
				SendFirstDlg( szMsg );
			}
			SendDialog( pNpc->GetName(), pNpc->GetDialog() );
			return;
		}
		if( cmd.compare( 0, 8, "npcshop " ) == 0 )
		{
			CNpc * pNpc = CNpcMgr::GetInstance()->Find( cmd.c_str() + 8 );
			if( pNpc == NULL ){ SendFirstDlg( "找不到这个 NPC。" ); return; }
			if( !Game_IsNearNpc( m_pPlayer, pNpc ) ){ SendFirstDlg( "你离 NPC 太远了。" ); return; }
			if( pNpc->GetNpcType() != 1 ){ SendFirstDlg( "对方不是商人。" ); return; }

			CStringFile shop( "data/shop.txt" );
			for( int i = 0; i < shop.GetLineCount(); ++i )
			{
				const char * pszLine = shop[i];
				if( pszLine == NULL || *pszLine == '\0' || *pszLine == '#' )continue;
				char szLine[128];
				o_strncpy( szLine, pszLine, sizeof( szLine ) - 1 );
				char * Params[4];
				if( SearchParam( szLine, Params, 4, '/' ) < 2 )continue;
				char szMsg[160];
				snprintf( szMsg, sizeof( szMsg ), "%s  %s 金币", Params[0], Params[1] );
				SendFirstDlg( szMsg );
			}
			return;
		}
		if( cmd.compare( 0, 6, "trade " ) == 0 )
		{
			std::string args = cmd.substr( 6 );
			while( !args.empty() && args[0] == ' ' )args.erase( 0, 1 );
			if( args.compare( 0, 4, "add " ) == 0 )
			{
				DWORD id = (DWORD)strtoul( args.c_str() + 4, NULL, 10 );
				if( !CTradeMgr::GetInstance()->AddItem( m_pPlayer->GetName(), id ) )
					SendFirstDlg( "放入失败（物品不存在/已锁定）。" );
				return;
			}
			if( args.compare( 0, 5, "gold " ) == 0 )
			{
				DWORD gold = (DWORD)strtoul( args.c_str() + 5, NULL, 10 );
				if( !CTradeMgr::GetInstance()->SetGold( m_pPlayer->GetName(), gold ) )
					SendFirstDlg( "设置金币失败（金币不足/已锁定）。" );
				return;
			}
			if( args == "lock" ){ CTradeMgr::GetInstance()->Lock( m_pPlayer->GetName() ); return; }
			if( args == "confirm" ){ CTradeMgr::GetInstance()->Confirm( m_pPlayer->GetName() ); return; }
			if( args == "cancel" ){ CTradeMgr::GetInstance()->Cancel( m_pPlayer->GetName() ); return; }
			CHumanPlayer * pTarget = CHumanPlayerMgr::GetInstance()->FindbyName( args.c_str() );
			if( pTarget == NULL ){ SendFirstDlg( "对方不在线。" ); return; }
			if( !CTradeMgr::GetInstance()->Open( m_pPlayer->GetName(), args.c_str() ) )
				SendFirstDlg( "发起交易失败（距离/地图/已有交易）。" );
			return;
		}
		if( cmd == "partylist" ){ SendParty(); return; }
		if( cmd == "guildinfo" ){ SendGuild(); return; }
		if( cmd == "dungeonlist" ){ SendDungeons(); return; }
		if( cmd.compare( 0, 13, "guild create " ) == 0 )
		{
			if( m_pPlayer->GetGold() < 1000 ){ SendFirstDlg( "创建行会需要 1000 金币。" ); return; }
			if( CGuildMgr::GetInstance()->FindByMember( m_pPlayer->GetName() ) != NULL )
			{ SendFirstDlg( "你已经在一个行会里了。" ); return; }
			std::string name = cmd.substr( 13 );
			if( CGuildMgr::GetInstance()->Create( name.c_str(), m_pPlayer->GetName() ) )
			{
				m_pPlayer->AddGold( -1000 );
				SendFirstDlg( "行会创建成功。" );
				SendGuild();
			}
			else SendFirstDlg( "创建失败（名字重复或参数错误）。" );
			return;
		}
		if( cmd.compare( 0, 11, "guild join " ) == 0 )
		{
			std::string name = cmd.substr( 11 );
			SendFirstDlg( CGuildMgr::GetInstance()->Join( name.c_str(), m_pPlayer->GetName() )
				? "加入行会成功。" : "加入失败（行会不存在/人数已满/已在行会）。" );
			SendGuild();
			return;
		}
		if( cmd == "guild leave" )
		{
			SendFirstDlg( CGuildMgr::GetInstance()->Leave( m_pPlayer->GetName() ) ? "已离开行会。" : "你不在行会中。" );
			SendGuild();
			return;
		}
		if( cmd.compare( 0, 13, "guild donate " ) == 0 )
		{
			int nGold = atoi( cmd.c_str() + 13 );
			int nNewLevel = 0;
			if( CGuildMgr::GetInstance()->Donate( m_pPlayer->GetName(), nGold, nNewLevel ) )
			{
				char szMsg[128];
				snprintf( szMsg, sizeof( szMsg ), "捐献成功，行会资金已更新%s。",
					nNewLevel > 0 ? "并升级" : "" );
				SendFirstDlg( szMsg );
				SendGuild();
			}
			else SendFirstDlg( "捐献失败（金币不足或未加入行会）。" );
			return;
		}
		if( cmd == "guild list" )
		{
			CGuildMgr * gm = CGuildMgr::GetInstance();
			for( int i = 0; i < gm->GetCount(); ++i )
			{
				CGuildMgr::GUILD * g = gm->GetByIndex( i );
				if( g == NULL )continue;
				char szMsg[128];
				snprintf( szMsg, sizeof( szMsg ), "%s  Lv.%d  会长:%s  成员:%d",
					g->name.c_str(), g->level, g->leader.c_str(), (int)g->members.size() );
				SendFirstDlg( szMsg );
			}
			return;
		}
		if( cmd == "party" )
		{
			std::vector<std::string> members;
			CPartyMgr::GetInstance()->GetMembers( m_pPlayer->GetName(), members );
			SendParty();
			if( members.empty() ){ SendFirstDlg( "你还没有队伍。" ); return; }
			for( size_t i = 0; i < members.size(); ++i )
			{
				char szMsg[96];
				snprintf( szMsg, sizeof( szMsg ), "队友：%s", members[i].c_str() );
				SendFirstDlg( szMsg );
			}
			return;
		}
		if( cmd.compare( 0, 12, "partyinvite " ) == 0 )
		{
			if( CPartyMgr::GetInstance()->Invite( m_pPlayer->GetName(), cmd.c_str() + 12 ) )
				SendFirstDlg( "邀请成功。" );
			else
				SendFirstDlg( "邀请失败（对方不在线/已有队伍/队伍已满）。" );
			return;
		}
		if( cmd == "partyleave" )
		{
			CPartyMgr::GetInstance()->Leave( m_pPlayer->GetName() );
			SendFirstDlg( "已离开队伍。" );
			SendParty();
			return;
		}
		if( cmd.compare( 0, 5, "psay " ) == 0 )
		{
			CPartyMgr::GetInstance()->Broadcast( m_pPlayer, cmd.c_str() + 5 );
			return;
		}
		if( cmd == "quests" )
		{
			CQuestMgr * pMgr = CQuestMgr::GetInstance();
			for( int i = 0; i < pMgr->GetCount(); ++i )
			{
				const QUESTTEMPLATE * q = pMgr->GetByIndex( i );
				if( q == NULL )continue;
				int nState = m_pPlayer->GetQuestState( q->id );
				int nProgress = m_pPlayer->GetQuestProgress( q->id );
				char szMsg[192];
				snprintf( szMsg, sizeof( szMsg ), "[%s] %s  %s x%d  进度 %d/%d",
					nState == 2 ? "已完成" : ( nState == 1 ? "进行中" : "可接" ),
					q->szName, q->szTarget, q->nCount, nProgress, q->nCount );
				SendFirstDlg( szMsg );
			}
			return;
		}
		if( cmd.compare( 0, 7, "accept " ) == 0 )
		{
			const QUESTTEMPLATE * q = CQuestMgr::GetInstance()->FindByName( cmd.c_str() + 7 );
			if( q == NULL ){ SendFirstDlg( "没有这个任务。" ); return; }
			if( !gs_IsQuestNpcNear( m_pPlayer, q ) ){ SendFirstDlg( "需要靠近发布任务的 NPC。" ); return; }
			if( m_pPlayer->AcceptQuest( q->id ) )SendFirstDlg( q->szDialog );
			else SendFirstDlg( "接受失败（等级不足/已接/已完成）。" );
			return;
		}
		if( cmd.compare( 0, 7, "submit " ) == 0 )
		{
			const QUESTTEMPLATE * q = CQuestMgr::GetInstance()->FindByName( cmd.c_str() + 7 );
			if( q == NULL ){ SendFirstDlg( "没有这个任务。" ); return; }
			if( !gs_IsQuestNpcNear( m_pPlayer, q ) ){ SendFirstDlg( "需要靠近发布任务的 NPC。" ); return; }
			if( m_pPlayer->SubmitQuest( q->id ) )
			{
				char szMsg[160];
				snprintf( szMsg, sizeof( szMsg ), "完成任务：%s（经验 +%d，金币 +%d）", q->szName, q->nExp, q->nGold );
				SendFirstDlg( szMsg );
			}
			else SendFirstDlg( "提交失败（条件未达成）。" );
			return;
		}
		if( cmd.compare( 0, 7, "choose " ) == 0 )
		{
			std::string args = cmd.substr( 7 );
			size_t sp = args.find( ' ' );
			if( sp == std::string::npos ){ SendFirstDlg( "用法: choose <NPC> <序号>" ); return; }
			std::string npcName = args.substr( 0, sp );
			int nIndex = atoi( args.c_str() + sp + 1 );

			CNpc * pNpc = CNpcMgr::GetInstance()->Find( npcName.c_str() );
			if( pNpc == NULL ){ SendFirstDlg( "找不到这个 NPC。" ); return; }
			if( !Game_IsNearNpc( m_pPlayer, pNpc ) ){ SendFirstDlg( "你离 NPC 太远了。" ); return; }

			const DIALOGOPTION * pOption = CDialogueMgr::GetInstance()->GetOption( pNpc->GetName(), nIndex );
			if( pOption == NULL ){ SendFirstDlg( "没有这个选项。" ); return; }

			if( strcmp( pOption->szAction, "teleport" ) == 0 )
			{
				UINT nMapId = 0;
				int x = 0, y = 0;
				if( sscanf( pOption->szParam, "%u,%d,%d", &nMapId, &x, &y ) == 3 &&
					Game_TeleportPlayer( m_pPlayer, nMapId, x, y ) )
					SendFirstDlg( "传送完成。" );
				else
					SendFirstDlg( "传送失败。" );
			}
			else if( strcmp( pOption->szAction, "close" ) == 0 )
			{
				SendFirstDlg( "再见。" );
			}
			else if( strcmp( pOption->szAction, "shop" ) == 0 )
			{
				SendFirstDlg( "请用 npcshop 命令查看货架。" );
			}
			else if( strcmp( pOption->szAction, "quests" ) == 0 )
			{
				SendFirstDlg( "请用 quests 命令查看任务。" );
			}
			return;
		}
		if( cmd.compare( 0, 12, "npcteleport " ) == 0 )
		{
			CNpc * pNpc = CNpcMgr::GetInstance()->Find( cmd.c_str() + 12 );
			if( pNpc == NULL ){ SendFirstDlg( "找不到这个 NPC。" ); return; }
			if( !Game_IsNearNpc( m_pPlayer, pNpc ) ){ SendFirstDlg( "你离 NPC 太远了。" ); return; }
			if( pNpc->GetNpcType() == 3 )
			{
				std::string msg = CDungeonMgr::GetInstance()->Enter( m_pPlayer, pNpc );
				SendFirstDlg( msg.c_str() );
				return;
			}
			if( pNpc->GetNpcType() != 2 ){ SendFirstDlg( "对方不是传送员。" ); return; }
			if( Game_TeleportPlayer( m_pPlayer, (UINT)pNpc->GetDestMap(), pNpc->GetDestX(), pNpc->GetDestY() ) )
				SendFirstDlg( "传送完成。" );
			else
				SendFirstDlg( "传送失败。" );
			return;
		}
		if( cmd.compare( 0, 5, "walk " ) == 0 )
		{
			int dir = atoi( cmd.c_str() + 5 ) & 7;
			if( m_pPlayer->Walk( dir ) )
			{
				//	移动广播默认不含自己，这里补发给本人，客户端才能更新坐标
				SendMsg( m_pPlayer->GetId(), SM_WALK, m_pPlayer->GetActionX(),
						 m_pPlayer->GetActionY(), (WORD)dir, NULL );
			}
			return;
		}
	}

	printf( "[client %u] 未识别文本消息: %s\n", GetId(), cmd.c_str() );
}

VOID CClientObj::OnCodedMsg( PMIRMSG pMsg, int datasize )
{
	if( pMsg == NULL )return;
	ProcClientMsg( pMsg, datasize );
}

VOID CClientObj::OnDBMsg( PMIRMSG pMsg, int datasize )
{
	(void)pMsg; (void)datasize;
}

VOID CClientObj::OnVerifyString( const char * pszString )
{
	m_State = GUM_VERIFING;
	printf( "[client %u] 验证串: %s\n", GetId(), pszString ? pszString : "(null)" );
}

VOID CClientObj::OnDisconnect()
{
	if( m_pPlayer != NULL )
	{
		CPartyMgr::GetInstance()->Leave( m_pPlayer->GetName() );	//	下线退队
		CTradeMgr::GetInstance()->OnPlayerLeave( m_pPlayer->GetName() );
		CPlayerStore::GetInstance()->Save( m_pPlayer );		//	下线先存档
		CGameWorld::GetInstance()->RemovePlayer( m_pPlayer );
		CHumanPlayerMgr::GetInstance()->DeletePlayer( m_pPlayer );
		m_pPlayer = NULL;
	}
	m_State = GUM_NONE;
	CClientObject::OnDisconnect();
}

VOID CClientObj::SendActionResult( int x, int y, BOOL bSuccess )
{
	SendMsg( GetId(), SM_ACTIONRESULT, (WORD)x, (WORD)y, (WORD)( bSuccess ? 1 : 0 ), NULL );
}

VOID CClientObj::SendAddItem( ITEM & item )
{
	SendMsg( item.dwMakeIndex, SM_ADDITEM, 0, 0, 0, (LPVOID)&item, (int)sizeof( item ) );
}

VOID CClientObj::SendEquipItemResult( BOOL bSuccess, int pos, DWORD dwMakeIndex )
{
	SendMsg( dwMakeIndex, SM_EQUIPRESULT, (WORD)( bSuccess ? 1 : 0 ), (WORD)pos, 0, NULL );
}

VOID CClientObj::SendUnEquipItemResult( BOOL bSuccess, int pos, DWORD dwMakeIndex )
{
	SendMsg( dwMakeIndex, SM_UNEQUIPRESULT, (WORD)( bSuccess ? 1 : 0 ), (WORD)pos, 0, NULL );
}

VOID CClientObj::ProcClientMsg( PMIRMSG pMsg, int datasize )
{
	switch( pMsg->wCmd )
	{
	case	SM_WALK:
		if( m_pPlayer != NULL )
		{
			int dir = pMsg->wParam[0] & 7;
			if( m_pPlayer->Walk( dir ) )
				SendMsg( m_pPlayer->GetId(), SM_WALK, m_pPlayer->GetActionX(),
						 m_pPlayer->GetActionY(), (WORD)dir, NULL );
		}
		break;
	case	SM_RUN:
		if( m_pPlayer != NULL )
		{
			int dir = pMsg->wParam[0] & 7;
			if( m_pPlayer->Run( dir ) )
				SendMsg( m_pPlayer->GetId(), SM_WALK, m_pPlayer->GetActionX(),
						 m_pPlayer->GetActionY(), (WORD)dir, NULL );
		}
		break;
	case	SM_TURN:
		if( m_pPlayer != NULL )m_pPlayer->Turn( pMsg->wParam[0] & 7 );
		break;
	case	SM_ATTACK:
		if( m_pPlayer != NULL )m_pPlayer->Attack( pMsg->wParam[0] & 7 );
		break;
	case	SM_SPELL:
		if( m_pPlayer != NULL )
			m_pPlayer->CastSkill( pMsg->wParam[0], pMsg->wParam[1] & 7 );
		break;
	case	SM_CHAT:
		if( m_pPlayer != NULL && datasize > 0 )
		{
			std::string text( pMsg->data, (size_t)datasize );
			m_pPlayer->Say( "%s", text.c_str() );
		}
		break;
	default:
		printf( "[client %u] 消息 cmd=0x%04x p0=%u p1=%u p2=%u size=%d\n",
			GetId(), pMsg->wCmd, pMsg->wParam[0], pMsg->wParam[1], pMsg->wParam[2], datasize );
		break;
	}
}

VOID CClientObj::SendFirstDlg( const char * pszString )
{
	SendMsg( 0, SM_CHAT, 0xff00, 0, 0, (LPVOID)pszString );
}

e_gm_command CClientObj::GetCmCommand( const char * pszCmd )
{
	struct
	{
		const char *	pszName;
		e_gm_command	eCmd;
	}	static s_Commands[] =
	{
		{ "show",				GC_SHOW },
		{ "makeitem",			GC_MAKEITEM },
		{ "dropitem",			GC_DROPITEM },
		{ "move",				GC_MOVE },
		{ "kick",				GC_KICK },
		{ "cleantestplayer",	GC_CLEANTESTPLAYER },
		{ "allowspecialpower",	GC_ALLOWSPECIALPOWER },
		{ "dropitemtest",		GC_DROPITEMTEST },
		{ "cleanallitem",		GC_CLEANALLITEM },
		{ "setitemclass",		GC_SETITEMCLASS },
		{ "sendaroundmsg",		GC_SENDAROUNDMSG },
		{ "sendsinglemsg",		GC_SENDSINGLEMSG },
		{ "sendmsg",			GC_SENDMSG },
		{ "sendallmsg",			GC_SENDALLMSG },
	};

	if( pszCmd == NULL )return GC_CTRL_MAX;
	for( size_t i = 0; i < sizeof( s_Commands ) / sizeof( s_Commands[0] ); ++i )
		if( strcmp( s_Commands[i].pszName, pszCmd ) == 0 )
			return s_Commands[i].eCmd;
	return GC_CTRL_MAX;
}

VOID CClientObj::OnCommand( const char * pszCommand )
{
	if( pszCommand == NULL )return;

	char	szBuffer[512];
	o_strncpy( szBuffer, pszCommand, sizeof( szBuffer ) - 1 );

	char *	Params[16];
	int		nParam = SearchParam( szBuffer, Params, 16, ' ' );
	if( nParam <= 0 )return;

	switch( GetCmCommand( Params[0] ) )
	{
	case	GC_SHOW:
		printf( "[GM] client=%u player=%s\n", GetId(), m_pPlayer ? m_pPlayer->GetName() : "(无)" );
		break;
	case	GC_MAKEITEM:
		if( nParam >= 2 )CreateTestItem( Params[1], 5, 0, 0, 0 );
		break;
	case	GC_KICK:
		CloseSocket();
		break;
	default:
		printf( "[GM] 未实现命令: %s\n", Params[0] );
		break;
	}
}

VOID CClientObj::CreateTestItem( const char * pszName, BYTE btStdMode, BYTE btShape, WORD wImage, BYTE btFlag )
{
	//	骨架：物品模板可以查询，但落库需要 DB 服务器
	ITEM item;
	if( CItemManager::GetInstance()->MakeupTemplateItem( pszName, item ) )
	{
		item.dwMakeIndex = (DWORD)o_GetTickCount();
		item.baseitem.btStdMode = btStdMode ? btStdMode : item.baseitem.btStdMode;
		item.baseitem.btShape = btShape;
		item.baseitem.wImageIndex = wImage ? wImage : item.baseitem.wImageIndex;
		printf( "[GM] 生成物品 %s (makeindex=%u)\n", item.baseitem.szName, item.dwMakeIndex );
	}
	else
	{
		printf( "[GM] 物品模板不存在: %s\n", pszName ? pszName : "(null)" );
	}
	(void)btFlag;
}

// ---------------------------------------------------------------------------
// 背包 / 装备 / 商店结构化快照（给图形客户端）
// ---------------------------------------------------------------------------
static void gs_PutU8( char * buf, int & off, unsigned char v )
{
	buf[off++] = (char)v;
}
static void gs_PutU16( char * buf, int & off, unsigned short v )
{
	buf[off++] = (char)( v & 0xFF );
	buf[off++] = (char)( ( v >> 8 ) & 0xFF );
}
static void gs_PutU32( char * buf, int & off, unsigned int v )
{
	buf[off++] = (char)( v & 0xFF );
	buf[off++] = (char)( ( v >> 8 ) & 0xFF );
	buf[off++] = (char)( ( v >> 16 ) & 0xFF );
	buf[off++] = (char)( ( v >> 24 ) & 0xFF );
}
static void gs_PutStr( char * buf, int & off, const char * psz )
{
	size_t len = strlen( psz );
	if( len > 255 )len = 255;
	//	避免在 UTF-8 多字节字符中间截断
	while( len > 0 && ( (unsigned char)psz[len] & 0xC0 ) == 0x80 )
		-- len;
	unsigned char n = (unsigned char)len;
	gs_PutU8( buf, off, n );
	memcpy( buf + off, psz, n );
	off += n;
}

VOID CClientObj::SendDialog( const char * pszNpc, const char * pszText )
{
	std::vector<DIALOGOPTION> options;
	int nOptions = CDialogueMgr::GetInstance()->GetOptions( pszNpc, options );

	char buf[2048];
	int off = 0;
	gs_PutStr( buf, off, pszNpc ? pszNpc : "" );
	gs_PutStr( buf, off, pszText ? pszText : "" );
	int nCountPos = off;
	gs_PutU16( buf, off, 0 );
	unsigned short nCount = 0;
	for( int i = 0; i < nOptions; ++i )
	{
		gs_PutU8( buf, off, (unsigned char)options[i].nIndex );
		gs_PutStr( buf, off, options[i].szText );
		gs_PutStr( buf, off, options[i].szAction );
		gs_PutStr( buf, off, options[i].szParam );
		++nCount;
	}
	buf[nCountPos] = (char)( nCount & 0xFF );
	buf[nCountPos + 1] = (char)( ( nCount >> 8 ) & 0xFF );
	SendMsg( GetId(), SM_DIALOG, 0, 0, 0, buf, off );
}

VOID CClientObj::SendAuth( const char * pszToken )
{
	if( pszToken == NULL )return;
	SendMsg( GetId(), SM_AUTH, 0, 0, 0, (LPVOID)pszToken, (int)strlen( pszToken ) );
}

VOID CClientObj::SendCharList()
{
	if( m_szAccount[0] == 0 )return;
	gs_CharEnsureMigration( m_szAccount );
	gs_CharLoad();

	char buf[2048];
	int off = 0;
	int nPos = off;
	gs_PutU16( buf, off, 0 );
	unsigned short n = 0;
	for( size_t i = 0; i < g_chars.size(); ++i )
	{
		if( g_chars[i].account != m_szAccount )continue;
		CHARDBINFO info;
		std::vector<ITEM> bag; std::vector<std::pair<int, ITEM> > eq;
		DWORD sk = 0; std::vector<PLAYERQUEST> qs; std::vector<int> ach;
		int title = 0, pts = 0; std::vector<int> shop;
		std::vector<std::string> codex, rewarded;
		if( !CPlayerStore::GetInstance()->Load( g_chars[i].name.c_str(), info, bag, eq, sk, qs, ach,
			title, pts, shop, codex, rewarded ) )continue;
		gs_PutStr( buf, off, g_chars[i].name.c_str() );
		gs_PutU8( buf, off, (unsigned char)info.btClass );
		gs_PutU16( buf, off, (unsigned short)info.wLevel );
		++ n;
	}
	buf[nPos] = (char)( n & 0xFF );
	buf[nPos + 1] = (char)( ( n >> 8 ) & 0xFF );
	SendMsg( GetId(), SM_CHARLIST, 0, 0, 0, buf, off );
}

VOID CClientObj::SendParty()
{
	if( m_pPlayer == NULL )return;
	std::vector<std::string> members;
	CPartyMgr::GetInstance()->GetMembers( m_pPlayer->GetName(), members );

	char buf[2048]; int off = 0;
	gs_PutU16( buf, off, (unsigned short)members.size() );
	for( size_t i = 0; i < members.size(); ++i )
	{
		CHumanPlayer * p = CHumanPlayerMgr::GetInstance()->FindbyName( members[i].c_str() );
		gs_PutStr( buf, off, members[i].c_str() );
		gs_PutU16( buf, off, p ? (unsigned short)p->GetPropValue( PI_LEVEL ) : 0 );
		gs_PutU8( buf, off, p ? (unsigned char)p->GetClass() : 0 );
		gs_PutU16( buf, off, p ? (unsigned short)p->GetPropValue( PI_CURHP ) : 0 );
		gs_PutU16( buf, off, p ? (unsigned short)p->GetPropValue( PI_MAXHP ) : 0 );
		gs_PutU8( buf, off, p ? 1 : 0 );
	}
	SendMsg( GetId(), SM_PARTY, 0, 0, 0, buf, off );
}

VOID CClientObj::SendGuild()
{
	if( m_pPlayer == NULL )return;
	CGuildMgr::GUILD * g = CGuildMgr::GetInstance()->FindByMember( m_pPlayer->GetName() );
	char buf[4096]; int off = 0;
	if( g == NULL )
	{
		gs_PutStr( buf, off, "" );
		SendMsg( GetId(), SM_GUILD, 0, 0, 0, buf, off );
		return;
	}
	gs_PutStr( buf, off, g->name.c_str() );
	gs_PutStr( buf, off, g->leader.c_str() );
	gs_PutStr( buf, off, g->vice.c_str() );
	gs_PutU8( buf, off, (unsigned char)g->level );
	gs_PutU32( buf, off, (unsigned int)( g->funds > 0xffffffffLL ? 0xffffffffLL : g->funds ) );
	gs_PutU16( buf, off, (unsigned short)g->members.size() );
	for( size_t i = 0; i < g->members.size(); ++i )
	{
		CHumanPlayer * p = CHumanPlayerMgr::GetInstance()->FindbyName( g->members[i].c_str() );
		gs_PutStr( buf, off, g->members[i].c_str() );
		gs_PutU16( buf, off, p ? (unsigned short)p->GetPropValue( PI_LEVEL ) : 0 );
		gs_PutU8( buf, off, p ? 1 : 0 );
		int rank = 0;
		if( g->members[i] == g->leader )rank = 2;
		else if( g->members[i] == g->vice )rank = 1;
		gs_PutU8( buf, off, (unsigned char)rank );
	}
	gs_PutU16( buf, off, (unsigned short)g->warehouse.size() );
	SendMsg( GetId(), SM_GUILD, 0, 0, 0, buf, off );
}

VOID CClientObj::SendDungeons()
{
	CDungeonMgr * dm = CDungeonMgr::GetInstance();
	char buf[8192]; int off = 0;
	int nDefs = dm->GetDefCount();
	gs_PutU16( buf, off, (unsigned short)nDefs );
	for( int i = 0; i < nDefs; ++i )
	{
		DUNGEONDEF * d = dm->GetDefByIndex( i );
		if( d == NULL )continue;
		gs_PutU8( buf, off, (unsigned char)d->id );
		gs_PutStr( buf, off, d->szName );
		gs_PutStr( buf, off, d->szBoss );
		gs_PutU8( buf, off, dm->IsFirstCleared( d->id ) ? 1 : 0 );
		gs_PutU8( buf, off, (unsigned char)d->maps.size() );
	}
	int nRec = dm->GetRecordCount();
	if( nRec > DUNGEON_MAX_RECORD )nRec = DUNGEON_MAX_RECORD;
	gs_PutU16( buf, off, (unsigned short)nRec );
	for( int i = 0; i < nRec; ++i )
	{
		DUNGEONRECORD * r = dm->GetRecordByIndex( i );
		if( r == NULL )continue;
		gs_PutStr( buf, off, r->szDungeon );
		gs_PutStr( buf, off, r->szParty );
		gs_PutU32( buf, off, r->dwSeconds );
		gs_PutStr( buf, off, r->szDate );
	}
	int nSlot = dm->GetSlotCount();
	gs_PutU16( buf, off, (unsigned short)nSlot );
	for( int i = 0; i < nSlot; ++i )
	{
		DUNGEONSLOT * sl = dm->GetSlotByIndex( i );
		if( sl == NULL )continue;
		gs_PutU16( buf, off, (unsigned short)sl->nMapId );
		gs_PutStr( buf, off, sl->strPartyName.c_str() );
	}
	SendMsg( GetId(), SM_DUNGEONS, 0, 0, 0, buf, off );
}

VOID CClientObj::SendBag()
{
	if( m_pPlayer == NULL )return;

	ITEM items[46];
	int nCount = m_pPlayer->GetBag().GetItems( items, 46 );

	char buf[4096];
	int off = 0;
	gs_PutU16( buf, off, (unsigned short)nCount );
	for( int i = 0; i < nCount; ++i )
	{
		gs_PutU32( buf, off, items[i].dwMakeIndex );
		gs_PutStr( buf, off, items[i].baseitem.szName );
		gs_PutU16( buf, off, items[i].wCount );
		gs_PutU8( buf, off, items[i].btUpgradeLevel );
		gs_PutU8( buf, off, items[i].btElement );
		gs_PutU8( buf, off, items[i].btElementPower );
		gs_PutU16( buf, off, items[i].wCurDura );
		gs_PutU16( buf, off, items[i].wMaxDura );
		gs_PutU8( buf, off, items[i].baseitem.btStdMode );
		gs_PutU8( buf, off, items[i].baseitem.btUseType );
	}
	SendMsg( GetId(), SM_BAG, 0, 0, 0, buf, off );
}

VOID CClientObj::SendEquipment()
{
	if( m_pPlayer == NULL )return;

	char buf[1024];
	int off = 0;
	unsigned short nCount = 0;

	//	先留数量位置，逐格写入
	int nCountPos = off;
	gs_PutU16( buf, off, 0 );
	for( int i = 0; i < _U_MAX; ++i )
	{
		ITEM * pItem = m_pPlayer->GetEquipments().GetEquipment( i );
		if( pItem == NULL )continue;
		gs_PutU8( buf, off, (unsigned char)i );
		gs_PutU32( buf, off, pItem->dwMakeIndex );
		gs_PutStr( buf, off, pItem->baseitem.szName );
		gs_PutU8( buf, off, pItem->btUpgradeLevel );
		gs_PutU8( buf, off, pItem->btElement );
		gs_PutU8( buf, off, pItem->btElementPower );
		gs_PutU16( buf, off, pItem->wCurDura );
		gs_PutU16( buf, off, pItem->wMaxDura );
		++nCount;
	}
	buf[nCountPos] = (char)( nCount & 0xFF );
	buf[nCountPos + 1] = (char)( ( nCount >> 8 ) & 0xFF );

	SendMsg( GetId(), SM_EQUIP, 0, 0, 0, buf, off );
}

VOID CClientObj::SendShop()
{
	CStringFile shop( "data/shop.txt" );
	char buf[2048];
	int off = 0;
	int nCountPos = off;
	gs_PutU16( buf, off, 0 );

	unsigned short nCount = 0;
	for( int i = 0; i < shop.GetLineCount(); ++i )
	{
		const char * pszLine = shop[i];
		if( pszLine == NULL || *pszLine == '\0' || *pszLine == '#' )continue;

		char szBuffer[128];
		o_strncpy( szBuffer, pszLine, sizeof( szBuffer ) - 1 );
		char * Params[4];
		if( SearchParam( szBuffer, Params, 4, '/' ) < 2 )continue;

		gs_PutStr( buf, off, Params[0] );
		gs_PutU32( buf, off, (unsigned int)atoi( Params[1] ) );
		++nCount;
	}
	buf[nCountPos] = (char)( nCount & 0xFF );
	buf[nCountPos + 1] = (char)( ( nCount >> 8 ) & 0xFF );

	SendMsg( GetId(), SM_SHOP, 0, 0, 0, buf, off );
}

//	按 stdMode 推断装备位
static int gs_EquipPosForStdMode( BYTE stdMode )
{
	if( stdMode == 5 || stdMode == 6 )return _U_WEAPON;
	if( stdMode == 10 || stdMode == 11 )return _U_DRESS;
	if( stdMode == 15 )return _U_HELMET;
	if( stdMode == 19 || stdMode == 20 || stdMode == 21 )return _U_NECKLACE;
	if( stdMode == 22 || stdMode == 23 )return _U_RINGL;
	if( stdMode == 24 || stdMode == 26 )return _U_ARMRINGL;
	return -1;
}

VOID CClientObj::OnBagCommand( const char * pszCmd )
{
	if( m_pPlayer == NULL )return;

	//	use <编号>
	if( strncmp( pszCmd, "use ", 4 ) == 0 )
	{
		DWORD dwId = (DWORD)strtoul( pszCmd + 4, NULL, 10 );
		m_pPlayer->UseItem( dwId );
		SendBag();
		m_pPlayer->UpdateProp();
		return;
	}
	//	equipauto <编号>
	if( strncmp( pszCmd, "equipauto ", 10 ) == 0 )
	{
		DWORD dwId = (DWORD)strtoul( pszCmd + 10, NULL, 10 );
		ITEM * pItem = m_pPlayer->GetBag().FindItem( dwId );
		if( pItem != NULL )
		{
			int nPos = gs_EquipPosForStdMode( pItem->baseitem.btStdMode );
			if( nPos >= 0 )
			{
				if( !m_pPlayer->EquipItem( nPos, dwId ) )
				{
					if( pItem->baseitem.btStdMode == 22 || pItem->baseitem.btStdMode == 23 )
						m_pPlayer->EquipItem( _U_RINGR, dwId );
					else if( pItem->baseitem.btStdMode == 24 || pItem->baseitem.btStdMode == 26 )
						m_pPlayer->EquipItem( _U_ARMRINGR, dwId );
				}
			}
		}
		SendBag();
		SendEquipment();
		m_pPlayer->UpdateProp();
		return;
	}
	//	unequip <位置> <编号>
	if( strncmp( pszCmd, "unequip ", 8 ) == 0 )
	{
		int nPos = atoi( pszCmd + 8 );
		const char * pszId = strchr( pszCmd + 8, ' ' );
		DWORD dwId = pszId != NULL ? (DWORD)strtoul( pszId + 1, NULL, 10 ) : 0;
		m_pPlayer->UnEquipItem( nPos, dwId );
		SendBag();
		SendEquipment();
		m_pPlayer->UpdateProp();
		return;
	}
	//	sellitem <编号>
	if( strncmp( pszCmd, "sellitem ", 9 ) == 0 )
	{
		DWORD dwId = (DWORD)strtoul( pszCmd + 9, NULL, 10 );
		ITEM * pItem = m_pPlayer->GetBag().FindItem( dwId );
		if( pItem != NULL )
		{
			int nPrice = pItem->baseitem.nPrice / 2;
			if( nPrice < 1 )nPrice = 1;
			m_pPlayer->GetBag().RemoveItem( dwId );
			m_pPlayer->AddGold( nPrice );
			char szMsg[96];
			snprintf( szMsg, sizeof( szMsg ), "卖出获得 %d 金币", nPrice );
			SendFirstDlg( szMsg );
		}
		SendBag();
		m_pPlayer->UpdateProp();
		return;
	}
	//	buy <物品> [数量]
	if( strncmp( pszCmd, "buy ", 4 ) == 0 )
	{
		char szBuffer[128];
		o_strncpy( szBuffer, pszCmd + 4, sizeof( szBuffer ) - 1 );
		char * Params[4];
		int nParam = SearchParam( szBuffer, Params, 4, ' ' );
		if( nParam >= 1 )
		{
			int nCount = nParam >= 2 ? atoi( Params[1] ) : 1;
			if( nCount <= 0 )nCount = 1;
			if( nCount > 99 )nCount = 99;

			CStringFile shop( "data/shop.txt" );
			int nPrice = -1;
			for( int i = 0; i < shop.GetLineCount(); ++i )
			{
				char szLine[128];
				o_strncpy( szLine, shop[i], sizeof( szLine ) - 1 );
				char * P[4];
				if( SearchParam( szLine, P, 4, '/' ) < 2 )continue;
				if( strcmp( P[0], Params[0] ) == 0 ){ nPrice = atoi( P[1] ); break; }
			}
			if( nPrice < 0 )
			{
				SendFirstDlg( "商店不出售该物品。" );
			}
			else
			{
				ITEM item;
				if( !CItemManager::GetInstance()->MakeupTemplateItem( Params[0], item ) )
				{
					SendFirstDlg( "物品模板不存在。" );
				}
				else if( !m_pPlayer->AddGold( -nPrice * nCount ) )
				{
					SendFirstDlg( "金币不足。" );
				}
				else
				{
					item.dwMakeIndex = CItemManager::NewMakeIndex();
					item.wCount = (WORD)nCount;
					if( !m_pPlayer->AddBagItem( item ) )
					{
						m_pPlayer->AddGold( nPrice * nCount );
						SendFirstDlg( "背包已满。" );
					}
					else
					{
						char szMsg[96];
						snprintf( szMsg, sizeof( szMsg ), "购买 %s x%d", Params[0], nCount );
						SendFirstDlg( szMsg );
					}
				}
			}
		}
		SendBag();
		m_pPlayer->UpdateProp();
		return;
	}
}

//	技能列表快照
VOID CClientObj::SendSkills()
{
	if( m_pPlayer == NULL )return;

	char buf[2048];
	int off = 0;
	int nCountPos = off;
	gs_PutU16( buf, off, 0 );

	unsigned short nCount = 0;
	CSkillMgr * pMgr = CSkillMgr::GetInstance();
	for( int i = 0; i < pMgr->GetCount(); ++i )
	{
		const SKILLTEMPLATE * pSkill = pMgr->GetByIndex( i );
		if( pSkill == NULL || pSkill->nClass != m_pPlayer->GetClass() )continue;

		gs_PutU8( buf, off, (unsigned char)pSkill->id );
		gs_PutStr( buf, off, pSkill->szName );
		gs_PutU16( buf, off, (unsigned short)pSkill->nNeedLevel );
		gs_PutU16( buf, off, (unsigned short)pSkill->nMp );
		gs_PutU32( buf, off, pSkill->dwCooldown );
		gs_PutU8( buf, off, m_pPlayer->HasSkill( pSkill->id ) ? 1 : 0 );
		gs_PutU8( buf, off, (unsigned char)pSkill->nType );
		gs_PutU8( buf, off, (unsigned char)pSkill->nRange );
		++nCount;
	}
	buf[nCountPos] = (char)( nCount & 0xFF );
	buf[nCountPos + 1] = (char)( ( nCount >> 8 ) & 0xFF );

	SendMsg( GetId(), SM_SKILLS, 0, 0, 0, buf, off );
}

//	任务列表快照（含玩家状态/进度）
VOID CClientObj::SendQuests()
{
	if( m_pPlayer == NULL )return;

	char buf[4096];
	int off = 0;
	int nCountPos = off;
	gs_PutU16( buf, off, 0 );

	unsigned short nCount = 0;
	CQuestMgr * pMgr = CQuestMgr::GetInstance();
	for( int i = 0; i < pMgr->GetCount(); ++i )
	{
		const QUESTTEMPLATE * q = pMgr->GetByIndex( i );
		if( q == NULL )continue;

		int nState = m_pPlayer->GetQuestState( q->id );
		int nProgress = m_pPlayer->GetQuestProgress( q->id );

		gs_PutU8( buf, off, (unsigned char)q->id );
		gs_PutStr( buf, off, q->szName );
		gs_PutStr( buf, off, q->szTarget );
		gs_PutU16( buf, off, (unsigned short)q->nCount );
		gs_PutU16( buf, off, (unsigned short)nProgress );
		gs_PutU8( buf, off, (unsigned char)nState );
		gs_PutU8( buf, off, (unsigned char)q->nType );
		gs_PutU32( buf, off, (unsigned int)q->nExp );
		gs_PutU32( buf, off, (unsigned int)q->nGold );
		gs_PutStr( buf, off, q->szNpc );
		gs_PutU8( buf, off, (unsigned char)q->nNeedLevel );
		gs_PutStr( buf, off, q->szRewardItem );
		gs_PutU16( buf, off, (unsigned short)q->nRewardCount );
		gs_PutU8( buf, off, (unsigned char)q->nPrevQuest );
		gs_PutStr( buf, off, q->szDialog );
		++nCount;
	}
	buf[nCountPos] = (char)( nCount & 0xFF );
	buf[nCountPos + 1] = (char)( ( nCount >> 8 ) & 0xFF );

	SendMsg( GetId(), SM_QUESTS, 0, 0, 0, buf, off );
}

//	玩家周围 61x41 的地形窗口：0 可走 / 1 阻挡 / 2 越界
VOID CClientObj::SendMapData()
{
	if( m_pPlayer == NULL )return;
	CVLogicMap * pMap = m_pPlayer->GetMap();
	if( pMap == NULL )return;

	const int W = 61;
	const int H = 41;
	int ox = (int)m_pPlayer->getX() - W / 2;
	int oy = (int)m_pPlayer->getY() - H / 2;

	char buf[8192];
	int off = 0;
	gs_PutU16( buf, off, (unsigned short)W );
	gs_PutU16( buf, off, (unsigned short)H );
	gs_PutU16( buf, off, (unsigned short)( ox & 0xFFFF ) );
	gs_PutU16( buf, off, (unsigned short)( oy & 0xFFFF ) );

	CVPhysicsMap * pPhysics = pMap->GetPhysicsMap();
	int nMapW = pMap->GetWidth();
	int nMapH = pMap->GetHeight();

	for( int y = 0; y < H; ++y )
	{
		for( int x = 0; x < W; ++x )
		{
			int wx = ox + x;
			int wy = oy + y;
			unsigned char v = 2;
			if( wx >= 0 && wy >= 0 && wx < nMapW && wy < nMapH )
				v = ( pPhysics != NULL && pPhysics->IsBlocked( wx, wy ) ) ? 1 : 0;
			buf[off++] = (char)v;
		}
	}

	SendMsg( GetId(), SM_MAPDATA, (WORD)W, (WORD)H, 0, buf, off );
}

//	称号收集快照
VOID CClientObj::SendTitles()
{
	if( m_pPlayer == NULL )return;

	char buf[4096];
	int off = 0;
	gs_PutU32( buf, off, (unsigned int)m_pPlayer->GetAvailablePoints() );
	int nCountPos = off;
	gs_PutU16( buf, off, 0 );
	unsigned short nCount = 0;

	//	成就称号
	for( int i = 0; i < CAchievementMgr::GetInstance()->GetCount(); ++i )
	{
		ACHIEVEMENT * pAch = CAchievementMgr::GetInstance()->GetByIndex( i );
		if( pAch == NULL )continue;
		gs_PutU16( buf, off, (unsigned short)pAch->id );
		gs_PutStr( buf, off, pAch->szName );
		gs_PutU8( buf, off, 0 );
		gs_PutU32( buf, off, 0 );
		gs_PutU8( buf, off, m_pPlayer->HasAchievement( pAch->id ) ? 1 : 0 );
		gs_PutU8( buf, off, m_pPlayer->GetTitle() == pAch->id ? 1 : 0 );
		++nCount;
	}

	//	点数兑换称号
	for( int i = 0; i < CTitleMgr::GetInstance()->GetCount(); ++i )
	{
		TITLEDEF * pTitle = CTitleMgr::GetInstance()->GetByIndex( i );
		if( pTitle == NULL )continue;
		gs_PutU16( buf, off, (unsigned short)pTitle->id );
		gs_PutStr( buf, off, pTitle->szName );
		gs_PutU8( buf, off, 1 );
		gs_PutU32( buf, off, (unsigned int)pTitle->nCost );
		gs_PutU8( buf, off, m_pPlayer->HasShopTitle( pTitle->id ) ? 1 : 0 );
		gs_PutU8( buf, off, m_pPlayer->GetTitle() == pTitle->id ? 1 : 0 );
		++nCount;
	}

	buf[nCountPos] = (char)( nCount & 0xFF );
	buf[nCountPos + 1] = (char)( ( nCount >> 8 ) & 0xFF );
	SendMsg( GetId(), SM_TITLES, 0, 0, 0, buf, off );
}

//	套装图鉴快照
VOID CClientObj::SendCodex()
{
	if( m_pPlayer == NULL )return;

	char buf[2048];
	int off = 0;
	gs_PutU32( buf, off, (unsigned int)m_pPlayer->GetCodex().size() );
	gs_PutU32( buf, off, (unsigned int)m_pPlayer->GetShopTitles().size() );
	gs_PutU32( buf, off, (unsigned int)m_pPlayer->GetAchievements().size() );
	int nCountPos = off;
	gs_PutU16( buf, off, 0 );
	unsigned short nCount = 0;

	for( int s = 0; s < CSetMgr::GetInstance()->GetCount(); ++s )
	{
		SETDEF * pDef = CSetMgr::GetInstance()->GetByIndex( s );
		if( pDef == NULL )continue;

		int nFound = 0;
		for( size_t j = 0; j < pDef->items.size(); ++j )
			if( m_pPlayer->HasCodex( pDef->items[j].c_str() ) )++nFound;

		gs_PutStr( buf, off, pDef->szName );
		gs_PutU8( buf, off, (unsigned char)nFound );
		gs_PutU8( buf, off, (unsigned char)pDef->items.size() );
		gs_PutU8( buf, off, nFound >= (int)pDef->items.size() ? 1 : 0 );
		++nCount;
	}
	buf[nCountPos] = (char)( nCount & 0xFF );
	buf[nCountPos + 1] = (char)( ( nCount >> 8 ) & 0xFF );
	SendMsg( GetId(), SM_CODEX, 0, 0, 0, buf, off );
}

//	GM 命令：由客户端聊天框 /gm ... 触发，按 gm.txt 权限校验
VOID CClientObj::HandleGmCommand( const std::string & args )
{
	if( m_pPlayer == NULL )return;

	int nLevel = CGmMgr::GetInstance()->GetLevel( m_pPlayer->GetName() );
	if( nLevel <= 0 )
	{
		SendFirstDlg( "你没有 GM 权限。" );
		return;
	}

	std::string text = args;
	while( !text.empty() && text[0] == ' ' )text.erase( 0, 1 );

	std::istringstream iss( text );
	std::string sub;
	iss >> sub;

	if( sub.empty() || sub == "help" )
	{
		SendFirstDlg( "GM 命令：/gm give <物品> [数量]  /gm exp <经验>  /gm gold <金币>" );
		SendFirstDlg( "  /gm tp <地图> <x> <y>  /gm heal  /gm goto <玩家>  /gm recall <玩家>" );
		if( nLevel >= 2 )SendFirstDlg( "  /gm spawn <怪物> [数量]  /gm announce <内容>  /gm gmlist" );
		if( nLevel >= 3 )SendFirstDlg( "  /gm kick <玩家>  /gm gmadd <玩家> <等级>  /gm gmremove <玩家>" );
		return;
	}

	if( sub == "give" )
	{
		std::string itemName;
		int nCount = 1;
		iss >> itemName >> nCount;
		if( itemName.empty() ){ SendFirstDlg( "用法: /gm give <物品> [数量]" ); return; }
		if( nCount <= 0 )nCount = 1;
		if( nCount > 99 )nCount = 99;

		ITEM item;
		if( !CItemManager::GetInstance()->MakeupTemplateItem( itemName.c_str(), item ) )
		{
			SendFirstDlg( "物品模板不存在。" );
			return;
		}
		item.dwMakeIndex = CItemManager::NewMakeIndex();
		item.wCount = (WORD)nCount;
		if( m_pPlayer->AddBagItem( item ) )
		{
			char szMsg[128];
			snprintf( szMsg, sizeof( szMsg ), "GM：已发放 %s x%d", itemName.c_str(), nCount );
			SendFirstDlg( szMsg );
		}
		else SendFirstDlg( "背包已满。" );
	}
	else if( sub == "exp" )
	{
		int nExp = 0;
		iss >> nExp;
		m_pPlayer->AddExp( nExp );
		char szMsg[96];
		snprintf( szMsg, sizeof( szMsg ), "GM：增加经验 %d", nExp );
		SendFirstDlg( szMsg );
	}
	else if( sub == "gold" )
	{
		int nGold = 0;
		iss >> nGold;
		m_pPlayer->AddGold( nGold );
		char szMsg[96];
		snprintf( szMsg, sizeof( szMsg ), "GM：金币 %+d", nGold );
		SendFirstDlg( szMsg );
	}
	else if( sub == "tp" )
	{
		int nMap = 0, x = 0, y = 0;
		iss >> nMap >> x >> y;
		if( Game_TeleportPlayer( m_pPlayer, (UINT)nMap, x, y ) )
			SendFirstDlg( "GM：传送完成。" );
		else
			SendFirstDlg( "GM：传送失败（地图或坐标不可用）。" );
	}
	else if( sub == "heal" )
	{
		m_pPlayer->RestoreAll();
		SendFirstDlg( "GM：已恢复 HP/MP。" );
	}
	else if( sub == "goto" || sub == "recall" )
	{
		std::string name;
		iss >> name;
		if( name.empty() ){ SendFirstDlg( "用法: /gm goto|recall <玩家>" ); return; }
		CHumanPlayer * pTarget = CHumanPlayerMgr::GetInstance()->FindbyName( name.c_str() );
		if( pTarget == NULL ){ SendFirstDlg( "玩家不在线。" ); return; }

		CHumanPlayer * pFrom = ( sub == "goto" ) ? m_pPlayer : pTarget;
		CHumanPlayer * pTo = ( sub == "goto" ) ? pTarget : m_pPlayer;
		if( Game_TeleportPlayer( pFrom, ( (CLogicMap*)pTo->GetMap() )->GetId(),
			pTo->getX(), pTo->getY() ) )
			SendFirstDlg( "GM：传送完成。" );
		else
			SendFirstDlg( "GM：传送失败。" );
	}
	else if( sub == "spawn" )
	{
		if( nLevel < 2 ){ SendFirstDlg( "权限不足（需要 GM 2 级）。" ); return; }
		std::string name;
		int nCount = 1;
		iss >> name >> nCount;
		if( name.empty() ){ SendFirstDlg( "用法: /gm spawn <怪物> [数量]" ); return; }
		if( nCount <= 0 )nCount = 1;
		if( nCount > 20 )nCount = 20;
		UINT nMap = ( (CLogicMap*)m_pPlayer->GetMap() )->GetId();
		int nOk = 0;
		for( int i = 0; i < nCount; ++i )
		{
			WORD x = (WORD)( m_pPlayer->getX() + Getrand( 5 ) - 2 );
			WORD y = (WORD)( m_pPlayer->getY() + Getrand( 5 ) - 2 );
			if( CMonsterMgr::GetInstance()->Spawn( name.c_str(), nMap, x, y ) != NULL )++nOk;
		}
		char szMsg[128];
		snprintf( szMsg, sizeof( szMsg ), "GM：刷出 %s x%d", name.c_str(), nOk );
		SendFirstDlg( szMsg );
	}
	else if( sub == "announce" )
	{
		if( nLevel < 2 ){ SendFirstDlg( "权限不足（需要 GM 2 级）。" ); return; }
		std::string msg;
		std::getline( iss, msg );
		while( !msg.empty() && msg[0] == ' ' )msg.erase( 0, 1 );
		if( msg.empty() ){ SendFirstDlg( "用法: /gm announce <内容>" ); return; }
		Game_Announce( msg.c_str() );
		SendFirstDlg( "GM：公告已发送。" );
	}
	else if( sub == "gmlist" )
	{
		if( nLevel < 2 ){ SendFirstDlg( "权限不足（需要 GM 2 级）。" ); return; }
		for( int i = 0; i < CGmMgr::GetInstance()->GetCount(); ++i )
		{
			GMACCOUNT * pAcc = CGmMgr::GetInstance()->GetByIndex( i );
			if( pAcc == NULL )continue;
			char szMsg[96];
			snprintf( szMsg, sizeof( szMsg ), "GM：%s 等级 %d", pAcc->name.c_str(), pAcc->level );
			SendFirstDlg( szMsg );
		}
	}
	else if( sub == "kick" )
	{
		if( nLevel < 3 ){ SendFirstDlg( "权限不足（需要 GM 3 级）。" ); return; }
		std::string name;
		iss >> name;
		CHumanPlayer * pTarget = CHumanPlayerMgr::GetInstance()->FindbyName( name.c_str() );
		if( pTarget == NULL || pTarget->GetClientObj() == NULL )
		{
			SendFirstDlg( "目标不在线或不是客户端角色。" );
			return;
		}
		CServer::GetInstance()->KickClient( pTarget->GetClientObj()->GetId() );
		SendFirstDlg( "GM：已踢下线。" );
	}
	else if( sub == "gmadd" || sub == "gmremove" )
	{
		if( nLevel < 3 ){ SendFirstDlg( "权限不足（需要 GM 3 级）。" ); return; }
		std::string name;
		int lv = 1;
		iss >> name >> lv;
		if( name.empty() ){ SendFirstDlg( "用法: /gm gmadd <玩家> <等级> | /gm gmremove <玩家>" ); return; }
		if( sub == "gmadd" )CGmMgr::GetInstance()->Add( name.c_str(), lv );
		else CGmMgr::GetInstance()->Remove( name.c_str() );
		CGmMgr::GetInstance()->Save( "data/gm.txt" );
		SendFirstDlg( "GM：账号已更新。" );
	}
	else
	{
		SendFirstDlg( "未知 GM 命令，输入 /gmhelp 查看。" );
	}
}
