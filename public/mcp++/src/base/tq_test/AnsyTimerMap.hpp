/******************************************************************************

�첽���������,���������ڶ���첽����Ự

2008��1��, nekeyzhong written 

 ******************************************************************************/
 
#ifndef _ANSYTIMERMAP_HPP
#define _ANSYTIMERMAP_HPP

#include <sys/time.h>
#include <sys/types.h>
#include <map>
using namespace std;

// ������Timer�У���ʱ����Timer����delete
namespace timer_map 
{
class CTimerInfo
{
public:
	CTimerInfo()
	{
		gettimeofday(&m_tCreateTime,NULL);
		m_ullDeadTimeMillSecs = m_tCreateTime.tv_sec*(unsigned long long)1000+
							m_tCreateTime.tv_usec/1000 + DEFAULT_ALIVE_TIMEOUT;
	};
	virtual ~CTimerInfo(){};
	
	virtual ssize_t Init(void* pArg1,void* pArg2){return 0;};

	//pArg��Ϣ,unSeq����seq
	virtual ssize_t OnMessage(size_t unSeq,void* pArg=NULL)=0;
	
	// ��ʱ,return 0��������,!0���ɾ��
	virtual ssize_t OnExpire(){ return -1;};

	//���ô�ʱʱ��ms
	void SetAliveTimeOutMs(size_t unTimeOutMs)
	{
		timeval tNow;
		gettimeofday(&tNow,NULL);
		m_ullDeadTimeMillSecs = tNow.tv_sec*(unsigned long long)1000+tNow.tv_usec/1000 + unTimeOutMs;
	}
public:
	//����ʱ��
	timeval m_tCreateTime;
private:
	//��ʱ����
	u_int64_t m_ullDeadTimeMillSecs;
	//Ĭ�ϴ��10��
	static const ssize_t DEFAULT_ALIVE_TIMEOUT = 10*1000;

	friend class CAnsyTimerMap;
};

class CAnsyTimerMap
{
public:	
	CAnsyTimerMap(){m_TimerInfoMap.clear();};
	~CAnsyTimerMap(){};

	ssize_t TimeTick(timeval *ptval=NULL);
	
	ssize_t Set(size_t unSeq,CTimerInfo* pTimerInfo);
	CTimerInfo* Take(size_t unSeq);
	CTimerInfo* Get(size_t unSeq);
	
	void Print( FILE *fpOut );

	size_t GetCount(){return m_TimerInfoMap.size();}
private:

	std::map<size_t, CTimerInfo*> m_TimerInfoMap;
};
}

#endif

