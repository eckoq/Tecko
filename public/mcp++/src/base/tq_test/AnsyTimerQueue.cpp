#include "AnsyTimerQueue.hpp"
#include <sys/time.h>

//����TBucketNode������
ssize_t TQ_SetNodeKey(void* pObj,void* pKey,ssize_t iKeyLen)
{
	CAnsyTimerQueue::THashNode* pHashNode = (CAnsyTimerQueue::THashNode*)pObj;
	memset(pHashNode,0,sizeof(CAnsyTimerQueue::THashNode));
	memcpy(&(pHashNode->m_unSeq),pKey,sizeof(pHashNode->m_unSeq));
	return 0;
}

//��ȡTBucketNode������
ssize_t TQ_GetNodeKey(void* pObj,void* pKey,ssize_t &iKeyLen)
{
	CAnsyTimerQueue::THashNode* pHashNode = (CAnsyTimerQueue::THashNode*)pObj;
	memcpy(pKey,&(pHashNode->m_unSeq),sizeof(pHashNode->m_unSeq));
	iKeyLen = sizeof(ssize_t);
	return 0;
}

CAnsyTimerQueue::CAnsyTimerQueue(ssize_t iTimerNum/*=100000*/)
{
	m_pMemPtr = NULL;
	Init(iTimerNum);
}

CAnsyTimerQueue::~CAnsyTimerQueue()
{

	try
	{
		if (m_pMemPtr)
		{
			delete m_pMemPtr;
		}
	}
	catch(...)
	{
		return;
	}
	
}

//100000 =>> 3600052�ֽ�
ssize_t CAnsyTimerQueue::CountBaseSize(ssize_t iTimerNum)
{
	return CHashTab::CountMemSize(iTimerNum) +
		TIdxObjMng::CountMemSize(sizeof(THashNode),iTimerNum,3) + CObjQueue::CountMemSize();
}

ssize_t CAnsyTimerQueue::Init(ssize_t iTimerNum)
{
	ssize_t iAttachBytes=0,iAllocBytes = 0;

	ssize_t iMemSize = CountBaseSize(iTimerNum);

	if (m_pMemPtr)
	{
		delete []m_pMemPtr;
	}
	m_pMemPtr = new char[iMemSize];

	//��������-----------------------------------
	iAttachBytes = m_stObjHashTab.AttachMem(m_pMemPtr,iMemSize,iTimerNum);
	if (iAttachBytes < 0)
		return -3;
	iAllocBytes += iAttachBytes;

	//hash��Ҫ1����,m_stExpireQueue ��Ҫ2��
	iAttachBytes = m_stObjMng.AttachMem(m_pMemPtr+iAllocBytes,iMemSize-iAllocBytes,sizeof(THashNode),iTimerNum,emInit,3);
	if (iAttachBytes < 0)
		return -4;
	iAllocBytes += iAttachBytes;
	
	if(m_stObjHashTab.AttachIdxObjMng(&m_stObjMng,TQ_SetNodeKey,TQ_GetNodeKey))
	{
		return -5;
	}

	//���������
	iAttachBytes = m_stExpireQueue.AttachMem(m_pMemPtr+iAllocBytes, iMemSize-iAllocBytes,emInit);
	if (iAttachBytes < 0)
		return -6;
	iAllocBytes += iAttachBytes;
	
	if(m_stExpireQueue.AttachIdxObjMng(&m_stObjMng))
	{
		return -7;
	}

	gettimeofday(&m_tNow,NULL);
	return 0;
}

