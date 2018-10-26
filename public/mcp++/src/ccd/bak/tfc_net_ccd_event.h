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
		_send_event = 0;				//������
	};

	CCCDEvent(	CFifoSyncMQ* req_mq,	//��Ϣ�ܵ�
				bool send_event,		//�Ƿ���������Ϣ�� mq);
				unsigned send_nearly_ok_threshold);
	
	~CCCDEvent(){};

	//������Ϣ�� ���� ccd_rsp_data
	void SendEvent(	ccd_reqrsp_type type,
					unsigned ip,
					unsigned port,
					unsigned flow,
					unsigned other);

	//������Ϣ�� ���� ccd_rsp_data
	void SendEvent(	ccd_reqrsp_type type,
					int fd,
					unsigned flow,
					unsigned other);

	//����������Ϣ
	int SendEvent(	CFifoSyncMQ* req_mq,
					TCCDEventHeader* header_and_data,	//ͷ�������ݵ�buf
					unsigned data_len,		//���ݵĳ���
					ccd_reqrsp_type type,
					int fd,
					unsigned flow,
					unsigned other);

	inline unsigned GetOtherValue(ccd_reqrsp_type type)
	{
		return 0;
	}

public:
	CFifoSyncMQ* _req_mq;		//����mcd ������ܵ�
	bool _send_event;
	unsigned _send_nearly_ok_threshold;	//when send cache less than threshold(byte), send send_nearlly_ok event to mq
	
	//��Ϣ��
	TCCDEventHeader _event_header;
	struct sockaddr_in _address;
	socklen_t _address_len;
	timeval time_val;
};

}}//end of tfc::net

#endif
