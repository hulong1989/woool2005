#pragma once
class CHumanPlayer;

class CEquipment
{
public:
	CEquipment(CHumanPlayer * pPlayer );
	virtual ~CEquipment(void);
	VOID	Clean();
public:
	BOOL	EquipItem( int pos, ITEM & inItem, ITEM & outItem );
	BOOL	UnEquipItem( int pos, ITEM & outItem );
	BOOL	CheckFitable( int pos, ITEM & inItem );
public:
	//	get prop value
	ITEM & operator[]( int pos ){ if( !VerifyPos(pos))return m_EmptyEquipment; return m_Equipments[pos];}
public:
	ITEM	* GetEquipment( int pos ){ if( !VerifyPos( pos ) || m_Equipments[pos].dwMakeIndex == 0 )return NULL; return &m_Equipments[pos]; }
	//	重建时补充：读档时直接把物品放回装备位（跳过穿戴条件检查）
	BOOL	RestoreItem( int pos, ITEM & item );
	int	CalcEquipmentsWeight( int expos )
	{
		int retweight = 0;
		for( int i = 0;i < 10;i ++ )
		{

			if( i == _U_WEAPON || i == expos || m_Equipments[i].dwMakeIndex == 0 )continue;
			retweight += m_Equipments[i].baseitem.btWeight;
		}
		return retweight;
	}
	const char * GetErrorMsg(){ return m_pErrorMsg;}

	int	GetProp( PROP_INDEX index ){ if( index < 0 || index >= PI_PROP_COUNT)return 0; return m_PropMap[index];}

	VOID	ResetPropCache();

	BOOL	GetStdModeMeaning( BYTE stdMode, int & dc1p, int & dc2p, int & mc1p, int & mc2p, int &sc1p, int & sc2p, int & ac1p, int & ac2p, int & mac1p, int & mac2p );
private:
	BOOL	VerifyPos( int pos ) { if( pos < 0 || pos >= 10 ) return FALSE; return TRUE;}
	BOOL	CheckStdModeFitPos( BYTE stdMode, int pos );
	BOOL	CheckNeedInfo( BYTE needtype, BYTE needlevel );
	VOID	AddPropCache( ITEM & item );
	VOID	RemovePropCache( ITEM & item );
	VOID	ApplySetBonuses();


	int		m_PropMap[PI_PROP_COUNT];
	ITEM	m_Equipments[10];
	ITEM	m_EmptyEquipment;
	const char * m_pErrorMsg;
	CHumanPlayer * m_pPlayer;
};
