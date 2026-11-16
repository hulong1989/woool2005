#include "stdafx.h"
#include "AliveObject.h"
#include "vMap.h"

#include "Server.h"
#include "LogicMap.h"
#include "Area.h"
#include "HumanPlayerMgr.h"

static CHAR	g_CodedMsgBuffer[1024];

CAliveObject::CAliveObject(void):m_ActionType(AT_STAND),m_Direction(ED_DN),
	m_btWalkSpeed(1),m_btRunSpeed(2),m_wDeInvisibleLevel(0),m_wActionX(0),m_wActionY(0),
	m_ActionDirection(ED_DN),m_dwActionCompleteTime(0),m_Id(0),m_Mapid(0),m_bPosLocked(FALSE),
	m_dwPoisonUntil(0),m_dwPoisonNext(0),m_nPoisonDamage(0),m_nPoisonSourceId(0),m_xPetList(NULL), m_PetListNode(this)
{

}

CAliveObject::~CAliveObject(void)
{

}

VOID CAliveObject::Clean()
{
	CMapObject::Clean();
	m_Direction = ED_DN;
	m_btWalkSpeed = 1;
	m_btRunSpeed = 2;
	m_wDeInvisibleLevel = 0;
	m_ActionType = AT_STAND;
	m_wActionX = 0;
	m_wActionY = 0;
	m_ActionDirection = ED_DN;
	m_dwActionCompleteTime = 0;
	m_Id = 0;
	m_Mapid = 0;
	m_bPosLocked = FALSE;
	m_dwPoisonUntil = 0;
	m_dwPoisonNext = 0;
	m_nPoisonDamage = 0;
	m_nPoisonSourceId = 0;
}

BOOL CAliveObject::Walk( int dir, BOOL bServerCall )
{
	if( !CanDoAction(AT_WALK) )return FALSE;
	int x = m_wX, y = m_wY;
	for( int i = 0;i < (int)GetWalkSpeed();i ++ )
	{
		GETNEXTPOS(x,y,dir);
		if( m_pMap->IsBlocked(x,y) )return FALSE;
	}
	DWORD	dwTime = 500;
	if( bServerCall )dwTime = 1000;
	if( !SetAction( AT_WALK, (e_direction)dir, x, y, dwTime ) )return FALSE;
	DWORD	dwView[] = { GetFeather(), 0 };
	SendAroundMsg(GetId(), SM_WALK, x, y, dir, (LPVOID)dwView, 8 );
	return TRUE;
}

BOOL CAliveObject::Run( int dir, BOOL bServerCall )
{
	if( !CanDoAction(AT_RUN) )return FALSE;
	int x = m_wX, y = m_wY;
	for( int i = 0;i < (int)GetRunSpeed();i ++ )
	{
		GETNEXTPOS( x, y, dir );
		if( m_pMap->IsBlocked(x,y)) return FALSE;
	}
	if( !SetAction( AT_RUN, (e_direction)dir, x, y, 500 ) )return FALSE;
	DWORD	dwView[] = { GetFeather(), 0 };
	SendAroundMsg(GetId(), SM_RUN, x, y, dir, (LPVOID)dwView, 8 );
	return TRUE;
}
BOOL CAliveObject::Turn( int dir, BOOL bServerCall )
{
	if( !CanDoAction(AT_TURN) )return FALSE;
	if( !SetAction(AT_TURN, (e_direction)dir, m_wX, m_wY, 100 ) )return FALSE;
	DWORD	dwView[] = { GetFeather(), 0, 0x00640064 };
	SendAroundMsg(GetId(), SM_APPEAR, m_wX, m_wY, dir, (LPVOID)dwView, 12 );
	return TRUE;
}
BOOL CAliveObject::Attack( int dir, BOOL bServerCall )
{
	if( !CanDoAction(AT_ATTACK) )return FALSE;
	if( !SetAction(AT_ATTACK, (e_direction)dir, m_wX, m_wY, 900 ) )return FALSE;

	DWORD	dwView[] = { GetFeather(), GetStatus() };
	SendAroundMsg(GetId(), SM_ATTACK, m_wX, m_wY, dir, (LPVOID)dwView, 8 );

	int x = m_wX, y = m_wY;
	GETNEXTPOS( x, y, dir );

	CVArea * pArea = m_pMap->GetArea( x, y );
	if( pArea != NULL )
	{
		//	重建时补充：玩家优先打怪，怪只打玩家，玩家之间仍可互殴
		e_object_type	targetType = ( GetType() == OBJ_MONSTER ) ? OBJ_PLAYER : OBJ_MONSTER;
		CAliveObject *	pObj = (CAliveObject*)pArea->FindObject( x, y, targetType );
		if( pObj == NULL && GetType() != OBJ_MONSTER )
			pObj = (CAliveObject*)pArea->FindObject( x, y, OBJ_PLAYER );
		if( pObj != NULL )
			pObj->BeAttack( this );
	}

	return TRUE;
}

