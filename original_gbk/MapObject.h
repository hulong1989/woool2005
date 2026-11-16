#pragma once
#include "vmap.h"
enum	e_listnode_index
{
	LNI_AREA,
	LNI_MAP,
	LNI_WORLD,
	LNI_MAX,
};

class CVLogicMap;
class CVArea;

class CMapObject
{
public:
	CMapObject(void);
	virtual ~CMapObject(void);
	xListHost<CMapObject>::xListNode/*<CMapObject>*/	*	GetLinkNode( int Index )
	{ 
		if( Index >= 0 || Index < LNI_MAX )return m_DLinkNodes[Index];
		return NULL;
	}
	WORD	getX(){return m_wX;}
	WORD	getY(){return m_wY;}
	VOID	setX(WORD x){ setXY( x, m_wY );}
	VOID	setY(WORD y){ setXY( m_wX, y );}
	VOID	setXY(WORD x, WORD y );
	//BOOL	EnterMap( CVLogicMap * pMap );
	//BOOL	LeaveMap();
	//BOOL	EnterArea( CVArea * pArea );
	//BOOL	LeaveArea();
	virtual BOOL CanRecvMsg(){return FALSE;}
	virtual VOID OnAroundMsg( CMapObject * pSender, const char * pszCodedMsg, int size ){}
	virtual UINT GetId(){ return 0;}
	virtual VOID OnEnterMap( CVLogicMap * pMap, CVArea * pArea );
	virtual VOID OnLeaveMap( CVLogicMap * pMap, CVArea * pArea );

	//virtual VOID OnEnterArea( CVArea * pArea ) {}
	//virtual VOID OnLeaveArea( CVArea * pArea ) {}
	virtual VOID Update(){}
	virtual VOID Clean();
	virtual BOOL GetViewmsg( char * pszMsg, int & length ){ return FALSE;}
	virtual BOOL GetOutViewmsg( char * pszMsg, int & length ){ return FALSE;}
	virtual e_object_type GetType(){ return OBJ_UNKNOWN;}
	CVLogicMap * GetMap(){return m_pMap;}
	CVArea *		GetArea(){return m_pArea;}
	VOID	SetArea( CVArea * pArea ){m_pArea = pArea;}
	virtual VOID	OnSetPos( WORD oldx, WORD oldy, WORD newx, WORD newy ){}
//	VOID	SetArea( CVArea * pArea ){ m_pArea = pArea;}
	WORD		GetInvisibleLevel(){ return m_wInvisibleLevel;}
	virtual WORD		GetDeInvisibleLevel(){ return 0;}
protected:
	int EncodeMsg( char * buffer, DWORD dwFlag, WORD wCmd, WORD w1, WORD w2, WORD w3, LPVOID lpdata, int datasize = -1 );
	CVLogicMap * m_pMap;
	CVArea * m_pArea;

	WORD	m_wInvisibleLevel;
	WORD	m_wX, m_wY;
	xListHost<CMapObject>::xListNode/*<CMapObject>*/	*	m_DLinkNodes[LNI_MAX];
};
