#include "tfc_net_ccd_event.h"

using namespace tfc::net;
using namespace std;

CCCDEvent::CCCDEvent(	CFifoSyncMQ* req_mq,	//��Ϣ�ܵ�
						bool send_event,		//�Ƿ���������Ϣ�� mq);
						unsigned send_nearly_ok_threshold)
						: _req_mq(req_mq), _send_event(send_event), _send_nearly_ok_threshold(send_nearly_ok_threshold)
{
	//��Ϣ��ͷ
	//memset(&_event_header, CCD_EVENT_HEADER_LEN, 0);
	memset(&_event_header, 0, CCD_EVENT_HEADER_LEN);
	_event_header._magic_num = CCD_EVENT_MAGIC_NUM;
	
	bzero(&_address, sizeof(_address));
	_address_len = sizeof(_address);
}

//������Ϣ�� ���� ccd_rsp_data
void CCCDEvent::SendEvent(	ccd_reqrsp_type type,
							unsigned ip,
							unsigned port,
							unsigned flow,
							unsigned other)
{
	//����������Ϣ���ܵ���
	if(_send_event)
	{
		_event_header._reqrsp_type = type;
		_event_header._ip = ip;
		_event_header._port = port;
		_event_header._flow = flow;
		_event_header._other = other;
		_req_mq->enqueue(&_event_header, CCD_EVENT_HEADER_LEN, flow);
	}
}

//������Ϣ�� ���� ccd_rsp_data
void CCCDEvent::SendEvent(	ccd_reqrsp_type type,
							int fd,
							unsigned flow,
							unsigned other)
{
	//����������Ϣ���ܵ���
	if(_send_event)
	{
		getpeername(fd, (sockaddr*)&_address, &_address_len);
		_event_header._reqrsp_type = type;
		_event_header._ip = _address.sin_addr.s_addr;
		_event_header._port = _address.sin_port;
		_event_header._flow = flow;
		_event_header._other = other;
		_req_mq->enqueue(&_event_header, CCD_EVENT_HEADER_LEN, flow);
	}
}

//������Ϣ
int CCCDEvent::SendEvent(	CFifoSyncMQ* req_mq,
							TCCDEventHeader* header_and_data,	//ͷ�������ݵ�buf
							unsigned data_len,		//���ݵĳ���
							ccd_reqrsp_type type,
							int fd,
							unsigned flow,
							unsigned other)
{
	//����������Ϣ���ܵ���
	if(_send_event)
	{
		getpeername(fd, (sockaddr*)&_address, &_address_len);

		header_and_data->_magic_num = CCD_EVENT_MAGIC_NUM;
		header_and_data->_reqrsp_type = type;
		header_and_data->_ip = _address.sin_addr.s_addr;
		header_and_data->_port = _address.sin_port;
		header_and_data->_flow = flow;
		header_and_data->_other = other;
		return req_mq->enqueue(header_and_data, CCD_EVENT_HEADER_LEN + data_len, flow);
	}

	return -1;
}
