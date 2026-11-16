#include "stdafx.h"
#include "HumanPlayer.h"
#include "gsClientObj.h"
#include "LogicMap.h"
#include "Server.h"
#include "DownItemMgr.h"
#include "Equipment.h"
#include "ClassTable.h"
#include "Skill.h"
#include "Quest.h"
#include "ItemManager.h"
#include "Event.h"
#include "Achievement.h"
#include "Title.h"
#include "Set.h"
#include "Monster.h"
#include "GameActions.h"
#define MAKEFEATHER(a,hair,b,c)  ((DWORD)(((DWORD)((a)&0xff) << 24) | ((DWORD)((hair)&0xff) << 16 )|( (DWORD)((b)&0xff) << 8 )|((DWORD)((c)&0xff))))

CHumanPlayer::CHumanPlayer(void):m_pClientObj(NULL),
	m_Equipments(this),m_xGroupList(NULL),
	m_GroupListNode(this),m_ExchangeObj(this)
{
	m_ItemBox.Create(46);
	m_nTitle = 0;
	m_nPointsSpent = 0;
	m_dwShieldUntil = 0;
	m_nBuffCount = 0;
}

CHumanPlayer::~CHumanPlayer(void)
{
}

BOOL CHumanPlayer::Init( CREATEHUMANDESC & desc )
{
	BOOL bNewPlayer = ( desc.dbinfo.wLevel == 0 );

	//	新角色：1 级、职业由创建方指定、1000 金币（参考传奇世界的初始状态）
	if( bNewPlayer )
	{
		if( desc.dbinfo.btClass > 2 )desc.dbinfo.btClass = 0;
		desc.dbinfo.wLevel = 1;
		desc.dbinfo.x = 477;
		desc.dbinfo.y = 222;
		desc.dbinfo.mapid = 1;
		desc.dbinfo.dwGold = 1000;
		desc.dbinfo.dwCurExp = 0;
	}
	m_Humandesc = desc;
	m_pClientObj = desc.pClientObj;

	//	新角色按职业成长表计算属性；读档角色保留存档里的属性
	if( bNewPlayer )
		ApplyClassStats();

	setXY( desc.dbinfo.x, desc.dbinfo.y );
	SetMapId( desc.dbinfo.mapid );

	for( int i = 0; i < 32; ++i )
		m_dwSkillReady[i] = 0;
	m_dwNextRegen = 0;
	m_dwLearnedSkills = 0;
	m_nQuestCount = 0;
	m_dwShieldUntil = 0;

	//	新角色自动掌握本职业 1 级技能
	if( bNewPlayer )
	{
		CSkillMgr * pSkillMgr = CSkillMgr::GetInstance();
		for( int i = 0; i < pSkillMgr->GetCount(); ++i )
		{
			const SKILLTEMPLATE * pSkill = pSkillMgr->GetByIndex( i );
			if( pSkill != NULL && pSkill->nClass == m_Humandesc.dbinfo.btClass && pSkill->nNeedLevel <= 1 )
				LearnSkill( pSkill->id );
		}
	}
	//ZeroMemory( m_Equipments, 0, sizeof( m_Equipments ));
	m_ItemBox.Clean();
	m_Equipments.Clean();
	//m_wX = desc.x;
	//m_wY = desc.y;
	//m_Mapid = desc.mapid;
	//m_pClientObj = desc.pClientObj;
	return TRUE;
}

BOOL CHumanPlayer::BeAttack( CAliveObject * pAttacker )
{
	if( pAttacker == NULL )return FALSE;
	int nAtkMin = pAttacker->GetPropValue( PI_MINDC );
	int nAtkMax = pAttacker->GetPropValue( PI_MAXDC );
	if( nAtkMax < nAtkMin )nAtkMax = nAtkMin;
	int nDefMin = GetPropValue( PI_MINAC );
	int nDefMax = GetPropValue( PI_MAXAC );
	if( nDefMax < nDefMin )nDefMax = nDefMin;
	int nAtk = nAtkMin + Getrand( nAtkMax - nAtkMin + 1 );
	int nDef = nDefMin + Getrand( nDefMax - nDefMin + 1 );
	int nDamage = nAtk - nDef;
	if( m_dwShieldUntil > timeGetTime() )
		nDamage = nDamage * 70 / 100;
	return ApplyDamage( nDamage > 0 ? nDamage : 1, pAttacker );
}

VOID CHumanPlayer::OnAroundMsg( CMapObject * pSender, const char * pszCodedMsg, int size )
{
	assert( m_pClientObj != NULL );
	m_pClientObj->PostMsg( pszCodedMsg, size );
	MIRMSG	header;
	char	szHeader[20];
	strncpy( szHeader, pszCodedMsg+1, 16 ); 
	szHeader[16] = 0;
	_UnGameCode( szHeader, (BYTE*)&header );
	//if( header.wCmd == SM_APPEAR )
	//{
	//	CServer::GetInstance()->GetIoConsole()->OutPut( 0xff00, "id = %d 向 id = %d 发送视觉信息!\n", 
	//																	pSender->GetId(), GetId() );

	//}
	//else if( header.wCmd == SM_DISAPPEAR )
	//{
	//	CServer::GetInstance()->GetIoConsole()->OutPut( 0xff00, "id = %d 向 id = %d 发送消失信息!\n", 
	//		pSender->GetId(), GetId() );
	//}

}

