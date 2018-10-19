#ifndef _STORAGE_BASE_MILLISECOND_TIMER_H_
#define _STORAGE_BASE_MILLISECOND_TIMER_H_

#include <sys/time.h>
#include <map>

namespace tfc{ namespace base {
	
	// 保存在Timer中，超时后由Timer负责delete
	class CSimpleMilliSecondTimerInfo
	{
	public:
		CSimpleMilliSecondTimerInfo(){};
		virtual ~CSimpleMilliSecondTimerInfo(){};

		friend class CSimpleMilliSecondTimerQueue;
		
		int      rtti;			// 记录子类的类型信息，以便强制转换，并选择处理函数
		unsigned ret_flow;		// 需要回复CCD的flow
		unsigned ret_msg_seq;	// 需要回复请求方的seq
		unsigned ret_destid;	// 需要回复请求放的smcdid

	public:
		// 超时后由timer调用，不要做大量事务
		virtual void on_expire(){ return; };

		// 超时后由timer调用，可以控制timer不删除timerinfo对象
		// 默认是会删除对象
		virtual bool on_expire_delete(){ return true; };

	protected:
		struct timeval _access_time;
		time_t         _gap;
		unsigned       _msg_seq;
	};
	
	class CSimpleMilliSecondTimerQueue
	{
	public:
		CSimpleMilliSecondTimerQueue(){};
		virtual ~CSimpleMilliSecondTimerQueue(){};
		//
		// 安装timer
		// return 0		成功
		//	      <0	失败
		//
		int set(unsigned msg_seq, CSimpleMilliSecondTimerInfo* timer_info, time_t gap = 10000 /* 10 seconds */);
		
		//
		// 获得timer对应的数据，并且卸载timer
		// return 0		成功
		//        <0    不存在
		//
		int get(unsigned msg_seq, CSimpleMilliSecondTimerInfo** timer_info);

		//
		// 获得timer对应的数据，瞭imer
		// return 0		成功
		//        <0    不存在
		//
		int reach(unsigned msg_seq, CSimpleMilliSecondTimerInfo** timer_info);

		// 
		//  检查msg_seq对应的数据是否存在
		//	return 0	存在
		//		   >0	不存在
		//
		int exist(unsigned msg_seq);

		//
		//	删除超时数据
		//
		virtual void check_expire(struct timeval& time_expire);

	protected:
		inline int time_used(struct timeval& end_time,struct timeval& begin_time)
		{
			int sec  = end_time.tv_sec  - begin_time.tv_sec;
		 	int usec = end_time.tv_usec - begin_time.tv_usec;

			int msec = (1000000*sec + usec)/1000;
			if(msec < 0)
				return 0;
			else	
				return msec;
		};

	protected:
		std::map<unsigned, CSimpleMilliSecondTimerInfo*> _mp_timer_info;
};

}}
#endif //_STORAGE_BASE_MILLISECOND_TIMER_H_
