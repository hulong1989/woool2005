#pragma once
class CVirtualObject;

class CVirtualGameWorld
{
public:
	virtual BOOL AddObject(CVirtualObject * pObject ) = 0;
	virtual BOOL DelObject(CVirtualObject * pObject ) = 0;
	virtual CVirtualObject * GetObject( UINT nObjectId ) = 0;
	virtual CVirtualObject * NewObject() = 0;
	virtual BOOL DelObject(CVirtualObject * pObject ) = 0;
};
/*

怪物的行为
	1-	走路
	2-	攻击
	3-	转身
	4-	说话
	5-	死亡
	6-	掉物
	
	7-	魔法攻击
	8-	自动回复
	9-	魔法治愈

	10-	骑乘(高级类人怪)
	
人的行为
	1-	走路
	2-	攻击
	3-	转身
	4-	挖肉
	5-	说话
	6-	掉物
	7-	死亡
	8-	上线下线
	9-	魔法
	10-	自动回复
	11-	骑乘
	12-	组队
	13-	


抽象出的一个行为层
	1-	走路/跑步/转身
	2-	攻击
	3-	说话
	4-	扔物品
	5-	死亡
	6-	地图切换
	7-	魔法
	8-	骑乘


*/