VOID CHumanPlayer::OnEnterMap( CVLogicMap * pMap, CVArea * pArea )
{
	BOOL	bSeeArea = CanRecvMsg();
	if( bSeeArea )
	{
		m_pClientObj->SendMsg( GetId(), SM_SETMAP, m_wX, m_wY, 0, (LPVOID)pMap->GetName() );
		DWORD	dwParam[4] = { MAKEFEATHER( 0, m_Humandesc.dbinfo.btHair, 0, 0 ), 0, 0, 0 };
		m_pClientObj->SendMsg( GetId(), SM_SETPLAYER, m_wX, m_wY, 0x100, (LPVOID)dwParam, sizeof( DWORD ) * 4 );
		m_pClientObj->SendMsg( GetId(), SM_SETPLAYERNAME, 0xff, 0, 0, (LPVOID)GetName() );
		m_pClientObj->SendMsg( 1, 0x2c4, 0, 0, 0, NULL );	//	设置地图的属性 dwflag = 1 战斗， dwflag = 0 非战斗
		m_pClientObj->SendMsg( 0, SM_SETMAPNAME, 0, 0, 0, (LPVOID)pMap->GetTitle() );
		m_pClientObj->SendMsg( 0xf2d505d7, SM_READY, 0, 0, 0, NULL );
		UpdateProp();
		pArea->AppearFor( this );
		for( int i = 0;i < 8;i ++ )
		{
			if( pArea->GetAroundArea(i) != NULL )
			{
				if( bSeeArea )pArea->GetAroundArea(i)->AppearFor( this );
			}
		}
	}
	CAliveObject::OnEnterMap( pMap, pArea );
}
BOOL	CHumanPlayer::GetViewmsg( char * pszMsg, int & length )
{
	//	00 24 41 5a 00 00 00 00 63 00 64 00
	char	szData[200];
	DWORD	* pdwData = (DWORD*)szData;
	pdwData[0] = MAKEFEATHER( 0, m_Humandesc.dbinfo.btHair, 0, 0 );
	pdwData[1] = 0;
	pdwData[2] = 0x00640064;
	snprintf( szData + 12, sizeof( szData ) - 12, "%s/%u", GetName(), (unsigned)255 );
	length = EncodeMsg( pszMsg, GetId(), SM_APPEAR, m_wX, m_wY, m_Direction, (LPVOID)szData, 12 + strlen( szData + 12 ));
	return TRUE;
}

DWORD CHumanPlayer::GetFeather()
{
	return MAKEFEATHER( 
		m_Equipments[_U_DRESS].baseitem.btShape,
		m_Humandesc.dbinfo.btHair, 
		m_Equipments[_U_WEAPON].baseitem.btShape,
		0
		);
}
DWORD CHumanPlayer::GetHealth()
{
	return 0x00640064;
}
DWORD CHumanPlayer::GetStatus()
{
	return 0;
}

VOID CHumanPlayer::SetCurHp( int nHp )
{
	int nMax = GetPropValue( PI_MAXHP );
	if( nMax <= 0 )nMax = 1;
	if( nHp < 0 )nHp = 0;
	if( nHp > nMax )nHp = nMax;
	m_Humandesc.dbinfo.hp = (WORD)nHp;
}

BOOL CHumanPlayer::OnDeath( CAliveObject * pAttacker )
{
	Say( "被%s打倒了！", pAttacker != NULL ? pAttacker->GetName() : "未知" );

	m_Humandesc.dbinfo.hp = m_Humandesc.dbinfo.maxhp;
	m_Humandesc.dbinfo.mp = m_Humandesc.dbinfo.maxmp;

	if( m_pClientObj != NULL )
		m_pClientObj->SendMsg( GetId(), SM_DEATH, 0, 0, 0, NULL );

	//	复活点：中州 (477,222)，若被占用则向外找空地
	if( m_pMap != NULL )
	{
		int x = 477, y = 222;
		if( m_pMap->IsBlocked( x, y ) )
		{
			for( int r = 1; r <= 8; ++r )
			{
				BOOL bFound = FALSE;
				for( int d = 0; d < 8 && !bFound; ++d )
				{
					int nx = 477 + g_dir_dx[d] * r;
					int ny = 222 + g_dir_dy[d] * r;
					if( !m_pMap->IsBlocked( nx, ny ) )
					{
						x = nx;
						y = ny;
						bFound = TRUE;
					}
				}
				if( bFound )break;
			}
		}
		setXY( (WORD)x, (WORD)y );
	}

	UpdateProp();
	if( m_pClientObj != NULL )
		m_pClientObj->SendMsg( GetId(), SM_WALK, m_wX, m_wY, (WORD)GetDirection(), NULL );
	return TRUE;
}

VOID CHumanPlayer::AddBuff( int nType, DWORD dwMs, int nValue )
{
	DWORD now = timeGetTime();
	for( int i = 0; i < m_nBuffCount; ++i )
	{
		if( m_Buffs[i].nType == nType )
		{
			m_Buffs[i].dwUntil = now + dwMs;
			m_Buffs[i].nValue = nValue;
			return;
		}
	}
	if( m_nBuffCount < 8 )
	{
		m_Buffs[m_nBuffCount].nType = nType;
		m_Buffs[m_nBuffCount].dwUntil = now + dwMs;
		m_Buffs[m_nBuffCount].nValue = nValue;
		++m_nBuffCount;
	}
}

int CHumanPlayer::GetBuffValue( int nType )
{
	DWORD now = timeGetTime();
	int n = 0;
	for( int i = 0; i < m_nBuffCount; ++i )
		if( m_Buffs[i].nType == nType && m_Buffs[i].dwUntil > now )
			n += m_Buffs[i].nValue;
	return n;
}

VOID CHumanPlayer::UpdateBuffs()
{
	DWORD now = timeGetTime();
	for( int i = 0; i < m_nBuffCount; )
	{
		if( m_Buffs[i].dwUntil <= now )
		{
			m_Buffs[i] = m_Buffs[m_nBuffCount - 1];
			--m_nBuffCount;
			continue;
		}
		++i;
	}
}

VOID CHumanPlayer::Update()
{
	CAliveObject::Update();
	UpdateBuffs();
}

VOID CHumanPlayer::AddExp( int nExp )
{
	if( nExp <= 0 )return;
	if( CEventMgr::GetInstance()->IsDoubleExp() )nExp *= 2;		//	全服双倍经验

	CPlayerClassMgr * pMgr = CPlayerClassMgr::GetInstance();
	int nLevelUps = 0;

	m_Humandesc.dbinfo.dwCurExp += (DWORD)nExp;
	while( m_Humandesc.dbinfo.wLevel < pMgr->MaxLevel() )
	{
		DWORD dwNeed = pMgr->LevelExpNeed( m_Humandesc.dbinfo.wLevel );
		if( dwNeed == 0 || m_Humandesc.dbinfo.dwCurExp < dwNeed )break;

		m_Humandesc.dbinfo.dwCurExp -= dwNeed;
		++m_Humandesc.dbinfo.wLevel;
		++nLevelUps;
	}

	if( nLevelUps > 0 )
	{
		CAchievementMgr::GetInstance()->CheckLevel( this, m_Humandesc.dbinfo.wLevel );
		ApplyClassStats();
		if( m_pClientObj != NULL )
			m_pClientObj->SendMsg( GetId(), SM_LEVELUP, (WORD)m_Humandesc.dbinfo.wLevel, (WORD)nLevelUps, 0, NULL );
	}

	if( m_pClientObj != NULL )
		UpdateProp();
}

