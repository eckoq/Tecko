#ifndef _STORAGE_BASE_MILLISECOND_TIMER_H_
#define _STORAGE_BASE_MILLISECOND_TIMER_H_

#include <sys/time.h>
#include <map>

namespace tfc{ namespace base {
	
	// ������Timer�У���ʱ����Timer����delete
	class CSimpleMilliSecondTimerInfo
	{
	public:
		CSimpleMilliSecondTimerInfo(){};
		virtual ~CSimpleMilliSecondTimerInfo(){};

		friend class CSimpleMilliSecondTimerQueue;
		
		int      rtti;			// ��¼�����������Ϣ���Ա�ǿ��ת������ѡ������
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
		// ��װtimer
		// return 0		�ɹ�
		//	      <0	ʧ��
		//
		int set(unsigned msg_seq, CSimpleMilliSecondTimerInfo* timer_info, time_t gap = 10000 /* 10 seconds */);
		
		//
		// ���timer��Ӧ�����ݣ�����ж��timer
		// return 0		�ɹ�
		//        <0    ������
		//
		int get(unsigned msg_seq, CSimpleMilliSecondTimerInfo** timer_info);

		//
		// ���timer��Ӧ�����ݣ��timer
		// return 0		�ɹ�
		//        <0    ������
		//
		int reach(unsigned msg_seq, CSimpleMilliSecondTimerInfo** timer_info);

		// 
		//  ���msg_seq��Ӧ�������Ƿ����
		//	return 0	����
		//		   >0	������
		//
		int exist(unsigned msg_seq);

		//
		//	ɾ����ʱ����
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
