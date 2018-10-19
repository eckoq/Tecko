#ifndef _STORAGE_TIMER_QUEUE_H_
#define _STORAGE_TIMER_QUEUE_H_

#include <sys/time.h>
#include <map>

namespace tfc{
namespace base{
	
	// 保存在Timer中，超时后由Timer负责delete
	class CTimerInfo
	{
	public:
		CTimerInfo(){};
		virtual ~CTimerInfo(){};
		friend class CTimerQueue;
		
	public:
		void reset_time()
		{
			_access_time = time(NULL);
		};
		// 超时后由timer调用，不要做大量事务
		virtual void on_expire(){ return; };

	public:
		time_t _access_time;
		time_t _gap;
		unsigned _msg_seq;		
	};
	
	//timer 队列
	class CTimerQueue
	{
	public:
		CTimerQueue(){};
		virtual ~CTimerQueue(){};
		
		// 安装timerInfo，0 成功， <0 失败
		int set(unsigned msg_seq, CTimerInfo* timer_info, time_t gap = 10 /* 10 seconds */);
		
		// 获得timerInfo，0 成功， <0 失败
		int get(unsigned msg_seq, CTimerInfo** timer_info);

		//卸载
		int remove(unsigned msg_seq, CTimerInfo** timer_info=NULL);

		//  检查msg_seq对应的数据是否存在， 0 存在， <0 不存在
		int exist(unsigned msg_seq);

		//	删除一个超时数据
		//返回值为1， 则返回msg_seq 和 timerInfo 对象
		//返回值为0,  检查完成， 没有超时对象
		virtual int check_expire(time_t time_expire, unsigned *msg_seq, CTimerInfo** timer_info);

		//	删除超时数据, 返回个数
		virtual int check_expire();

		unsigned size(){ return _mp_timer_info.size();};
	protected:
		std::map<unsigned, CTimerInfo*> _mp_timer_info;

	};

}}


#endif