BOOL CAliveObject::BeAttack( CAliveObject * pAttacker )
{
	if( pAttacker == NULL )return FALSE;

	//	普攻：攻击方 MINDC-MAXDC 随机，受击方 MINAC-MAXAC 随机
	int nAtkMin = pAttacker->GetPropValue( PI_MINDC );
	int nAtkMax = pAttacker->GetPropValue( PI_MAXDC );
	if( nAtkMax < nAtkMin )nAtkMax = nAtkMin;
	int nDefMin = GetPropValue( PI_MINAC );
	int nDefMax = GetPropValue( PI_MAXAC );
	if( nDefMax < nDefMin )nDefMax = nDefMin;
	int nAtk = nAtkMin + Getrand( nAtkMax - nAtkMin + 1 );
	int nDef = nDefMin + Getrand( nDefMax - nDefMin + 1 );
	return ApplyDamage( std::max( 1, nAtk - nDef ), pAttacker );
}

BOOL CAliveObject::ApplyDamage( int nDamage, CAliveObject * pAttacker )
{
	if( nDamage < 1 )nDamage = 1;

	SetCurHp( GetCurHp() - nDamage );

	WORD	wHp = (WORD)( GetCurHp() < 0 ? 0 : GetCurHp() );
	DWORD	dwView[] = { GetFeather(), 0, pAttacker != NULL ? pAttacker->GetId() : 0, (DWORD)nDamage };
	SendMsg( GetId(), SM_BEATTACK, wHp, (WORD)GetPropValue( PI_CURMP ), (WORD)nDamage, (LPVOID)dwView, sizeof( dwView ) );
	SendAroundMsg( GetId(), SM_BEATTACK, wHp, (WORD)GetPropValue( PI_CURMP ), (WORD)nDamage, (LPVOID)dwView, sizeof( dwView ) );

	if( GetCurHp() <= 0 )
		return OnDeath( pAttacker );

	return TRUE;
}

BOOL CAliveObject::GetMeal( int dir )
{
	//DWORD	dwView[] = { GetFeather(), 0 };
	//SendAroundMsg( GetId(), )
	return TRUE;
}



BOOL CAliveObject::Backward( int dir )
{
	return TRUE;
}

VOID	CAliveObject::Say( const char * pszMsg, ... )
{

	//pszMsg[100] = 0;

	char	szBuff[248];
	snprintf( szBuff, sizeof( szBuff ), "%s:", GetName() );
	
	va_list	vl;
	va_start( vl, pszMsg );
	vsnprintf( szBuff + strlen( szBuff ), sizeof( szBuff ) - strlen( szBuff ), pszMsg, vl );
	va_end( vl);
	szBuff[120] = '\0';
	
	int size = EncodeMsg( g_CodedMsgBuffer, GetId(), SM_CHAT, 0xff00, 0, 0, (LPVOID)szBuff );
	if( CanRecvMsg() )
	{
		OnAroundMsg( this, g_CodedMsgBuffer, size );
	}
	if( m_pArea == NULL )return;
	m_pArea->SendAroundMsg( this, g_CodedMsgBuffer, size );
}

