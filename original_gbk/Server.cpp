#include "StdAfx.h"
#include ".\server.h"
#include "gameworld.h"
CServer *CServer::m_pInstance = NULL;

CServer::CServer(void)
{
	m_pInstance = this;
}

CServer::~CServer(void)
{
}

CClientObject * CServer::NewClientObject()
{
	CClientObj * pObj;
	UINT id = m_ClientObjects.New(&pObj);
	if( id == 0 || pObj == NULL )return NULL;
	pObj->Clean();
	pObj->SetId(id);
	return pObj;
}

VOID CServer::DeleteClientObject( CClientObject * pObject )
{
	UINT id = pObject->GetId();
	m_ClientObjects.Del(id);
}

BOOL CServer::InitServer(CSettingFile&s)
{
	m_pGameWorld = CGameWorld::GetInstance();
	return m_pGameWorld->Init();
}

VOID CServer::CleanServer()
{

}

VOID CServer::Update()
{
	CClientObj * pObject = m_ClientObjects.First();
	while( pObject )
	{
		pObject->Update();
		pObject = m_ClientObjects.Next();
	}
	
	UpdateSCServer();
	UpdateDBServer();

	m_pGameWorld->Update();

}

VOID CServer::OnDBMsg( PMIRMSG pMsg, int datasize )
{

}

VOID CServer::OnMASMsg( WORD wCmd, WORD wType, WORD wIndex, const char * pszData, int datasize )
{
	switch( wCmd )
	{
	case	MAS_ENTERGAMESERVER:
		{
			ENTERGAMESERVER * pEnterInfo = (ENTERGAMESERVER*)pszData;
			UINT id = pEnterInfo->nClientId;

			pEnterInfo->result = AddEnterAccount( pEnterInfo->nLoginId, pEnterInfo->nSelCharId, pEnterInfo->szAccount );
			m_SCClientObj.SendMsgAcrossServer( id, MAS_ENTERGAMESERVER, MST_SINGLE, wIndex, pszData, datasize );
		}
		break;
	}
}

VOID CServer::OnSCMsg(PMIRMSG pMsg, int datasize )
{
	switch( pMsg->wCmd )
	{
	case	SCM_FINDSERVER:
		{
			if( pMsg->wParam[0] == SE_OK )
			{
				SERVERADDR * pAddr = (SERVERADDR*)pMsg->data;
				m_pIoConsole->OutPut( RGB(255,0,0), "找到了服务器地址 %s:%d\n", pAddr->addr, pAddr->nPort );
			}
			else
				m_pIoConsole->OutPut( RGB(255,0,0), "没有找到服务器 %s 的地址!\n", pMsg->data );
		}
		break;
	default:
		CBaseServer::OnSCMsg(pMsg, datasize );
		break;
	}
}

VOID CServer::OnSCServerReady()
{
	//CSCClientObj * pObj = (CSCClientObj *)GetSCConnection();
	//if( pObj )
	//{
	//	pObj->FindServer( 0, ST_SELCHARSERVER, "淡抹夕阳" );
	//}
}

SERVER_ERROR CServer::AddEnterAccount( UINT nLoginId, UINT nSelCharId, const char * pszAccount )
{
	ENTERGAMESERVER	*	pEnterInfo = NULL;
	UINT id = m_EnterObjects.New( &pEnterInfo);
	if( id == 0 || pEnterInfo == NULL )return SE_SERVERFULL;
	if( !m_Inthash.HAdd( nLoginId, id ) )
	{
		m_EnterObjects.Del(id);
		return SE_SERVERFULL;
	}
	pEnterInfo->nClientId = id;
	pEnterInfo->nLoginId = nLoginId;
	pEnterInfo->nSelCharId = nSelCharId;
	pEnterInfo->dwEnterTime = timeGetTime();
	strncpy( pEnterInfo->szAccount, pszAccount, 10 );
	pEnterInfo->szAccount[10] = 0;
	return SE_OK;
}

BOOL CServer::GetEnterInfo( UINT nLoginId, UINT nSelCharId, const char * pszAccount, ENTERGAMESERVER & enterinfo )
{
	UINT id = (UINT)m_Inthash.HGet(nLoginId);
	if( id == 0 )return FALSE;
	ENTERGAMESERVER * pEnterInfo = m_EnterObjects.Get(id);
	if( pEnterInfo == NULL )return FALSE;
	if( pEnterInfo->nLoginId != nLoginId || pEnterInfo->nSelCharId != nSelCharId )return FALSE;
	enterinfo = *pEnterInfo;
	m_EnterObjects.Del(id);
	m_Inthash.HDel(nLoginId);
	return TRUE;
}

