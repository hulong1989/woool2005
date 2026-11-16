// Dialogue.h  ——  NPC 对话选项（对话树）
#pragma once

#include "public.h"
#include <vector>

struct DIALOGOPTION
{
	char	szNpc[32];
	int		nIndex;
	char	szText[64];
	char	szAction[24];	//	shop / quests / teleport / close
	char	szParam[64];
};

class CDialogueMgr
{
public:
	static CDialogueMgr * GetInstance();
	BOOL	Load( const char * pszFile );
	int		GetOptions( const char * pszNpc, std::vector<DIALOGOPTION> & options );
	const DIALOGOPTION *	GetOption( const char * pszNpc, int nIndex );

private:
	static CDialogueMgr * m_pInstance;
	std::vector<DIALOGOPTION>	m_Options;
};
