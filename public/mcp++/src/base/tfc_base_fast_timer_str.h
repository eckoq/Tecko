#ifndef _TFC_BASE_FAST_TIMER_STR_H_
#define _TFC_BASE_FAST_TIMER_STR_H_
#include <sys/time.h>
#include <time.h>
#include <string>
#include "../ccd/list.h"
#define MAP_SIZE			(1<<19)
#define TQ_STR_KEY_LEN		4

using namespace std;

namespace tfc{ namespace base {
	
	class CFastTimerStrInfo;

	typedef struct {
		unsigned long long 	_expire_time;	//到期时间,ms
		list_head_t	   		_next;
		list_head_t	   		_snext;
		CFastTimerStrInfo* 	_info;
		string				_key;			//这是定时器的唯一标识
	} StrInfoBroker;
	class CFastTimerStrInfo
	{
	public:
		CFastTimerStrInfo() 
		{
			INIT_LIST_HEAD(&_broker._next);
			INIT_LIST_HEAD(&_broker._snext);
			_broker._info = this;
		}
		virtual ~CFastTimerStrInfo(){};

		friend class CFastTimerQueueStr;
		
		unsigned ret_flow;		// 需要回复CCD的flow
		unsigned ret_msg_seq;	// 需要回复请求方的seq
		unsigned ret_destid;	// 需要回复请求放的smcdid

	public:
		virtual void on_expire(){ return; };
		virtual bool on_expire_delete(){ return true; };

	protected:
		StrInfoBroker		_broker;
	};
	
	class CFastTimerQueueStr
	{
	public:
		CFastTimerQueueStr()
		{
			_timer_map = new list_head_t[MAP_SIZE];
			for(int i = 0; i < MAP_SIZE; ++i)
				INIT_LIST_HEAD(&_timer_map[i]);	
			INIT_LIST_HEAD(&_timer_list);
			_count = 0;
			_nowtime = (unsigned long long)(time(0) + 1) * 1000;
		}
		virtual ~CFastTimerQueueStr()
		{
			if(_timer_map)
				delete [] _timer_map;
		}
		int set(string &key, CFastTimerStrInfo* timer_info, time_t gap = 10000 /* 10 seconds */);
		int get(string &key, CFastTimerStrInfo** timer_info);
		int reach(string &key, CFastTimerStrInfo** timer_info);
		int exist(string &key);
		void check_expire(struct timeval& cur_time);
		int size() { return _count;}	
	protected:
		void insert_sortlist(StrInfoBroker* broker);
		unsigned str_2_hashidx(string &key)
		{
			char hash_key[TQ_STR_KEY_LEN];

			memset(hash_key, 0, TQ_STR_KEY_LEN);
			strncpy(hash_key, key.c_str(), TQ_STR_KEY_LEN);

			return ((*((unsigned *)hash_key)) & ((unsigned)(MAP_SIZE - 1)));
		}
		
		list_head_t*		_timer_map;
		list_head_t			_timer_list;
		unsigned long long 	_nowtime;
		int 				_count;
	};

}}
#endif //_TFC_BASE_FAST_TIMER_STR_H_
