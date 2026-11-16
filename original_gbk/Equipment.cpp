#include "StdAfx.h"
#include ".\equipment.h"
#include "humanplayer.h"
CEquipment::CEquipment(CHumanPlayer * pPlayer)
{
	assert( pPlayer != NULL );
	m_pPlayer = pPlayer;
	Clean();
}

CEquipment::~CEquipment(void)
{
}

BOOL	CEquipment::EquipItem( int pos, ITEM & inItem, ITEM & outItem )
{
	if( pos < 0 || pos >= 10 )return FALSE;
	//	step1: check condition fitable
	if( !CheckFitable( pos, inItem ) )return FALSE;
	//	step2: unequip old item if its exist
	if( m_Equipments[pos].dwMakeIndex != 0 )
	{
		outItem.dwMakeIndex = 0;
		if( !UnEquipItem( pos, outItem ) )
			return FALSE;
	}
	//	step3: put in item on
	m_Equipments[pos] = inItem;
	AddPropCache( inItem );
	//	last: return true
	return TRUE;
}

BOOL	CEquipment::UnEquipItem( int pos, ITEM & outItem )
{
	if( pos < 0 || pos >= 10 )return FALSE;

	if( m_Equipments[pos].dwMakeIndex == 0 )return FALSE;
	outItem = m_Equipments[pos];
	memset( m_Equipments+pos, 0, sizeof( ITEM ));
	RemovePropCache( outItem );
	return TRUE;
}


VOID	CEquipment::Clean()
{
	m_pErrorMsg = "装备成功!";
	memset( m_Equipments, 0, sizeof( m_Equipments ));
	memset( m_PropMap, 0, sizeof( m_PropMap ));
}

BOOL	CEquipment::CheckFitable( int pos, ITEM & inItem )
{
	if( !VerifyPos( pos ) )return FALSE;
	//	step0: check pos and can equip
	if( !CheckStdModeFitPos( inItem.baseitem.btStdMode, pos ) )return FALSE;
	//	step1: check need type and need level
	if( !CheckNeedInfo( inItem.baseitem.needtype, inItem.baseitem.needvalue ))return FALSE;
	//	step2: check weight
	m_pErrorMsg = "太重了";	
	if( pos == _U_WEAPON )
	{
		if( inItem.baseitem.btWeight > m_pPlayer->GetPropValue(PI_MAXHANDWEIGHT))
		{
			return FALSE;
		}
	}
	else
	{
		int weight = CalcEquipmentsWeight( pos );
		int maxweight =  (int)m_pPlayer->GetPropValue(PI_MAXBODYWEIGHT);
		if( weight >= maxweight )return FALSE;
		if( inItem.baseitem.btWeight + weight > maxweight )return FALSE;
	}
	//	step3: check what?( nothing to check )
	return TRUE;
}

