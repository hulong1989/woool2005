// Skill.h  ——  技能表与技能管理
#pragma once

#include "public.h"
#include <vector>

//	类型：1 物理伤害(DC) 2 魔法伤害(MC) 3 治疗(SC) 4 道术伤害(SC)
struct SKILLTEMPLATE
{
	int		id;
	char	szName[24];
	int		nClass;
	int		nNeedLevel;
	int		nMp;
	DWORD	dwCooldown;
	int		nPower;
	int		nType;
	int		nRange;
};

class CSkillMgr
{
public:
	static CSkillMgr * GetInstance();

	BOOL	Load( const char * pszFile );
	const SKILLTEMPLATE * Get( int nSkillId );
	const SKILLTEMPLATE * FindByName( const char * pszName );
	const SKILLTEMPLATE * GetByIndex( int nIndex );
	int		GetCount();

private:
	static CSkillMgr * m_pInstance;
	std::vector<SKILLTEMPLATE>	m_Skills;
};
