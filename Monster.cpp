// Monster.cpp  ——  怪物实现（模板、生成、简单 AI、死亡掉落）
#include "stdafx.h"
#include "Monster.h"
#include "HumanPlayer.h"
#include "HumanPlayerMgr.h"
#include "LogicMapMgr.h"
#include "ItemManager.h"
#include "DownItemMgr.h"
#include "Party.h"
#include "Boss.h"
#include "Guild.h"
#include "Dungeon.h"

CMonsterMgr * CMonsterMgr::m_pInstance = NULL;

CMonsterMgr * CMonsterMgr::GetInstance()
{
	if( m_pInstance == NULL )
		m_pInstance = new CMonsterMgr;
	return m_pInstance;
}

// ---------------------------------------------------------------------------
// CMonster
// ---------------------------------------------------------------------------
CMonster::CMonster()
	: m_nHp( 1 ), m_nMaxHp( 1 ), m_nMinAtk( 1 ), m_nMaxAtk( 1 ), m_nDef( 0 ),
	  m_nExp( 0 ), m_nLevel( 1 ), m_nShape( 0 ), m_nDropRate( 0 ),
	  m_dwTargetId( 0 ), m_dwNextThink( 0 ), m_bDead( FALSE ), m_nListId( 0 )
{
	m_szName[0] = '\0';
	m_szDropItem[0] = '\0';
}

CMonster::~CMonster()
{
}

BOOL CMonster::Init( const MONSTERTEMPLATE & tpl, WORD x, WORD y )
{
	o_strncpy( m_szName, tpl.szName, sizeof( m_szName ) - 1 );
	o_strncpy( m_szDropItem, tpl.szDropItem, sizeof( m_szDropItem ) - 1 );

	m_nMaxHp = tpl.nMaxHp > 0 ? tpl.nMaxHp : 1;
	m_nHp = m_nMaxHp;
	m_nMinAtk = tpl.nMinAtk;
	m_nMaxAtk = tpl.nMaxAtk > tpl.nMinAtk ? tpl.nMaxAtk : tpl.nMinAtk;
	m_nDef = tpl.nDef;
	m_nExp = tpl.nExp;
	m_nLevel = tpl.nLevel;
	m_nShape = tpl.nShape;
	m_nDropRate = tpl.nDropRate;

	m_bDead = FALSE;
	m_bPet = FALSE;
	m_nOwnerId = 0;
	m_dwTargetId = 0;
	m_dwNextThink = 0;

	SetDirection( ED_DN );
	setXY( x, y );
	return TRUE;
}

int CMonster::GetPropValue( PROP_INDEX index )
{
	switch( index )
	{
	case	PI_CURHP:	return m_nHp;
	case	PI_MAXHP:	return m_nMaxHp;
	case	PI_MINAC:
	case	PI_MAXAC:	return m_nDef;
	case	PI_MINMAC:
	case	PI_MAXMAC:	return 0;
	case	PI_MINDC:	return m_nMinAtk;
	case	PI_MAXDC:	return m_nMaxAtk;
	case	PI_LEVEL:	return m_nLevel;
	default:			return 0;
	}
}

VOID CMonster::SetCurHp( int nHp )
{
	if( nHp < 0 )nHp = 0;
	if( nHp > m_nMaxHp )nHp = m_nMaxHp;
	m_nHp = nHp;
}

BOOL CMonster::BeAttack( CAliveObject * pAttacker )
{
	if( pAttacker != NULL && pAttacker->GetType() == OBJ_PLAYER )
		m_dwTargetId = pAttacker->GetId();
	return CAliveObject::BeAttack( pAttacker );
}

