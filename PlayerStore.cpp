// PlayerStore.cpp  ——  角色存档实现
#include "stdafx.h"
#include "PlayerStore.h"
#include "Set.h"
#include "HumanPlayer.h"
#include "HumanPlayerMgr.h"
#include "ItemBox.h"
#include "Equipment.h"

#include <sys/stat.h>
#ifdef _WIN32
#  include <direct.h>
#endif

static const char * STORE_MAGIC = "GSDB11";
static const DWORD STORE_VERSION = 5;

CPlayerStore * CPlayerStore::m_pInstance = NULL;

CPlayerStore * CPlayerStore::GetInstance()
{
	if( m_pInstance == NULL )
		m_pInstance = new CPlayerStore;
	return m_pInstance;
}

//	把角色名变成安全文件名：只保留字母、数字、>=0x80 的字节（中文），其余替换为 '_'
static std::string StoreSanitizeName( const char * pszName )
{
	std::string out;
	if( pszName != NULL )
	{
		for( const unsigned char * p = (const unsigned char *)pszName; *p != 0; ++p )
		{
			unsigned char c = *p;
			if( ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ) ||
				( c >= '0' && c <= '9' ) || c >= 0x80 )
				out.push_back( (char)c );
			else
				out.push_back( '_' );
		}
	}
	if( out.empty() )out = "unnamed";
	return out;
}

VOID CPlayerStore::EnsureDir()
{
#ifdef _WIN32
	_mkdir( "data" );
	_mkdir( "data/players" );
#else
	mkdir( "data", 0755 );
	mkdir( "data/players", 0755 );
#endif
}

std::string CPlayerStore::PathFor( const char * pszName ) const
{
	return std::string( "data/players/" ) + StoreSanitizeName( pszName ) + ".dat";
}

BOOL CPlayerStore::Exists( const char * pszName )
{
	if( pszName == NULL || *pszName == '\0' )return FALSE;
	FILE * fp = fopen( PathFor( pszName ).c_str(), "rb" );
	if( fp == NULL )return FALSE;
	fclose( fp );
	return TRUE;
}

BOOL CPlayerStore::Save( CHumanPlayer * pPlayer )
{
	if( pPlayer == NULL )return FALSE;
	EnsureDir();

	FILE * fp = fopen( PathFor( pPlayer->GetName() ).c_str(), "wb" );
	if( fp == NULL )return FALSE;

	CHARDBINFO info;
	pPlayer->GetDBInfo( info );

	DWORD dwVersion = STORE_VERSION;
	DWORD dwLearned = pPlayer->GetLearnedSkills();
	fwrite( STORE_MAGIC, 1, 6, fp );
	fwrite( &dwVersion, sizeof( dwVersion ), 1, fp );
	fwrite( &info, sizeof( info ), 1, fp );
	fwrite( &dwLearned, sizeof( dwLearned ), 1, fp );

	int nQuestCount = pPlayer->GetQuestCount();
	fwrite( &nQuestCount, sizeof( nQuestCount ), 1, fp );
	if( nQuestCount > 0 && pPlayer->GetQuestArray() != NULL )
		fwrite( pPlayer->GetQuestArray(), sizeof( PLAYERQUEST ), (size_t)nQuestCount, fp );

	ITEM	bag[46];
	int		nBag = pPlayer->GetBag().GetItems( bag, 46 );
	fwrite( &nBag, sizeof( nBag ), 1, fp );
	if( nBag > 0 )fwrite( bag, sizeof( ITEM ), (size_t)nBag, fp );

	UINT nEquip = 0;
	for( int i = 0; i < _U_MAX; ++i )
		if( pPlayer->GetEquipments().GetEquipment( i ) != NULL )++nEquip;
	fwrite( &nEquip, sizeof( nEquip ), 1, fp );
	for( int i = 0; i < _U_MAX; ++i )
	{
		ITEM * pItem = pPlayer->GetEquipments().GetEquipment( i );
		if( pItem == NULL )continue;
		BYTE pos = (BYTE)i;
		fwrite( &pos, 1, 1, fp );
		fwrite( pItem, sizeof( ITEM ), 1, fp );
	}

	//	成就与称号
	int nAchCount = (int)pPlayer->GetAchievements().size();
	fwrite( &nAchCount, sizeof( nAchCount ), 1, fp );
	if( nAchCount > 0 )
		fwrite( &pPlayer->GetAchievements()[0], sizeof( int ), (size_t)nAchCount, fp );
	int nTitle = pPlayer->GetTitle();
	fwrite( &nTitle, sizeof( nTitle ), 1, fp );

	//	成就点数消费与兑换称号
	int nSpent = pPlayer->GetPointsSpent();
	fwrite( &nSpent, sizeof( nSpent ), 1, fp );
	int nShop = (int)pPlayer->GetShopTitles().size();
	fwrite( &nShop, sizeof( nShop ), 1, fp );
	if( nShop > 0 )
		fwrite( &pPlayer->GetShopTitles()[0], sizeof( int ), (size_t)nShop, fp );

	//	图鉴：已见过的物品名
	const std::set<std::string> & codex = pPlayer->GetCodex();
	int nCodex = (int)codex.size();
	fwrite( &nCodex, sizeof( nCodex ), 1, fp );
	for( std::set<std::string>::const_iterator it = codex.begin(); it != codex.end(); ++it )
	{
		int nLen = (int)it->size();
		fwrite( &nLen, sizeof( nLen ), 1, fp );
		if( nLen > 0 )fwrite( it->data(), 1, (size_t)nLen, fp );
	}

	//	已发放过奖励的套装
	int nRewarded = 0;
	for( int i = 0; i < CSetMgr::GetInstance()->GetCount(); ++i )
	{
		SETDEF * pDef = CSetMgr::GetInstance()->GetByIndex( i );
		if( pDef != NULL && pPlayer->HasCodexReward( pDef->szName ) )++nRewarded;
	}
	fwrite( &nRewarded, sizeof( nRewarded ), 1, fp );
	for( int i = 0; i < CSetMgr::GetInstance()->GetCount(); ++i )
	{
		SETDEF * pDef = CSetMgr::GetInstance()->GetByIndex( i );
		if( pDef == NULL || !pPlayer->HasCodexReward( pDef->szName ) )continue;
		int nLen = (int)strlen( pDef->szName );
		fwrite( &nLen, sizeof( nLen ), 1, fp );
		fwrite( pDef->szName, 1, (size_t)nLen, fp );
	}

	fclose( fp );
	return TRUE;
}