BOOL	CEquipment::GetStdModeMeaning( BYTE stdMode, int & dc1p, int & dc2p, int & mc1p, int & mc2p, int &sc1p, int & sc2p, int & ac1p, int & ac2p, int & mac1p, int & mac2p )
{
	ac1p = -1, ac2p = -1, mac1p = -1, mac2p = -1;
	dc1p = PI_MINDC, dc2p = PI_MAXDC, mc1p = PI_MINMC, mc2p = PI_MAXMC, sc1p = PI_MINSC, sc2p = PI_MAXSC;

	switch( stdMode )
	{
	case	5:
	case	6:
		ac1p = PI_LUCKY, ac2p = PI_HITRATE, mac1p = PI_DAWN;
		return TRUE;
	case	10:
	case	11:
	case	15:
		ac1p = PI_MINAC, ac2p = PI_MAXAC, mac1p = PI_MINMAC, mac2p = PI_MAXMAC;
		return FALSE;
	case	19:
		ac2p = PI_MAGESCAPE,	//	魔法躲避+300%	maxdef*10
		mac2p = PI_LUCKY,		//	幸运+50		maxmagdef
		mac1p = PI_DAWN;		//	诅咒+40		minmagdef
		return FALSE;
	case	20:
		ac2p = PI_HITRATE,		//	命中+30		maxdef
		mac2p = PI_ESCAPE;		//	躲避+50		maxmagdef
		return FALSE;
	case	21:
		ac2p = PI_HPRECOVER,		//体力恢复+30	maxdef
		ac1p = PI_ATTACKSPEED,		//攻击速度+20	mindef
		mac2p = PI_MPRECOVER,		//魔法恢复+50	maxmagdef
		mac1p = PI_ATTACKSPEED * -1;//攻击速度-40	minmagdef
		return TRUE;				//强度+33		specialpower
	case	22:
	case	26:
		ac1p = PI_MINAC, ac2p = PI_MAXAC, mac1p = PI_MINMAC, mac2p = PI_MAXMAC;
		return TRUE;
	case	23:
		ac2p = PI_POISONESCAPE,			//毒物躲避+300%	maxdef*10
		ac1p = PI_ATTACKSPEED,			//攻击速度+20	mindef
		mac2p = PI_POISONRECOVER,		//中毒恢复+500%	maxmagdef*10
		mac1p = PI_ATTACKSPEED * -1;	//攻击速度-40	minmagdef
		return TRUE;					//强度+33		specialpower
	case	24:
		ac2p = PI_HITRATE,	//命中+30		maxdef
		ac2p = PI_ESCAPE;	//躲避+50		maxmagdef
		return TRUE;		//强度+33		specialpower
	case	25:
		dc1p = -1, dc2p = -1, mc1p = -1, mc2p = -1, sc1p = -1, sc2p = -1;
		return FALSE;
	}
	dc1p = -1, dc2p = -1, mc1p = -1, mc2p = -1, sc1p = -1, sc2p = -1;
	return FALSE;
}


VOID	CEquipment::AddPropCache( ITEM & item )
{
	int	dc1p, dc2p, mc1p, mc2p, sc1p, sc2p,ac1p = PI_MINAC, ac2p = PI_MAXAC, mac1p = PI_MINMAC, mac2p = PI_MAXMAC;
	BOOL bAddSpecialPower = GetStdModeMeaning(item.baseitem.btStdMode,dc1p, dc2p, mc1p, mc2p, sc1p, sc2p, ac1p, ac2p, mac1p, mac2p );

	if( dc1p != -1 )
		m_PropMap[dc1p] += item.baseitem.btMinAtk;
	if( dc2p != -1 )
		m_PropMap[dc2p] += item.baseitem.btMaxAtk;
	
	if( mc1p != -1 )
		m_PropMap[mc1p] += item.baseitem.btMinMagAtk;
	if( mc2p != -1 )
		m_PropMap[mc2p] += item.baseitem.btMaxMagAtk;

	if( sc1p != -1 )
		m_PropMap[sc1p] += item.baseitem.btMinSouAtk;
	if( sc2p != -1 )
		m_PropMap[sc2p] += item.baseitem.btMaxSouAtk;

	if( ac1p != -1 )
	{
		if( ac1p < 0 )
			m_PropMap[ac1p*-1] -= item.baseitem.btMinDef;
		else
			m_PropMap[ac1p] += item.baseitem.btMinDef;
	}
	if( ac2p != -1 )
	{
		if( ac2p < 0 )
			m_PropMap[ac2p*-1] -= item.baseitem.btMaxDef;
		else
			m_PropMap[ac2p] += item.baseitem.btMaxDef;
	}
	if( mac1p != -1 )
	{
		if( mac1p < 0 )
			m_PropMap[mac1p*-1] -= item.baseitem.btMinMagDef;
		else
			m_PropMap[mac1p] += item.baseitem.btMinMagDef;
	}
	if( mac2p != -1 )
	{
		if( mac2p < 0 )
			m_PropMap[mac2p*-1] -= item.baseitem.btMaxMagDef;
		else
			m_PropMap[mac2p] += item.baseitem.btMaxMagDef;
	}

	if( bAddSpecialPower )
	{
		char value = (char)item.baseitem.btSpecialpower;
		if( value > 0 )
			m_PropMap[PI_HARD] += value;
		else
			m_PropMap[PI_HOLLY] += value * -1;
	}

}

