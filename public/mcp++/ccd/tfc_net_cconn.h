#ifndef _TCP_NET_CCONN_H_
#define _TCP_NET_CCONN_H_

#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "list.h"
#include "tfc_net_raw_cache.h"
#include "tfc_load_grid.h"
#include "log_type.h"
#include "stat.h"
#include "fastmem.h"
#include "common/clock.h"

#define  E_NEED_CLOSE          10000
#define  E_NOT_FINDFD          10001
#define  E_NEED_SEND           10002
#define  E_NEED_RECV           10003
#define  E_FORCE_CLOSE         10004
#define  E_NEED_PENDING        10005
#define  E_MEM_ALLOC	       10006
#define  E_NEED_PENDING_NOTIFY 10007
#define  E_RECVED              10009	// For UDP recive.
#define  E_TRUNC               10010	// For UDP recive.
#define  E_SEND_COMP           10011	// For UDP send.

#define MIN_START_ADDR 4096
#define SEGMENT_SIZE   (1<<12)
#define SMALL_SEG_SIZE (1<<10)

namespace app{
class ServerNetHandler;
class ClientNetHandler;
}

namespace tfc{ namespace net{

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
	typedef int (*mcd_route_init)(const char* msg);
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
    typedef void (*send_notify_2_mcd_call_back)(int type, ConnCache* cc, unsigned short arg, const char *data, unsigned int data_len);

	typedef int (*sync_request)(void* data, unsigned data_len, void **user);
	typedef int (*sync_response)(char *outbuff, unsigned buff_max, void *user);

	typedef struct
    {
		unsigned _msg_count;					//消息数目
		unsigned long long _total_time;			//处理消息总时间
		unsigned _max_time;						//处理消息最大时间
		unsigned _min_time;						//处理消息最小时间
		unsigned _avg_time;						//处理消息平均时间
		unsigned long long _total_recv_size; 	//接收字节数
		unsigned long long _total_send_size;	//发送字节数
		unsigned _conn_num;						//当前连接数
		unsigned _mq_msg_count; 				//取管道消息次数
		unsigned long long _mq_wait_total;		//消息在管道中总时间
		unsigned _mq_wait_max;  				//消息在管道中最大时间
		unsigned _mq_wait_min;					//消息在管道中最小时间
		unsigned _overload;                     //ccd 发生过载的次数

        uint32_t check_complete_err_count;      // error count while checking package complete
        uint32_t mq_msg_timeout_count;          // timeout msg
        uint32_t cc_timeout_close_count;        // cc timeout close count
        uint32_t recv_buff_max_count;			// connections closed because recv_buff_max reach
        uint32_t send_buff_max_count;			// connections closed because send_buff_max reach
        uint32_t cc_mem_overload_close_count;   // connections closed because of memory overload
	}CCSStat;

	typedef struct
    {
		unsigned			_msglen;			// Not include this header.
		struct sockaddr_in	_addr;
		struct timeval		_tm;
	}CUDPSendHeader;

	class CConnSet
	{
	public:
		CConnSet(
            app::ServerNetHandler* ccd_net_handler,
            app::ClientNetHandler* dcc_net_handler,
            unsigned max_conn,
            unsigned rbsize,
            unsigned wbsize,
            close_callback close_func = NULL,
            send_notify_2_mcd_call_back send_notify_2_mcd = NULL);
		~CConnSet();

		ConnCache* AddConn(int fd, unsigned long long &flow, int type = cc_tcp);
		int  TryCloseCC(ConnCache* cc, unsigned short event);
		void CloseCC(ConnCache* cc, unsigned short event);

		int Recv(ConnCache* cc);
		int RecvUDP(ConnCache* cc, char* buff, size_t buff_size, size_t& recvd_len,
					struct sockaddr &from, socklen_t &fromlen);

