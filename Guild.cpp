// Guild.cpp  ——  行会实现
#include "stdafx.h"
#include "Guild.h"
#include "HumanPlayer.h"
#include "HumanPlayerMgr.h"

#include <fstream>

CGuildMgr * CGuildMgr::m_pInstance = NULL;

CGuildMgr * CGuildMgr::GetInstance()
{
	if( m_pInstance == NULL )
		m_pInstance = new CGuildMgr;
	return m_pInstance;
}

BOOL CGuildMgr::Load( const char * pszFile )
{
	m_Guilds.clear();
	if( pszFile == NULL )return FALSE;

	std::ifstream in( pszFile, std::ios::binary );
	if( !in.is_open() )return FALSE;

	std::string line;
	while( std::getline( in, line ) )
	{
		if( line.empty() || line[0] == '#' )continue;

		//	格式：name/leader/vice/level/funds/member1,member2,...
		std::vector<std::string> parts;
		size_t pos = 0;
		while( true )
		{
			size_t slash = line.find( '/', pos );
			if( slash == std::string::npos )
			{
				parts.push_back( line.substr( pos ) );
				break;
			}
			parts.push_back( line.substr( pos, slash - pos ) );
			pos = slash + 1;
		}
		if( parts.size() < 2 )continue;

		GUILD g;
		g.name = parts[0];
		g.leader = parts[1];
		g.vice = parts.size() > 2 ? parts[2] : "";
		g.level = parts.size() > 3 ? atoi( parts[3].c_str() ) : 1;
		g.funds = parts.size() > 4 ? atoll( parts[4].c_str() ) : 0;
		if( g.level < 1 )g.level = 1;

		std::string list = parts.size() > 5 ? parts[5] : "";
		if( list.empty() )
		{
			g.members.push_back( g.leader );
		}
		else
		{
			size_t start = 0;
			while( start <= list.size() )
			{
				size_t comma = list.find( ',', start );
				std::string member = list.substr( start, comma == std::string::npos ? std::string::npos : comma - start );
				if( !member.empty() )g.members.push_back( member );
				if( comma == std::string::npos )break;
				start = comma + 1;
			}
		}
		m_Guilds.push_back( g );
	}
	return TRUE;
}

BOOL CGuildMgr::Save( const char * pszFile )
{
	if( pszFile == NULL )return FALSE;

	std::ofstream out( pszFile, std::ios::binary | std::ios::trunc );
	if( !out.is_open() )return FALSE;

	for( size_t i = 0; i < m_Guilds.size(); ++i )
	{
		out << m_Guilds[i].name << "/" << m_Guilds[i].leader << "/" << m_Guilds[i].vice
			<< "/" << m_Guilds[i].level << "/" << m_Guilds[i].funds << "/";
		for( size_t j = 0; j < m_Guilds[i].members.size(); ++j )
		{
			if( j > 0 )out << ",";
			out << m_Guilds[i].members[j];
		}
		out << "\n";
	}
	return TRUE;
}

CGuildMgr::GUILD * CGuildMgr::Find( const char * pszGuild )
{
	if( pszGuild == NULL )return NULL;
	for( size_t i = 0; i < m_Guilds.size(); ++i )
		if( m_Guilds[i].name == pszGuild )return &m_Guilds[i];
	return NULL;
}

CGuildMgr::GUILD * CGuildMgr::FindByMember( const char * pszPlayer )
{
	if( pszPlayer == NULL )return NULL;
	for( size_t i = 0; i < m_Guilds.size(); ++i )
	{
		for( size_t j = 0; j < m_Guilds[i].members.size(); ++j )
			if( m_Guilds[i].members[j] == pszPlayer )return &m_Guilds[i];
	}
	return NULL;
}

CGuildMgr::GUILD * CGuildMgr::GetByIndex( int nIndex )
{
	if( nIndex < 0 || (size_t)nIndex >= m_Guilds.size() )return NULL;
	return &m_Guilds[(size_t)nIndex];
}

int CGuildMgr::GetCount()
{
	return (int)m_Guilds.size();
}