BOOL CMonster::OnDeath( CAliveObject * pAttacker )
{
	if( m_bDead )return TRUE;
	m_bDead = TRUE;

	if( m_bPet )
	{
		if( GetMap() != NULL )GetMap()->RemoveObject( this );
		return TRUE;
	}

	//	经验给最后一击的玩家
	if( pAttacker != NULL && pAttacker->GetType() == OBJ_PLAYER )
	{
		CHumanPlayer * pPlayer = CHumanPlayerMgr::GetInstance()->FindbyId( pAttacker->GetId() );
		if( pPlayer != NULL )
		{
			int nExp = m_nExp;
			CGuildMgr::GUILD * pGuild = CGuildMgr::GetInstance()->FindByMember( pPlayer->GetName() );
			if( pGuild != NULL )
				nExp += nExp * CGuildMgr::ExpBonusPercent( pGuild->level ) / 100;	//	行会等级经验加成

			pPlayer->AddExp( nExp );
			pPlayer->OnKillMonster( m_szName );		//	杀怪任务进度
			CPartyMgr::GetInstance()->ShareExp( pPlayer, nExp );	//	组队分经验
		}
	}

	//	掉落：优先用 drops.txt 多掉落表，否则回退到模板里的单掉落
	if( GetMap() != NULL )
	{
		if( CMonsterMgr::GetInstance()->HasDrops( m_szName ) )
		{
			CMonsterMgr::GetInstance()->RollDrops( m_szName, GetMap(), m_wX, m_wY );
		}
		else if( m_nDropRate > 0 && Getrand( 100 ) < m_nDropRate )
		{
			ITEM item;
			if( CItemManager::GetInstance()->MakeupTemplateItem( m_szDropItem, item ) )
			{
				item.dwMakeIndex = CItemManager::NewMakeIndex();
				CDownItemMgr::GetInstance()->DropItem( GetMap(), item, m_wX, m_wY, 0 );
			}
		}
	}

	CBossMgr::GetInstance()->OnMonsterDead( m_szName, GetMapId(), GetId(), pAttacker );
	CDungeonMgr::GetInstance()->OnMonsterDead( m_szName, GetMapId() );

	if( GetMap() != NULL )
		GetMap()->RemoveObject( this );
	return TRUE;
}

BOOL CMonster::GetViewmsg( char * pszMsg, int & length )
{
	//	8 字节血量 + 名字（客户端需要显示怪物名）
	char szData[64];
	DWORD dwData[2] = { (DWORD)m_nHp, (DWORD)m_nMaxHp };
	memcpy( szData, dwData, sizeof( dwData ) );
	o_strncpy( szData + sizeof( dwData ), m_szName, sizeof( szData ) - sizeof( dwData ) - 1 );

	length = EncodeMsg( pszMsg, GetId(), SM_APPEAR, m_wX, m_wY, (WORD)m_nShape,
						(LPVOID)szData, (int)( sizeof( dwData ) + strlen( m_szName ) ) );
	return TRUE;
}

BOOL CMonster::GetOutViewmsg( char * pszMsg, int & length )
{
	length = EncodeMsg( pszMsg, GetId(), SM_DISAPPEAR, m_wX, m_wY, 0, NULL );
	return TRUE;
}

//	把 dx/dy 归一成 8 方向
int CMonster::DirTo( int dx, int dy )
{
	int sx = ( dx > 0 ) ? 1 : ( ( dx < 0 ) ? -1 : 0 );
	int sy = ( dy > 0 ) ? 1 : ( ( dy < 0 ) ? -1 : 0 );

	if( sx == 0 && sy < 0 )return ED_UP;
	if( sx > 0 && sy < 0 )return ED_UPRIGHT;
	if( sx > 0 && sy == 0 )return ED_RIGHT;
	if( sx > 0 && sy > 0 )return ED_DOWNRIGHT;
	if( sx == 0 && sy > 0 )return ED_DN;
	if( sx < 0 && sy > 0 )return ED_DOWNLEFT;
	if( sx < 0 && sy == 0 )return ED_LEFT;
	return ED_UPLEFT;
}