		int SendForce(ConnCache* cc, const char* data, size_t data_len);
		int Send(ConnCache* cc, const char* data, size_t data_len);
		int SendFromCache(ConnCache* cc);
		int SendUDP(ConnCache* cc, const char* data, size_t data_len,
					const struct sockaddr &to, socklen_t tolen);
		int SendFromCacheUDP(ConnCache* cc);

#ifdef _SHMMEM_ALLOC_
		int GetMessage(ConnCache* cc, void* buf, unsigned buf_size, unsigned& data_len, bool* is_shm_alloc);
#else
		int GetMessage(ConnCache* cc, void* buf, unsigned buf_size, unsigned& data_len);
#endif

		void CheckTimeout(time_t access_deadline);

		inline ConnCache* GetConnCache(unsigned long long flow)
        {
			ConnCache* cc = _flow_2_cc[flow % _flow_slot_size];
			while(NULL != cc)
            {
				if(cc->_flow == flow)
				{
				    return cc;
                }
				else
				{
				    cc = cc->_next_cc;
                }
			}

			return NULL;
		}

		//记录CCD接收到消息开始时间
		inline void StartStat(ConnCache* cc)
		{
			if(0 == cc->_start_time.tv_sec)
			{
                cc->_start_time = *GetMonotonicNowTime();
            }
		}

		//记录CCD处理完消息的时间
		inline void EndStat(ConnCache* cc, const sockaddr_in *add_in=NULL)
		{
			if(cc->_start_time.tv_sec != 0)
            {
				struct timeval tmp = *GetMonotonicNowTime();

				unsigned use_time = (tmp.tv_sec - cc->_start_time.tv_sec) * 1000 + (tmp.tv_usec - cc->_start_time.tv_usec) / 1000;
				if(use_time > _stat._max_time)
				{
				    _stat._max_time = use_time;
                }

                if(use_time < _stat._min_time)
				{
				    _stat._min_time = use_time;
                }

                _stat._total_time += use_time;

				cc->_start_time.tv_sec = 0;
				_stat._msg_count++;

                UpdateDelay(cc, add_in, use_time);
			}
		}

		inline void OverLoadStat()
		{
			_stat._overload++;
		}

        void UpdateDelay(const ConnCache *cc, const sockaddr_in *add_in, uint32_t use_time);

        inline void IncMQMsgTimeoutCount() {
            _stat.mq_msg_timeout_count++;
        }

        inline void ResetStat() {
            _stat._mq_msg_count = 0;
            _stat._mq_wait_total = 0;
            _stat._mq_wait_max = 0;
            _stat._mq_wait_min = UINT_MAX;

  			_stat._msg_count = 0;
			_stat._total_time = 0;
			_stat._max_time = 0;
			_stat._min_time = UINT_MAX;
			_stat._avg_time = 0;
			_stat._total_recv_size = 0;
			_stat._total_send_size = 0;

            _stat.check_complete_err_count = 0;
            _stat.mq_msg_timeout_count = 0;
            _stat.cc_timeout_close_count = 0;
			_stat.recv_buff_max_count = 0;
			_stat.send_buff_max_count = 0;
			_stat._overload           =0;
            // _stat._conn_num = 0; // do not reset the conn_num
        }

		inline void GetStatResult(CCSStat* stat)
        {
			memcpy(stat, &_stat, sizeof(CCSStat));

			if(stat->_min_time == UINT_MAX)
			{
			    stat->_min_time = 0;
            }

			stat->_avg_time = (stat->_msg_count > 0 ? (unsigned)(stat->_total_time / stat->_msg_count) : 0);
  		}

