#ifndef _TFC_BASE_FAST_TIMER_H_
#define _TFC_BASE_FAST_TIMER_H_
#include <sys/time.h>
#include <time.h>
#include "../ccd/list.h"
#define MAP_SIZE	(1<<19)

namespace tfc{ namespace base {
	
	class CFastTimerInfo;

	typedef struct InfoBroker {
		unsigned       		_msg_seq;		//���Ƕ�ʱ����Ψһ��ʶ
		unsigned long long 	_expire_time;	//����ʱ��,ms
		list_head_t	   		_next;
		list_head_t	   		_snext;
		CFastTimerInfo* 	_info;	
	};
	class CFastTimerInfo
	{
	public:
		CFastTimerInfo() 
		{
			INIT_LIST_HEAD(&_broker._next);
			INIT_LIST_HEAD(&_broker._snext);
			_broker._info = this;
		}
		virtual ~CFastTimerInfo(){};

		friend class CFastTimerQueue;
		
		unsigned ret_flow;		// ��Ҫ�ظ�CCD��flow
		unsigned ret_msg_seq;	// ��Ҫ�ظ����󷽵�seq
		unsigned ret_destid;	// ��Ҫ�ظ�����ŵ�smcdid

	public:
		virtual void on_expire(){ return; };
		virtual bool on_expire_delete(){ return true; };

	protected:
		InfoBroker		_broker;
	};
	
	class CFastTimerQueue
	{
	public:
		CFastTimerQueue()
		{
			_timer_map = new list_head_t[MAP_SIZE];
			for(int i = 0; i < MAP_SIZE; ++i)
				INIT_LIST_HEAD(&_timer_map[i]);	
			INIT_LIST_HEAD(&_timer_list);
			_count = 0;
			_nowtime = (unsigned long long)(time(0) + 1) * 1000;
		}
		virtual ~CFastTimerQueue()
		{
			if(_timer_map)
				delete [] _timer_map;
		}
		int set(unsigned msg_seq, CFastTimerInfo* timer_info, time_t gap = 10000 /* 10 seconds */);
		int get(unsigned msg_seq, CFastTimerInfo** timer_info);
		int reach(unsigned msg_seq, CFastTimerInfo** timer_info);
		int exist(unsigned msg_seq);
		void check_expire(struct timeval& cur_time);
		int size() { return _count;}	
	protected:
		void insert_sortlist(InfoBroker* broker);
		
		list_head_t*		_timer_map;
		list_head_t			_timer_list;
		unsigned long long 	_nowtime;
		int 				_count;
	};

}}
#endif //_TFC_BASE_FAST_TIMER_H_