//	按职业成长表重算 1..MaxLevel 的属性（装备加成不在这里）
VOID CHumanPlayer::ApplyClassStats()
{
	const PLAYERCLASS * pc = CPlayerClassMgr::GetInstance()->Get( m_Humandesc.dbinfo.btClass );
	if( pc == NULL )return;

	int nLevel = m_Humandesc.dbinfo.wLevel;
	if( nLevel < 1 )nLevel = 1;
	int grow = nLevel - 1;

	m_Humandesc.dbinfo.maxhp = (WORD)( 50 + pc->hpGrow * grow );
	m_Humandesc.dbinfo.maxmp = (WORD)( 20 + pc->mpGrow * grow );

	m_Humandesc.dbinfo.minac = 0 + pc->minAcGrow * grow;
	m_Humandesc.dbinfo.maxac = 1 + pc->maxAcGrow * grow;

	m_Humandesc.dbinfo.mindc = 1 + pc->minDcGrow * grow;
	m_Humandesc.dbinfo.maxdc = 2 + pc->maxDcGrow * grow;

	m_Humandesc.dbinfo.minmc = 0 + pc->mcGrow * grow;
	m_Humandesc.dbinfo.maxmc = ( m_Humandesc.dbinfo.btClass == 1 ? 1 : 0 ) + pc->mcGrow * grow;

	m_Humandesc.dbinfo.minsc = 0 + pc->scGrow * grow;
	m_Humandesc.dbinfo.maxsc = ( m_Humandesc.dbinfo.btClass == 2 ? 1 : 0 ) + pc->scGrow * grow;

	m_Humandesc.dbinfo.weight = 300 + pc->weightGrow * grow;
	m_Humandesc.dbinfo.handweight = 50 + 2 * grow;
	m_Humandesc.dbinfo.bodyweight = 60 + 3 * grow;

	m_Humandesc.dbinfo.hp = m_Humandesc.dbinfo.maxhp;
	m_Humandesc.dbinfo.mp = m_Humandesc.dbinfo.maxmp;
}

VOID CHumanPlayer::RestoreAll()
{
	m_Humandesc.dbinfo.hp = m_Humandesc.dbinfo.maxhp;
	m_Humandesc.dbinfo.mp = m_Humandesc.dbinfo.maxmp;
	if( m_pClientObj != NULL )UpdateProp();
}