VOID CMonster::Think()
{
	if( m_bPet )
	{
		CHumanPlayer * pOwner = CHumanPlayerMgr::GetInstance()->FindbyId( m_nOwnerId );
		if( pOwner == NULL || pOwner->GetMap() != GetMap() )
		{
			m_bDead = TRUE;
			return;
		}
		CMonster * pBest = NULL;
		int nBest = 999;
		for( int dy = -7; dy <= 7; ++dy )
		{
			for( int dx = -7; dx <= 7; ++dx )
			{
				int tx = (int)m_wX + dx, ty = (int)m_wY + dy;
				if( tx < 0 || ty < 0 )continue;
				CVArea * pArea = GetMap()->GetArea( tx, ty );
				if( pArea == NULL )continue;
				CMapObject * pObj = pArea->FindObject( (WORD)tx, (WORD)ty, OBJ_MONSTER );
				if( pObj == NULL || pObj == this )continue;
				CMonster * pMon = (CMonster *)pObj;
				if( pMon->IsPet() || pMon->IsDead() )continue;
				int d = ( dx < 0 ? -dx : dx ) + ( dy < 0 ? -dy : dy );
				if( d < nBest ){ nBest = d; pBest = pMon; }
			}
		}
		if( pBest != NULL )
		{
			int dx = (int)pBest->getX() - (int)m_wX;
			int dy = (int)pBest->getY() - (int)m_wY;
			int dir = DirTo( dx, dy );
			SetDirection( (e_direction)dir );
			if( ( dx < 0 ? -dx : dx ) <= 1 && ( dy < 0 ? -dy : dy ) <= 1 )
			{
				if( GetActionType() == AT_STAND )
				{
					int dmg = GetPropValue( PI_MINDC ) + Getrand( GetPropValue( PI_MAXDC ) - GetPropValue( PI_MINDC ) + 1 );
					pBest->ApplyDamage( dmg > 0 ? dmg : 1, pOwner );
					Attack( dir );
				}
			}
			else if( GetActionType() == AT_STAND )
				Walk( dir, TRUE );
			return;
		}
		int odx = (int)pOwner->getX() - (int)m_wX;
		int ody = (int)pOwner->getY() - (int)m_wY;
		if( ( odx < 0 ? -odx : odx ) + ( ody < 0 ? -ody : ody ) > 2 && GetActionType() == AT_STAND )
			Walk( DirTo( odx, ody ), TRUE );
		else if( Getrand( 5 ) == 0 )Walk( Getrand( 8 ), TRUE );
		return;
	}

	CHumanPlayer * pTarget = NULL;
	if( m_dwTargetId != 0 )
		pTarget = CHumanPlayerMgr::GetInstance()->FindbyId( m_dwTargetId );
	if( pTarget != NULL && pTarget->GetMap() != GetMap() )
		pTarget = NULL;

	if( pTarget != NULL )
	{
		int dx = (int)pTarget->getX() - (int)m_wX;
		int dy = (int)pTarget->getY() - (int)m_wY;
		int adx = dx < 0 ? -dx : dx;
		int ady = dy < 0 ? -dy : dy;

		if( adx <= 1 && ady <= 1 )
		{
			int dir = DirTo( dx, dy );
			SetDirection( (e_direction)dir );
			Attack( dir );
			return;
		}
		if( adx + ady <= 12 )
		{
			int dir = DirTo( dx, dy );
			if( !Walk( dir, TRUE ) )
			{
				if( !Walk( ( dir + 1 ) & 7, TRUE ) )
					Walk( ( dir + 7 ) & 7, TRUE );
			}
			return;
		}
		m_dwTargetId = 0;		//	目标跑远了
	}

	if( Getrand( 3 ) == 0 )
		Walk( Getrand( 8 ), TRUE );
}

VOID CMonster::Update()
{
	if( m_bDead )return;

	CAliveObject::Update();
	if( GetActionType() != AT_STAND )return;

	DWORD dwNow = timeGetTime();
	if( dwNow < m_dwNextThink )return;
	m_dwNextThink = dwNow + 400 + Getrand( 400 );
	Think();
}

// ---------------------------------------------------------------------------
// CMonsterMgr
// ---------------------------------------------------------------------------
BOOL CMonsterMgr::LoadTemplates( const char * pszFile )
{
	m_Templates.clear();

	CStringFile sf( pszFile );
	if( sf.GetLineCount() <= 0 )return FALSE;

	for( int i = 0; i < sf.GetLineCount(); ++i )
	{
		const char * pszLine = sf[i];
		if( pszLine == NULL || *pszLine == '\0' || *pszLine == '#' )continue;

		char	szBuffer[512];
		o_strncpy( szBuffer, pszLine, sizeof( szBuffer ) - 1 );

		char *	Params[16];
		int		nParam = SearchParam( szBuffer, Params, 16, '/' );
		if( nParam < 8 )continue;

		MONSTERTEMPLATE tpl;
		memset( &tpl, 0, sizeof( tpl ) );
		o_strncpy( tpl.szName, Params[0], sizeof( tpl.szName ) - 1 );
		tpl.nMaxHp = atoi( Params[1] );
		tpl.nMinAtk = atoi( Params[2] );
		tpl.nMaxAtk = atoi( Params[3] );
		tpl.nDef = atoi( Params[4] );
		tpl.nExp = atoi( Params[5] );
		o_strncpy( tpl.szDropItem, Params[6], sizeof( tpl.szDropItem ) - 1 );
		tpl.nDropRate = atoi( Params[7] );
		tpl.nLevel = ( nParam >= 9 ) ? atoi( Params[8] ) : 1;
		tpl.nShape = ( nParam >= 10 ) ? atoi( Params[9] ) : 0;
		if( tpl.nMaxHp <= 0 )tpl.nMaxHp = 1;

		m_Templates.push_back( tpl );
	}
	return TRUE;
}

