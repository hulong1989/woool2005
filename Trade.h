// Trade.h —— 玩家间交易
#pragma once
#include "public.h"
#include <string>
#include <vector>

struct TRADEITEM
{
	DWORD	dwMakeIndex;
	ITEM	item;
};

struct TRADESIDE
{
	std::string				name;
	std::vector<TRADEITEM>	items;
	DWORD					dwGold;
	BOOL					bLocked;
	BOOL					bConfirmed;
};

struct TRADE
{
	TRADESIDE	a;
	TRADESIDE	b;
};

class CTradeMgr
{
public:
	static CTradeMgr * GetInstance();

	BOOL Open( const char * pszA, const char * pszB );
	BOOL AddItem( const char * pszName, DWORD dwMakeIndex );
	BOOL SetGold( const char * pszName, DWORD dwGold );
	BOOL Lock( const char * pszName );
	BOOL Confirm( const char * pszName );
	VOID Cancel( const char * pszName, const char * pszReason = NULL );
	VOID OnPlayerLeave( const char * pszName );
	TRADE * Find( const char * pszName );

private:
	CTradeMgr();
	VOID Send( TRADE & t );
	VOID SendSide( TRADE & t, int nSide );
	BOOL Execute( TRADE & t );
	VOID Clear( TRADE & t );
	int FindIndex( const char * pszName );

	static CTradeMgr * m_pInstance;
	std::vector<TRADE> m_Trades;
};