//	施放技能：校验职业/等级/MP/冷却，然后结算伤害或治疗
BOOL CHumanPlayer::CastSkill( int nSkillId, int nDir )
{
	if( m_pMap == NULL )return FALSE;

	const SKILLTEMPLATE * pSkill = CSkillMgr::GetInstance()->Get( nSkillId );
	if( pSkill == NULL )return FALSE;
	if( pSkill->nClass != m_Humandesc.dbinfo.btClass )return FALSE;
	if( !HasSkill( nSkillId ) )return FALSE;	//	未学技能
	if( m_Humandesc.dbinfo.wLevel < pSkill->nNeedLevel )return FALSE;
	if( m_Humandesc.dbinfo.mp < pSkill->nMp )return FALSE;
	if( GetActionType() != AT_STAND )return FALSE;

	DWORD dwNow = timeGetTime();
	int nIndex = nSkillId & 31;
	if( dwNow < m_dwSkillReady[nIndex] )return FALSE;

	m_Humandesc.dbinfo.mp -= (WORD)pSkill->nMp;
	m_dwSkillReady[nIndex] = dwNow + pSkill->dwCooldown;

	int nRealDir = nDir & 7;
	SetDirection( (e_direction)nRealDir );
	SetAction( AT_SPELLSKILL, (e_direction)nRealDir, m_wX, m_wY, 400 );

	DWORD dwView[2] = { (DWORD)nSkillId, (DWORD)m_wX };
	SendAroundMsg( GetId(), SM_SPELL, (WORD)nSkillId, (WORD)nRealDir, 0, (LPVOID)dwView, sizeof( dwView ) );

	if( pSkill->nType == 3 )
	{
		int nHeal = 20 + GetPropValue( PI_MAXSC ) * pSkill->nPower / 100;
		SetCurHp( GetCurHp() + nHeal );
	}
	else if( pSkill->nType == 6 )
	{
		//	魔法盾：10 秒内受到的伤害降低 30%
		m_dwShieldUntil = timeGetTime() + 10000;
		if( m_pClientObj != NULL )
		{
			const char * pszMsg = "魔法盾已开启（10 秒减伤 30%）";
			m_pClientObj->SendMsg( GetId(), SM_CHAT, 0xffd700, 0, 0, (LPVOID)pszMsg );
		}
	}
	else if( pSkill->nType == 8 )
	{
		const char * pszPet = ( pSkill->id == 35 ) ? "神兽" : "骷髅宝宝";
		CMonster * pPet = CMonsterMgr::GetInstance()->Spawn( pszPet, GetMapId(), m_wX, m_wY );
		if( pPet != NULL )
		{
			pPet->SetPet( TRUE );
			pPet->SetOwner( GetId() );
			if( m_pClientObj != NULL )
			{
				char szMsg[128];
				snprintf( szMsg, sizeof( szMsg ), "%s已召唤，会自动攻击附近怪物。", pszPet );
				m_pClientObj->SendMsg( GetId(), SM_CHAT, 0xffd700, 0, 0, (LPVOID)szMsg );
			}
		}
	}
	else if( pSkill->nType == 7 )
	{
		//	增益：幽灵盾(魔防) / 神圣战甲(物防)
		int nValue = 3 + GetPropValue( PI_MAXSC ) / 12;
		int nBuffType = ( strstr( pSkill->szName, "幽灵" ) != NULL ) ? 2 : 1;
		AddBuff( nBuffType, 20000, nValue );
		if( m_pClientObj != NULL )
		{
			char szMsg[128];
			snprintf( szMsg, sizeof( szMsg ), "%s已开启（20 秒，+%d 防御）", pSkill->szName, nValue );
			m_pClientObj->SendMsg( GetId(), SM_CHAT, 0xffd700, 0, 0, (LPVOID)szMsg );
		}
	}
	else if( pSkill->nType == 9 )
	{
		//	施毒术：初始伤害 + 8 秒 DoT
		int nRange = pSkill->nRange > 0 ? pSkill->nRange : 1;
		int x = m_wX, y = m_wY;
		CAliveObject * pTarget = NULL;
		for( int i = 0; i < nRange; ++i )
		{
			GETNEXTPOS( x, y, nRealDir );
			if( m_pMap->IsBlocked( x, y ) )break;
			CVArea * pArea = m_pMap->GetArea( x, y );
			if( pArea == NULL )break;
			CMapObject * pObj = pArea->FindObject( (WORD)x, (WORD)y, OBJ_MONSTER );
			if( pObj == NULL )pObj = pArea->FindObject( (WORD)x, (WORD)y, OBJ_PLAYER );
			if( pObj != NULL && pObj != this ){ pTarget = (CAliveObject *)pObj; break; }
		}
		if( pTarget != NULL )
		{
			int nDamage = GetPropValue( PI_MAXSC ) * pSkill->nPower / 100 + 2;
			pTarget->ApplyDamage( nDamage / 2, this );
			pTarget->ApplyPoison( nDamage / 4 > 0 ? nDamage / 4 : 1, 8000, this );
			if( m_pClientObj != NULL )
			{
				const char * pszMsg = "施毒成功：目标每秒损失生命，持续 8 秒。";
				m_pClientObj->SendMsg( GetId(), SM_CHAT, 0xff8800, 0, 0, (LPVOID)pszMsg );
			}
		}
	}
	else if( pSkill->nType == 5 )
	{
		//	范围技能：沿方向找到中心点，命中周围 3x3 的怪物
		int nRange = pSkill->nRange > 0 ? pSkill->nRange : 1;
		int tx = m_wX, ty = m_wY;
		for( int i = 0; i < nRange; ++i )
		{
			GETNEXTPOS( tx, ty, nRealDir );
			if( m_pMap->IsBlocked( tx, ty ) )break;
		}
		int nStat = pSkill->nClass == 0 ? PI_MAXDC : ( pSkill->nClass == 1 ? PI_MAXMC : PI_MAXSC );
		//	范围技能按 75% 伤害结算
		int nDamage = GetPropValue( (PROP_INDEX)nStat ) * pSkill->nPower * 3 / 400 + 2;
		for( int dy = -1; dy <= 1; ++dy )
		{
			for( int dx = -1; dx <= 1; ++dx )
			{
				CVArea * pArea = m_pMap->GetArea( tx + dx, ty + dy );
				if( pArea == NULL )continue;
				CMapObject * pObj = pArea->FindObject( (WORD)(tx + dx), (WORD)(ty + dy), OBJ_MONSTER );
				if( pObj != NULL )
				{
					CAliveObject * pTarget = (CAliveObject *)pObj;
					pTarget->ApplyDamage( nDamage, this );
					if( pSkill->id == 28 )
						pTarget->ApplyPoison( nDamage / 5 > 0 ? nDamage / 5 : 1, 8000, this );
				}
			}
		}
	}
	else
	{
		int nRange = pSkill->nRange > 0 ? pSkill->nRange : 1;
		int x = m_wX, y = m_wY;
		CAliveObject * pTarget = NULL;
		for( int i = 0; i < nRange; ++i )
		{
			GETNEXTPOS( x, y, nRealDir );
			if( m_pMap->IsBlocked( x, y ) )break;

			CVArea * pArea = m_pMap->GetArea( x, y );
			if( pArea == NULL )break;

			CMapObject * pObj = pArea->FindObject( (WORD)x, (WORD)y, OBJ_MONSTER );
			if( pObj == NULL )
				pObj = pArea->FindObject( (WORD)x, (WORD)y, OBJ_PLAYER );
			if( pObj != NULL && pObj != this )
			{
				pTarget = (CAliveObject *)pObj;
				break;
			}
		}

		if( pTarget != NULL )
		{
			int nStat = 0;
			if( pSkill->nType == 1 )nStat = GetPropValue( PI_MAXDC );
			else if( pSkill->nType == 2 )nStat = GetPropValue( PI_MAXMC );
			else nStat = GetPropValue( PI_MAXSC );

			int nDamage = nStat * pSkill->nPower / 100 + 2;
			pTarget->ApplyDamage( nDamage, this );
		}
	}

	UpdateProp();
	return TRUE;
}

// ---------------------------------------------------------------------------
// 任务
// ---------------------------------------------------------------------------
int CHumanPlayer::GetQuestState( int nQuestId )
{
	for( int i = 0; i < m_nQuestCount; ++i )
		if( m_Quests[i].nId == nQuestId )return m_Quests[i].nState;
	return 0;
}

int CHumanPlayer::GetQuestProgress( int nQuestId )
{
	for( int i = 0; i < m_nQuestCount; ++i )
	{
		if( m_Quests[i].nId != nQuestId )continue;
		if( m_Quests[i].nState != 1 )return m_Quests[i].nProgress;

		const QUESTTEMPLATE * q = CQuestMgr::GetInstance()->Get( nQuestId );
		if( q != NULL && q->nType == 2 )
			return m_ItemBox.CountItemByName( q->szTarget );	//	收集类实时统计
		return m_Quests[i].nProgress;
	}
	return 0;
}

const PLAYERQUEST * CHumanPlayer::GetQuestByIndex( int nIndex )
{
	if( nIndex < 0 || nIndex >= m_nQuestCount )return NULL;
	return &m_Quests[nIndex];
}

BOOL CHumanPlayer::AcceptQuest( int nQuestId )
{
	const QUESTTEMPLATE * q = CQuestMgr::GetInstance()->Get( nQuestId );
	if( q == NULL )return FALSE;
	if( m_Humandesc.dbinfo.wLevel < q->nNeedLevel )return FALSE;
	if( q->nPrevQuest != 0 && GetQuestState( q->nPrevQuest ) != 2 )return FALSE;	//	前置未完成
	if( GetQuestState( nQuestId ) != 0 )return FALSE;		//	已接/已完成
	if( m_nQuestCount >= QUEST_MAX_PER_PLAYER )return FALSE;

	m_Quests[m_nQuestCount].nId = nQuestId;
	m_Quests[m_nQuestCount].nProgress = 0;
	m_Quests[m_nQuestCount].nState = 1;
	++m_nQuestCount;

	if( m_pClientObj != NULL )
	{
		char szMsg[128];
		snprintf( szMsg, sizeof( szMsg ), "接受任务：%s", q->szName );
		m_pClientObj->SendMsg( GetId(), SM_CHAT, 0xff00, 0, 0, (LPVOID)szMsg );
	}
	return TRUE;
}

