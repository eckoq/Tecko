#include "tfc_base_timer2.h"

using namespace std;
using namespace tfc::base;


int CTimerQueue::set(unsigned msg_seq, CTimerInfo* timer_info, time_t gap/* = 10 seconds */)
{
	time_t cur_time = time(NULL);
	// for cancel(TFC_HANDLE);
	timer_info->_msg_seq = msg_seq;
	timer_info->_access_time = cur_time;
	timer_info->_gap = gap;
	_mp_timer_info[msg_seq] = timer_info;
	return 0;
}

int CTimerQueue::get(unsigned msg_seq, CTimerInfo** timer_info)
{
	map<unsigned, CTimerInfo*>::iterator it = _mp_timer_info.find(msg_seq);
	if (it == _mp_timer_info.end())
	{
		*timer_info = NULL;
		return -1;
	}
	*timer_info = it->second;
	return 0;
}

int CTimerQueue::remove(unsigned msg_seq, CTimerInfo** timer_info)
{
	map<unsigned, CTimerInfo*>::iterator it = _mp_timer_info.find(msg_seq);
	if (it == _mp_timer_info.end())
	{
		return -1;
	}

	if(timer_info != NULL)
	{
		*timer_info = it->second;
	}
	else
	{
		delete it->second;
	}

	_mp_timer_info.erase(it);
	return 0;
}

int CTimerQueue::exist(unsigned msg_seq)
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

int CTimerQueue::check_expire(time_t time_expire, unsigned *msg_seq, CTimerInfo** timer_info)
{
	time_t cur_time = time(NULL);
	map<unsigned, CTimerInfo*>::iterator it = _mp_timer_info.begin();
	while (it != _mp_timer_info.end())
	{
		CTimerInfo *info  = it->second;
		if ((cur_time - info->_access_time) >info->_gap)
		{
		    info->on_expire();

			*msg_seq = (unsigned)it->first;
			*timer_info = info;

			_mp_timer_info.erase(it);
			return 1;
		}
		else
		{
			it++;
		}
	}

	return 0;
}

//	É¾³ý³¬Ê±Êý¾Ý
int CTimerQueue::check_expire()
{
	int count = 0;
	time_t cur_time = time(NULL);
	map<unsigned, CTimerInfo*>::iterator it = _mp_timer_info.begin();
	while (it != _mp_timer_info.end())
	{
		CTimerInfo *info  = it->second;
		if ((cur_time - info->_access_time) >info->_gap)
		{
		    info->on_expire();
			_mp_timer_info.erase(it++);
			count ++;
			delete info;
		}
		else
		{
			it++;
		}
	}

	return count;
}