ssize_t CAnsyTimerQueue::Set(size_t unSeq,CTimerInfo* pTimerInfo)
{
	if (!pTimerInfo)
		return -1;
	
	ssize_t iObjIdx = -1;
	//����ɾ��
	THashNode* pHashNode = (THashNode*)m_stObjHashTab.GetObjectByKey(&unSeq,sizeof(size_t),iObjIdx);
	if (pHashNode)
	{
		delete Take(unSeq);
	}

	pHashNode = (THashNode*)m_stObjHashTab.CreateObjectByKey(&unSeq,sizeof(size_t),iObjIdx);
	if (!pHashNode)
		return -2;	

	//renew timeout
	if(pTimerInfo->m_unTimeOutMs <= 0)
	{
		assert(0);
	}	
	pTimerInfo->m_ullDeadTimeMillSecs = m_tNow.tv_sec*(unsigned long long)1000+m_tNow.tv_usec/1000 + pTimerInfo->m_unTimeOutMs;

	pHashNode->m_pTimerInfo = pTimerInfo;

	/*
	ʱ�����ϲ���,������ʱ������,һ���,�½ڵ���������ڵ�,
	���ԴӺ���ǰ��
	*/
	
	ssize_t iCurrIdx = m_stExpireQueue.GetTailItem();
	THashNode*  pCurrHashNode = (THashNode*)m_stObjMng.GetAttachObj(iCurrIdx);
	if (pCurrHashNode && (pCurrHashNode->m_pTimerInfo->m_ullDeadTimeMillSecs <= 
				pTimerInfo->m_ullDeadTimeMillSecs))
	{
		m_stExpireQueue.AppendToTail(iObjIdx);
		return 0;
	}
	
	while (pCurrHashNode && 
			(pCurrHashNode->m_pTimerInfo->m_ullDeadTimeMillSecs > pTimerInfo->m_ullDeadTimeMillSecs))
	{
		ssize_t iPrevIdx = 0;
		m_stExpireQueue.GetPrevItem(iCurrIdx,iPrevIdx);
		iCurrIdx = iPrevIdx;

		pCurrHashNode =  (THashNode*)m_stObjMng.GetAttachObj(iCurrIdx);
	}

	if (!pCurrHashNode)
	{
		m_stExpireQueue.PushToHead(iObjIdx);
	}
	else
	{
		m_stExpireQueue.InsertAfter(iCurrIdx,iObjIdx);
	}
	return 0;
}

//����
CTimerInfo* CAnsyTimerQueue::Take(size_t unSeq)
{
	ssize_t iObjIdx = -1;
	//���򴴽�,���򷵻�
	THashNode* pHashNode = (THashNode*)m_stObjHashTab.GetObjectByKey(&unSeq,sizeof(unSeq),iObjIdx);
	if (!pHashNode)
	{
		return NULL;	
	}

	CTimerInfo* pRtTimerInfo =  pHashNode->m_pTimerInfo;

	m_stExpireQueue.DeleteItem(iObjIdx);
	m_stObjHashTab.DeleteObjectByKey(&unSeq,sizeof(unSeq));
	return pRtTimerInfo;
}

CTimerInfo* CAnsyTimerQueue::Get(size_t unSeq)
{
	ssize_t iObjIdx = -1;
	//���򴴽�,���򷵻�
	THashNode* pHashNode = (THashNode*)m_stObjHashTab.GetObjectByKey(&unSeq,sizeof(unSeq),iObjIdx);
	if (!pHashNode)
	{
		return NULL;	
	}

	return  pHashNode->m_pTimerInfo;
}

ssize_t CAnsyTimerQueue::TimeTick(timeval *ptval/*=NULL*/)
{
	if (ptval)
	{
		memcpy(&m_tNow,ptval,sizeof(timeval));
	}
	else
	{
		gettimeofday(&m_tNow,NULL);
	}
	
	unsigned long long ullNowMillSecs = m_tNow.tv_sec*(unsigned long long)1000+m_tNow.tv_usec/1000;

	ssize_t iExpireCnt = 0;
	/*
	ʱ�����ϱ���,�ڵ㰴��ʱʱ������,�ڵ�Aû�е���,������
	���нڵ�Ҳ���ᵽ��
	*/
	THashNode* pCurrHashNode = NULL;
	ssize_t iCurrIdx = m_stExpireQueue.GetHeadItem();	
	while (iCurrIdx >= 0)
	{
		ssize_t iNextIdx = 0;
		m_stExpireQueue.GetNextItem(iCurrIdx,iNextIdx);

		pCurrHashNode =  (THashNode*)m_stObjMng.GetAttachObj(iCurrIdx);

		//��ʱ����
		if (pCurrHashNode->m_pTimerInfo->m_ullDeadTimeMillSecs <= ullNowMillSecs)
		{
			iExpireCnt++;
			ssize_t iRet = pCurrHashNode->m_pTimerInfo->OnExpire();
			if(iRet == 0)
			{
				CTimerInfo* pTimerInfo = Take(pCurrHashNode->m_unSeq);
				Set(pCurrHashNode->m_unSeq,pTimerInfo);
			}
			else
				delete Take(pCurrHashNode->m_unSeq);
		}
		else
		{
			break;
		}
		
		iCurrIdx = iNextIdx;
	}	

	return iExpireCnt;
}

