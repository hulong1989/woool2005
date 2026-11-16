#pragma	once
template <class T>
class xListHost
{
public:
//	template <class T>
	class xListNode
	{
		xListNode * m_pNext;
		xListNode * m_pPrev;
		xListHost<T> * m_pHost;
		T * m_pObject;
	public:
		xListNode( T * pObject ) : 
		  m_pObject(pObject),m_pNext(NULL),m_pPrev(NULL),m_pHost(NULL)
		{

		}
		~xListNode()
		{
			m_pNext = NULL;
			m_pPrev = NULL;
			m_pObject = NULL;
		}
		xListNode * getNext(){ return m_pNext;}
		xListNode * getPrev(){ return m_pPrev;}
		xListHost<T> * getHost(){ return m_pHost;}
		T * getObject(){ return m_pObject;}
		void	setNext( xListNode * pNext ){ m_pNext = pNext;}
		void	setPrev( xListNode * pPrev ){ m_pPrev = pPrev;}
		void	setHost( xListHost<T> * pHost ){ m_pHost = pHost;}
		BOOL	Leave()
		{
			if( m_pHost == NULL )return FALSE;
			return m_pHost->removeNode( this );
		}
		BOOL	Enter( xListHost<T> * pHost )
		{
			return pHost->addNode( this );
		}
		BOOL	BelongTo( xListHost<T> * pHost )
		{
			return ( pHost == m_pHost );
		}
	};
//	template <class T>
	class xEventListener
	{
	public:
		virtual void OnAddNode( xListHost<T> * pHost, xListNode * pNode ) = 0;
		virtual void OnRemoveNode( xListHost<T> * pHost, xListNode * pNode ) = 0;
	};


private:
	xListNode * m_pHead;
	int	m_iNodeCount;
	xEventListener * m_pEventListener;
public:
	xListHost(xEventListener*pListener) : m_pHead(NULL),m_iNodeCount(0),m_pEventListener(pListener){/*assert(pListener!=NULL);*/}
	~xListHost(){}
	xEventListener * getListener(){return m_pEventListener;}
	xListNode * getHead(){ return m_pHead;}

	BOOL	removeNode( xListNode * pNode )
	{ 
		if( pNode == NULL )return FALSE;
		if( pNode->getHost() != this )return FALSE;

		xListNode * pNext = pNode->getNext();
		xListNode * pPrev = pNode->getPrev();
		if( pNext != NULL )pNext->setPrev(pPrev);
		if( pPrev != NULL )pPrev->setNext(pNext);

		if( m_pHead == pNode )
			m_pHead = pNext;
		pNode->setNext(NULL);
		pNode->setPrev(NULL);
		pNode->setHost(NULL);
		//OnRemove(pNode);
		if( m_pEventListener )m_pEventListener->OnRemoveNode(this,pNode);
		m_iNodeCount--;
		assert(m_iNodeCount>=0);
		return TRUE;
	}
	BOOL	addNode( xListNode * pNode )
	{
		if( pNode == NULL )return FALSE;
		pNode->Leave();
		pNode->setHost(this);
		pNode->setPrev(NULL);
		pNode->setNext(m_pHead);
		if( m_pHead != NULL )
			m_pHead->setPrev(pNode);
		m_pHead = pNode;
		if( m_pEventListener )m_pEventListener->OnAddNode(this,pNode);
		m_iNodeCount++;
		return TRUE;
	}

	int	getCount(){ return m_iNodeCount;}

//public:
//	virtual VOID OnAdd( xListNode<T> * pNode ){}
//	virtual VOID OnRemove( xListNode<T> * pNode ){}
};