VOID CAliveObject::SendMsg( DWORD dwFlag, WORD wCmd, WORD w1, WORD w2, WORD w3, LPVOID lpdata, int datasize )
{
	if( !CanRecvMsg() )return;
	int size = EncodeMsg( g_CodedMsgBuffer, dwFlag, wCmd, w1, w2, w3, lpdata, datasize );
	OnAroundMsg(this,g_CodedMsgBuffer,size);
}

VOID CAliveObject::SendMsgToObject( CAliveObject * pReceiver, DWORD dwFlag, WORD wCmd, WORD w1, WORD w2, WORD w3, LPVOID lpdata, int datasize )
{
	if( !pReceiver->CanRecvMsg() )return;
	int size = EncodeMsg( g_CodedMsgBuffer, dwFlag, wCmd, w1, w2, w3, lpdata, datasize );
	pReceiver->OnAroundMsg(this,g_CodedMsgBuffer,size);
}

VOID CAliveObject::SendAroundMsg( DWORD dwFlag, WORD wCmd, WORD w1, WORD w2, WORD w3, LPVOID lpdata, int datasize )
{
	if( m_pArea == NULL )return;
	int size = EncodeMsg( g_CodedMsgBuffer, dwFlag, wCmd, w1, w2, w3, lpdata, datasize );
	m_pArea->SendAroundMsg( this,g_CodedMsgBuffer,size );
}

VOID CAliveObject::SendMapMsg( DWORD dwFlag, WORD wCmd, WORD w1, WORD w2, WORD w3, LPVOID lpdata, int datasize )
{
	if( m_pMap == NULL )return;
	int size = EncodeMsg( g_CodedMsgBuffer, dwFlag, wCmd, w1, w2, w3, lpdata, datasize );
	m_pArea->SendAroundMsg(this,g_CodedMsgBuffer,size);
}



//	设置要去到的坐标
BOOL CAliveObject::SetAction( actiontype action, e_direction dir, WORD x, WORD y, DWORD dwActionTime )
{
	if( this->m_pMap == NULL )return FALSE;
	//if( x != m_wX || y != m_wY )
	//	if( !this->m_pMap->LockPos( (int)x, (int)y) )return FALSE;
	//CServer::GetInstance()->GetIoConsole()->OutPut(255, "SetAction( %d, %d, %d, %d, %d )\n", action, dir, x, y, dwActionTime );
	m_ActionType = action;
	m_wActionX = x;
	m_wActionY = y;
	m_ActionDirection = dir;
	m_dwActionCompleteTime = timeGetTime() + dwActionTime;
	return TRUE;
}

//	完整移动
BOOL CAliveObject::CompleteAction()
{
	if( m_pMap == NULL )return FALSE;

	if( m_ActionType == AT_STAND )return FALSE;

	if( m_wX != m_wActionX || m_wY != m_wActionY )
	{
		//	fixme: 这里应该检测下，如果要去的格子有障碍，就拒绝去到障碍的格子里面
		//	这样用来弥补不锁定目标格子带来的问题，从而有效地防止穿人
		if( m_bPosLocked )
			m_pMap->UnLockPos( (int)m_wX, (int)m_wY );
		m_bPosLocked = m_pMap->LockPos(m_wActionX, m_wActionY );
		setXY(m_wActionX, m_wActionY );
	}
	m_ActionType = AT_STAND;
	SetDirection( m_ActionDirection );
	//m_bFrozen = TRUE;
	//m_dwActionCompleteTime = timeGetTime() + 20; 
	//CServer::GetInstance()->GetIoConsole()->OutPut(255, "CompleteAction( %d, %d, %d, %d, %d )\n", m_ActionType, m_ActionDirection, m_wActionX,
	//																		m_wActionY, 0 );
	return TRUE;
}

