#include "tfc_base_ms_timer.h"

using namespace std;
using namespace tfc::base;

int CSimpleMilliSecondTimerQueue::set(unsigned msg_seq, CSimpleMilliSecondTimerInfo* timer_info, time_t gap/*=10000ms*/)
{
	struct timeval cur_time;
	gettimeofday(&cur_time,NULL);

	timer_info->_msg_seq     = msg_seq;
	timer_info->_access_time = cur_time;
	timer_info->_gap = gap;

	_mp_timer_info[msg_seq] = timer_info;

	return 0;
}

int CSimpleMilliSecondTimerQueue::get(unsigned msg_seq, CSimpleMilliSecondTimerInfo** timer_info)
{
	map<unsigned, CSimpleMilliSecondTimerInfo*>::iterator it = _mp_timer_info.find(msg_seq);
	if (it == _mp_timer_info.end())
	{
		*timer_info = NULL;
		return -1;
	}

	*timer_info = it->second;
	_mp_timer_info.erase(it);
	
	return 0;
}

int CSimpleMilliSecondTimerQueue::reach(unsigned msg_seq, CSimpleMilliSecondTimerInfo** timer_info)
{
	map<unsigned, CSimpleMilliSecondTimerInfo*>::iterator it = _mp_timer_info.find(msg_seq);
	if (it == _mp_timer_info.end())
	{
		*timer_info = NULL;
		return -1;
	}

	*timer_info = it->second;	
	return 0;
}

int CSimpleMilliSecondTimerQueue::exist(unsigned msg_seq)
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

void CSimpleMilliSecondTimerQueue::check_expire(struct timeval& cur_time)
{
	map<unsigned, CSimpleMilliSecondTimerInfo*>::iterator it = _mp_timer_info.begin();
	while (it != _mp_timer_info.end())
	{
		CSimpleMilliSecondTimerInfo* timer_info  = it->second;

		unsigned elapsed_time = time_used(cur_time,timer_info->_access_time);
		if (elapsed_time >= (unsigned)timer_info->_gap)
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
	}
}