VOID	CEquipment::RemovePropCache( ITEM & item )
{
	int	dc1p, dc2p, mc1p, mc2p, sc1p, sc2p,ac1p = PI_MINAC, ac2p = PI_MAXAC, mac1p = PI_MINMAC, mac2p = PI_MAXMAC;
	BOOL bAddSpecialPower = GetStdModeMeaning(item.baseitem.btStdMode,dc1p, dc2p, mc1p, mc2p, sc1p, sc2p, ac1p, ac2p, mac1p, mac2p );

	if( dc1p != -1 )
		m_PropMap[dc1p] -= item.baseitem.btMinAtk;
	if( dc2p != -1 )
		m_PropMap[dc2p] -= item.baseitem.btMaxAtk;

	if( mc1p != -1 )
		m_PropMap[mc1p] -= item.baseitem.btMinMagAtk;
	if( mc2p != -1 )
		m_PropMap[mc2p] -= item.baseitem.btMaxMagAtk;

	if( sc1p != -1 )
		m_PropMap[sc1p] -= item.baseitem.btMinSouAtk;
	if( sc2p != -1 )
		m_PropMap[sc2p] -= item.baseitem.btMaxSouAtk;

	if( ac1p != -1 )
	{
		if( ac1p < 0 )
			m_PropMap[ac1p*-1] += item.baseitem.btMinDef;
		else
			m_PropMap[ac1p] -= item.baseitem.btMinDef;
	}
	if( ac2p != -1 )
	{
		if( ac2p < 0 )
			m_PropMap[ac2p*-1] += item.baseitem.btMaxDef;
		else
			m_PropMap[ac2p] -= item.baseitem.btMaxDef;
	}
	if( mac1p != -1 )
	{
		if( mac1p < 0 )
			m_PropMap[mac1p*-1] += item.baseitem.btMinMagDef;
		else
			m_PropMap[mac1p] -= item.baseitem.btMinMagDef;
	}
	if( mac2p != -1 )
	{
		if( mac2p < 0 )
			m_PropMap[mac2p*-1] += item.baseitem.btMaxMagDef;
		else
			m_PropMap[mac2p] -= item.baseitem.btMaxMagDef;
	}

	if( bAddSpecialPower )
	{
		char value = (char)item.baseitem.btSpecialpower;
		if( value > 0 )
			m_PropMap[PI_HARD] -= value;
		else
			m_PropMap[PI_HOLLY] -= value * -1;
	}
}

BOOL	CEquipment::CheckStdModeFitPos( BYTE stdMode, int pos )
{
	m_pErrorMsg = "错误的装备位置!";
	switch( pos )
	{
	case	_U_WEAPON:
		return ( stdMode == 5 || stdMode == 6 );
	case	_U_DRESS:
		return ( stdMode == 10 || stdMode == 11 );
	case	_U_NECKLACE:
		return ( stdMode == 19 || stdMode == 20 || stdMode == 21 );
	case	_U_RINGL:
	case	_U_RINGR:
		return ( stdMode == 22 || stdMode == 23 );
	case	_U_ARMRINGL:
		if( stdMode == 26 )return TRUE;
	case	_U_ARMRINGR:
		return ( stdMode == 24 || stdMode == 26 );
	case	_U_HELMET:
		return ( stdMode == 15 );
	}
	return FALSE;
}

BOOL	CEquipment::CheckNeedInfo( BYTE needtype, BYTE needlevel )
{
	//(11)Need表示限制种类：
	//0 为等级限制
	//1 为攻击限制
	//2 为魔法限制
	//3 为道术限制
	//(12)NeedLevel表示了(Need)限制的具体数量.
	switch( needtype )
	{
	case	INT_LEVEL:
		return (m_pPlayer != NULL && m_pPlayer->GetPropValue( PI_LEVEL ) >= needlevel);
	case	INT_DC:
		return (m_pPlayer != NULL && m_pPlayer->GetPropValue( PI_MAXDC ) >= needlevel);
	case	INT_MC:
		return (m_pPlayer != NULL && m_pPlayer->GetPropValue( PI_MAXMC ) >= needlevel);
	case	INT_SC:
		return (m_pPlayer != NULL && m_pPlayer->GetPropValue( PI_MAXSC ) >= needlevel);
	}
	return TRUE;
}

VOID	CEquipment::ResetPropCache()
{
	//	
}
