// Server.h  ——  重建（原快照缺失）
#pragma once

#include "public.h"
#include "gsClientObj.h"

class CGameWorld;

class CServer :
	public CBaseServer,
	public CInputListener
{
public:
	CServer(void);
	virtual ~CServer(void);

	static CServer* GetInstance()
	{
		if( m_pInstance == NULL )
			m_pInstance = new CServer;
		return m_pInstance;
	}

	//	生命周期
	virtual BOOL InitServer( CSettingFile & s );
	virtual VOID CleanServer();
	virtual VOID Update();

	//	消息
	virtual VOID OnDBMsg( PMIRMSG pMsg, int datasize );
	virtual VOID OnMASMsg( WORD wCmd, WORD wType, WORD wIndex, const char * pszData, int datasize );
	virtual VOID OnSCMsg( PMIRMSG pMsg, int datasize );
	virtual VOID OnSCServerReady();

	//	连接对象
	virtual CClientObject * NewClientObject();
	virtual VOID DeleteClientObject( CClientObject * pObject );

	//	进入游戏（由选人服务器转发）
	SERVER_ERROR AddEnterAccount( UINT nLoginId, UINT nSelCharId, const char * pszAccount );
	BOOL GetEnterInfo( UINT nLoginId, UINT nSelCharId, const char * pszAccount, ENTERGAMESERVER & enterinfo );

	//	控制台命令（实现见 ServerConsole.cpp）
	virtual VOID OnInputCommand( const char * pszCommand );

private:
	static CServer * m_pInstance;

	CIndexList<CClientObj, MAX_CLIENT>			m_ClientObjects;
	CIndexList<ENTERGAMESERVER, MAX_ENTER>		m_EnterObjects;
	CIntHash									m_Inthash;
	CGameWorld *								m_pGameWorld = NULL;
};
