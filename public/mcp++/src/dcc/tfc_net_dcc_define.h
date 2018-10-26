#ifndef TFC_NET_DCC_DEFINE_H_
#define TFC_NET_DCC_DEFINE_H_

enum dcc_reqrsp_type {
	//������mcd->dcc
	dcc_req_connect = 1,	//������Ŀ��
	dcc_req_disconnect,		//��Է��Ͽ�����
	dcc_req_send,			//���͵��Է�,�Զ���������,���������ӽ���֪ͨ
	dcc_req_send_shm,		//���ù����ڴ�������������ݵ�dcc_req_send����dcc�ͷ��ڴ�
	
	//������dcc->mcd	
	dcc_rsp_connect_ok = 20,//֪ͨ���ӳɹ�
	dcc_rsp_connect_failed, //֪ͨ����ʧ��
	dcc_rsp_disconnected,	//֪ͨ���Ӷ���
	dcc_rsp_data,			//Я����Ӧ����
	dcc_rsp_data_shm,		//���ù����ڴ�������������ݵ�dcc_rsp_data����mcd�ͷ��ڴ�
	dcc_rsp_send_failed,   	//֪ͨ����ʧ��
	dcc_rsp_overload_conn,	//���ӹ���֪ͨ
	dcc_rsp_overload_mem,	//�ڴ����֪ͨ

	// Before 2.1.11, close_nodify_details enabled
	dcc_rsp_disconnect_timeout,			//��Ϊ���ӳ�ʱ������ܶϿ�����
	dcc_rsp_disconnect_local,			//���ӱ�ҵ��so�����ر�
	dcc_rsp_disconnect_peer_or_error,	//�Զ˶Ͽ����ӻ����������
	dcc_rsp_disconnect_overload,		//���ظߵ������ӶϿ�
	dcc_rsp_disconnect_error			//������Ϊ���󱻶Ͽ�
};
typedef struct {
	unsigned int 		_ip;			//Ŀ�������ip
	unsigned short  	_port;			//Ŀ�������port
	unsigned short		_reservd;		//����ã���ʱ�޾�����;
	unsigned short 		_type;			//��Ϣ���ͣ�reqrsp_type����֮һֵ
	unsigned short 		_arg;			//��Ϣ���͹�������
	struct {
		time_t			_timestamp;		//��Ϣʱ�������Ϣ���ɵ�ʱ�䣬һ����dcc���ɣ�mcd�ɸ��ݴ�ʱ���ж��Ƿ���˰�
		time_t 			_timestamp_msec;//��Ϣʱ���micro sec
	};
}TDCCHeader;

#define DCC_HEADER_LEN		((unsigned)(sizeof(TDCCHeader)))

#endif