BOOL CGuildMgr::Create( const char * pszGuild, const char * pszLeader )
{
	if( pszGuild == NULL || pszLeader == NULL )return FALSE;
	if( Find( pszGuild ) != NULL )return FALSE;
	if( FindByMember( pszLeader ) != NULL )return FALSE;

	GUILD g;
	g.name = pszGuild;
	g.leader = pszLeader;
	g.vice = "";
	g.level = 1;
	g.funds = 0;
	g.members.push_back( pszLeader );
	m_Guilds.push_back( g );
	return TRUE;
}

BOOL CGuildMgr::Join( const char * pszGuild, const char * pszPlayer )
{
	if( pszGuild == NULL || pszPlayer == NULL )return FALSE;
	if( FindByMember( pszPlayer ) != NULL )return FALSE;

	GUILD * g = Find( pszGuild );
	if( g == NULL )return FALSE;
	if( (int)g->members.size() >= MemberCap( g->level ) )return FALSE;

	g->members.push_back( pszPlayer );
	return TRUE;
}

BOOL CGuildMgr::Leave( const char * pszPlayer )
{
	if( pszPlayer == NULL )return FALSE;

	for( size_t i = 0; i < m_Guilds.size(); ++i )
	{
		GUILD & g = m_Guilds[i];
		for( size_t j = 0; j < g.members.size(); ++j )
		{
			if( g.members[j] != pszPlayer )continue;

			g.members.erase( g.members.begin() + j );
			if( g.members.empty() )
			{
				//	最后一人退出，行会解散
				m_Guilds.erase( m_Guilds.begin() + i );
			}
			else if( g.leader == pszPlayer )
			{
				g.leader = g.members[0];		//	会长自动移交给第一位成员
			}
			return TRUE;
		}
	}
	return FALSE;
}

VOID CGuildMgr::Broadcast( const char * pszGuild, const char * pszSender, const char * pszText )
{
	if( pszGuild == NULL || pszSender == NULL || pszText == NULL )return;
	GUILD * g = Find( pszGuild );
	if( g == NULL )return;

	char szMsg[256];
	snprintf( szMsg, sizeof( szMsg ), "【行会】%s: %s", pszSender, pszText );

	for( size_t i = 0; i < g->members.size(); ++i )
	{
		CHumanPlayer * pMember = CHumanPlayerMgr::GetInstance()->FindbyName( g->members[i].c_str() );
		if( pMember == NULL )continue;
		if( pMember->CanRecvMsg() )
			pMember->SendMsg( pMember->GetId(), SM_CHAT, 0xff00, 0, 0, (LPVOID)szMsg );
	}
}

//	资金 -> 行会等级
int CGuildMgr::LevelForFunds( long long funds )
{
	if( funds >= 100000 )return 5;
	if( funds >= 50000 )return 4;
	if( funds >= 20000 )return 3;
	if( funds >= 5000 )return 2;
	return 1;
}

BOOL CGuildMgr::Donate( const char * pszPlayer, int nGold, int & nNewLevel )
{
	nNewLevel = 0;
	if( pszPlayer == NULL || nGold <= 0 )return FALSE;

	GUILD * g = FindByMember( pszPlayer );
	if( g == NULL )return FALSE;

	g->funds += nGold;
	int nLevel = LevelForFunds( g->funds );
	if( nLevel > g->level )
	{
		g->level = nLevel;
		nNewLevel = nLevel;
	}
	return TRUE;
}

BOOL CGuildMgr::Kick( const char * pszLeader, const char * pszMember )
{
	if( pszLeader == NULL || pszMember == NULL )return FALSE;
	if( strcmp( pszLeader, pszMember ) == 0 )return FALSE;

	for( size_t i = 0; i < m_Guilds.size(); ++i )
	{
		GUILD & g = m_Guilds[i];
		if( g.leader != pszLeader )continue;

		for( size_t j = 0; j < g.members.size(); ++j )
		{
			if( g.members[j] != pszMember )continue;
			g.members.erase( g.members.begin() + j );
			if( g.vice == pszMember )g.vice.clear();
			return TRUE;
		}
	}
	return FALSE;
}

