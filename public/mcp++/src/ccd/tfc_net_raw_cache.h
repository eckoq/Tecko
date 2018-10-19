
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

			char* _data;				//����buffer
			unsigned _size;				//buffer����
			unsigned _len;				//ʵ�����ݳ���
			unsigned _offset;			//ʵ������ƫ����
			unsigned _buff_size;		//��ʼbuffer����
		};

		struct ConnCache {
			list_head_t _next;
			
			unsigned long long  _flow;			//����flow��
			int _fd;							//�Զ�socket���
			unsigned _ip;						//�Զ�ip
			unsigned short _port;				//�Զ˶˿�
			unsigned short _listen_port;		//�����Ӷ�Ӧ��listen_skt�ķ���˿�
			time_t _access;						//����ʱ���
			timeval _start_time;				//����ʼʱ��
			union {
				struct {
					unsigned _finclose:1;		//�Ƿ��ڴ������֮��ر����ӣ�1-�رգ����򲻹ر�
					unsigned _connstatus:5;		//����������״̬
					unsigned _reqmqidx:5;		//��ʾ�����ӵ����󱻷��͵���mq��index
				};
				unsigned _flag;
			};
#ifdef _SPEEDLIMIT_
			unsigned _du_ticks;					//�ϴ�������һƬ����������Сticks��Ŀ
			unsigned long long _deadline_tick;	//�ϴ����������ٵĵ���tick
#endif							
			struct CRawCache _r;				//����
			struct CRawCache _w;				//д��
		
			struct ConnCache* _next_cc;					//��dcc�е�cc����flow��ϣ��ͻ��ʱ����������ָ��
		};
	}
}

//////////////////////////////////////////////////////////////////////////
#endif//_TFC_NET_RAW_CACHE_H_
///:~