		inline void GetMqStat(tools::CCDStatInfo* stat) {
            if(_stat._mq_wait_min == UINT_MAX) {
                _stat._mq_wait_min = 0;
            }
            // 消息延时
            stat->mq_wait_avg = (_stat._mq_msg_count > 0 ? (unsigned)(_stat._mq_wait_total / _stat._mq_msg_count) : 0);       // average mq wait time
            stat->mq_wait_min = _stat._mq_wait_min;       // minimum mq wait time, from ccsstat
            stat->mq_wait_max = _stat._mq_wait_max;       // maximum mq wait time, from ccsstat
            stat->mq_wait_total = _stat._mq_wait_total;     // 消息在管道中总时间
            // Mq消息延时、管道统计
            stat->max_time = _stat._max_time;          // 处理消息最大时间
            stat->min_time = _stat._min_time;          // 处理消息最小时间
            stat->avg_time = (_stat._msg_count > 0 ? (unsigned)(_stat._total_time / _stat._msg_count) : 0);          // 处理消息平均时间
            stat->total_time = _stat._total_time;        // 处理消息总时间
            stat->mq_msg_timeout_count = _stat.mq_msg_timeout_count;   // msg timeout while in mq count.
            stat->mq_msg_count = _stat._mq_msg_count;      // 取管道消息次数
            // 连接统计
            stat->conn_num = _stat._conn_num;          // connection number, from ccsstat
            stat->msg_count = _stat._msg_count;         // 消息数目
            // stat->load;              // load
            stat->complete_err_count = _stat.check_complete_err_count; // complete function check error count.
            stat->total_recv_size = _stat._total_recv_size;   // 接收字节数
            stat->total_send_size = _stat._total_send_size;   // 发送字节数
            stat->cc_timeout_close_count = _stat.cc_timeout_close_count;
            // 其它
            // stat->is_ccd;
            // stat->sample_gap;        // above data is sampling during gap seconds.

  		}

		inline void* BeginAddr() { return _cc_begin_addr; }
		inline void* EndAddr() { return _cc_end_addr; }

		inline unsigned GetSendBufferSize(ConnCache* cc) { return cc->_w->data_len(); }

		void Watch(unsigned cc_timeout, unsigned cc_stattime, tfc::base::CLoadGrid* pload = NULL);
        void HandleMemOverload(long long delta_bytes);
#ifdef _SPEEDLIMIT_
		void SetSpeedLimit(unsigned download_speed, unsigned upload_speed, unsigned low_buff_size);

		static inline struct timeval* GetNowTick()
		{
            _monotonic_clock_nowtime = tools::GET_MONOTONIC_CLOCK();
            _wall_clock_nowtime = tools::GET_WALL_CLOCK();
			return &_monotonic_clock_nowtime;
		}

		inline unsigned get_send_speed(ConnCache *cc)
		{
            return (cc->_set_send_speed? cc->_set_send_speed: _config_send_speed) << 10;
		}

		inline unsigned get_recv_speed(ConnCache *cc)
		{
			return (cc->_set_recv_speed ? cc->_set_recv_speed : _config_recv_speed) << 10;
		}

		inline bool is_send_speed_cfg(ConnCache *cc)
		{
			return (cc->_set_send_speed != 0 || _config_send_speed != 0);
		}

		list_head_t _pending_recv;		//等待接收数据的连接链表
		list_head_t _pending_send;		//等待发送数据的连接链表
#endif
		static inline struct timeval* GetMonotonicNowTime()
		{
			return &_monotonic_clock_nowtime;
		}
        static inline struct timeval* GetWallNowTime() {
            return &_wall_clock_nowtime;
        }

		static inline time_t GetMonotonicNowTimeSec()
        {
			return _monotonic_clock_nowtime.tv_sec;
		}

        static inline time_t GetWallNowTimeSec() {
            return _wall_clock_nowtime.tv_sec;
        }

		static inline void GetMonotonicNowTimeVal(time_t& sec, time_t& msec)
        {
			sec = _monotonic_clock_nowtime.tv_sec;
			msec = _monotonic_clock_nowtime.tv_usec / 1000;
		}

        static inline void GetWallNowTimeVal(time_t& sec, time_t& msec) {
            sec = _wall_clock_nowtime.tv_sec;
            msec = _wall_clock_nowtime.tv_usec / 1000;
        }

