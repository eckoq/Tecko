
#ifndef _TFC_NET_RAW_CACHE_H_
#define _TFC_NET_RAW_CACHE_H_
#include "list.h"
#include <time.h>

//////////////////////////////////////////////////////////////////////////

namespace tfc {
	namespace net {
		struct CRawCache {
			int append(const char* data, unsigned data_len);
			void skip(unsigned length);
			void reinit();
			inline char* data() {
				return _data + _offset;
			}
			inline unsigned data_len() {
				return _len;
			}

			char* _data;				//数据buffer
			unsigned _size;				//buffer长度
			unsigned _len;				//实际数据长度
			unsigned _offset;			//实际数据偏移量
			unsigned _buff_size;		//初始buffer长度
		};

		struct ConnCache {
			list_head_t _next;
			
			unsigned long long  _flow;			//连接flow号
			int _fd;							//对端socket句柄
			unsigned _ip;						//对端ip
			unsigned short _port;				//对端端口
			unsigned short _listen_port;		//该连接对应的listen_skt的服务端口
			time_t _access;						//连接时间戳
			timeval _start_time;				//请求开始时间
			union {
				struct {
					unsigned _finclose:1;		//是否在处理完成之后关闭连接，1-关闭，否则不关闭
					unsigned _connstatus:5;		//连接所处的状态
					unsigned _reqmqidx:5;		//表示该连接的请求被发送到的mq的index
				};
				unsigned _flag;
			};
#ifdef _SPEEDLIMIT_
			unsigned _du_ticks;					//上传或下载一片数据所需最小ticks数目
			unsigned long long _deadline_tick;	//上传或下载限速的到期tick
#endif							
			struct CRawCache _r;				//读池
			struct CRawCache _w;				//写池
		
			struct ConnCache* _next_cc;					//在dcc中的cc，当flow哈希冲突的时候串联起来的指针
		};
	}
}

//////////////////////////////////////////////////////////////////////////
#endif//_TFC_NET_RAW_CACHE_H_
///:~
