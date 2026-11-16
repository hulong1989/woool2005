#pragma once
class CHumanPlayer;


enum	e_exchangestate
{
	EE_FREE,
	EE_PUTITEMS,
	EE_WAITFOROTHER,
	EE_MAX,
};

class CExchangeObj
{
public:
	CExchangeObj(CHumanPlayer * pPlayer);
	virtual ~CExchangeObj(void);
	BOOL	ConnectOther( CHumanPlayer * pPlayer );
	VOID	DisconnectOther();
	BOOL	AddItem( ITEM & item );
	BOOL	TakeItem( DWORD dwMakeIndex, ITEM & item );
	BOOL	DoExchange();
	BOOL	Cancel();
	BOOL	Complete();
public:
	const char * GetErrorMsg(){return m_pErrorMsg;}
	e_exchangestate GetState(){return m_State;}
private:
	CHumanPlayer * m_pPlayer;
	CHumanPlayer * m_pOtherSide;
	DWORD	m_dwGold;
	DWORD	m_dwYuanbao;
	ITEM	m_Items[10];
	const char	*	m_pErrorMsg;
	e_exchangestate m_State;
};