BOOL CHumanPlayer::SubmitQuest( int nQuestId )
{
	const QUESTTEMPLATE * q = CQuestMgr::GetInstance()->Get( nQuestId );
	if( q == NULL )return FALSE;
	if( GetQuestState( nQuestId ) != 1 )return FALSE;

	PLAYERQUEST * pState = NULL;
	for( int i = 0; i < m_nQuestCount; ++i )
		if( m_Quests[i].nId == nQuestId ){ pState = &m_Quests[i]; break; }
	if( pState == NULL )return FALSE;

	if( q->nType == 1 )
	{
		if( pState->nProgress < q->nCount )return FALSE;
	}
	else
	{
		int nHave = m_ItemBox.CountItemByName( q->szTarget );
		if( nHave < q->nCount )return FALSE;
		m_ItemBox.RemoveItemsByName( q->szTarget, q->nCount );
	}

	//	奖励
	AddExp( q->nExp );
	CAchievementMgr::GetInstance()->OnQuestCompleted( this, GetCompletedQuestCount() );
	AddGold( q->nGold );
	if( q->szRewardItem[0] != '\0' )
	{
		ITEM item;
		if( CItemManager::GetInstance()->MakeupTemplateItem( q->szRewardItem, item ) )
		{
			item.dwMakeIndex = CItemManager::NewMakeIndex();
			item.wCount = (WORD)( q->nRewardCount > 0 ? q->nRewardCount : 1 );
			AddBagItem( item );
		}
	}

	//	日常任务提交后回到可接状态；普通任务标记完成
	if( q->bRepeatable )
	{
		pState->nState = 0;
		pState->nProgress = 0;
	}
	else
	{
		pState->nState = 2;
		pState->nProgress = q->nCount;
	}

	if( m_pClientObj != NULL )
	{
		char szMsg[160];
		snprintf( szMsg, sizeof( szMsg ), "完成任务：%s（经验 +%d，金币 +%d）", q->szName, q->nExp, q->nGold );
		m_pClientObj->SendMsg( GetId(), SM_CHAT, 0xff00, 0, 0, (LPVOID)szMsg );
	}
	UpdateProp();
	return TRUE;
}

VOID CHumanPlayer::OnKillMonster( const char * pszMonster )
{
	if( pszMonster == NULL )return;
	for( int i = 0; i < m_nQuestCount; ++i )
	{
		if( m_Quests[i].nState != 1 )continue;
		const QUESTTEMPLATE * q = CQuestMgr::GetInstance()->Get( m_Quests[i].nId );
		if( q == NULL || q->nType != 1 )continue;
		if( strcmp( q->szTarget, pszMonster ) != 0 )continue;
		if( m_Quests[i].nProgress >= q->nCount )continue;

		++m_Quests[i].nProgress;
		if( m_pClientObj != NULL )
		{
			char szMsg[128];
			snprintf( szMsg, sizeof( szMsg ), "任务 %s：%d/%d", q->szName, m_Quests[i].nProgress, q->nCount );
			m_pClientObj->SendMsg( GetId(), SM_CHAT, 0xff00, 0, 0, (LPVOID)szMsg );
		}
	}
}

VOID CHumanPlayer::RestoreQuests( const PLAYERQUEST * pQuests, int nCount )
{
	if( pQuests == NULL || nCount <= 0 )return;
	if( nCount > QUEST_MAX_PER_PLAYER )nCount = QUEST_MAX_PER_PLAYER;
	memcpy( m_Quests, pQuests, sizeof( PLAYERQUEST ) * nCount );
	m_nQuestCount = nCount;
}

BOOL CHumanPlayer::LearnSkill( int nSkillId )
{
	const SKILLTEMPLATE * pSkill = CSkillMgr::GetInstance()->Get( nSkillId );
	if( pSkill == NULL )return FALSE;
	if( pSkill->nClass != m_Humandesc.dbinfo.btClass )return FALSE;		//	职业不符
	if( m_Humandesc.dbinfo.wLevel < pSkill->nNeedLevel )return FALSE;	//	等级不足
	if( HasSkill( nSkillId ) )return FALSE;							//	已学会

	m_dwLearnedSkills |= ( (DWORD)1 << ( nSkillId & 31 ) );
	return TRUE;
}

BOOL CHumanPlayer::UseItem( DWORD dwMakeIndex )
{
	ITEM * pItem = m_ItemBox.FindItem( dwMakeIndex );
	if( pItem == NULL )return FALSE;

	int nType = pItem->baseitem.btUseType;
	int nValue = pItem->baseitem.wUseValue;
	if( nType == 0 )return FALSE;

	char szName[32];
	o_strncpy( szName, pItem->baseitem.szName, sizeof( szName ) - 1 );

	if( nType == 1 )
	{
		SetCurHp( GetCurHp() + nValue );
	}
	else if( nType == 2 )
	{
		int nMaxMp = GetPropValue( PI_MAXMP );
		int nMp = (int)m_Humandesc.dbinfo.mp + nValue;
		if( nMp > nMaxMp )nMp = nMaxMp;
		m_Humandesc.dbinfo.mp = (WORD)nMp;
	}
	else if( nType == 3 )
	{
		//	技能书：学会对应技能后才消耗
		if( !LearnSkill( nValue ) )return FALSE;
		if( m_pClientObj != NULL )m_pClientObj->SendSkills();
	}
	else
	{
		return FALSE;
	}

	m_ItemBox.RemoveItem( dwMakeIndex );

	if( m_pClientObj != NULL )
	{
		char szMsg[96];
		snprintf( szMsg, sizeof( szMsg ), "使用了%s", szName );
		m_pClientObj->SendMsg( GetId(), SM_CHAT, 0xff00, 0, 0, (LPVOID)szMsg );
	}
	UpdateProp();
	return TRUE;
}

BOOL CHumanPlayer::AddGold( int nDelta )
{
	if( nDelta < 0 )
	{
		DWORD dwCost = (DWORD)( -nDelta );
		if( dwCost > m_Humandesc.dbinfo.dwGold )return FALSE;
		m_Humandesc.dbinfo.dwGold -= dwCost;
	}
	else
	{
		m_Humandesc.dbinfo.dwGold += (DWORD)nDelta;
	}

	if( m_pClientObj != NULL )UpdateProp();
	return TRUE;
}