BOOL CPlayerStore::Load( const char * pszName, CHARDBINFO & info,
						 std::vector<ITEM> & bag,
						 std::vector<std::pair<int, ITEM> > & equips,
						 DWORD & dwLearnedSkills,
						 std::vector<PLAYERQUEST> & quests,
						 std::vector<int> & achievements,
						 int & nTitle,
						 int & nPointsSpent,
						 std::vector<int> & shopTitles,
						 std::vector<std::string> & codex,
						 std::vector<std::string> & codexRewarded )
{
	bag.clear();
	equips.clear();
	quests.clear();
	achievements.clear();
	shopTitles.clear();
	codex.clear();
	nTitle = 0;
	nPointsSpent = 0;
	dwLearnedSkills = 0;
	if( pszName == NULL || *pszName == '\0' )return FALSE;

	FILE * fp = fopen( PathFor( pszName ).c_str(), "rb" );
	if( fp == NULL )return FALSE;

	char	magic[6];
	DWORD	dwVersion = 0;
	if( fread( magic, 1, 6, fp ) != 6 || memcmp( magic, STORE_MAGIC, 6 ) != 0 ||
		fread( &dwVersion, sizeof( dwVersion ), 1, fp ) != 1 || dwVersion != STORE_VERSION )
	{
		fclose( fp );
		return FALSE;
	}
	if( fread( &info, sizeof( info ), 1, fp ) != 1 ||
		fread( &dwLearnedSkills, sizeof( dwLearnedSkills ), 1, fp ) != 1 )
	{
		fclose( fp );
		return FALSE;
	}

	int nQuestCount = 0;
	if( fread( &nQuestCount, sizeof( nQuestCount ), 1, fp ) != 1 || nQuestCount < 0 || nQuestCount > QUEST_MAX_PER_PLAYER )
	{
		fclose( fp );
		return FALSE;
	}
	quests.resize( (size_t)nQuestCount );
	if( nQuestCount > 0 && fread( &quests[0], sizeof( PLAYERQUEST ), (size_t)nQuestCount, fp ) != (size_t)nQuestCount )
	{
		fclose( fp );
		return FALSE;
	}

	int nBag = 0;
	if( fread( &nBag, sizeof( nBag ), 1, fp ) != 1 || nBag < 0 || nBag > 200 )
	{
		fclose( fp );
		return FALSE;
	}
	bag.resize( (size_t)nBag );
	if( nBag > 0 && fread( &bag[0], sizeof( ITEM ), (size_t)nBag, fp ) != (size_t)nBag )
	{
		fclose( fp );
		return FALSE;
	}

	UINT nEquip = 0;
	if( fread( &nEquip, sizeof( nEquip ), 1, fp ) != 1 || nEquip > (UINT)_U_MAX )
	{
		fclose( fp );
		return FALSE;
	}
	for( UINT i = 0; i < nEquip; ++i )
	{
		BYTE pos = 0;
		ITEM item;
		if( fread( &pos, 1, 1, fp ) != 1 || fread( &item, sizeof( item ), 1, fp ) != 1 )
		{
			fclose( fp );
			return FALSE;
		}
		if( pos < _U_MAX )
			equips.push_back( std::make_pair( (int)pos, item ) );
	}

	int nAchCount = 0;
	if( fread( &nAchCount, sizeof( nAchCount ), 1, fp ) != 1 || nAchCount < 0 || nAchCount > 256 )
	{
		fclose( fp );
		return FALSE;
	}
	achievements.resize( (size_t)nAchCount );
	if( nAchCount > 0 && fread( &achievements[0], sizeof( int ), (size_t)nAchCount, fp ) != (size_t)nAchCount )
	{
		fclose( fp );
		return FALSE;
	}
	if( fread( &nTitle, sizeof( nTitle ), 1, fp ) != 1 )
	{
		fclose( fp );
		return FALSE;
	}

	int nSpent = 0, nShop = 0;
	if( fread( &nSpent, sizeof( nSpent ), 1, fp ) != 1 || nSpent < 0 ||
		fread( &nShop, sizeof( nShop ), 1, fp ) != 1 || nShop < 0 || nShop > 64 )
	{
		fclose( fp );
		return FALSE;
	}
	shopTitles.resize( (size_t)nShop );
	if( nShop > 0 && fread( &shopTitles[0], sizeof( int ), (size_t)nShop, fp ) != (size_t)nShop )
	{
		fclose( fp );
		return FALSE;
	}
	nPointsSpent = nSpent;

	int nCodex = 0;
	if( fread( &nCodex, sizeof( nCodex ), 1, fp ) != 1 || nCodex < 0 || nCodex > 256 )
	{
		fclose( fp );
		return FALSE;
	}
	for( int i = 0; i < nCodex; ++i )
	{
		int nLen = 0;
		if( fread( &nLen, sizeof( nLen ), 1, fp ) != 1 || nLen < 0 || nLen > 64 )
		{
			fclose( fp );
			return FALSE;
		}
		std::string name( (size_t)nLen, '\0' );
		if( nLen > 0 && fread( &name[0], 1, (size_t)nLen, fp ) != (size_t)nLen )
		{
			fclose( fp );
			return FALSE;
		}
		codex.push_back( name );
	}

	int nRewarded = 0;
	if( fread( &nRewarded, sizeof( nRewarded ), 1, fp ) != 1 || nRewarded < 0 || nRewarded > 256 )
	{
		fclose( fp );
		return FALSE;
	}
	for( int i = 0; i < nRewarded; ++i )
	{
		int nLen = 0;
		if( fread( &nLen, sizeof( nLen ), 1, fp ) != 1 || nLen < 0 || nLen > 64 )
		{
			fclose( fp );
			return FALSE;
		}
		std::string name( (size_t)nLen, '\0' );
		if( nLen > 0 && fread( &name[0], 1, (size_t)nLen, fp ) != (size_t)nLen )
		{
			fclose( fp );
			return FALSE;
		}
		codexRewarded.push_back( name );
	}

	fclose( fp );
	return TRUE;
}

int CPlayerStore::SaveAll()
{
	int nSaved = 0;
	CIndexList<CHumanPlayer, MAX_HUMANPLAYER> & list = CHumanPlayerMgr::GetInstance()->GetPlayerList();
	for( CHumanPlayer * pPlayer = list.First(); pPlayer != NULL; pPlayer = list.Next() )
	{
		if( Save( pPlayer ) )++nSaved;
	}
	return nSaved;
}
