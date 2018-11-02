#ifndef _TFC_NET_RAW_CACHE_H_
#define _TFC_NET_RAW_CACHE_H_

#include "list.h"
#include <time.h>
#include <stdint.h>
#include "tfc_net_flow_monitor.h"

//////////////////////////////////////////////////////////////////////////

namespace tfc {  namespace net {
		enum
        {
			cc_tcp			= 0,
			cc_server_udp	= 1,
			cc_client_udp	= 2
		};

		enum
        {
			cc_stat_sync	= 0,
			cc_stat_data	= 1
		};

        //
        //  网络链接Buf功能封装，拥有skip和append两个基本操作
        //当buffer不足时，重新分配新Buf，将原Buf内容拷贝过去并释放老Buf
        //注意到此Buf不是circle buf实现,当头部有空间，尾部空间不够时通过
        //将有效数据拷贝到头部来实现空间碎片整理
		struct CRawCache
        {
        	virtual ~CRawCache() {}

            virtual void reinit();
            virtual void skip(unsigned length);
		    virtual int  append(const char *data0, unsigned data_len0, const char *data1, unsigned data_len1);
		    virtual int  append(const char* data, unsigned data_len);

			inline virtual void  clean_data() { _offset = 0; _len = 0; }
			inline char* data() {return _data + _offset;}
			inline unsigned data_len() {return _len;}
			inline unsigned size() {return _size;}

			virtual void add_new_msg_time(time_t sec, time_t msec) {}
			virtual bool is_msg_timeout(int expire_time_ms, const struct timeval* now) { return false; }

            int calculate_new_buffer_size(unsigned append_size);

    		unsigned _buff_size;	//数据buffer的最小长度
			char*    _data;			//数据buffer
			unsigned _size;			//数据buffer长度
			unsigned _len;			//实际数据长度
			unsigned _offset;		//实际数据偏移量
            const static int LINEAR_MALLOC_THRESHOLD = 1024 * 1024;
            const static int EXPONENT_INCREMENT_PERCENT = 10;
		};

		struct CTimeRawCache : public CRawCache
		{
			CTimeRawCache()
				: _total_bytes_cached(0),
				  _total_bytes_skip(0),
				  _head(0),
				  _tail(0) {}

			virtual void reinit()
			{
				CRawCache::reinit();
				_head = 0;
				_tail = 0;
				_total_bytes_cached = 0;
				_total_bytes_skip = 0;
			}

			virtual void  clean_data() {
				CRawCache::clean_data();
				_head = 0;
				_tail = 0;
				_total_bytes_cached = 0;
				_total_bytes_skip = 0;
			}

			virtual int  append(const char *data0, unsigned data_len0, const char *data1, unsigned data_len1)
			{
				int ret = CRawCache::append(data0, data_len0, data1, data_len1);
				if (ret != 0)
					return ret;
				_total_bytes_cached += data_len0 + data_len1;
				return 0;
			}

			virtual int  append(const char* data, unsigned data_len)
			{
				int ret = CRawCache::append(data, data_len);
				if (ret != 0)
					return ret;
				_total_bytes_cached += data_len;
				return 0;
			}

			virtual void skip(unsigned length);
			virtual void add_new_msg_time(time_t sec, time_t msec);
			virtual bool is_msg_timeout(int expire_time_ms, const struct timeval* now);

			struct CacheTimeStamp
			{
				uint64_t offset;
				time_t sec;
				time_t msec;
			};

			const static int MAX_MSG_TIMEOUT = 60; // s
			const static int INTERVAL_PER_GRID = 100;  // ms
			const static int TIME_QUEUE_LEN = MAX_MSG_TIMEOUT * 1000 / INTERVAL_PER_GRID;

			CacheTimeStamp _msg_time_queue[TIME_QUEUE_LEN];
			uint64_t _total_bytes_cached;
			uint64_t _total_bytes_skip;
			int _head;
			int _tail;
		};

		struct ConnCache
        {
			list_head_t       _next;
#ifdef _SPEEDLIMIT_
            list_head_t       _pending_send_next;
            list_head_t       _pending_recv_next;
#endif

			struct ConnCache* _next_cc;			//在dcc中的cc，当flow哈希冲突的时候串联起来的指针

			int _type;							// Connection type.
			int _smachine;						// Stat machine.
			unsigned long long  _flow;			//连接flow号
			int      _fd;						//对端socket句柄
			int		 _epoll_flag;				// 目前只在tcp连接中使用
			unsigned _ip;						//对端ip
			unsigned short _port;				//对端端口
			unsigned short _listen_port;		//该连接对应的listen_skt的服务端口

			time_t  _access;					//连接时间戳
			timeval _start_time;				//请求开始时间

            union
            {
				struct
                {
					unsigned _finclose:1;		//是否在处理完成之后关闭连接，1-关闭，否则不关闭
					unsigned _connstatus:5;		//连接所处的状态
					unsigned _reqmqidx:6;		//表示该连接的请求被发送到的mq的index
#ifdef _SPEEDLIMIT_
                    unsigned _spdlmt:1;         //是否被限速：0(未限速); 1(已限速)
#endif
				};
				unsigned _flag;
			};

			struct CRawCache* _r;				//读池
			struct CRawCache* _w;				//写池

#ifdef _SPEEDLIMIT_
			unsigned           _set_send_speed;		//mcd设置的发送速度(KB/s)
			unsigned 		   _set_recv_speed;		// mcd设置的接收速度(KB/s)
			struct CFlowMonitor _send_mon;         // 发送流量监控
			struct CFlowMonitor _recv_mon;         // 接收流量监控
#endif
		};
	}
}

//////////////////////////////////////////////////////////////////////////
#endif//_TFC_NET_RAW_CACHE_H_
///:~