BOOL CAliveObject::GetOutViewmsg( char * pszMsg, int & length )
{
	length = EncodeMsg( pszMsg, GetId(), SM_DISAPPEAR, m_wX, m_wY, 0, NULL );
	return TRUE;
}

BOOL CAliveObject::CanDoAction(actiontype action)
{
	if( m_ActionType != AT_STAND )return FALSE;
	//if( m_bFrozen )return FALSE;
	return TRUE;
}

VOID CAliveObject::Update()
{
	UpdatePoison();
	//	更新动作寄存器，在动作结束的时候结束动作
	DWORD dwCurtime = timeGetTime();
	if( m_ActionType != AT_STAND && dwCurtime >= m_dwActionCompleteTime &&( dwCurtime - m_dwActionCompleteTime )<= (DWORD)0x7fffffff)
	{
		//	动作准备完成
		//	动作完成失败的时候，停止到原来的那个坐标
		//	
		if( !CompleteAction() )
			ResetPos( m_wX, m_wY );
	
	}
	//	处理移动后的僵持
	//if( m_bFrozen && dwCurtime >= m_dwActionCompleteTime &&( dwCurtime - m_dwActionCompleteTime )<= (DWORD)0x7fffffff)
	//	m_bFrozen = FALSE;
}

VOID CAliveObject::OnLeaveMap( CVLogicMap * pMap, CVArea * pArea )
{
	if( pMap != NULL )
	{
		if( m_bPosLocked )
		{
			pMap->UnLockPos(m_wX, m_wY );
			m_bPosLocked = FALSE;
		}
	}
	CMapObject::OnLeaveMap( pMap, pArea );
}

VOID CAliveObject::OnEnterMap( CVLogicMap * pMap, CVArea * pArea )
{
	m_bPosLocked = pMap->LockPos( getX(), getY());
	CMapObject::OnEnterMap( pMap, pArea );
}

VOID	CAliveObject::ResetPos( WORD x, WORD y )
{
	CHAR	szMsg[200];
	DWORD	dwdata[3] = { GetFeather(), GetStatus(), 0 };
	int size = EncodeMsg( szMsg, GetId(), SM_BACK, x, y, (WORD)GetDirection(), (LPVOID)dwdata, sizeof( DWORD ) * 2 + sizeof( WORD ) );
	if( CanRecvMsg() )
		OnAroundMsg( this, szMsg, size );
	if( m_pArea != NULL )
		m_pArea->SendAroundMsg( this, szMsg, size );
	SetAction( AT_BACK, m_Direction, x, y, 200 );
}

BOOL CAliveObject::AddPet( CAliveObject * pObject )
{
	return TRUE;
}

BOOL	CAliveObject::RemovePet( CAliveObject * pObject )
{
	return TRUE;
}

//VOID CAliveObject::OnEnterArea( CVArea * pArea )
//{
//	m_pArea = pArea;
//}
//VOID CAliveObject::OnLeaveArea( CVArea * pArea )
//{
//	m_pArea = NULL;
//}




VOID CAliveObject::ApplyPoison( int nDamage, DWORD dwDuration, CAliveObject * pSource )
{
	if( nDamage <= 0 )return;
	m_nPoisonDamage = nDamage;
	m_dwPoisonUntil = timeGetTime() + dwDuration;
	m_dwPoisonNext = timeGetTime() + 1000;
	m_nPoisonSourceId = pSource ? pSource->GetId() : 0;
}

VOID CAliveObject::UpdatePoison()
{
	DWORD now = timeGetTime();
	if( m_nPoisonDamage <= 0 || now > m_dwPoisonUntil + 2000 )
	{
		return;
	}
	if( now < m_dwPoisonNext )return;
	m_dwPoisonNext = now + 1000;
	CAliveObject * pSource = NULL;
	if( m_nPoisonSourceId != 0 )
		pSource = CHumanPlayerMgr::GetInstance()->FindbyId( m_nPoisonSourceId );
	ApplyDamage( m_nPoisonDamage, pSource );
	if( now >= m_dwPoisonUntil )m_nPoisonDamage = 0;
}
