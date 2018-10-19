#ifndef _STORAGE_TIMER_QUEUE_H_
#define _STORAGE_TIMER_QUEUE_H_

#include <sys/time.h>
#include <map>

namespace tfc{
namespace base{
	
	// ������Timer�У���ʱ����Timer����delete
	class CTimerInfo
	{
	public:
		CTimerInfo(){};
		virtual ~CTimerInfo(){};
		friend class CTimerQueue;
		
	public:
		void reset_time()
		{
			_access_time = time(NULL);
		};
		// ��ʱ����timer���ã���Ҫ����������
		virtual void on_expire(){ return; };

	public:
		time_t _access_time;
		time_t _gap;
		unsigned _msg_seq;		
	};
	
	//timer ����
	class CTimerQueue
	{
	public:
		CTimerQueue(){};
		virtual ~CTimerQueue(){};
		
		// ��װtimerInfo��0 �ɹ��� <0 ʧ��
		int set(unsigned msg_seq, CTimerInfo* timer_info, time_t gap = 10 /* 10 seconds */);
		
		// ���timerInfo��0 �ɹ��� <0 ʧ��
		int get(unsigned msg_seq, CTimerInfo** timer_info);

		//ж��
		int remove(unsigned msg_seq, CTimerInfo** timer_info=NULL);

		//  ���msg_seq��Ӧ�������Ƿ���ڣ� 0 ���ڣ� <0 ������
		int exist(unsigned msg_seq);

		//	ɾ��һ����ʱ����
		//����ֵΪ1�� �򷵻�msg_seq �� timerInfo ����
		//����ֵΪ0,  �����ɣ� û�г�ʱ����
		virtual int check_expire(time_t time_expire, unsigned *msg_seq, CTimerInfo** timer_info);

		//	ɾ����ʱ����, ���ظ���
		virtual int check_expire();

		unsigned size(){ return _mp_timer_info.size();};
	protected:
		std::map<unsigned, CTimerInfo*> _mp_timer_info;

	};

}}


#endif
