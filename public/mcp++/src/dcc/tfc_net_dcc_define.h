#ifndef TFC_NET_DCC_DEFINE_H_
#define TFC_NET_DCC_DEFINE_H_

enum dcc_reqrsp_type {
	//以下是mcd->dcc
	dcc_req_connect = 1,	//仅连接目标
	dcc_req_disconnect,		//与对方断开连接
	dcc_req_send,			//发送到对方,自动建立连接,不发送连接建立通知
	dcc_req_send_shm,		//采用共享内存分配器传输数据的dcc_req_send，由dcc释放内存
	
	//以下是dcc->mcd	
	dcc_rsp_connect_ok = 20,//通知连接成功
	dcc_rsp_connect_failed, //通知连接失败
	dcc_rsp_disconnected,	//通知连接断裂
	dcc_rsp_data,			//携带响应数据
	dcc_rsp_data_shm,		//采用共享内存分配器传输数据的dcc_rsp_data，由mcd释放内存
	dcc_rsp_send_failed,   	//通知发送失败
	dcc_rsp_overload_conn,	//连接过载通知
	dcc_rsp_overload_mem,	//内存过载通知

	// Before 2.1.11, close_nodify_details enabled
	dcc_rsp_disconnect_timeout,			//因为连接超时而被框架断开连接
	dcc_rsp_disconnect_local,			//连接被业务so主动关闭
	dcc_rsp_disconnect_peer_or_error,	//对端断开连接或者网络错误
	dcc_rsp_disconnect_overload,		//负载高导致连接断开
	dcc_rsp_disconnect_error			//连接因为错误被断开
};
typedef struct {
	unsigned int 		_ip;			//目标服务器ip
	unsigned short  	_port;			//目标服务器port
	unsigned short		_reservd;		//填充用，暂时无具体用途
	unsigned short 		_type;			//消息类型，reqrsp_type其中之一值
	unsigned short 		_arg;			//消息类型关联参数
	struct {
		time_t			_timestamp;		//消息时间戳，消息生成的时间，一般由dcc生成，mcd可根据此时间判断是否处理此包
		time_t 			_timestamp_msec;//消息时间戳micro sec
	};
}TDCCHeader;

#define DCC_HEADER_LEN		((unsigned)(sizeof(TDCCHeader)))

#endif
