#include <unistd.h>
#include <stddef.h>
#include "tfc_base_fast_timer_str.h"

using namespace std;
using namespace tfc::base;

int CFastTimerQueueStr::set(string &key, CFastTimerStrInfo* timer_info, time_t gap/*=10000ms*/) {
	list_head_t* head = &_timer_map[str_2_hashidx(key)];
	CFastTimerStrInfo* old_info = NULL;
	StrInfoBroker* broker;
	list_for_each_entry(broker, head, _next) {
		if(broker->_key == key) {
			old_info = broker->_info;
			break;	
		}
	}
	
	if(!old_info) {
		timer_info->_broker._expire_time = _nowtime + gap;
		try {
			timer_info->_broker._key = key;
		} catch (...) {
			return -1;
		}
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
		// KEY existed but not the same node.
		return -2;
	}
	
	return 0;
}

int CFastTimerQueueStr::get(string &key, CFastTimerStrInfo** timer_info) {
	list_head_t* head = &_timer_map[str_2_hashidx(key)];
	StrInfoBroker* broker;
	list_for_each_entry(broker, head, _next) {
		if(broker->_key == key) {
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

int CFastTimerQueueStr::reach(string &key, CFastTimerStrInfo** timer_info) {
	list_head_t* head = &_timer_map[str_2_hashidx(key)];
	StrInfoBroker* broker;
	list_for_each_entry(broker, head, _next) {
		if(broker->_key == key) {
			*timer_info = broker->_info;
			return 0;
		}
	}
	*timer_info = NULL;	
	return -1;
}

int CFastTimerQueueStr::exist(string &key) {
	list_head_t* head = &_timer_map[str_2_hashidx(key)];
	StrInfoBroker* broker;
	list_for_each_entry(broker, head, _next) {
		if(broker->_key == key) {
			return 0;
		}
	}
	return -1;
}

//����Ҫȷ��check_expire������<10ms���ȱ����ã�����_nowtime�޷���ʱ���»ᵼ�¶�ʱ�жϲ�׼ȷ
//���check_expire����û����<10ms���ȱ����ã���ô��ʾ�����߶Զ�ʱ�жϵ�׼ȷ��Ҫ�󲢲���...
void CFastTimerQueueStr::check_expire(struct timeval& cur_time) {
	CFastTimerStrInfo* info;
	StrInfoBroker* broker;
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
void CFastTimerQueueStr::insert_sortlist(StrInfoBroker* broker) {
	
	StrInfoBroker* tmp;
	list_head_t* pos;
	list_for_each_prev(pos, &_timer_list) {
		tmp = list_entry(pos, StrInfoBroker, _snext);
		if(broker->_expire_time >= tmp->_expire_time) {
			// list_move_tail(&broker->_snext, &tmp->_snext);
			list_del_init(&broker->_snext);
			list_add(&broker->_snext, &tmp->_snext);
			return;	
		}
	}

	list_move(&broker->_snext, &_timer_list);
}