        static inline void GetHeaderTimestamp(time_t& sec, time_t& msec, bool use_monotonic) {
            if (use_monotonic) {
                CConnSet::GetMonotonicNowTimeVal(sec, msec);
            } else {
                CConnSet::GetWallNowTimeVal(sec, msec);
            }
        }

        // check mq wait time
        inline unsigned CheckWaitTime(time_t ts_sec, time_t ts_msec, const struct timeval& now)
        {
            unsigned tdiff = 0;
            if (ts_sec)
            {
                time_t sec = now.tv_sec;
                time_t msec = now.tv_usec / 1000;
                long time_diff = (sec - ts_sec) * 1000 + (msec - ts_msec);
                if (time_diff < 0)
                {
                    time_diff = 0;
                }
                tdiff = time_diff;
            }

            if(tdiff > _stat._mq_wait_max)
            {
                _stat._mq_wait_max = tdiff;
            }
            if(tdiff < _stat._mq_wait_min)
            {
                _stat._mq_wait_min = tdiff;
            }
            _stat._mq_msg_count++;
            _stat._mq_wait_total += tdiff;
            return tdiff;
        }

	private:
		int SendCC(ConnCache* cc, const char* data, size_t data_len, size_t& sent_len);
		int RecvCC(ConnCache* cc, char* buff, size_t buff_size, size_t& recvd_len);
		int SendCCUDP(ConnCache* cc, const char* data, size_t data_len, size_t& sent_len,
							const struct sockaddr &to, socklen_t tolen);
		int RecvCCUDP(ConnCache* cc, char* buff, size_t buff_size, size_t& recvd_len,
							struct sockaddr &from, socklen_t &fromlen);

		int GetCcdFlow(unsigned long long &flow);

    private:
		ConnCache** _flow_2_cc;		//flow到连接的映射
		ConnCache* _ccs;			//连接集合数组
		list_head_t _free_ccs;		//空闲连接链表
		list_head_t _used_ccs;		//试用中连接链表

		close_callback  _close_func;	//连接关闭回调函数
        send_notify_2_mcd_call_back _send_notify_2_mcd; //通知mcd的回调函数

		unsigned _max_conn;			//最大连接数
		unsigned _recv_buff_size;	//接收缓冲区大小
		unsigned _send_buff_size;	//发送缓冲区大小

		CCSStat _stat;				//统计值

        // CAUTION: _monotonic_clock_nowtime is initialized by clock_gettime(CLOCK_MONOTONIC)
        // _wall_clock_nowtime is initialized by gettimeofday()
		//全局当前时间，所有地方都可以调用GetNowTime或者GetNowTimeSec来取得当前时间
		//由于连接池使用当前时间最多，所以放到连接池里
		static struct timeval _monotonic_clock_nowtime;
        static struct timeval _wall_clock_nowtime;

#ifdef _SPEEDLIMIT_
		//速率控制参数
		unsigned _config_send_speed; 	// 发送速度(KB/s)
		unsigned _config_recv_speed;	//接收速度(KB/s)
		unsigned _low_buff_size;	//表示当发送缓冲区剩余数据长度少于该值时通知MCD，单位KB
#endif

		void*   _cc_begin_addr;	    // cc array start memory address.
		void*   _cc_end_addr;		// cc array end memory address.

		unsigned _flow_slot_size;	//_flow_2_cc数组的大小，该值根据_max_conn自动计算出来

        // for handling memory overload
        ConnCache** _normal_ccs;
        ConnCache** _enlarged_ccs;

    public:
        // for dcc remote info stat
        tools::RemoteStatInfo _remote_stat_info;
        app::ServerNetHandler* _ccd_net_handler;
        app::ClientNetHandler* _dcc_net_handler;
	};
}}

#endif  //_TCP_NET_CCONN_H_
///////////////////////////////////////////////////////////////
//:~
