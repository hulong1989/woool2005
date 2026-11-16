// Trade.cpp —— 玩家间交易
#include "stdafx.h"
#include "Trade.h"
#include "HumanPlayer.h"
#include "HumanPlayerMgr.h"
#include "ItemBox.h"
#include "gsClientObj.h"
#include <cmath>

CTradeMgr * CTradeMgr::m_pInstance = NULL;

static void put8(char * b, int & o, unsigned char v){ b[o++] = (char)v; }
static void put16(char * b, int & o, unsigned short v){ b[o++] = (char)(v & 255); b[o++] = (char)(v >> 8); }
static void put32(char * b, int & o, unsigned int v){ for(int i=0;i<4;++i) b[o++] = (char)((v >> (i*8)) & 255); }
static void putstr(char * b, int & o, const char * s)
{
	size_t n = s ? strlen(s) : 0; if(n > 255)n = 255;
	put8(b,o,(unsigned char)n); if(n)memcpy(b+o,s,n); o+=(int)n;
}

CTradeMgr::CTradeMgr(){}
CTradeMgr * CTradeMgr::GetInstance()
{
	if(m_pInstance == NULL)m_pInstance = new CTradeMgr;
	return m_pInstance;
}

int CTradeMgr::FindIndex( const char * pszName )
{
	if(pszName == NULL)return -1;
	for(size_t i=0;i<m_Trades.size();++i)
		if(m_Trades[i].a.name == pszName || m_Trades[i].b.name == pszName)return (int)i;
	return -1;
}
TRADE * CTradeMgr::Find( const char * pszName )
{
	int i = FindIndex(pszName); return i < 0 ? NULL : &m_Trades[i];
}

static CHumanPlayer * P(const std::string & n){ return CHumanPlayerMgr::GetInstance()->FindbyName(n.c_str()); }

VOID CTradeMgr::SendSide( TRADE & t, int nSide )
{
	TRADESIDE & me = nSide == 0 ? t.a : t.b;
	TRADESIDE & other = nSide == 0 ? t.b : t.a;
	CHumanPlayer * p = P(me.name);
	if(p == NULL || p->GetClientObj() == NULL)return;

	char buf[4096]; int o = 0;
	put8(buf,o,1);
	putstr(buf,o,other.name.c_str());
	put32(buf,o,me.dwGold);
	put32(buf,o,other.dwGold);
	put8(buf,o,me.bLocked ? 1 : 0);
	put8(buf,o,other.bLocked ? 1 : 0);
	put8(buf,o,me.bConfirmed ? 1 : 0);
	put8(buf,o,other.bConfirmed ? 1 : 0);
	put16(buf,o,(unsigned short)me.items.size());
	for(size_t i=0;i<me.items.size();++i){
		put32(buf,o,me.items[i].dwMakeIndex);
		putstr(buf,o,me.items[i].item.baseitem.szName);
		put16(buf,o,me.items[i].item.wCount);
	}
	put16(buf,o,(unsigned short)other.items.size());
	for(size_t i=0;i<other.items.size();++i){
		put32(buf,o,other.items[i].dwMakeIndex);
		putstr(buf,o,other.items[i].item.baseitem.szName);
		put16(buf,o,other.items[i].item.wCount);
	}
	p->GetClientObj()->SendMsg(p->GetId(), SM_TRADE, 0,0,0, buf, o);
}

VOID CTradeMgr::Send( TRADE & t )
{
	SendSide(t,0);
	SendSide(t,1);
}

VOID CTradeMgr::Clear( TRADE & t )
{
	// 交易关闭通知
	const char * names[2] = { t.a.name.c_str(), t.b.name.c_str() };
	for(int k=0;k<2;++k){
		CHumanPlayer * p = P(names[k]);
		if(p && p->GetClientObj()){
			char buf[64]; int o=0; put8(buf,o,0); putstr(buf,o,"");
			p->GetClientObj()->SendMsg(p->GetId(), SM_TRADE, 0,0,0, buf, o);
		}
	}
}

BOOL CTradeMgr::Open( const char * pszA, const char * pszB )
{
	if(!pszA || !pszB || !*pszA || !*pszB || strcmp(pszA,pszB)==0)return FALSE;
	if(FindIndex(pszA) >= 0 || FindIndex(pszB) >= 0)return FALSE;
	CHumanPlayer * a = P(pszA); CHumanPlayer * b = P(pszB);
	if(a == NULL || b == NULL)return FALSE;
	if(a->GetMapId() != b->GetMapId())return FALSE;
	if(std::abs((int)a->getX()-(int)b->getX()) > 6 || std::abs((int)a->getY()-(int)b->getY()) > 6)return FALSE;

	TRADE t;
	t.a.name = pszA; t.b.name = pszB;
	t.a.dwGold = t.b.dwGold = 0;
	t.a.bLocked = t.b.bLocked = FALSE;
	t.a.bConfirmed = t.b.bConfirmed = FALSE;
	m_Trades.push_back(t);
	Send(m_Trades.back());
	return TRUE;
}