//	每 3 秒恢复 5% 的 HP/MP（参考传奇世界的自然恢复）
VOID CHumanPlayer::RegenTick( DWORD dwNow )
{
	if( dwNow < m_dwNextRegen )return;
	m_dwNextRegen = dwNow + 3000;
	if( GetMap() == NULL )return;

	int nMaxHp = GetPropValue( PI_MAXHP );
	if( GetCurHp() < nMaxHp )
	{
		int nHeal = nMaxHp / 20;
		SetCurHp( GetCurHp() + ( nHeal > 0 ? nHeal : 1 ) );
	}

	int nMaxMp = GetPropValue( PI_MAXMP );
	if( (int)m_Humandesc.dbinfo.mp < nMaxMp )
	{
		int nMp = (int)m_Humandesc.dbinfo.mp + ( nMaxMp / 20 > 0 ? nMaxMp / 20 : 1 );
		if( nMp > nMaxMp )nMp = nMaxMp;
		m_Humandesc.dbinfo.mp = (WORD)nMp;
		if( m_pClientObj != NULL )UpdateProp();
	}
}

BOOL CHumanPlayer::CastSkillByName( const char * pszName, int nDir )
{
	const SKILLTEMPLATE * pSkill = CSkillMgr::GetInstance()->FindByName( pszName );
	if( pSkill == NULL )return FALSE;
	return CastSkill( pSkill->id, nDir );
}

BOOL	CHumanPlayer::GetDBInfo( CHARDBINFO & info )
{
	info = m_Humandesc.dbinfo;
	info.x = m_wX;
	info.y = m_wY;
	info.mapid = m_Mapid;
	return TRUE;
}

BOOL	CHumanPlayer::AddBagItem( ITEM & item )
{
	//	fixme:
	if( m_ItemBox.AddItem(item) )
	{
		AddCodex( item.baseitem.szName );
		if( m_pClientObj != NULL )
			m_pClientObj->SendAddItem( item );
		return TRUE;
	}
	return FALSE;
}

BOOL	CHumanPlayer::DropItem( ITEM & item )
{
	return CDownItemMgr::GetInstance()->DropItem( m_pMap, item, m_wX + Getrand( 4 ) - 2, m_wY + Getrand( 4 ) - 2, GetId());
}

BOOL	CHumanPlayer::PickupItem()
{
	CDownItemObject * pObj = (CDownItemObject *)m_pArea->FindObject( m_wX, m_wY, OBJ_DOWNITEM );
	if( pObj == NULL )return FALSE;
	return CDownItemMgr::GetInstance()->PickupItem( m_pMap, pObj, this );
}

BOOL	CHumanPlayer::DropBagItem( DWORD dwMakeIndex )
{
	ITEM * pItem = m_ItemBox.FindItem(dwMakeIndex );
	if( pItem == NULL )return FALSE;

	//	堆叠物品一次只丢一个
	ITEM dropItem = *pItem;
	dropItem.wCount = 1;
	if( !DropItem(dropItem) )return FALSE;
	m_ItemBox.RemoveItem(dwMakeIndex);
	return TRUE;
}

BOOL	CHumanPlayer::EquipItem( int pos, DWORD dwMakeIndex )
{
	ITEM * pItem = m_ItemBox.FindItem(dwMakeIndex);
	if( pItem == NULL )return FALSE;
	//	todo: add more condition
	//m_ItemBox.RemoveItem(dwMakeIndex);
	ITEM	itemout;
	ZeroMemory( &itemout, sizeof( itemout ) );
	if( m_Equipments.EquipItem( pos, *pItem, itemout ) )
	{
		m_ItemBox.RemoveItem(dwMakeIndex);
		if( itemout.dwMakeIndex != 0 )AddBagItem(itemout);
		return TRUE;
	}
	return FALSE;
}

BOOL CHumanPlayer::UnEquipItem( int pos, DWORD dwMakeIndex )
{
	if( dwMakeIndex == 0 || m_Equipments[pos].dwMakeIndex != dwMakeIndex )
		return FALSE;
	ITEM	item;
	if( !m_ItemBox.AddItem(m_Equipments[pos]))
		return FALSE;
	if( !m_Equipments.UnEquipItem(pos, item ))
	{
		m_ItemBox.RemoveItem(dwMakeIndex);
		return FALSE;
	}
	if( m_pClientObj != NULL )
		m_pClientObj->SendAddItem( item );

	return TRUE;
}

BOOL	CHumanPlayer::AddGroupMember( CHumanPlayer * pObject )
{
	return TRUE;
}

BOOL	CHumanPlayer::RemoveGroupMember( CHumanPlayer * pObject )
{
	return TRUE;
}


