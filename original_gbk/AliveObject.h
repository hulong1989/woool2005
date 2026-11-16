#pragma once
#include "mapobject.h"
enum	actiontype
{
	AT_STAND,
	AT_WALK,
	AT_RUN,
	AT_BACK,
	AT_TURN,
	AT_ATTACK,
	AT_GETMEAL,
	AT_BEATTACK,
	AT_SPELLSKILL,
};
#define MAX_ACTION_TIME	500000
class CAliveObject :
	public CMapObject
{
public:
	CAliveObject(void);
	virtual ~CAliveObject(void);
	virtual VOID Clean();
	BOOL Walk( int dir, BOOL bServerCall = FALSE );
	BOOL Run( int dir, BOOL bServerCall = FALSE );
	BOOL Turn( int dir, BOOL bServerCall = FALSE );
	BOOL Attack( int dir, BOOL bServerCall = FALSE );
	BOOL BeAttack( CAliveObject * pAttacker );
	BOOL GetMeal( int dir );
	BOOL Backward( int dir );
	VOID Say( const char * pszMsg, ... );

	virtual VOID OnLeaveMap( CVLogicMap * pMap, CVArea * pArea );
	virtual VOID OnEnterMap( CVLogicMap * pMap, CVArea * pArea );

	//virtual VOID OnEnterArea( CVArea * pArea );
	//virtual VOID OnLeaveArea( CVArea * pArea );

	//CVLogicMap * GetMap() {return m_pMap;}
	//CVArea *		GetArea() {return m_pArea;}
	//VOID	SetArea( CVArea * pArea ) { m_pArea = pArea;}

	virtual BOOL CanDoAction(actiontype action);
	virtual VOID Update();

	VOID SendMsg( DWORD dwFlag, WORD wCmd, WORD w1, WORD w2, WORD w3, LPVOID lpdata, int datasize = -1 );

	VOID SendMsgToObject( CAliveObject * pReceiver, DWORD dwFlag, WORD wCmd, WORD w1, WORD w2, WORD w3, LPVOID lpdata, int datasize = -1 );

	VOID SendAroundMsg( DWORD dwFlag, WORD wCmd, WORD w1, WORD w2, WORD w3, LPVOID lpdata, int datasize = -1 );

	VOID SendMapMsg( DWORD dwFlag, WORD wCmd, WORD w1, WORD w2, WORD w3, LPVOID lpdata, int datasize = -1 );

	virtual DWORD GetFeather(){return 0;}
	virtual DWORD GetHealth(){return 0;}
	virtual DWORD GetStatus(){return 0;}

	virtual const char * GetName(){ return "NONAME";}

	UINT GetId(){return m_Id;}
	VOID SetId( UINT Id ){ m_Id = Id;}

	BOOL	SetAction( actiontype action, e_direction dir, WORD x, WORD y, DWORD dwActionTime );

	BOOL	CompleteAction();

	BOOL GetOutViewmsg( char * pszMsg, int & length );
	BYTE GetWalkSpeed(){ return m_btWalkSpeed;}
	VOID SetWalkSpeed( BYTE btSpeed ){ m_btWalkSpeed = btSpeed;}
	BYTE GetRunSpeed(){ return m_btRunSpeed;}
	VOID SetRunSpeed( BYTE btSpeed ){ m_btRunSpeed = btSpeed;}

	WORD GetDeInvisibleLevel(){ return m_wDeInvisibleLevel;}
	e_direction GetDirection(){ return m_Direction;}
	void	SetDirection( e_direction dir ){ m_Direction = dir;	}
	UINT	GetMapId(){return m_Mapid;}
	VOID SetMapId( UINT mapid ){ m_Mapid = mapid;}

	WORD	GetActionX(){ return m_wActionX;}
	WORD	GetActionY(){ return m_wActionY;}
	actiontype	GetActionType(){ return m_ActionType;}


	VOID	ResetPos( WORD x, WORD y );
public:
	virtual int	GetPropValue( PROP_INDEX index ){ return 0;}
	//virtual WORD	GetCurHp();
	//virtual WORD	GetCurMp();
	//virtual WORD	GetMaxHp();
	//virtual WORD	GetMaxMp();

	//virtual BYTE	GetMinDC();
	//virtual BYTE	GetMaxDC();
	//virtual BYTE	GetMinAC();
	//virtual BYTE	GetMaxAC();
	//virtual BYTE	GetMinMC();
	//virtual BYTE	GetMaxMC();
	//virtual BYTE	GetMinSC();
	//virtual BYTE	GetMaxSC();
	//virtual BYTE	GetMinMAC();
	//virtual BYTE	GetMaxMAC();

	BOOL	AddPet( CAliveObject * pObject );
	BOOL	RemovePet( CAliveObject * pObject );

	xListHost<CAliveObject>	& GetPetList(){ return m_xPetList;}
	xListHost<CAliveObject>::xListNode & GetPetListNode(){ return m_PetListNode;}

protected:
	e_direction	m_Direction;
	BYTE	m_btWalkSpeed;
	BYTE	m_btRunSpeed;
	WORD m_wDeInvisibleLevel;
	actiontype	m_ActionType;
	WORD	m_wActionX, m_wActionY;
	e_direction	m_ActionDirection;
	DWORD	m_dwActionCompleteTime;
	UINT	m_Id;
	UINT	m_Mapid;
	BOOL	m_bPosLocked;
	xListHost<CAliveObject>	m_xPetList;
	xListHost<CAliveObject>::xListNode m_PetListNode;
};
