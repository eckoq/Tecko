#ifndef TFC_NET_CCD_DEFINE_H__
#define TFC_NET_CCD_DEFINE_H__
#include <time.h>

enum ccd_reqrsp_type {
    //以下是ccd->mcd的type
    ccd_rsp_connect = 1,	//有主机连接
    ccd_rsp_disconnect,		//有主机断开连接(Also default)
    ccd_rsp_data,			//ccd发数据来
    ccd_rsp_data_shm,		//与ccd_rsp_data相同，只是通过共享内存存储数据，由mcd释放内存
    ccd_rsp_overload,		//请求过载通知
    ccd_rsp_overload_conn,	//连接过载通知
    ccd_rsp_overload_mem,	//内存过载通知
    ccd_rsp_send_ok,		//表示连接发送缓冲区的数据已经发送完毕，_arg反映了ccd当前发送速度
    ccd_rsp_send_nearly_ok,	//表示连接发送缓冲区的数据已经少于某个配置的长度，_arg反映了ccd当前发送速度

    // add in mcp++ 2.3.1, event_notify and conn_notify_details enabled
    ccd_rsp_recv_data,      //表示ccd收到了请求的数据，header后4个字节为收到数据大小
    ccd_rsp_send_data,      //表示ccd发出了回复的数据，header后4个字节为发送数据大小
    ccd_rsp_check_complete_ok,      //表示ccd包完整性检查函数成功了一次，header后4个字节为返回码
    ccd_rsp_check_complete_error,   //表示ccd包完整性检查函数失败了，header后4个字节为返回码
    ccd_rsp_cc_ok,          //表示mcd中指定的flow对应的cc存在，可以发送数据
    ccd_rsp_cc_closed,      //表示mcd中指定的flow已经失效，对应的cc不存在了
    ccd_rsp_reqdata_recved, //表示ccd已正确收到mcd的回复包，准备发送出去, header后4个字节为数据实际大小

    //以下是mcd->ccd的type
    ccd_req_data = 20,		//mcd回复业务数据给ccd
    ccd_req_data_shm,		//与ccd_req_data相同，只是通过共享内存存储数据，由ccd释放内存
    ccd_req_disconnect,		//mcd通知在数据发送完成后断开连接
    ccd_req_force_disconnect, //mcd通知强制断开连接
    ccd_req_set_dspeed,		//mcd设置下载速率，在_arg中填写速率KB/S
    ccd_req_set_uspeed,		//mcd设置上传速率，在_arg中填写速率KB/S
    ccd_req_set_duspeed,    // mcd设置上传和下载速率为同一个值，在_arg中填写速率KB/S

    // Before 2.1.11, close_nodify_details enabled
    ccd_rsp_disconnect_timeout,			//因为连接超时而被框架断开连接
    ccd_rsp_disconnect_local,			//连接被业务so主动关闭
    ccd_rsp_disconnect_peer_or_error,	//对端断开连接或者网络错误
    ccd_rsp_disconnect_overload,		//负载高导致连接断开
    ccd_rsp_disconnect_error,			//连接因为错误被断开
    // For others, use ccd_rsp_disconnect as default.

    ccd_rsp_data_udp = 60,		// CCD -> MCD
    ccd_rsp_data_shm_udp,		// CCD -> MCD

    ccd_req_data_udp = 80,		// MCD -> CCD
    ccd_req_data_shm_udp,		// MCD -> CCD

    ccd_invalid_type = 10000
};
typedef struct {
    unsigned int		_ip;			//客户端ip
    unsigned short  	_port;			//客户端port
    unsigned short		_listen_port;	//接收该连接的socket监听端口
    unsigned short		_type;			//消息类型，ccd_reqrsp_type其中之一值
    unsigned short		_arg;			//消息类型关联参数
    unsigned short      _ccd_id;        //用于MCD从多个CCD接收消息时区分消息来源
    struct {
        time_t 		_timestamp;			//消息时间戳sec，消息生成的时间，一般由ccd生成，mcd可根据此时间判断是否处理此包
        time_t 		_timestamp_msec;	//消息时间戳micro sec
    };
}TCCDHeader;

#define CCD_HEADER_LEN      ((unsigned)(sizeof(TCCDHeader)))

#endif
