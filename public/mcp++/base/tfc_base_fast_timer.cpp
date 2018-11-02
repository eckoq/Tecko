#include <unistd.h>
#include <stddef.h>
#include "tfc_base_fast_timer.h"

using namespace std;
using namespace tfc::base;

int CFastTimerQueue::set(unsigned msg_seq, CFastTimerInfo* timer_info, time_t gap/*=10000ms*/) {

	list_head_t* head = &_timer_map[msg_seq & (MAP_SIZE - 1)];
	CFastTimerInfo* old_info = NULL;
	InfoBroker* broker;
	list_for_each_entry(broker, head, _next) {
		if(broker->_msg_seq == msg_seq) {
			old_info = broker->_info;
			break;	
		}
	}
	
	if(!old_info) {
		timer_info->_broker._expire_time = _nowtime + gap;
		timer_info->_broker._msg_seq     = msg_seq;
		list_add_tail(&timer_info->_broker._next, head);
		insert_sortlist(&timer_info->_broker);
		++_count;
	}
	else if(old_info == timer_info){
		broker->_expire_time = _nowtime + gap;
		insert_sortlist(broker);
	}
	else {
		//bug? how to do with it?
	}
	
	return 0;
}

int CFastTimerQueue::get(unsigned msg_seq, CFastTimerInfo** timer_info) {
	list_head_t* head = &_timer_map[msg_seq & (MAP_SIZE - 1)];
	InfoBroker* broker;
	list_for_each_entry(broker, head, _next) {
		if(broker->_msg_seq == msg_seq) {
			*timer_info = broker->_info;
			list_del_init(&broker->_next);
			list_del_init(&broker->_snext);
			--_count;
			return 0;
		}
	}
	*timer_info = NULL;	
	return -1;
}

int CFastTimerQueue::reach(unsigned msg_seq, CFastTimerInfo** timer_info) {
	list_head_t* head = &_timer_map[msg_seq & (MAP_SIZE - 1)];
	InfoBroker* broker;
	list_for_each_entry(broker, head, _next) {
		if(broker->_msg_seq == msg_seq) {
			*timer_info = broker->_info;
			return 0;
		}
	}
	*timer_info = NULL;	
	return -1;
}

int CFastTimerQueue::exist(unsigned msg_seq) {
	list_head_t* head = &_timer_map[msg_seq & (MAP_SIZE - 1)];
	InfoBroker* broker;
	list_for_each_entry(broker, head, _next) {
		if(broker->_msg_seq == msg_seq) {
			return 0;
		}
	}
	return -1;
}

//这里要确保check_expire方法以<10ms粒度被调用，否则_nowtime无法及时更新会导致定时判断不准确
//如果check_expire方法没有以<10ms粒度被调用，那么表示调用者对定时判断的准确性要求并不高...
void CFastTimerQueue::check_expire(struct timeval& cur_time) {
	CFastTimerInfo* info;
	InfoBroker* broker;
	list_head_t* tmp;
	unsigned long long expire_time = (unsigned long long)cur_time.tv_sec * 1000 + cur_time.tv_usec / 1000;
	list_for_each_entry_safe_l(broker, tmp, &_timer_list, _snext) {
		if(broker->_expire_time <= expire_time) { 
			info = broker->_info;
			info->on_expire();
			if(info->on_expire_delete()) {
				list_del(&broker->_next);
				list_del(&broker->_snext);
				delete info;
				--_count;
			}
		}
		else
			break;
	}
	_nowtime = expire_time;
}
void CFastTimerQueue::insert_sortlist(InfoBroker* broker) {
	
	InfoBroker* tmp;
	list_head_t* pos;
	list_for_each_prev(pos, &_timer_list) {
		tmp = list_entry(pos, InfoBroker, _snext);
		if(broker->_expire_time >= tmp->_expire_time) {
			//list_move_tail(&broker->_snext, &tmp->_snext);
			list_del_init(&broker->_snext);
			list_add(&broker->_snext, &tmp->_snext);
			return;	
		}
	}

	list_move(&broker->_snext, &_timer_list);
}
