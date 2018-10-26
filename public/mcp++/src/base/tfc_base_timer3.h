#ifndef _STORAGE_BASE_TIMER_H_
#define _STORAGE_BASE_TIMER_H_

#include <sys/time.h>
#include <map>

namespace tfc{ namespace base {
	
	// ������Timer�У���ʱ����Timer����delete
	class CSimpleTimerInfo
	{
	public:
		CSimpleTimerInfo(){};
		virtual ~CSimpleTimerInfo(){};
		friend class CSimpleTimerQueue;

		
		int rtti;				// ��¼�����������Ϣ���Ա�ǿ��ת������ѡ������
		unsigned ret_flow;		// ��Ҫ�ظ�CCD��flow
		unsigned ret_msg_seq;	// ��Ҫ�ظ����󷽵�seq
		unsigned ret_destid;	// ��Ҫ�ظ�����ŵ�smcdid
	public:
		// ��ʱ����timer���ã���Ҫ����������
		virtual void on_expire(){ return; };
		// ��ʱ����timer���ã����Կ���timer��ɾ��timerinfo����
		// Ĭ���ǻ�ɾ������
		virtual bool on_expire_delete(){ return true; };
	protected:
		time_t _access_time;
		time_t _gap;
		unsigned _msg_seq;
	};
	
	class CSimpleTimerQueue
	{
	public:
		CSimpleTimerQueue(){};
		virtual ~CSimpleTimerQueue(){};
		//
		// ��װtimer
		// return 0		�ɹ�
		//	      <0	ʧ��
		//
		int set(unsigned msg_seq, CSimpleTimerInfo* timer_info, time_t gap = 10 /* 10 seconds */);
		
		//
		// ���timer��Ӧ�����ݣ�����ж��timer
		// return 0		�ɹ�
		//        <0    ������
		//
		int get(unsigned msg_seq, CSimpleTimerInfo** timer_info);

		// 
		//  ���msg_seq��Ӧ�������Ƿ����
		//	return 0	����
		//		   >0	������
		//
		int exist(unsigned msg_seq);

		//
		//	ɾ����ʱ����
		//
		virtual void check_expire(time_t time_expire);
	protected:
		std::map<unsigned, CSimpleTimerInfo*> _mp_timer_info;

		//modified by regan
		std::multimap<unsigned, CSimpleTimerInfo*> _mp_access_info;

};

}}


#endif