int CHumanPlayer::GetPropValue( PROP_INDEX index )
{
	int	sRet = 0;
	switch( index )
	{
	case PI_MINAC:	sRet = m_Equipments.GetProp(index) + m_Humandesc.dbinfo.minac + GetBuffValue(1);break;
	case PI_MAXAC:	sRet = m_Equipments.GetProp(index) + m_Humandesc.dbinfo.maxac + GetBuffValue(1);break;

	case PI_MINMAC:	sRet = m_Equipments.GetProp(index) + m_Humandesc.dbinfo.minmac + GetBuffValue(2);break;
	case PI_MAXMAC:	sRet = m_Equipments.GetProp(index) + m_Humandesc.dbinfo.maxmac + GetBuffValue(2);break;

	case PI_MINDC:	sRet = m_Equipments.GetProp(index) + m_Humandesc.dbinfo.mindc;break;
	case PI_MAXDC:	sRet = m_Equipments.GetProp(index) + m_Humandesc.dbinfo.maxdc;break;

	case PI_MINMC:	sRet = m_Equipments.GetProp(index) + m_Humandesc.dbinfo.minmc;break;
	case PI_MAXMC:	sRet = m_Equipments.GetProp(index) + m_Humandesc.dbinfo.maxmc;break;

	case PI_MINSC:	sRet = m_Equipments.GetProp(index) + m_Humandesc.dbinfo.minsc;break;
	case PI_MAXSC:	sRet = m_Equipments.GetProp(index) + m_Humandesc.dbinfo.maxsc;break;

	case PI_HITRATE:		sRet = m_Equipments.GetProp(index);break;
	case PI_ESCAPE:			sRet = m_Equipments.GetProp(index);break;
	case PI_MAGESCAPE:		sRet = m_Equipments.GetProp(index);break;
	case PI_POISONESCAPE:	sRet = m_Equipments.GetProp(index);break;

	case PI_ATTACKSPEED:	sRet = m_Equipments.GetProp(index);break;
	case PI_LUCKY:			sRet = m_Equipments.GetProp(index);break;
	case PI_DAWN:			sRet = m_Equipments.GetProp(index);break;

	case PI_HPRECOVER:		sRet = m_Equipments.GetProp(index);break;
	case PI_MPRECOVER:		sRet = m_Equipments.GetProp(index);break;
	case PI_POISONRECOVER:	sRet = m_Equipments.GetProp(index);break;

	case PI_HARD:			sRet = m_Equipments.GetProp(index);break;
	case PI_HOLLY:			sRet = m_Equipments.GetProp(index);break;

	case PI_LEVEL:			sRet = m_Humandesc.dbinfo.wLevel;break;

	case PI_CURBAGWEIGHT:	sRet = m_ItemBox.CalcWeight();break;
	case PI_MAXBAGWEIGHT:	sRet = m_Humandesc.dbinfo.weight;break;

	case PI_CURHANDWEIGHT:	sRet = m_Equipments[_U_WEAPON].baseitem.btWeight;break;
	case PI_MAXHANDWEIGHT:	sRet = m_Humandesc.dbinfo.handweight;break;

	case PI_CURBODYWEIGHT:	sRet = m_Equipments.CalcEquipmentsWeight(-1);break;
	case PI_MAXBODYWEIGHT:	sRet = m_Humandesc.dbinfo.bodyweight;break;

	case PI_CURHP:			sRet = m_Humandesc.dbinfo.hp; break;
	case PI_CURMP:			sRet = m_Humandesc.dbinfo.mp; break;
	case PI_MAXHP:			sRet = m_Humandesc.dbinfo.maxhp; break;
	case PI_MAXMP:			sRet = m_Humandesc.dbinfo.maxmp; break;
	default:				sRet = 0; break;
	}
	sRet += GetTitleBonus( index );
	return sRet;
}

VOID CHumanPlayer::UpdateProp()
{
	if( m_pClientObj == NULL )return;	//	测试角色没有客户端

	HUMANPROP	prop;
	prop.wLevel = m_Humandesc.dbinfo.wLevel;
	prop.dwCurexp = m_Humandesc.dbinfo.dwCurExp;
	prop.dwMaxexp = CPlayerClassMgr::GetInstance()->LevelExpNeed( m_Humandesc.dbinfo.wLevel );
	prop.wCurBagWeight = GetPropValue( PI_CURBAGWEIGHT );											//	fixme: 计算背包重量
	prop.wCurHp = GetPropValue( PI_CURHP );
	prop.wCurMp = GetPropValue( PI_CURMP );
	prop.wMaxBagWeight = GetPropValue( PI_MAXBAGWEIGHT );
	prop.wMaxHp = GetPropValue( PI_MAXHP );
	prop.wMaxMp = GetPropValue( PI_MAXMP );
	prop.btCurBodyWeight = GetPropValue( PI_CURBODYWEIGHT );
	prop.btCurHandWeight = GetPropValue( PI_CURHANDWEIGHT );

	prop.btMaxAtk = GetPropValue( PI_MAXDC );
	prop.btMinAtk = GetPropValue( PI_MINDC );
	prop.btMaxDef = GetPropValue( PI_MAXAC );
	prop.btMinDef = GetPropValue( PI_MINAC );

	prop.btMaxMagAtk = GetPropValue( PI_MAXMC );
	prop.btMinMagAtk = GetPropValue( PI_MINMC );
	prop.btMaxMagicDef = GetPropValue( PI_MAXMAC );
	prop.btMinMagicDef = GetPropValue( PI_MINMAC );

	prop.btMaxSprAtk = GetPropValue( PI_MAXSC );
	prop.btMinSprAtk = GetPropValue( PI_MINSC );

	prop.btMaxBodyWeight = GetPropValue( PI_MAXBODYWEIGHT );
	prop.btMaxHandWeight = GetPropValue( PI_MAXHANDWEIGHT );
	prop.dwUnknown1 = 0xf12233f4;
	//prop.wP1 = 1200;
	//prop.wP2 = 2000;
	m_pClientObj->SendMsg( m_Humandesc.dbinfo.dwGold, SM_UPDATEPROP, 
		m_Humandesc.dbinfo.btHair<<8|m_Humandesc.dbinfo.btClass, 
		m_Humandesc.dbinfo.btSex, 0, &prop, sizeof( prop ));
}

BOOL	CHumanPlayer::Trade()
{
	if( m_ExchangeObj.GetState() != EE_FREE )
	{
		Say( "您正在和别人交易，不能再次交易!" );
		return FALSE;
	}
	int x = getX();
	int y = getY();
	GETNEXTPOS( x, y, GetDirection() );
	CHumanPlayer * pPlayer = (CHumanPlayer*)m_pMap->FindObject(x,y,OBJ_PLAYER);
	if( pPlayer != NULL )
	{
		
		if( !m_ExchangeObj.ConnectOther(pPlayer) )
		{
			Say( m_ExchangeObj.GetErrorMsg());
			return FALSE;
		}
	}
	return TRUE;
}


//	成就 / 称号
BOOL CHumanPlayer::UnlockAchievement( int nId )
{
	for( size_t i = 0; i < m_Achievements.size(); ++i )
		if( m_Achievements[i] == nId )return FALSE;
	m_Achievements.push_back( nId );
	return TRUE;
}

BOOL CHumanPlayer::HasAchievement( int nId )
{
	for( size_t i = 0; i < m_Achievements.size(); ++i )
		if( m_Achievements[i] == nId )return TRUE;
	return FALSE;
}

VOID CHumanPlayer::RestoreAchievements( const std::vector<int> & list, int nTitle )
{
	m_Achievements = list;
	m_nTitle = ( HasAchievement( nTitle ) || HasShopTitle( nTitle ) ) ? nTitle : 0;

	int nMaxHp = GetPropValue( PI_MAXHP );
	if( m_Humandesc.dbinfo.hp > nMaxHp )m_Humandesc.dbinfo.hp = nMaxHp;
}

