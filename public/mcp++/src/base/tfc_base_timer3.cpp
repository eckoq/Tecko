#include "tfc_base_timer3.h"
#include <list>

using namespace std;
using namespace tfc::base;

int CSimpleTimerQueue::set(unsigned msg_seq, CSimpleTimerInfo* timer_info, time_t gap/* = 10 seconds */)
{
	time_t cur_time = time(NULL);
	// for cancel(TFC_HANDLE);
	timer_info->_msg_seq = msg_seq;
	timer_info->_access_time = cur_time;
	timer_info->_gap = gap;
	_mp_timer_info[msg_seq] = timer_info;

	// Modified by Regan
	unsigned expire_time = cur_time + gap;
	_mp_access_info.insert(std::multimap<unsigned, CSimpleTimerInfo*>::value_type(expire_time, timer_info));
            
	return 0;
}

int CSimpleTimerQueue::get(unsigned msg_seq, CSimpleTimerInfo** timer_info)
{
	map<unsigned, CSimpleTimerInfo*>::iterator it = _mp_timer_info.find(msg_seq);
	if (it == _mp_timer_info.end())
	{
		*timer_info = NULL;
		return -1;
	}
	*timer_info = it->second;
	_mp_timer_info.erase(it);

	// Modified by Regan
	std::multimap<unsigned, CSimpleTimerInfo*>::iterator it_ai;
	for(it_ai=_mp_access_info.find((*timer_info)->_access_time + (*timer_info)->_gap)
			; it_ai!=_mp_access_info.end()
			; it_ai++)
	{
		if(it_ai->second->_msg_seq == msg_seq)
		{
			_mp_access_info.erase(it_ai);
			break; 
		}
	}
     
	return 0;
}

int CSimpleTimerQueue::exist(unsigned msg_seq)
{
	if (_mp_timer_info.find(msg_seq) != _mp_timer_info.end())
	{
		return 0;
	}
	else
	{
		return 1;
	}
}

void CSimpleTimerQueue::check_expire(time_t time_expire)
{
/*	time_t cur_time = time(NULL);
	map<unsigned, CSimpleTimerInfo*>::iterator it = _mp_timer_info.begin();
	while (it != _mp_timer_info.end())
	{
		CSimpleTimerInfo *timer_info  = it->second;
		        
		if ((cur_time - timer_info->_access_time) > timer_info->_gap)
		{
		    timer_info->on_expire();
			// 控制是否delete超时的对象
			if (timer_info->on_expire_delete() == true)
			{
				delete timer_info;
			}
			_mp_timer_info.erase(it++);
		}
		else
		{
			it++;
		}
	}*/
	
	// Modified by Regan
	time_t cur_time = time(NULL);
	list<unsigned> vec_seq;
	list<unsigned> vec_time;

	std::multimap<unsigned, CSimpleTimerInfo*>::iterator it_ai;
	for(it_ai=_mp_access_info.begin(); it_ai!=_mp_access_info.end(); it_ai++)
	{
		CSimpleTimerInfo *timer_info  = it_ai->second;
		if(cur_time > (int)(it_ai->first) )
		{
			vec_time.push_back(timer_info->_access_time);
			vec_seq.push_back(timer_info->_msg_seq);
		}
		else
		{
			break;
		}
	}

	list<unsigned>::iterator it;
	std::map<unsigned, CSimpleTimerInfo*>::iterator it_ti;

	//erase from seq map        
	for(it=vec_seq.begin(); it!=vec_seq.end(); it++)
	{
		it_ti=_mp_timer_info.find(*it);
		if(it_ti != _mp_timer_info.end())
		{
			CSimpleTimerInfo *timer_info  = it_ti->second;
			timer_info->on_expire();
			// 控制是否delete超时的对象
			if (timer_info->on_expire_delete() == true)
			{
				delete timer_info;
			}                
			_mp_timer_info.erase(it_ti);
		}
	}

	//erase from access time map
	for(it=vec_time.begin(); it!=vec_time.end(); it++)
		_mp_access_info.erase(*it);

}
