/******************************************************************************

异步事务管理器,用来管理众多的异步事务会话

2008年1月, nekeyzhong written 

 ******************************************************************************/
 
#ifndef _ANSYTIMERMAP_HPP
#define _ANSYTIMERMAP_HPP

#include <sys/time.h>
#include <sys/types.h>
#include <map>
using namespace std;

// 保存在Timer中，超时后由Timer负责delete
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

	//pArg消息,unSeq自身seq
	virtual ssize_t OnMessage(size_t unSeq,void* pArg=NULL)=0;
	
	// 超时,return 0继续放入,!0外层删除
	virtual ssize_t OnExpire(){ return -1;};

	//设置存活超时时间ms
	void SetAliveTimeOutMs(size_t unTimeOutMs)
	{
		timeval tNow;
		gettimeofday(&tNow,NULL);
		m_ullDeadTimeMillSecs = tNow.tv_sec*(unsigned long long)1000+tNow.tv_usec/1000 + unTimeOutMs;
	}
public:
	//创建时间
	timeval m_tCreateTime;
private:
	//超时控制
	u_int64_t m_ullDeadTimeMillSecs;
	//默认存活10秒
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

