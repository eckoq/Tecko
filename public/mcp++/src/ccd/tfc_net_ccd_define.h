#ifndef TFC_NET_CCD_DEFINE_H__
#define TFC_NET_CCD_DEFINE_H__
#include <time.h>

enum ccd_reqrsp_type {
	//������ccd->mcd��type
	ccd_rsp_connect = 1,	//����������
	ccd_rsp_disconnect,		//�������Ͽ�����(Also default)
	ccd_rsp_data,			//ccd��������
	ccd_rsp_data_shm,		//��ccd_rsp_data��ͬ��ֻ��ͨ�������ڴ�洢���ݣ���mcd�ͷ��ڴ�
	ccd_rsp_overload,		//�������֪ͨ
	ccd_rsp_overload_conn,	//���ӹ���֪ͨ
	ccd_rsp_overload_mem,	//�ڴ����֪ͨ
	ccd_rsp_send_ok,		//��ʾ���ӷ��ͻ������������Ѿ��������
	ccd_rsp_send_nearly_ok,	//��ʾ���ӷ��ͻ������������Ѿ�����ĳ�����õĳ���
		
	//������mcd->ccd��type
	ccd_req_data = 20,		//mcd�ظ�ҵ�����ݸ�ccd
	ccd_req_data_shm,		//��ccd_req_data��ͬ��ֻ��ͨ�������ڴ�洢���ݣ���ccd�ͷ��ڴ�
	ccd_req_disconnect,		//mcd֪ͨ�����ݷ�����ɺ�Ͽ�����
	ccd_req_force_disconnect, //mcd֪ͨǿ�ƶϿ�����
	ccd_req_set_dspeed,		//mcd�����������ʣ���_arg����д����KB/S
	ccd_req_set_uspeed,		//mcd�����ϴ����ʣ���_arg����д����KB/S

// Before 2.1.11, close_nodify_details enabled
	ccd_rsp_disconnect_timeout,			//��Ϊ���ӳ�ʱ������ܶϿ�����
	ccd_rsp_disconnect_local,			//���ӱ�ҵ��so�����ر�
	ccd_rsp_disconnect_peer_or_error,	//�Զ˶Ͽ����ӻ����������
	ccd_rsp_disconnect_overload,		//���ظߵ������ӶϿ�
	ccd_rsp_disconnect_error			//������Ϊ���󱻶Ͽ�
	// For others, use ccd_rsp_disconnect as default.
};
typedef struct {
    unsigned int		_ip;			//�ͻ���ip
    unsigned short  	_port;			//�ͻ���port
	unsigned short		_listen_port;	//���ո����ӵ�socket�����˿�
    unsigned short		_type;			//��Ϣ���ͣ�ccd_reqrsp_type����֮һֵ
	unsigned short		_arg;			//��Ϣ���͹�������
	struct {
		time_t 		_timestamp;			//��Ϣʱ���sec����Ϣ���ɵ�ʱ�䣬һ����ccd���ɣ�mcd�ɸ��ݴ�ʱ���ж��Ƿ���˰�
		time_t 		_timestamp_msec;	//��Ϣʱ���micro sec
	};
}TCCDHeader;

#define CCD_HEADER_LEN      ((unsigned)(sizeof(TCCDHeader)))

#endif
