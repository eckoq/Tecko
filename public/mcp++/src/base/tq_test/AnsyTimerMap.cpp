#include "AnsyTimerMap.hpp"
#include <sys/time.h>

using namespace timer_map;

ssize_t CAnsyTimerMap::Set(size_t unSeq,CTimerInfo* pTimerInfo)
{
	if (!pTimerInfo)
		return -1;

	delete Take(unSeq);
	
	m_TimerInfoMap[unSeq] = pTimerInfo;
	return 0;
}

//拿走
CTimerInfo* CAnsyTimerMap::Take(size_t unSeq)
{
	map<size_t, CTimerInfo*>::iterator it = m_TimerInfoMap.find(unSeq);
	if (it == m_TimerInfoMap.end())
	{
		return NULL;
	}
	CTimerInfo *pTimerInfo = it->second;
	m_TimerInfoMap.erase(it);
	return pTimerInfo;
}

CTimerInfo* CAnsyTimerMap::Get(size_t unSeq)
{
	map<size_t, CTimerInfo*>::iterator it = m_TimerInfoMap.find(unSeq);
	if (it == m_TimerInfoMap.end())
	{
		return NULL;
	}
	CTimerInfo *pTimerInfo = it->second;
	return pTimerInfo;
}

ssize_t CAnsyTimerMap::TimeTick(timeval *ptval/*=NULL*/)
{
	timeval tval;
	if (ptval)
	{
		memcpy(&tval,ptval,sizeof(timeval));
	}
	else
	{
		gettimeofday(&tval,NULL);
	}
	
	unsigned long long ullNowMillSecs = tval.tv_sec*(unsigned long long)1000+tval.tv_usec/1000;

	int iExpireCnt = 0;
	map<size_t, CTimerInfo*>::iterator it = m_TimerInfoMap.begin();
	while (it != m_TimerInfoMap.end())
	{
		size_t unSeq = it->first;
		CTimerInfo *pTimerInfo  = it->second;
		
		if (pTimerInfo->m_ullDeadTimeMillSecs <= ullNowMillSecs)
		{
			it++;
			iExpireCnt++;
			ssize_t iRet = pTimerInfo->OnExpire();
			if(iRet == 0)
				Set(unSeq,Take(unSeq));
			else
				delete Take(unSeq);
		}
		else
		{
			it++;
		}
	}

	return iExpireCnt;
}

void CAnsyTimerMap::Print(FILE *fpOut)
{
	map<size_t, CTimerInfo*>::iterator it = m_TimerInfoMap.begin();
	while (it != m_TimerInfoMap.end())
	{
		size_t unSeq = it->first;
		CTimerInfo *pTimerInfo  = it->second;

		fprintf(fpOut,"[%lu,%lu] ",unSeq,pTimerInfo->m_ullDeadTimeMillSecs);

		it++;
	}	
}


#if 0
//---------------------------------------------

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
	
	// 超时,由外层删除
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
	size_t iseq = 1;
	
	CAnsyTimerMap mAnsyTimerQueue;

	CTransInfo* pTransInfo = new CTransInfo;
	pTransInfo->SetAliveTimeOutMs(2000);
	mAnsyTimerQueue.Set(iseq++, (CTimerInfo *)pTransInfo);

	pTransInfo = new CTransInfo;
	pTransInfo->SetAliveTimeOutMs(2000);
	mAnsyTimerQueue.Set(iseq++, (CTimerInfo *)pTransInfo);

	pTransInfo = new CTransInfo;
	pTransInfo->SetAliveTimeOutMs(2000);
	mAnsyTimerQueue.Set(iseq++, (CTimerInfo *)pTransInfo);

	pTransInfo = new CTransInfo;
	pTransInfo->SetAliveTimeOutMs(2000);
	mAnsyTimerQueue.Set(iseq++, (CTimerInfo *)pTransInfo);

	/*
	CTimerInfo* pTimerInfo = mAnsyTimerQueue.Take(iseq-1);
	size_t unnn;
	pTimerInfo->OnMessage(iseq-1,NULL);
	mAnsyTimerQueue.Set(iseq-1,pTimerInfo);
	*/

//mAnsyTimerQueue.Print(stdout);
	while(1)
	{
		usleep(1000000);
		size_t iExpireCnt = mAnsyTimerQueue.TimeTick();

		if (iExpireCnt)
			{
				mAnsyTimerQueue.Print(stdout);
				//break;
			}
	}
	
}
#endif

