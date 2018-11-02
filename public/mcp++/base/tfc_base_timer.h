#ifndef _STORAGE_BASE_TIMER_H_
#define _STORAGE_BASE_TIMER_H_

#include <sys/time.h>
#include <map>

namespace tfc{ namespace base {
	
	// 保存在Timer中，超时后由Timer负责delete
	class CSimpleTimerInfo
	{
	public:
		CSimpleTimerInfo():_is_expire_delete(true){};
		virtual ~CSimpleTimerInfo(){};
		friend class CSimpleTimerQueue;
		int rtti;				// 记录子类的类型信息，以便强制转换，并选择处理函数
		unsigned ret_flow;		// 需要回复CCD的flow
		unsigned ret_msg_seq;	// 需要回复请求方的seq
		unsigned ret_destid;	// 需要回复请求放的smcdid
	public:
		// 超时后由timer调用，不要做大量事务
		virtual void on_expire(){ return; };
		// 超时的情况下，控制是否删除timerinfo对象
		virtual void set_expire_delete(bool del){_is_expire_delete = del;};
		// 超时后由timer调用，可以控制timer不删除timerinfo对象
		// 默认是会删除对象
		virtual bool on_expire_delete(){ return _is_expire_delete; };
	protected:
		time_t _access_time;
		time_t _gap;
		unsigned _msg_seq;
		bool _is_expire_delete;
	};
	
	class CSimpleTimerQueue
	{
	public:
		CSimpleTimerQueue(){};
		virtual ~CSimpleTimerQueue(){};
		//
		// 安装timer
		// return 0		成功
		//	      <0	失败
		//
		int set(unsigned msg_seq, CSimpleTimerInfo* timer_info, time_t gap = 10 /* 10 seconds */);
		
		//
		// 获得timer对应的数据，并且卸载timer
		// return 0		成功
		//        <0    不存在
		//
		int get(unsigned msg_seq, CSimpleTimerInfo** timer_info);

		// 
		//  检查msg_seq对应的数据是否存在
		//	return 0	存在
		//		   >0	不存在
		//
		int exist(unsigned msg_seq);
		int size();
		//
		//	删除超时数据
		//
		virtual void check_expire(time_t time_expire);
	//protected:
		std::map<unsigned, CSimpleTimerInfo*> _mp_timer_info;
};
}}


#endif
