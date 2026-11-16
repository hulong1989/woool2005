#pragma once
typedef struct tagITEMCLASS
{
	char	szName[32];
	BYTE	stdMode;
	BYTE	btShape;
	WORD	wImage;
	BYTE	btWeight;
	char	specialpower;
	BYTE	ac[2];
	BYTE	mac[2];
	BYTE	dc[2];
	BYTE	mc[2];
	BYTE	sc[2];
	BYTE	needtype;
	BYTE	needlevel;
	BYTE	maxdura;
	int		nPrice;
	//	重建时补充：药品字段（可选，默认 0）
	BYTE	useType;
	WORD	useValue;
}ITEMCLASS;
class CItemManager
{
public:
	CItemManager(void);
	virtual ~CItemManager(void);
	
	BOOL AddItemClassString( const char * pszItemClassDesc );

	BOOL CreateItem( const char * pszName, DWORD dwId, DWORD dwKey, DWORD dwOwner, WORD wPos, BYTE btFlag );
	//	name/stdmode/shape/image/specialpower/ac1/ac2/mac1/mac2/dc1/dc2/mc1/mc2/sc1/sc2/weight/maxdura/price/needtype/needlevel
	BOOL AddItemClass( const char * pszName, BYTE stdmode, BYTE shape, WORD wImage, char specialpower, 
		BYTE ac1, BYTE ac2, BYTE mac1, BYTE mac2, BYTE dc1, BYTE dc2, BYTE mc1, BYTE mc2, BYTE sc1, BYTE sc2, BYTE weight, 
		BYTE maxdura, int price, BYTE needtype, BYTE needlevel ){ return FALSE;}
	static CItemManager * GetInstance()
	{
		if( m_pInstance == NULL )
			m_pInstance = new CItemManager;
		return m_pInstance;
	}
	//	重建时补充：全局唯一的物品实例编号（掉落、发放、读档都走这里）
	static DWORD NewMakeIndex();
	BOOL MakeupTemplateItem( const char * pszName, ITEM & item );
	ITEMCLASS *	GetItemClassByName( const char * pszName );
	const char * GetErrorMsg() { return m_pErrorMsg;}

private:
	const char * m_pErrorMsg;
	BOOL AddItemClass( ITEMCLASS * pItemclass );
	static CItemManager * m_pInstance;
	xListHost<ITEMCLASS>	m_xItemClassList;
	CNameHash m_ItemClassHash;
};