BOOL CTradeMgr::AddItem( const char * pszName, DWORD dwMakeIndex )
{
	int i = FindIndex(pszName); if(i < 0)return FALSE;
	TRADE & t = m_Trades[i];
	TRADESIDE & me = t.a.name == pszName ? t.a : t.b;
	if(me.bLocked)return FALSE;
	CHumanPlayer * p = P(me.name); if(p == NULL)return FALSE;
	ITEM * it = p->GetBag().FindItem(dwMakeIndex);
	if(it == NULL)return FALSE;
	for(size_t k=0;k<me.items.size();++k)
		if(me.items[k].dwMakeIndex == dwMakeIndex)return FALSE;

	TRADEITEM ti; memset(&ti,0,sizeof(ti));
	ti.dwMakeIndex = dwMakeIndex; ti.item = *it;
	if(!p->GetBag().RemoveItem(dwMakeIndex))return FALSE;
	me.items.push_back(ti);
	me.bConfirmed = FALSE; t.a.bConfirmed = t.b.bConfirmed = FALSE;
	Send(t);
	return TRUE;
}

BOOL CTradeMgr::SetGold( const char * pszName, DWORD dwGold )
{
	int i = FindIndex(pszName); if(i < 0)return FALSE;
	TRADE & t = m_Trades[i];
	TRADESIDE & me = t.a.name == pszName ? t.a : t.b;
	if(me.bLocked)return FALSE;
	CHumanPlayer * p = P(me.name); if(p == NULL || p->GetGold() < dwGold)return FALSE;
	me.dwGold = dwGold;
	me.bConfirmed = FALSE; t.a.bConfirmed = t.b.bConfirmed = FALSE;
	Send(t);
	return TRUE;
}

BOOL CTradeMgr::Lock( const char * pszName )
{
	int i = FindIndex(pszName); if(i < 0)return FALSE;
	TRADE & t = m_Trades[i];
	TRADESIDE & me = t.a.name == pszName ? t.a : t.b;
	me.bLocked = TRUE; me.bConfirmed = FALSE;
	Send(t);
	return TRUE;
}

BOOL CTradeMgr::Execute( TRADE & t )
{
	CHumanPlayer * a = P(t.a.name); CHumanPlayer * b = P(t.b.name);
	if(a == NULL || b == NULL)return FALSE;
	if(a->GetMapId() != b->GetMapId())return FALSE;
	if(std::abs((int)a->getX()-(int)b->getX()) > 6 || std::abs((int)a->getY()-(int)b->getY()) > 6)return FALSE;
	if(a->GetGold() < t.a.dwGold || b->GetGold() < t.b.dwGold)return FALSE;
	if(a->GetBag().GetFreeCount() < (int)t.b.items.size())return FALSE;
	if(b->GetBag().GetFreeCount() < (int)t.a.items.size())return FALSE;

	for(size_t i=0;i<t.a.items.size();++i)
		if(!b->AddBagItem(t.a.items[i].item))return FALSE;
	for(size_t i=0;i<t.b.items.size();++i)
		if(!a->AddBagItem(t.b.items[i].item))return FALSE;
	if(t.a.dwGold > 0)a->AddGold(-(int)t.a.dwGold);
	if(t.b.dwGold > 0)b->AddGold(-(int)t.b.dwGold);
	if(t.b.dwGold > 0)a->AddGold((int)t.b.dwGold);
	if(t.a.dwGold > 0)b->AddGold((int)t.a.dwGold);

	char msg[96];
	snprintf(msg,sizeof(msg),"与 %s 交易完成。", t.b.name.c_str());
	a->SendMsg(a->GetId(), SM_CHAT, 0xffd700,0,0,(LPVOID)msg);
	snprintf(msg,sizeof(msg),"与 %s 交易完成。", t.a.name.c_str());
	b->SendMsg(b->GetId(), SM_CHAT, 0xffd700,0,0,(LPVOID)msg);
	return TRUE;
}

BOOL CTradeMgr::Confirm( const char * pszName )
{
	int i = FindIndex(pszName); if(i < 0)return FALSE;
	TRADE & t = m_Trades[i];
	TRADESIDE & me = t.a.name == pszName ? t.a : t.b;
	if(!me.bLocked || !(t.a.name == pszName ? t.b.bLocked : t.a.bLocked))return FALSE;
	me.bConfirmed = TRUE;
	Send(t);
	if(t.a.bConfirmed && t.b.bConfirmed)
	{
		if(Execute(t))
		{
			Clear(t); m_Trades.erase(m_Trades.begin()+i); return TRUE;
		}
		t.a.bConfirmed = t.b.bConfirmed = FALSE;
		Send(t);
		return FALSE;
	}
	return TRUE;
}

VOID CTradeMgr::Cancel( const char * pszName, const char * pszReason )
{
	int i = FindIndex(pszName); if(i < 0)return;
	TRADE & t = m_Trades[i];
	CHumanPlayer * a = P(t.a.name); CHumanPlayer * b = P(t.b.name);
	for(size_t k=0;k<t.a.items.size();++k) if(a) a->AddBagItem(t.a.items[k].item);
	for(size_t k=0;k<t.b.items.size();++k) if(b) b->AddBagItem(t.b.items[k].item);
	const char * reason = pszReason ? pszReason : "交易已取消。";
	if(a) a->SendMsg(a->GetId(), SM_CHAT, 0xff8888,0,0,(LPVOID)reason);
	if(b) b->SendMsg(b->GetId(), SM_CHAT, 0xff8888,0,0,(LPVOID)reason);
	Clear(t);
	m_Trades.erase(m_Trades.begin()+i);
}

VOID CTradeMgr::OnPlayerLeave( const char * pszName )
{
	if(FindIndex(pszName) >= 0)Cancel(pszName, "对方离开，交易取消。");
}
