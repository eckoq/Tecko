#ifndef _TCP_NET_CCONN_H_
#define _TCP_NET_CCONN_H_
#include <time.h>
#include <sys/time.h>
#include "list.h"
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include "tfc_net_raw_cache.h"

#include <stdio.h>

#define MIN_START_ADDR 4096

#define  E_NEED_CLOSE  10000
#define  E_NOT_FINDFD  10001
#define  E_NEED_SEND   10002
#define  E_NEED_RECV   10003
#define  E_FORCE_CLOSE 10004
#define  E_NEED_PENDING 10005
#define  E_MEM_ALLOC	10006
#define  E_NEED_PENDING_NOTIFY 10007

#define SEGMENT_SIZE (1<<12)

namespace tfc{namespace net
{
	// Just return string information.
	typedef const char* (*get_plugin_version)();
	typedef const char* (*get_addinfo_0)();
	typedef const char* (*get_addinfo_1)();

	/*
	 * MCD route initialize function.
	 * @msg:			Config of "root/mcd_route_init_msg" in CCD config file.
	 *					Could config some information such as another config file name.
	 * Returns:			0 on success, others on error and stop the CCD.
	 */
	typedef int (*mcd_route_init)(const char *msg);
	/*
 	* MCD route by message call back. Must named "mcd_route_func" in so file.
 	* @msg:				Network message.
 	* @msg_len:			Length of message.
 	* @flow:				Flow number.
 	* @listen_port:		Listen port which create the connection.
 	* @req_mq_cnt:		Request mq count.
 	* Returns:			Return the request mq index to enqueue when 0 <= return_value < req_mq_cnt.
 	*					Otherwise ccd will choose last MCD.
 	*/
	typedef unsigned (*mcd_route)(void *msg, unsigned msg_len, unsigned long long flow, unsigned short listen_port, unsigned req_mq_cnt);
	typedef unsigned (*mcd_pre_route)(unsigned ip, unsigned short port, unsigned short listen_port, unsigned long long flow, unsigned req_mq_cnt);
	typedef int (*check_complete)(void* data, unsigned data_len);
	typedef void (*close_callback)(ConnCache* cc, unsigned short event);
	
	typedef struct {
		unsigned _msg_count;					//��Ϣ��Ŀ
		unsigned long long _total_time;			//������Ϣ��ʱ��
		unsigned _max_time;						//������Ϣ���ʱ��
		unsigned _min_time;						//������Ϣ��Сʱ��
		unsigned _avg_time;						//������Ϣƽ��ʱ��
		unsigned long long _total_recv_size; 	//�����ֽ���
		unsigned long long _total_send_size;	//�����ֽ���
		unsigned _conn_num;						//��ǰ������
	}CCSStat;

	class CConnSet
	{
	public:
		CConnSet(check_complete func, unsigned max_conn, unsigned rbsize, unsigned wbsize,
			close_callback close_func = NULL, mcd_route mcd_route_func = NULL, check_complete *func_array = NULL);
		~CConnSet();
		
		ConnCache* AddConn(int fd, unsigned long long &flow);
		int Recv(ConnCache* cc);
		int SendForce(ConnCache* cc, const char* data, size_t data_len);
		int Send(ConnCache* cc, const char* data, size_t data_len);
		int SendFromCache(ConnCache* cc);
#ifdef _SHMMEM_ALLOC_
		int GetMessage(ConnCache* cc, void* buf, unsigned buf_size, unsigned& data_len, bool* is_shm_alloc);
#else			
		int GetMessage(ConnCache* cc, void* buf, unsigned buf_size, unsigned& data_len);
#endif		
		int TryCloseCC(ConnCache* cc, unsigned short event);
		void CloseCC(ConnCache* cc, unsigned short event);
		
		void CheckTimeout(time_t access_deadline);

		inline ConnCache* GetConnCache(unsigned long long flow) {
			ConnCache* cc = _flow_2_cc[flow % _flow_slot_size];
			while(cc != NULL) {
				if(cc->_flow == flow) 
					return cc;
				else
					cc = cc->_next_cc;	
			}
			return NULL;
		}