BOOL CHumanPlayer::SetTitle( int nId )
{
	if( !HasAchievement( nId ) && !HasShopTitle( nId ) )return FALSE;

	int nOldMaxHp = GetPropValue( PI_MAXHP );
	m_nTitle = nId;
	int nNewMaxHp = GetPropValue( PI_MAXHP );
	if( nNewMaxHp > nOldMaxHp )
		m_Humandesc.dbinfo.hp += ( nNewMaxHp - nOldMaxHp );
	if( m_Humandesc.dbinfo.hp > nNewMaxHp )
		m_Humandesc.dbinfo.hp = nNewMaxHp;

	UpdateProp();
	return TRUE;
}

int CHumanPlayer::GetAchievementPoints()
{
	int nPoints = 0;
	for( size_t i = 0; i < m_Achievements.size(); ++i )
	{
		ACHIEVEMENT * pAch = CAchievementMgr::GetInstance()->Find( m_Achievements[i] );
		if( pAch != NULL )nPoints += pAch->nPoints;
	}
	return nPoints;
}

int CHumanPlayer::GetTitleBonus( PROP_INDEX index )
{
	if( m_nTitle == 0 )return 0;

	int nBonus = 0;
	ACHIEVEMENT * pAch = CAchievementMgr::GetInstance()->Find( m_nTitle );
	if( pAch != NULL )
	{
		for( size_t i = 0; i < pAch->bonuses.size(); ++i )
			if( pAch->bonuses[i].nIndex == (int)index )
				nBonus += pAch->bonuses[i].nValue;
	}

	TITLEDEF * pTitle = CTitleMgr::GetInstance()->Find( m_nTitle );
	if( pTitle != NULL )
	{
		for( size_t i = 0; i < pTitle->bonuses.size(); ++i )
			if( pTitle->bonuses[i].nIndex == (int)index )
				nBonus += pTitle->bonuses[i].nValue;
	}
	return nBonus;
}

int CHumanPlayer::GetAvailablePoints()
{
	int nPoints = GetAchievementPoints() - m_nPointsSpent;
	return nPoints > 0 ? nPoints : 0;
}

BOOL CHumanPlayer::HasShopTitle( int nId )
{
	for( size_t i = 0; i < m_ShopTitles.size(); ++i )
		if( m_ShopTitles[i] == nId )return TRUE;
	return FALSE;
}

VOID CHumanPlayer::AddCodex( const char * pszItem )
{
	if( pszItem == NULL || pszItem[0] == '\0' )return;
	if( m_Codex.insert( pszItem ).second )
		CheckSetRewards();
}

BOOL CHumanPlayer::HasCodexReward( const char * pszSet )
{
	return pszSet != NULL && m_CodexRewarded.find( pszSet ) != m_CodexRewarded.end();
}

VOID CHumanPlayer::CheckSetRewards()
{
	int nComplete = 0;
	for( int s = 0; s < CSetMgr::GetInstance()->GetCount(); ++s )
	{
		SETDEF * pDef = CSetMgr::GetInstance()->GetByIndex( s );
		if( pDef == NULL )continue;

		BOOL bAll = TRUE;
		for( size_t j = 0; j < pDef->items.size(); ++j )
			if( m_Codex.find( pDef->items[j] ) == m_Codex.end() ){ bAll = FALSE; break; }
		if( !bAll )continue;

		++nComplete;
		if( m_CodexRewarded.find( pDef->szName ) != m_CodexRewarded.end() )continue;

		m_CodexRewarded.insert( pDef->szName );
		AddGold( 3000 );
		AddExp( 3000 );

		char szMsg[192];
		snprintf( szMsg, sizeof( szMsg ), "%s 集齐套装【%s】，获得 3000 金币 / 3000 经验！",
			GetName(), pDef->szName );
		Game_Announce( szMsg );
	}

	CAchievementMgr::GetInstance()->CheckCodex( this, nComplete );

	//	图鉴里程碑称号
	int nMilestones[3][2] = { { 3, 301 }, { 6, 302 }, { 10, 303 } };
	for( int i = 0; i < 3; ++i )
	{
		if( nComplete < nMilestones[i][0] || HasShopTitle( nMilestones[i][1] ) )continue;
		if( GrantTitle( nMilestones[i][1] ) )
		{
			TITLEDEF * pTitle = CTitleMgr::GetInstance()->Find( nMilestones[i][1] );
			char szMsg[160];
			snprintf( szMsg, sizeof( szMsg ), "%s 集齐 %d 套图鉴，获得称号【%s】！",
				GetName(), nComplete, pTitle != NULL ? pTitle->szName : "收藏家" );
			Game_Announce( szMsg );
		}
	}
}

VOID CHumanPlayer::RestoreCodexRewards( const std::vector<std::string> & list )
{
	m_CodexRewarded.clear();
	for( size_t i = 0; i < list.size(); ++i )
		if( !list[i].empty() )m_CodexRewarded.insert( list[i] );
}

BOOL CHumanPlayer::HasCodex( const char * pszItem )
{
	return pszItem != NULL && m_Codex.find( pszItem ) != m_Codex.end();
}

BOOL CHumanPlayer::GrantTitle( int nId )
{
	if( nId <= 0 || HasShopTitle( nId ) )return FALSE;
	m_ShopTitles.push_back( nId );
	return TRUE;
}

BOOL CHumanPlayer::BuyTitle( int nId, int nCost )
{
	if( nCost < 0 || HasShopTitle( nId ) )return FALSE;
	if( GetAvailablePoints() < nCost )return FALSE;
	m_nPointsSpent += nCost;
	m_ShopTitles.push_back( nId );
	return TRUE;
}

VOID CHumanPlayer::RestoreCodex( const std::vector<std::string> & list )
{
	m_Codex.clear();
	for( size_t i = 0; i < list.size(); ++i )
		if( !list[i].empty() )m_Codex.insert( list[i] );
}

VOID CHumanPlayer::RestoreShopTitles( int nSpent, const std::vector<int> & list )
{
	m_nPointsSpent = nSpent;
	m_ShopTitles = list;
}

const char * CHumanPlayer::GetTitleName()
{
	if( m_nTitle == 0 )return "";
	ACHIEVEMENT * pAch = CAchievementMgr::GetInstance()->Find( m_nTitle );
	if( pAch != NULL )return pAch->szName;
	TITLEDEF * pTitle = CTitleMgr::GetInstance()->Find( m_nTitle );
	if( pTitle != NULL )return pTitle->szName;
	return "";
}

int CHumanPlayer::GetCompletedQuestCount()
{
	int nCount = 0;
	for( int i = 0; i < m_nQuestCount; ++i )
		if( m_Quests[i].nState == 2 )++nCount;
	return nCount;
}