void CAnsyTimerQueue::Print(FILE *fpOut)
{
	ssize_t iCurrIdx = m_stExpireQueue.GetHeadItem();	
	while (iCurrIdx >= 0)
	{
		ssize_t iNextIdx = 0;
		m_stExpireQueue.GetNextItem(iCurrIdx,iNextIdx);

		THashNode* pCurrHashNode =  (THashNode*)m_stObjMng.GetAttachObj(iCurrIdx);

		fprintf(fpOut,"IDX%lld[SEQ %llu,TIME %llu] ",(long long)iCurrIdx,(unsigned long long)pCurrHashNode->m_unSeq,
			(unsigned long long)pCurrHashNode->m_pTimerInfo->m_ullDeadTimeMillSecs);

		iCurrIdx = iNextIdx;
	}	
}
	
//---------------------------------------------
/*
class CTransInfo : public  CTimerInfo
{
public:
	CTransInfo(){};
	virtual ~CTransInfo(){};
	
public:
	virtual ssize_t OnMessage(size_t unSeq,void* pArg)
	{
		SetAliveTimeOutMs(8000);
		printf("OnMessage unSeq=%d\n",unSeq);
	};
	
	// ��ʱ,�����ɾ��
	virtual ssize_t OnExpire()
	{
		timeval ttt;
		gettimeofday(&ttt,NULL);
		printf("\n Expired! BirthTime %d.%d,now %d.%d\n",
					m_tCreateTime.tv_sec,m_tCreateTime.tv_usec,
					ttt.tv_sec,ttt.tv_usec);
		return -1;
	};
protected:
};


 #include <unistd.h>
main()
{
	int32_t iseq = -4545454;
	
	CAnsyTimerQueue mAnsyTimerQueue(1000);

	printf("%d\n",CAnsyTimerQueue::CountBaseSize(100000));
	printf("%d\n",CAnsyTimerQueue::CountBaseSize(200000));
	printf("%d\n",CAnsyTimerQueue::CountBaseSize(500000));

	CTransInfo* pTransInfo = new CTransInfo;
	pTransInfo->SetAliveTimeOutMs(1000);

	mAnsyTimerQueue.Set(iseq, (CTimerInfo *)pTransInfo);

	CTransInfo* pTransInfo2 = (CTransInfo*)mAnsyTimerQueue.Take(iseq);

	pTransInfo = new CTransInfo;
	pTransInfo->SetAliveTimeOutMs(2000);
	mAnsyTimerQueue.Set(iseq++, (CTimerInfo *)pTransInfo);

	pTransInfo = new CTransInfo;
	pTransInfo->SetAliveTimeOutMs(3000);
	mAnsyTimerQueue.Set(iseq++, (CTimerInfo *)pTransInfo);

	CTimerInfo* pTimerInfo = mAnsyTimerQueue.Take(iseq-1);
	size_t unnn;
	pTimerInfo->OnMessage(iseq-1,NULL);
	mAnsyTimerQueue.Set(iseq-1,pTimerInfo);

//mAnsyTimerQueue.Print(stdout);
	while(1)
	{
		usleep(1000);
		int32_t iExpireCnt = mAnsyTimerQueue.TimeTick();

		if (iExpireCnt)
			{
				mAnsyTimerQueue.Print(stdout);
				//break;
			}
	}
	
}
*/