BOOL CGuildMgr::Appoint( const char * pszLeader, const char * pszMember )
{
	if( pszLeader == NULL || pszMember == NULL )return FALSE;
	if( strcmp( pszLeader, pszMember ) == 0 )return FALSE;

	for( size_t i = 0; i < m_Guilds.size(); ++i )
	{
		GUILD & g = m_Guilds[i];
		if( g.leader != pszLeader )continue;

		for( size_t j = 0; j < g.members.size(); ++j )
		{
			if( g.members[j] == pszMember )
			{
				g.vice = pszMember;
				return TRUE;
			}
		}
	}
	return FALSE;
}

int CGuildMgr::MemberCap( int nLevel )
{
	if( nLevel < 1 )nLevel = 1;
	if( nLevel > 5 )nLevel = 5;
	return 20 + ( nLevel - 1 ) * 10;		//	20/30/40/50/60
}

int CGuildMgr::ExpBonusPercent( int nLevel )
{
	if( nLevel < 1 )nLevel = 1;
	if( nLevel > 5 )nLevel = 5;
	return ( nLevel - 1 ) * 10;			//	2 级 +10%，5 级 +40%
}

BOOL CGuildMgr::AddToWarehouse( const char * pszPlayer, ITEM & item )
{
	GUILD * g = FindByMember( pszPlayer );
	if( g == NULL )return FALSE;
	if( g->warehouse.size() >= 50 )return FALSE;
	g->warehouse.push_back( item );
	return TRUE;
}

BOOL CGuildMgr::TakeFromWarehouse( const char * pszPlayer, int nIndex, ITEM & item )
{
	GUILD * g = FindByMember( pszPlayer );
	if( g == NULL )return FALSE;
	if( nIndex < 0 || (size_t)nIndex >= g->warehouse.size() )return FALSE;

	item = g->warehouse[(size_t)nIndex];
	g->warehouse.erase( g->warehouse.begin() + nIndex );
	return TRUE;
}

BOOL CGuildMgr::SaveWarehouse( const char * pszFile )
{
	if( pszFile == NULL )return FALSE;
	FILE * fp = fopen( pszFile, "wb" );
	if( fp == NULL )return FALSE;

	fwrite( "GWH1", 1, 4, fp );
	UINT nCount = (UINT)m_Guilds.size();
	fwrite( &nCount, sizeof( nCount ), 1, fp );

	for( size_t i = 0; i < m_Guilds.size(); ++i )
	{
		GUILD & g = m_Guilds[i];
		UINT nNameLen = (UINT)g.name.size();
		fwrite( &nNameLen, sizeof( nNameLen ), 1, fp );
		fwrite( g.name.data(), 1, nNameLen, fp );

		UINT nItems = (UINT)g.warehouse.size();
		fwrite( &nItems, sizeof( nItems ), 1, fp );
		if( nItems > 0 )
			fwrite( &g.warehouse[0], sizeof( ITEM ), nItems, fp );
	}
	fclose( fp );
	return TRUE;
}

BOOL CGuildMgr::LoadWarehouse( const char * pszFile )
{
	if( pszFile == NULL )return FALSE;
	FILE * fp = fopen( pszFile, "rb" );
	if( fp == NULL )return FALSE;

	char magic[4] = { 0 };
	if( fread( magic, 1, 4, fp ) != 4 || memcmp( magic, "GWH1", 4 ) != 0 )
	{
		fclose( fp );
		return FALSE;
	}

	UINT nCount = 0;
	if( fread( &nCount, sizeof( nCount ), 1, fp ) != 1 || nCount > 10000 )
	{
		fclose( fp );
		return FALSE;
	}

	for( UINT i = 0; i < nCount; ++i )
	{
		UINT nNameLen = 0;
		if( fread( &nNameLen, sizeof( nNameLen ), 1, fp ) != 1 || nNameLen > 128 )break;
		std::string name( nNameLen, '\0' );
		if( nNameLen > 0 && fread( &name[0], 1, nNameLen, fp ) != nNameLen )break;

		UINT nItems = 0;
		if( fread( &nItems, sizeof( nItems ), 1, fp ) != 1 || nItems > 50 )break;

		GUILD * g = Find( name.c_str() );
		for( UINT j = 0; j < nItems; ++j )
		{
			ITEM item;
			if( fread( &item, sizeof( ITEM ), 1, fp ) != 1 )break;
			if( g != NULL )g->warehouse.push_back( item );
		}
	}
	fclose( fp );
	return TRUE;
}