		//��¼CCD���յ���Ϣ��ʼʱ��	
		inline void StartStat(ConnCache* cc) {
			if(cc->_start_time.tv_sec == 0)
				gettimeofday(&cc->_start_time, NULL);
		}
		//��¼CCD��������Ϣ��ʱ��
		inline void EndStat(ConnCache* cc) {
			if(cc->_start_time.tv_sec != 0) {	
				struct timeval tmp;
				gettimeofday(&tmp, NULL);
				unsigned use_time = (tmp.tv_sec - cc->_start_time.tv_sec) * 1000 + (tmp.tv_usec - cc->_start_time.tv_usec) / 1000;
				if(use_time > _stat._max_time)
					_stat._max_time = use_time;
				if(use_time < _stat._min_time)
					_stat._min_time = use_time;
				_stat._total_time += use_time;		

				cc->_start_time.tv_sec = 0;
				_stat._msg_count++;
			}
		}
		inline void GetStatResult(CCSStat* stat) {
			memcpy(stat, &_stat, sizeof(CCSStat));
			if(stat->_min_time == UINT_MAX)
				stat->_min_time = 0; 
			stat->_avg_time = (stat->_msg_count > 0 ? (unsigned)(stat->_total_time / stat->_msg_count) : 0);
			
			unsigned tmp = _stat._conn_num;	
			memset(&_stat, 0x0, sizeof(CCSStat));
			_stat._conn_num = tmp;
			_stat._min_time = UINT_MAX;
		}

		inline void* BeginAddr() { return _cc_begin_addr; }
		inline void* EndAddr() { return _cc_end_addr; }
		
		void Watch(unsigned cc_timeout, unsigned cc_stattime);
#ifdef _SPEEDLIMIT_		
		void SetSpeedLimit(unsigned download_speed, unsigned upload_speed, unsigned low_buff_size);
		static inline unsigned long long GetNowTick() {
			gettimeofday(&_nowtime, NULL);
			return ((unsigned long long)_nowtime.tv_sec) * 1000000ULL + (unsigned long long)_nowtime.tv_usec;
		}

		list_head_t _pending_recv;		//�ȴ��������ݵ���������
		list_head_t _pending_send;		//�ȴ��������ݵ���������
#endif
		static inline struct timeval* GetNowTime() {
			return &_nowtime;
		}
		static inline time_t GetNowTimeSec() {
			return _nowtime.tv_sec;
		}
		static inline void GetNowTimeVal(time_t& sec, time_t& msec) {
			sec = _nowtime.tv_sec;
			msec = _nowtime.tv_usec / 1000;	
		}
		
	private:
		int SendCC(ConnCache* cc, const char* data, size_t data_len, size_t& sent_len);
		int RecvCC(ConnCache* cc, char* buff, size_t buff_size, size_t& recvd_len);
			
		ConnCache** _flow_2_cc;		//flow�����ӵ�ӳ��
		ConnCache* _ccs;			//���Ӽ�������
		list_head_t _free_ccs;		//������������
		list_head_t _used_ccs;		//��������������

		check_complete _func;
		check_complete *_func_array;	// Specific complete func array.
		mcd_route _mr_func;			// Mcd route function.
		unsigned _max_conn;			//���������
		unsigned _recv_buff_size;	//���ջ�������С
		unsigned _send_buff_size;	//���ͻ�������С
		close_callback _close_func;	//���ӹرջص�����
		
		CCSStat _stat;				//ͳ��ֵ
#ifdef _SPEEDLIMIT_		
		//���ʿ��Ʋ���	
		unsigned _download_ticks; 	//����SEGMENT_SIZE�ֽ�������Ҫ��tick��Ŀ,1tick=1/100ms
		unsigned _upload_ticks;		//����SEGMENT_SIZE�ֽ�������Ҫ��tick��Ŀ
		unsigned _low_buff_size;	//��ʾ�����ͻ�����ʣ�����ݳ������ڸ�ֵʱ֪ͨMCD����λKB
#endif
		//ȫ�ֵ�ǰʱ�䣬���еط������Ե���GetNowTime����GetNowTimeSec��ȡ�õ�ǰʱ��
		//�������ӳ�ʹ�õ�ǰʱ����࣬���Էŵ����ӳ���
		static struct timeval _nowtime;	

		void	*_cc_begin_addr;	// cc array start memory address.
		void	*_cc_end_addr;		// cc array end memory address.
		
		unsigned _flow_slot_size;	//_flow_2_cc����Ĵ�С����ֵ����_max_conn�Զ��������
	};
}}

#endif