BOOL CMonsterMgr::LoadDrops( const char * pszFile )
{
	m_Drops.clear();

	CStringFile sf( pszFile );
	for( int i = 0; i < sf.GetLineCount(); ++i )
	{
		const char * pszLine = sf[i];
		if( pszLine == NULL || *pszLine == '\0' || *pszLine == '#' )continue;

		char	szBuffer[256];
		o_strncpy( szBuffer, pszLine, sizeof( szBuffer ) - 1 );
		char *	Params[8];
		if( SearchParam( szBuffer, Params, 8, '/' ) < 3 )continue;

		MONSTERDROP drop;
		memset( &drop, 0, sizeof( drop ) );
		o_strncpy( drop.szItem, Params[1], sizeof( drop.szItem ) - 1 );
		drop.nRate = atoi( Params[2] );
		if( drop.nRate < 0 )drop.nRate = 0;
		if( drop.nRate > 100 )drop.nRate = 100;
		m_Drops[Params[0]].push_back( drop );
	}
	return !m_Drops.empty();
}

BOOL CMonsterMgr::HasDrops( const char * pszMonster )
{
	if( pszMonster == NULL )return FALSE;
	return m_Drops.find( pszMonster ) != m_Drops.end();
}

int CMonsterMgr::GetDropCount( const char * pszMonster )
{
	if( pszMonster == NULL )return 0;
	std::map<std::string, std::vector<MONSTERDROP> >::iterator it = m_Drops.find( pszMonster );
	if( it == m_Drops.end() )return 0;
	return (int)it->second.size();
}

const MONSTERDROP * CMonsterMgr::GetDropByIndex( const char * pszMonster, int nIndex )
{
	if( pszMonster == NULL || nIndex < 0 )return NULL;
	std::map<std::string, std::vector<MONSTERDROP> >::iterator it = m_Drops.find( pszMonster );
	if( it == m_Drops.end() || (size_t)nIndex >= it->second.size() )return NULL;
	return &it->second[(size_t)nIndex];
}

VOID CMonsterMgr::RollDrops( const char * pszMonster, CVLogicMap * pMap, WORD x, WORD y )
{
	if( pszMonster == NULL || pMap == NULL )return;
	std::map<std::string, std::vector<MONSTERDROP> >::iterator it = m_Drops.find( pszMonster );
	if( it == m_Drops.end() )return;

	for( size_t i = 0; i < it->second.size(); ++i )
	{
		const MONSTERDROP & drop = it->second[i];
		if( drop.nRate <= 0 || Getrand( 100 ) >= drop.nRate )continue;

		ITEM item;
		if( !CItemManager::GetInstance()->MakeupTemplateItem( drop.szItem, item ) )continue;
		item.dwMakeIndex = CItemManager::NewMakeIndex();
		CDownItemMgr::GetInstance()->DropItem( pMap, item, x, y, 0 );
	}
}

CMonster * CMonsterMgr::Spawn( const char * pszName, UINT mapid, WORD x, WORD y )
{
	const MONSTERTEMPLATE * pTpl = NULL;
	for( size_t i = 0; i < m_Templates.size(); ++i )
	{
		if( strcmp( m_Templates[i].szName, pszName ) == 0 )
		{
			pTpl = &m_Templates[i];
			break;
		}
	}
	if( pTpl == NULL )return NULL;

	CVLogicMap * pMap = CLogicMapMgr::GetInstance()->GetLogicMapById( mapid );
	if( pMap == NULL )return NULL;

	CMonster *	pMonster = NULL;
	UINT		id = m_Monsters.New( &pMonster );
	if( id == 0 || pMonster == NULL )return NULL;

	if( !pMonster->Init( *pTpl, x, y ) )
	{
		m_Monsters.Del( id );
		return NULL;
	}
	pMonster->SetListId( id );
	pMonster->SetId( id | ( (UINT)OBJ_MONSTER << 24 ) );
	pMonster->SetMapId( mapid );

	if( !pMap->AddObject( pMonster ) )
	{
		m_Monsters.Del( id );
		return NULL;
	}
	return pMonster;
}

