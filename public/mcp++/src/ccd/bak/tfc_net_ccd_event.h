#ifndef TFC_NET_CCD_EVENT_H__
#define TFC_NET_CCD_EVENT_H__

#include <netinet/in.h>
#include <sys/socket.h>
#include "tfc_net_open_mq.h"
#include "tfc_net_ccd_define.h"

namespace tfc { namespace net {

class CCCDEvent
{
public:
	CCCDEvent()
	{
		_send_event = 0;				//不发送
	};

	CCCDEvent(	CFifoSyncMQ* req_mq,	//消息管道
				bool send_event,		//是否发送连接消息到 mq);
				unsigned send_nearly_ok_threshold);
	
	~CCCDEvent(){};

	//发送消息， 除了 ccd_rsp_data
	void SendEvent(	ccd_reqrsp_type type,
					unsigned ip,
					unsigned port,
					unsigned flow,
					unsigned other);

	//发送消息， 除了 ccd_rsp_data
	void SendEvent(	ccd_reqrsp_type type,
					int fd,
					unsigned flow,
					unsigned other);

	//发送数据消息
	int SendEvent(	CFifoSyncMQ* req_mq,
					TCCDEventHeader* header_and_data,	//头部和数据的buf
					unsigned data_len,		//数据的长度
					ccd_reqrsp_type type,
					int fd,
					unsigned flow,
					unsigned other);

	inline unsigned GetOtherValue(ccd_reqrsp_type type)
	{
		return 0;
	}

public:
	CFifoSyncMQ* _req_mq;		//发往mcd 的请求管道
	bool _send_event;
	unsigned _send_nearly_ok_threshold;	//when send cache less than threshold(byte), send send_nearlly_ok event to mq
	
	//消息包
	TCCDEventHeader _event_header;
	struct sockaddr_in _address;
	socklen_t _address_len;
	timeval time_val;
};

}}//end of tfc::net

#endif