int CMonsterMgr::SpawnFromFile( const char * pszFile )
{
	CStringFile sf( pszFile );
	if( sf.GetLineCount() <= 0 )return 0;

	int nSpawned = 0;
	for( int i = 0; i < sf.GetLineCount(); ++i )
	{
		const char * pszLine = sf[i];
		if( pszLine == NULL || *pszLine == '\0' || *pszLine == '#' )continue;

		char	szBuffer[512];
		o_strncpy( szBuffer, pszLine, sizeof( szBuffer ) - 1 );

		char *	Params[8];
		int		nParam = SearchParam( szBuffer, Params, 8, '/' );
		if( nParam < 5 )continue;

		UINT	mapid = (UINT)atoi( Params[0] );
		WORD	x = (WORD)atoi( Params[1] );
		WORD	y = (WORD)atoi( Params[2] );
		int		nCount = atoi( Params[3] );
		if( nCount <= 0 )continue;
		if( nCount > 50 )nCount = 50;

		SPAWNDEF def;
		memset( &def, 0, sizeof( def ) );
		def.nMapId = mapid;
		def.x = x;
		def.y = y;
		def.nCount = nCount;
		o_strncpy( def.szName, Params[4], sizeof( def.szName ) - 1 );
		m_Spawns.push_back( def );

		for( int n = 0; n < nCount; ++n )
		{
			WORD sx = (WORD)( x + Getrand( 5 ) - 2 );
			WORD sy = (WORD)( y + Getrand( 5 ) - 2 );
			if( Spawn( Params[4], mapid, sx, sy ) != NULL )++nSpawned;
		}
	}
	return nSpawned;
}

VOID CMonsterMgr::Update()
{
	std::vector<UINT> dead;
	for( CMonster * pMonster = m_Monsters.First(); pMonster != NULL; pMonster = m_Monsters.Next() )
	{
		pMonster->Update();
		if( pMonster->IsDead() )dead.push_back( pMonster->GetListId() );
	}
	for( size_t i = 0; i < dead.size(); ++i )
		m_Monsters.Del( dead[i] );
}

VOID CMonsterMgr::ClearAll()
{
	//	先从地图上移除，避免地图/区域里留下悬空指针
	for( CMonster * pMonster = m_Monsters.First(); pMonster != NULL; pMonster = m_Monsters.Next() )
	{
		if( pMonster->GetMap() != NULL )
			pMonster->GetMap()->RemoveObject( pMonster );
	}
	m_Monsters.Clean();
}

int CMonsterMgr::CountMap( UINT mapid )
{
	int nCount = 0;
	for( CMonster * p = m_Monsters.First(); p != NULL; p = m_Monsters.Next() )
		if( p->GetMapId() == mapid )++nCount;
	return nCount;
}

VOID CMonsterMgr::ResetMap( UINT mapid )
{
	CVLogicMap * pMap = CLogicMapMgr::GetInstance()->GetLogicMapById( mapid );
	if( pMap == NULL )return;

	std::vector<UINT> ids;
	for( CMonster * pMonster = m_Monsters.First(); pMonster != NULL; pMonster = m_Monsters.Next() )
	{
		if( pMonster->GetMapId() != mapid )continue;
		pMap->RemoveObject( pMonster );
		ids.push_back( pMonster->GetListId() );
	}
	for( size_t i = 0; i < ids.size(); ++i )
		m_Monsters.Del( ids[i] );

	//	按保存的刷怪点重新生成
	for( size_t i = 0; i < m_Spawns.size(); ++i )
	{
		const SPAWNDEF & def = m_Spawns[i];
		if( def.nMapId != mapid )continue;
		for( int n = 0; n < def.nCount; ++n )
		{
			WORD sx = (WORD)( def.x + Getrand( 5 ) - 2 );
			WORD sy = (WORD)( def.y + Getrand( 5 ) - 2 );
			Spawn( def.szName, mapid, sx, sy );
		}
	}
}

CMonster * CMonsterMgr::FindById( UINT nObjectId )
{
	for( CMonster * p = m_Monsters.First(); p != NULL; p = m_Monsters.Next() )
		if( p->GetId() == nObjectId )return p;
	return NULL;
}

int CMonsterMgr::GetCount()
{
	return m_Monsters.getCount();
}
