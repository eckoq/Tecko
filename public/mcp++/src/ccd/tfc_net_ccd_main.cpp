#include "tfc_base_config_file.h"
#include "tfc_net_epoll_flow.h"
#include "tfc_net_open_mq.h"
#include "tfc_net_cconn.h"
#include "tfc_base_str.h"
#include "tfc_base_so.h"
#include "tfc_load_grid.h"
#include "tfc_net_ccd_define.h"
#include "tfc_net_socket_tcp.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <syslog.h>
#include <sched.h>
#include "version.h"
#include "mydaemon.h"
#include "fastmem.h"
#ifdef _SHMMEM_ALLOC_
#include "tfc_net_open_shmalloc.h"
#include "myalloc.h"
#endif
#include "tfc_base_watchdog_client.h"
#include "wtg_client_api.h"

#ifdef _DEBUG_	
#define SAY(fmt, args...) fprintf(stderr, "%s:%d,"fmt, __FILE__, __LINE__, ##args)
#else
#define SAY(fmt, args...)
#endif

#define MAX_MQ_NUM		(1<<5)				//最大支持的mq对数	
#define FD_MQ_MAP_SIZE	(1<<10)				//fd->mq 映射数组最大长度，mq'fd < FD_MQ_MAP_SIZE	
#define FD_LISTEN_MAP_SIZE	MIN_START_ADDR	//fd->listen port 映射数组最大长度, listen socket'fd < FD_LISTEN_MAP_SIZE 
using namespace tfc::base;
using namespace tfc::net;
using namespace tfc::watchdog;
using namespace tfc::wtgapi;

int is_ccd = 1;
unsigned ccd_stat_time;					//profile统计间隔时间== stat_time.

typedef struct {
	CFifoSyncMQ* _mq;					//管道
	bool _active;						//是否被epoll激活
}MQINFO;

#ifdef _OUTPUT_LOG
unsigned output_log = 0;				// 0 not output log, others output log.
#endif
#define CC_ARRAY_SIZE 65536				// Net complete function array size.
check_complete cc_func_array[CC_ARRAY_SIZE];	// Specific net complete function array.
unsigned send_buff_max = 0;				// 当该项被配置时，连接的Sendbuf数据堆积超过该值则认为出错并关闭连接，为0时不启用该检测
unsigned recv_buff_max = 0;				// 当该项被配置时，连接的Recvbuf数据堆积超过该值则认为出错并关闭连接，为0时不启用该检测
static bool event_notify = false;		//true，则CCD发送连接建立、断开等通知给MCD，否则不发送  
bool close_notify_details = false;		//启用连接关闭细节事件，event_notify启用时配置方有效
static unsigned stat_time = 60;			//profile统计间隔时间
static unsigned time_out = 60;			//连接超时时间
static unsigned max_conn = 40000;		//最大连接数
static bool defer_accept = true;		//是否启用listen socket的defer accept特性
static bool tcp_nodelay = true;			//是否启用socket的tcp_nodelay特性
static bool fetch_load_enable = true;	//是否启用远程获取ccd实时负载功能 
static bool enqueue_fail_close = false;	//当enqueue失败时是否关闭连接，启用后CCD为长连接时对MCD散列可能有影响澹
static CFifoSyncMQ* req_mq[MAX_MQ_NUM];	//ccd->mcd mq
static CFifoSyncMQ* rsp_mq[MAX_MQ_NUM];	//mcd->ccd mq
static unsigned short listenfd2port[FD_LISTEN_MAP_SIZE];// Listen socket fd -> listen port.
unsigned req_mq_num;					//req_mq数目	
static unsigned rsp_mq_num;				//rsp_mq数目
static MQINFO mq_mapping[FD_MQ_MAP_SIZE];//fd->mq映射
static int mq_mapping_min = INT_MAX;	//rsp_mq fd min
static int mq_mapping_max = 0;			//rsp_mq fd max
static CLoadGrid* pload_grid = NULL;	//负载检测
static check_complete cc_func = NULL;	//包完整性检查
static char cs_mr_init[128] = {0};		// MCD route initialize message.
static mcd_route_init	mr_init = NULL;	// MCD route initialize.
static mcd_route mr_func = NULL;		// MCD route callback.
static mcd_pre_route mpr_func = NULL;	// MCD pre route callback
static const unsigned C_TMP_BUF_LEN = (1<<27);  	//数据缓冲区，128M              
static char tmp_buffer[C_TMP_BUF_LEN];				//头部+数据的buf
static TCCDHeader* ccd_header = (TCCDHeader*)tmp_buffer;	//ccd->mcd 消息包头部
static char* _buf = (char*)tmp_buffer + CCD_HEADER_LEN; 	//ccd->mcd 消息消息体
static unsigned _BUF_LEN = C_TMP_BUF_LEN - CCD_HEADER_LEN;	//可容纳最大消息体长度
static CConnSet* ccs = NULL;			//连接池
static int epfd = -1;					//epoll句柄
static struct epoll_event* epev = NULL;	//epoll事件数组
#ifdef _SHMMEM_ALLOC_
//使用共享内存分配器存储数据的时候mq请求或回复包的格式如下：
//  TCCDHeader + mem_handle + len
//mem_handle是数据在内存分配器的地址，len是该数据的长度；且ccdheader里面_type为ccd_req_data_shm或者ccd_rsp_data_shm
//该方式减少了数据的内存拷贝，但是需要enqueue的进程分配内存，dequeue的进程释放内存
static bool enqueue_with_shm_alloc = false;	//true表示ccd->mcd使用共享内存分配器存储数据，否则不使用
static bool dequeue_with_shm_alloc = false;	//true表示mcd->ccd使用共享内存分配器存储数据，否则不使用
//在下载类业务中，往往只开启dequeue_with_shm_alloc；而在上传类业务中往往只开启enqueue_with_shm_alloc
#endif
CWatchdogClient* wdc = NULL;		//watchdog client
CWtgClientApi*   wtg = NULL;		//Wtg client
static wtg_api_base_t wdbase;		//watchdog 监控基本信息

//inline void send_notify_2_mcd(int type, unsigned long long flow, unsigned ip, unsigned short port, unsigned short listen_port, unsigned mcd_idx = 0) {
inline void send_notify_2_mcd(int type, ConnCache* cc) {	
	ccd_header->_type = type;
	CConnSet::GetNowTimeVal(ccd_header->_timestamp, ccd_header->_timestamp_msec);
	
	if(cc) {
		ccd_header->_ip = cc->_ip;
		ccd_header->_port = cc->_port;
		ccd_header->_listen_port = cc->_listen_port;
		req_mq[cc->_reqmqidx]->enqueue(tmp_buffer, CCD_HEADER_LEN, cc->_flow);
	}
	else {
		ccd_header->_ip = 0;
		ccd_header->_port = 0;
		ccd_header->_listen_port = 0;
		for(unsigned i = 0; i < req_mq_num; ++i) {
			req_mq[i]->enqueue(tmp_buffer, CCD_HEADER_LEN, 0ULL);
		}
	}
}
void handle_cc_close(ConnCache* cc, unsigned short event) {
	static TCCDHeader header;
	header._ip = cc->_ip;
	header._port = cc->_port;
	header._listen_port = cc->_listen_port;
	header._type = event;
	CConnSet::GetNowTimeVal(header._timestamp, header._timestamp_msec);
	req_mq[cc->_reqmqidx]->enqueue(&header, CCD_HEADER_LEN, cc->_flow);
}
void handle_socket_message(ConnCache* cc) {
	
	int ret;
	unsigned data_len;
	struct timeval* time_now = NULL;
#ifdef _OUTPUT_LOG
	static time_t log_time = 0;
#endif

#ifdef _SHMMEM_ALLOC_
	bool is_shm_alloc;	
#endif	
	while( !stop ) {
		data_len = 0;
#ifdef _SHMMEM_ALLOC_
		is_shm_alloc = false;
		if(enqueue_with_shm_alloc)
			ret = ccs->GetMessage(cc, _buf, _BUF_LEN, data_len, &is_shm_alloc);
		else
			ret = ccs->GetMessage(cc, _buf, _BUF_LEN, data_len, NULL);
#else
		ret = ccs->GetMessage(cc, _buf, _BUF_LEN, data_len);
#endif
		SAY("GetMessage ret=%d,data_len=%u\n", ret, data_len);
		if (ret == 0 && data_len > 0) {
			time_now = CConnSet::GetNowTime();
			ret = pload_grid->check_load(*time_now);
			
			//这是一个负载探测包, 不够优雅，硬编码了长度，且是asn协议
			//由于探测请求比较小，不会采用shm alloc方式，所以这里无需释放shm内存
			//如果配置了fetch_load为false，则不进行判断，以避免与业务数据包的冲突
			if((data_len == 10) && fetch_load_enable) {
				unsigned milliseconds;
				unsigned req_cnt;
				pload_grid->fetch_load(milliseconds, req_cnt);
				*(unsigned*)(_buf + 2) = milliseconds;
				*(unsigned*)(_buf + 6) = req_cnt;

				//如果rsp_mq没有进行互斥，这里会导致ccd和mcd可能同时enqueue同一个mq导致程序crash，
				//所以这里直接调用Send发送回包，因为回包很短，这里简化处理，不判断发送结果
				//ret = rsp_mq->enqueue(_buf, 10, flow);
				ccs->Send(cc, _buf, 10);
				break;
			}

			if (ret == CLoadGrid::LR_FULL) {
				fprintf(stderr, "loadgrid full. close.\n");
#ifdef _OUTPUT_LOG
				if ( output_log ) {
					if ( time_now->tv_sec > log_time ) {
						syslog(LOG_USER | LOG_CRIT | LOG_PID, "MCP++ CCD: Loadgrid full! Close connection.\n");
						log_time = time_now->tv_sec + 300;
					}
				}
#endif

				if(event_notify) {	
					//发送过载通知给所有MCD
					send_notify_2_mcd(ccd_rsp_overload, cc);
				}
				ccs->CloseCC(cc, ccd_rsp_disconnect_overload);
#ifdef _SHMMEM_ALLOC_				
				//此处要防止内存泄露
				if(is_shm_alloc)
					myalloc_free(*((unsigned long*)_buf));
#endif
				break;
			}

			ccd_header->_ip = cc->_ip;
			ccd_header->_port = cc->_port;
			ccd_header->_listen_port = cc->_listen_port;
#ifdef _SHMMEM_ALLOC_
			if(is_shm_alloc)
				ccd_header->_type = ccd_rsp_data_shm;
			else
#endif					
				ccd_header->_type = ccd_rsp_data;
			CConnSet::GetNowTimeVal(ccd_header->_timestamp, ccd_header->_timestamp_msec);
			ret = req_mq[cc->_reqmqidx]->enqueue(tmp_buffer, CCD_HEADER_LEN + data_len, cc->_flow);
			if(ret) {
				fprintf(stderr, "ccd enqueue failed, close, ret=%d,data_len=%u\n", ret, CCD_HEADER_LEN + data_len);
#ifdef _OUTPUT_LOG
				if ( output_log ) {
					syslog(LOG_USER | LOG_CRIT | LOG_PID, "MCP++ CCD: Enqueue fail in handle_socket_message! Close connection. ret - %d, data_len - %u, flow - %llu, ip - %u, port - %hu.\n",
						ret, CCD_HEADER_LEN + data_len, cc->_flow, cc->_ip, cc->_port);
				}
#endif
				if ( enqueue_fail_close ) {
					ccs->CloseCC(cc, ccd_rsp_disconnect_overload);
				}
#ifdef _SHMMEM_ALLOC_				
				//此处要防止内存泄露
				if(is_shm_alloc)
					myalloc_free(*((unsigned long*)_buf));
#endif
				break;
			}
		}
		else if (ret == -E_NEED_CLOSE) {
			SAY("GetMessage err, close, flow=%llu,ip=%u,port=%d\n", cc->_flow, cc->_ip, cc->_port);
#ifdef _OUTPUT_LOG
			if ( output_log ) {
				syslog(LOG_USER | LOG_CRIT | LOG_PID, "MCP++ CCD: GetMessage error in handle_socket_message! flow - %llu, ip - %u, port - %hu.\n",
					cc->_flow, cc->_ip, cc->_port);
			}
#endif
			ccs->CloseCC(cc, ccd_rsp_disconnect_error);
			break;
		}
		else {
			//ret == -E_NEED_RECV
			break;
		}
	}

	//这里要检查一下是否在Recv的时候发现对端断开连接了，如果是则关闭
	if(cc->_finclose) {
		SAY("fin close cc, flow=%llu,ip=%u,port=%d\n", cc->_flow, cc->_ip, cc->_port);
		ccs->CloseCC(cc, ccd_rsp_disconnect);					
	}
}
inline bool handle_socket_recv(ConnCache* cc) {

	//接收消息，如果发现对端断开连接但是读缓冲里还有未处理的数据则延迟关闭_finclose被设置
	int ret = ccs->Recv(cc);
	if (ret == -E_NEED_CLOSE) {
		SAY("Recv err, close, flow=%llu,ip=%u,port=%d\n", cc->_flow, cc->_ip, cc->_port);
		ccs->CloseCC(cc, ccd_rsp_disconnect_peer_or_error);
		return false;
	}
#ifdef _SPEEDLIMIT_
	else if(ret == -E_NEED_PENDING) {
		SAY("Recv pending, flow=%llu,ip=%u,port=%d\n", cc->_flow, cc->_ip, cc->_port);
		epoll_mod(epfd, cc->_fd, cc, 0);	
		return true;
	}
#endif
	else if(ret == -E_MEM_ALLOC) {
		fprintf(stderr, "alloc mem fail, %m\n");
#ifdef _OUTPUT_LOG
		if ( output_log ) {
			syslog(LOG_USER | LOG_CRIT | LOG_PID, "MCP++ CCD: Memory overload in handle_socket_recv! flow - %llu, ip - %u, port - %hu.\n",
				cc->_flow, cc->_ip, cc->_port);
		}
#endif
		if(event_notify) {
			//通知MCD内存过载了
			send_notify_2_mcd(ccd_rsp_overload_mem, NULL);
		}
		//收缩缓冲内存池
		fastmem_shrink();
		return true;
	}
	else {
#ifdef _SPEEDLIMIT_
		//这里之所以要重新加入EPOLLIN监控，原因是可能在上一次限速的时候把EPOLLIN给去掉了
		epoll_mod(epfd, cc->_fd, cc, EPOLLIN | EPOLLET);			
#endif
		return true;
	}	
}
inline bool handle_socket_send(ConnCache* cc, const char* data, unsigned data_len) {

	int ret;
	if(data) {
		ret = ccs->Send(cc, data, data_len);
		if(ret != -E_NEED_CLOSE) {
			if(ret == 0) {
				//未发送完，继续监视EPOLLOUT
				epoll_mod(epfd, cc->_fd, cc, EPOLLET | EPOLLIN | EPOLLOUT); 
				SAY("Send continue, flow=%llu,ip=%u,port=%d\n", cc->_flow, cc->_ip, cc->_port);
			}
#ifdef _SPEEDLIMIT_
			else if(ret == -E_NEED_PENDING) {
				//被限速挂起了
				//epoll_del(epfd, cc->_fd, 0, 0);	
				SAY("Send pending, flow=%llu,ip=%u,port=%d\n", cc->_flow, cc->_ip, cc->_port);
			}
#endif		
			else if(ret == -E_MEM_ALLOC) {
				fprintf(stderr, "alloc mem fail, %m\n");
#ifdef _OUTPUT_LOG
				if ( output_log ) {
					syslog(LOG_USER | LOG_CRIT | LOG_PID, "MCP++ CCD: Memory overload in handle_socket_send! flow - %llu, ip - %u, port - %hu.\n",
						cc->_flow, cc->_ip, cc->_port);
				}
#endif

				if(event_notify) {
					//通知MCD内存过载了
					send_notify_2_mcd(ccd_rsp_overload_mem, NULL);
				}
				//收缩缓冲内存池
				fastmem_shrink();
			}
			else {
				//ret==1,发送完毕
				//请求计时结束
				ccs->EndStat(cc);
				SAY("Send finished, flow=%llu,ip=%u,port=%d\n", cc->_flow, cc->_ip, cc->_port);
#ifdef _SPEEDLIMIT_
				send_notify_2_mcd(ccd_rsp_send_ok, cc);
#endif			
			}
			return true;
		}
		else {
			SAY("Send err, close, flow=%llu,ip=%u,port=%d\n", cc->_flow, cc->_ip, cc->_port);
#ifdef _OUTPUT_LOG
			if ( output_log ) {
				syslog(LOG_USER | LOG_CRIT | LOG_PID, "MCP++ CCD: Send error in handle_socket_send! Connection will be close! flow - %llu, ip - %u, port - %hu.\n",
					cc->_flow, cc->_ip, cc->_port);
			}
#endif
			ccs->CloseCC(cc, ccd_rsp_disconnect_peer_or_error);
			return false;
		}
	}
	else {
		ret = ccs->SendFromCache(cc);
		if (ret == 0) {
			//缓存发送完毕,去除EPOLLOUT
			epoll_mod(epfd, cc->_fd, cc, EPOLLIN | EPOLLET);
			ccs->EndStat(cc);
			SAY("SendFromCache finished, flow=%llu,ip=%u,port=%d\n", cc->_flow, cc->_ip, cc->_port);
#ifdef _SPEEDLIMIT_
			send_notify_2_mcd(ccd_rsp_send_ok, cc);
#endif			
			return true;
		}
		else if (ret == -E_NEED_CLOSE) {
			SAY("SendFromCache err, close, flow=%llu,ip=%u,port=%d\n", cc->_flow, cc->_ip, cc->_port);
#ifdef _OUTPUT_LOG
			if ( output_log ) {
				syslog(LOG_USER | LOG_CRIT | LOG_PID, "MCP++ CCD: SendFromCache error in handle_socket_Send! Connection will be close! flow - %llu, ip - %u, port - %hu.\n",
					cc->_flow, cc->_ip, cc->_port);
			}
#endif
			ccs->CloseCC(cc, ccd_rsp_disconnect_peer_or_error);
			return false;
		}
#ifdef _SPEEDLIMIT_
		else if(ret == -E_NEED_PENDING || ret == -E_NEED_PENDING_NOTIFY) {
			epoll_mod(epfd, cc->_fd, cc, EPOLLIN | EPOLLET);
			SAY("SendFromCache pending, flow=%llu,ip=%u,port=%d\n", cc->_flow, cc->_ip, cc->_port);
			if(ret == -E_NEED_PENDING_NOTIFY) {
				send_notify_2_mcd(ccd_rsp_send_nearly_ok, cc);	
				SAY("pending and notify, flow=%llu\n", cc->_flow);
			}
			return true;
		}
#endif
		else if(ret != -E_FORCE_CLOSE) {
#ifdef _SPEEDLIMIT_
			//由于可能上一轮发送被限速了没有监控EPOLLOUT事件，所以这轮发送如果没被限速需要重新加入EPOLLOUT监控
			epoll_mod(epfd, cc->_fd, cc, EPOLLIN | EPOLLOUT | EPOLLET); 		
#endif
			SAY("SendFromCache continue, flow=%llu,ip=%u,port=%d\n", cc->_flow, cc->_ip, cc->_port);
			return true;
		}
		else {
			SAY("SendFromCache close or force close, flow=%llu,ip=%u,port=%d\n", cc->_flow, cc->_ip, cc->_port);
			return false;
		}
	}
}
void handle_accept(int listenfd) {
	unsigned long long flow;
	static CSocketTCP sock(-1, false);
	ConnCache* cc = NULL;
	int ret;
		
	while(true) {	
		ret = ::accept(listenfd, NULL, NULL);
		if(ret >= 0) {
			cc = ccs->AddConn(ret, flow);
			if(cc == NULL) {	
				close(ret);
				fprintf(stderr, "add cc fail, %m\n");
#ifdef _OUTPUT_LOG
				if ( output_log ) {
					syslog(LOG_USER | LOG_CRIT | LOG_PID, "MCP++ CCD: Add connection fail when accept! Maybe no free connection or flow map too crowd!\n");
				}	
#endif
				if(event_notify) {
					//通知MCD连接过载了
					send_notify_2_mcd(ccd_rsp_overload_conn, NULL);
				}
				break;
			}
			sock.attach(ret);
			sock.set_nonblock(tcp_nodelay);
			epoll_add(epfd, ret, cc, EPOLLET | EPOLLIN);
			cc->_listen_port = listenfd2port[listenfd];
			
			//选择mq
			if(!mpr_func) {
				cc->_reqmqidx = flow % req_mq_num;
			}
			else {
				cc->_reqmqidx = mpr_func(cc->_ip, cc->_port, cc->_listen_port, flow, req_mq_num);
				if(cc->_reqmqidx >= req_mq_num)
					cc->_reqmqidx = 0;
			}
			SAY("Listen port %hu accept a connection.\n", cc->_listen_port);

			if(event_notify) {
				//通知MCD有新连接建立
				ccd_header->_ip = cc->_ip;
				ccd_header->_port = cc->_port;	
				ccd_header->_listen_port = cc->_listen_port;
				ccd_header->_type = ccd_rsp_connect;
				CConnSet::GetNowTimeVal(ccd_header->_timestamp, ccd_header->_timestamp_msec);
				req_mq[cc->_reqmqidx]->enqueue(tmp_buffer, CCD_HEADER_LEN, flow);
			}
		}
		else
			break;
	}
}

void handle_socket(struct ConnCache *cc, struct epoll_event* ev) {
	int events = ev->events;
	//unsigned long long flow = cc->_flow;
	//int ret;

	// Check cc by fd.
	if ( cc->_fd < 0 ) {
		fprintf(stderr, "handle_socket(): cc->fd < 0! cc->fd %d.\n", cc->_fd);
		return;
	}

	if(!(events & (EPOLLIN | EPOLLOUT))) {
		SAY("events error, close, flow=%llu,ip=%u,port=%d\n", cc->_flow, cc->_ip, cc->_port);
#ifdef _OUTPUT_LOG
		if ( output_log ) {
			syslog(LOG_USER | LOG_CRIT | LOG_PID, "MCP++ CCD: Events error when handle socket! flow - %llu, ip - %u, port - %hu.\n",
				cc->_flow, cc->_ip, cc->_port);
		}
#endif
		ccs->CloseCC(cc, ccd_rsp_disconnect_error);
		return;
	}

	if(events & EPOLLOUT) {
		if(!handle_socket_send(cc, NULL, 0))
			return;
	}

	if(!(events & EPOLLIN)) {
		return;
	}

	//从收请求开始计时
	ccs->StartStat(cc);

	if(!handle_socket_recv(cc))
		return;
	
	//处理收到的socket数据
	handle_socket_message(cc);

}
#ifdef _SPEEDLIMIT_
void handle_pending() {
	
	ConnCache* cc = NULL;
	list_head_t *tmp;
	unsigned long long nowtick = CConnSet::GetNowTick(); 
	list_for_each_entry_safe_l(cc, tmp, &ccs->_pending_send, _next) {
		if(cc->_deadline_tick < nowtick) {
			list_del_init(&cc->_next);
			handle_socket_send(cc, NULL, 0);
		}
	}
	
	list_for_each_entry_safe_l(cc, tmp, &ccs->_pending_recv, _next) {
		if(cc->_deadline_tick < nowtick) {
			list_del_init(&cc->_next);
			if(handle_socket_recv(cc)) {
				handle_socket_message(cc);		
			}
		}
	} 
}
#endif
void handle_rsp_mq(CFifoSyncMQ* mq, bool is_epoll_event = true) {
	unsigned data_len;
	unsigned long long flow;
	int ret;
	ConnCache* cc = NULL;

	MQINFO *mi = &mq_mapping[mq->fd()];
	if ( mi->_mq != mq ) {
		fprintf(stderr, "handle_rsp_mq(): MQ check fail!\n");
		return;
	}
	
	if(is_epoll_event) {
		//设置mq激活标志
		mi->_active = true;
		//清除mq通知
		mq->clear_flag();
	}

	while( !stop ) {
		data_len = 0;
		ret = mq->try_dequeue(tmp_buffer, _BUF_LEN, data_len, flow);

		if(data_len == 0) //没有数据
			break;
		
		SAY("dequeue message flow=%llu,data_len=%u,ret=%d\n", flow, data_len, ret);
		cc = ccs->GetConnCache(flow);
		if(cc == NULL) {	//此种情况可能由于MCD处理超时，导致CCD把超时的连接已经关闭了；或者同一个连接之前的处理导致连接关闭
#ifdef _OUTPUT_LOG
			if ( output_log ) {
				syslog(LOG_USER | LOG_CRIT | LOG_PID, "MCP++ CCD: No cc found when handle_rsp_mq! flow - %llu.\n",
					flow);
			}
#endif
			SAY("no cc found, %llu\n", flow);
#ifdef _SHMMEM_ALLOC_
			//这里要释放在shm分配的内存，否则会导致内存泄露
			if(dequeue_with_shm_alloc && (ccd_header->_type == ccd_req_data_shm)) { 
				myalloc_free(*((unsigned long*)_buf));
			}
#endif
			continue;
		}

		//ccd暂时不对mcd发过来的消息包的timestamp进行处理，因为mcd可能不会对timestamp赋值

		if(ccd_header->_type == ccd_req_disconnect) {
			//MCD要求数据发送完成后主动关闭连接
			//这里尝试立即关闭连接，如果不成功，则记下标识以便数据发送完成关闭连接
			ret = ccs->TryCloseCC(cc, ccd_rsp_disconnect_local);
			SAY("closing cc %llu,%d\n", flow, ret);
			continue;
		}
		
		if(ccd_header->_type == ccd_req_force_disconnect) {
			//MCD强制要求关闭连接
			ccs->CloseCC(cc, ccd_rsp_disconnect_local);
			SAY("force closing cc %llu\n", flow);
			continue;
		}
	
#ifdef _SPEEDLIMIT_
		//设置该连接的上传或者下载速率
		if(ccd_header->_type == ccd_req_set_dspeed || ccd_header->_type == ccd_req_set_uspeed) {
			cc->_du_ticks =  (unsigned)(((double)SEGMENT_SIZE / (double)ccd_header->_arg) * 100);
			SAY("set cc download/upload speed %llu,%d,%u\n", flow, ccd_header->_type, ccd_header->_arg);
			continue;
		}
#endif

#ifdef _SHMMEM_ALLOC_
		if(dequeue_with_shm_alloc && (ccd_header->_type == ccd_req_data_shm)) { 
			memhead* head = (memhead*)_buf;
			handle_socket_send(cc, (char*)myalloc_addr(head->mem), head->len);
			//这里使用完需要释放内存，否则会造成内存泄露
			myalloc_free(head->mem);
		}
		else {	
#endif
			handle_socket_send(cc, _buf, data_len - CCD_HEADER_LEN);
#ifdef _SHMMEM_ALLOC_
		}
#endif
	}
}
void init_mq_conf(CFileConfig& page, bool is_req) {
	
	char mq_name[64] = {0};
	string mq_path;
	CFifoSyncMQ* mq;
	int fd;
	for(unsigned i = 0; i < MAX_MQ_NUM; ++i) {
		if(i == 0) {
			if(is_req)
				sprintf(mq_name, "root\\req_mq_conf");	
			else
				sprintf(mq_name, "root\\rsp_mq_conf");
		}
		else {
			if(is_req)
				sprintf(mq_name, "root\\req_mq%u_conf", i + 1);	
			else
				sprintf(mq_name, "root\\rsp_mq%u_conf", i + 1);
		}

		try {
			mq_path = page[mq_name];
		}
		catch(...) { 
			break;
		}
		
		try {	
			mq = GetMQ(mq_path);
		}
		catch(...) {
			fprintf(stderr, "get mq fail, %s, %m\n", mq_path.c_str());
			err_exit();
		}

		if(is_req) {
			req_mq[req_mq_num++] = mq;
		}
		else {
			//rsp mq需要登记映射表和加入epoll监控
			rsp_mq[rsp_mq_num++] = mq;
			fd = mq->fd();
			if(fd < FD_MQ_MAP_SIZE) {
				mq_mapping[fd]._mq = mq;
				mq_mapping[fd]._active = false;
				epoll_add(epfd, fd, mq, EPOLLIN); 
				if(fd < mq_mapping_min)
					mq_mapping_min = fd;
				if(fd > mq_mapping_max)
					mq_mapping_max = fd;
			}
			else {
				fprintf(stderr, "%s mq's fd is too large, %d >= %d\n", mq_name, fd, FD_MQ_MAP_SIZE);
				err_exit();
			}
		}
	}
}
int init_cc_func(CFileConfig &page, CSOFile &so_file)
{
	char				cc_name[256], c_buf[256];
	char				*pos = NULL;
	string				str_name;
	long				tmp_port;
	unsigned short		port;
	int					i;
	
	memset(cc_func_array, 0, sizeof(check_complete) * CC_ARRAY_SIZE);
	
	try {
		str_name = page["root\\default_complete_func"];
	} catch (...) {
		str_name = "net_complete_func";
	}
	cc_func = (check_complete)so_file.get_func(str_name.c_str());
	if( cc_func == NULL ) {
		fprintf(stderr, "Get default net complete func \"%s\" fail! %m\n", str_name.c_str());
		return -1;
	}

	for ( i = 0; !stop && i < 1024; i++ ) {
		memset(cc_name, 0, 256);
		memset(c_buf, 0, 256);
		pos = NULL;
		
		snprintf(cc_name, 255, "root\\spec_complete_func_%d", i);
		try {
			str_name = page[cc_name];
		} catch (...) {
			break;
		}
		
		strncpy(c_buf, str_name.c_str(), 255);

		pos = strchr(c_buf, ':');
		if ( pos == NULL ) {
			fprintf(stderr, "Invalid \"%s\" param - \"%s\"!\n", cc_name, c_buf);
			return -1;
		}

		*pos = 0;
		pos++;

		if ( strlen(c_buf) == 0 || strlen(pos) == 0 ) {
			fprintf(stderr, "Empty port string or complete function name in \"%s\"!\n", cc_name);
			return -1;
		}

		tmp_port = strtol(c_buf, NULL, 10);
		if ( ((tmp_port == LONG_MIN || tmp_port == LONG_MAX) && errno == ERANGE)
			|| (tmp_port == 0 && errno == EINVAL)
			|| (tmp_port <= 0 || tmp_port >= CC_ARRAY_SIZE) ) {
			fprintf(stderr, "Invalid port string \"%ld\" in \"%s\".\n", tmp_port, cc_name);
			return -1;
		}
		port = (unsigned short)tmp_port;

		cc_func_array[port] = (check_complete)so_file.get_func(pos);
		if ( cc_func_array[port] == NULL ) {
			fprintf(stderr, "Load complete func \"%s\" fail! %m\n", pos);
			return -1;
		}

		fprintf(stderr, "Specific complete function \"PORT - %hu, FUNC - %s\" has been load!\n",
			port, pos);
	}

	return 0;
}
int main(int argc, char* argv[]) {
	if (argc < 2) {
		printf("%s conf_file [non-daemon]\n", argv[0]);
		err_exit();
		// return 0;
	}
	
	if(!strncasecmp(argv[1], "-v", 2)) {
		printf("ccd\n");
		printf("%s\n", version_string);
		printf("%s\n", compiling_date);
#ifdef _SPEEDLIMIT_
		printf("speed limit enable\n");
#endif
#ifdef _SHMMEM_ALLOC_
		printf("shm memory alloc enable\n"); 	
#endif				
#ifdef _DEBUG_
		printf("debug log enable\n");
#endif
		err_exit();
		// return 0;
	}

	if(argc == 2)
		mydaemon(argv[0]);
	else
		initenv(argv[0]);

	CFileConfig& page = * new CFileConfig();
	try {
		page.Init(argv[1]);
	}
	catch(...) {
		fprintf(stderr, "open config fail, %s, %m\n", argv[1]);
		err_exit();
	}

	try {   
		event_notify = from_str<bool>(page["root\\event_notify"]);
	}   
	catch(...) {   
	}   
	try {
		close_notify_details = from_str<bool>(page["root\\close_notify_details"]);
	} catch (...) {
		close_notify_details = false;
	}
	try {
		stat_time = from_str<unsigned>(page["root\\stat_time"]);
	}   
	catch(...) {   
	}
	try {
		time_out = from_str<unsigned>(page["root\\time_out"]);
	}
	catch(...) {
	}   
	try {
		max_conn = from_str<unsigned>(page["root\\max_conn"]);
	}
	catch(...) {
	}
	try {   
		fetch_load_enable = from_str<bool>(page["root\\fetch_load"]);
	}   
	catch(...) {   
	}
	try {
		enqueue_fail_close = from_str<bool>(page["root\\enqueue_fail_close"]);
	}
	catch(...) {
	}
	if ( enqueue_fail_close ) {
		fprintf(stderr, "CCD enqueue_fail_close enabled!\n");
	} else {
		fprintf(stderr, "CCD enqueue_fail_close not enabled!\n");
	}

	// Bind CPU.
	try {
		int bind_cpu = from_str<int>(page["root\\bind_cpu"]);
		cpubind(argv[0], bind_cpu);
	}
	catch (...) {
	}

#ifdef _OUTPUT_LOG
	try {
		output_log = from_str<unsigned>(page["root\\output_log"]);
	} catch (...) {
		output_log = 1;
	}
	if ( output_log ) {
		fprintf(stderr, "CCD log open!\n");
	} else {
		fprintf(stderr, "CCD log close!\n");
	}
#endif

	//
	//	epoll
	//
	epfd = epoll_create(max_conn);
	if(epfd < 0) {
		fprintf(stderr, "create epoll fail, %u,%m\n", max_conn);
		err_exit();
	}
	epev = new struct epoll_event[max_conn];

	//
	//	open mq
	//
	memset(mq_mapping, 0x0, sizeof(MQINFO) * FD_MQ_MAP_SIZE);
	init_mq_conf(page, true);
	init_mq_conf(page, false);	
	if(req_mq_num < 1 || rsp_mq_num < 1) {
		fprintf(stderr, "no req mq or rsp mq, %u,%u\n", req_mq_num, rsp_mq_num);
		err_exit();
	}
	//fprintf(stderr, "req_mq_num=%u,rsp_mq_num=%u,mq_mapping_min=%d,mq_mapping_max=%d\n", req_mq_num, rsp_mq_num, mq_mapping_min, mq_mapping_max);
	
	//
	// 	net load 
	//
	unsigned grid_count = 100, grid_distant = 100, req_water_mark = 1000000;
	try {
		//这里兼容老mcp使用习惯，需要配置这三个参数，参数意义见文档
		grid_count = from_str<unsigned>(page["root\\grid_num"]);
		grid_distant = from_str<unsigned>(page["root\\grid_distant"]);
		req_water_mark = from_str<unsigned>(page["root\\req_water_mark"]);
	}
	catch(...) {
		//这里是mcp++新配置方式，只配置req_water_mark，表示每秒最大请求数
		try {
			req_water_mark = from_str<unsigned>(page["root\\req_water_mark"]);
			if(req_water_mark < 100000000) {
				//这里req_water_mark*10是因为grid_count*grid_distant=10000ms=10s
				req_water_mark *= 10;
			}	
			else {
				fprintf(stderr, "req_water_mark is too large, %u\n", req_water_mark);
			}
		}
		catch(...) {
		}
	}
	timeval time_now;
	gettimeofday(&time_now,NULL);
	pload_grid = new CLoadGrid(grid_count,grid_distant,req_water_mark,time_now);
	
	//
	//	load check_complete and mcd_route, mcd_pre_route
	//
	const string comeplete_so_file = page["root\\complete_so_file"];
	CSOFile so_file;
	if(so_file.open(comeplete_so_file) != 0) {
		fprintf(stderr, "so_file open fail, %s, %m\n", comeplete_so_file.c_str());
		err_exit();
	}

	if ( init_cc_func(page, so_file) ) {
		fprintf(stderr, "Load complete function fail!\n");
		err_exit();
	}
	
	memset(cs_mr_init, 0, 128);
	try {
		const string s_mr_init_msg = page["root\\mcd_route_init_msg"];
		strncpy(cs_mr_init, s_mr_init_msg.c_str(), 127);
	} catch (...) {
		cs_mr_init[0] = 0;
	}
	mr_init = (mcd_route_init)so_file.get_func("mcd_route_init_func");
	if ( mr_init ) {
		if ( mr_init((strlen(cs_mr_init) ? cs_mr_init : NULL)) ) {
			fprintf(stderr, "mcd_route_init_func() run fail! Prepare stop CCD!\n");
			err_exit();
		}
	}
	mr_func = (mcd_route)so_file.get_func("mcd_route_func");
	if ( !mr_func ) {
		fprintf(stderr, "No \"mcd_route_func\", use default mcd route tactic.\n");
	} else {
		fprintf(stderr, "Use custom mcd route tactic.\n");
	}
	
	mpr_func = (mcd_pre_route)so_file.get_func("mcd_pre_route_func");
	if ( !mpr_func ) {
		//如果打开了事件通知开关且定义了mcd自定义路由，那么必须要定义mcd_pre_route，否则
		//连接建立的通知无法发送给对应的mcd...
		if(event_notify && mr_func) {
			fprintf(stderr, "event notify enable & mcd route define, mcd pre route must required!!!\n");
			err_exit();
		}
		else
			fprintf(stderr, "No \"mcd_pre_route_func\", use default mcd pre route tactic.\n");
	} else {
		fprintf(stderr, "Use custom mcd pre route tactic.\n");
	}

	//
	//	cached conn set
	//
	unsigned rbsize = 1<<10;//1kb，接收初始缓冲区大小，这个要根据业务实际情况和内存使用酌情配置
	unsigned wbsize = 1<<14;//16kb，发送初始缓冲区大小，这个要根据业务实际情况和内存使用酌情配置
	try {
		rbsize = from_str<unsigned>(page["root\\recv_buff_size"]);
		wbsize = from_str<unsigned>(page["root\\send_buff_size"]);
	}
	catch(...) {
	}
	if(event_notify)
		ccs = new CConnSet(cc_func, max_conn, rbsize, wbsize, handle_cc_close, mr_func, cc_func_array);
	else
		ccs = new CConnSet(cc_func, max_conn, rbsize, wbsize, NULL, mr_func, cc_func_array);

	try {
		send_buff_max = from_str<unsigned>(page["root\\send_buff_max"]);
	} catch (...) {
		send_buff_max = 0;
	}
	if ( send_buff_max ) {
		fprintf(stderr, "CCD send buffer max check enabled, buffer max length - %u.\n", send_buff_max);
	}

	try {
		recv_buff_max = from_str<unsigned>(page["root\\recv_buff_max"]);
	} catch (...) {
		recv_buff_max = 0;
	}
	if ( recv_buff_max ) {
		fprintf(stderr, "CCD recive buffer max check enabled, buffer max length - %u.\n", recv_buff_max);
	}

#ifdef _SPEEDLIMIT_
	unsigned download_speed = 0;
	unsigned upload_speed = 0;
	unsigned low_buff_size = 0;
	try {
		download_speed = from_str<unsigned>(page["root\\download_speed"]);
		upload_speed = from_str<unsigned>(page["root\\upload_speed"]);
		low_buff_size = from_str<unsigned>(page["root\\low_buff_size"]);
	}
	catch(...) {
	}
	//这里设置全局的上传下载速率，0值表示不限制
	ccs->SetSpeedLimit(download_speed, upload_speed, low_buff_size);
#endif
	
	//
	//	acceptor
	//
	//支持多ip多端口侦听，至少要侦听一个地址	
	try {   
		defer_accept = from_str<bool>(page["root\\defer_accept"]);
	}   
	catch(...) {   
	}
	try {
		tcp_nodelay = from_str<bool>(page["root\\tcp_nodelay"]);
	}
	catch(...) {
	}
	
	CSocketTCP acceptor(-1, false);
	string bind_ip;
	unsigned short bind_port;
	char tmp[64] = {0};	
	memset(listenfd2port, 0, sizeof(unsigned short) * FD_LISTEN_MAP_SIZE);
	memset(&wdbase, 0, sizeof(wtg_api_base_t));
	for(unsigned i = 0; i < 100; ++i) {
		try {
			if(i == 0) {
				sprintf(tmp, "root\\bind_ip");
				bind_ip = page[tmp];
				sprintf(tmp, "root\\bind_port");
				bind_port = from_str<unsigned short>(page[tmp]);
			}
			else {
				sprintf(tmp, "root\\bind_ip%d", i + 1);
				bind_ip = page[tmp];
				sprintf(tmp, "root\\bind_port%d", i + 1);
				bind_port = from_str<unsigned short>(page[tmp]);
			}

			wdbase.ports[wdbase.port_cnt++] = bind_port;	
		}
		catch(...) {
			if(i == 0) {
				fprintf(stderr, "no port to bind\n");
				err_exit();
			}
			break;
		}
	
		if(acceptor.create()) {
			fprintf(stderr, "create acceptor fail, %m\n");
			err_exit();
		}
		acceptor.set_reuseaddr();
		if(acceptor.bind(bind_ip, bind_port)) {
			fprintf(stderr, "bind port fail, %m\n");
			err_exit();
		}
		acceptor.set_nonblock(tcp_nodelay);		
		acceptor.listen(defer_accept);
		if ( acceptor.fd() >= FD_LISTEN_MAP_SIZE) {
			fprintf(stderr, "Too large listen fd %d.\n", acceptor.fd());
			err_exit();
		}
		listenfd2port[acceptor.fd()] = bind_port;
		// For listen socket, add fd to epoll data.
		epoll_add(epfd, acceptor.fd(), (void *)(acceptor.fd()), EPOLLIN);
	}

#ifdef _SHMMEM_ALLOC_
	//
	// share memory allocator
	//
	try {
		enqueue_with_shm_alloc = from_str<bool>(page["root\\shmalloc\\enqueue_enable"]);
	}
	catch(...) {
	}
	try {
		dequeue_with_shm_alloc = from_str<bool>(page["root\\shmalloc\\dequeue_enable"]);
	}
	catch(...) {
	}
	if(enqueue_with_shm_alloc || dequeue_with_shm_alloc) {
		try {
			if(OpenShmAlloc(page["root\\shmalloc\\shmalloc_conf_file"])) {
				fprintf(stderr, "shmalloc init fail, %m\n");
				err_exit();
			}
		}
		catch(...) {
			fprintf(stderr, "shmalloc config error\n");
			err_exit();
		}
	}
	fprintf(stderr, "ccd shmalloc enqueue=%d, dequeue=%d\n", (int)enqueue_with_shm_alloc, (int)dequeue_with_shm_alloc);
#endif	
	
	//
	// watchdog client
	//
	try {
		wtg = new CWtgClientApi;
	} catch (...) {
		fprintf(stderr, "Wtg client alloc fail, %m\n");
		err_exit();
	}
	try {
		string wdc_conf = page["root\\watchdog_conf_file"];
		try {
			wdc = new CWatchdogClient;
		} catch (...) {
			fprintf(stderr, "Watchdog client alloc fail, %m\n");
			err_exit();
		}

		// Construct server PORTs.
		int left_len = 127, port_loop, cur_len, port_str_len;
		char server_ports[128];
		char *port_pos = server_ports;
		memset(server_ports, 0, 128);
		for ( port_loop = 0; port_loop < wdbase.port_cnt; port_loop++ ) {
			if ( left_len < 7 ) {
				// Buffer full.
				break;
			}
			cur_len = sprintf(port_pos, "%hu,", wdbase.ports[port_loop]);
			left_len -= cur_len;
			port_pos += cur_len;
		}
		port_str_len = strlen(server_ports);
		if ( port_str_len >= 1 && server_ports[port_str_len - 1] == ',' ) {
			server_ports[port_str_len - 1] = 0;
		}

		// Get frame version.
		char *frame_version = (strlen(version_string) > 0 ? version_string : NULL);

		// Get plugin version.
		const char *plugin_version = NULL;
		get_plugin_version pv_func = (get_plugin_version)so_file.get_func("get_plugin_version_func");
		if ( pv_func ) {
			plugin_version = pv_func();
		} else {
			plugin_version = NULL;
		}

		// Get addition 0.
		const char *add_0 = NULL;
		get_addinfo_0 add0_func = (get_addinfo_0)so_file.get_func("get_addinfo_0_func");
		if ( add0_func ) {
			add_0 = add0_func();
		} else {
			add_0 = NULL;
		}

		// Get addition 1.
		const char *add_1 = NULL;
		get_addinfo_1 add1_func = (get_addinfo_1)so_file.get_func("get_addinfo_1_func");
		if ( add1_func ) {
			add_1 = add1_func();
		} else {
			add_1 = NULL;
		}
		
		if( wdc->Init(wdc_conf.c_str(), PROC_TYPE_CCD, frame_version, plugin_version,
			(strlen(server_ports) > 0 ? server_ports : NULL),
			add_0, add_1) ) {
			fprintf(stderr, "watchdog client init fail, %s,%m\n", wdc_conf.c_str());
			err_exit();
		}
		
		if ( wtg->Init(WTG_API_TYPE_MCP, 0, NULL, NULL)  ) {
			fprintf(stderr, "Wtg client init fail, %s,%m\n", wdc_conf.c_str());
			err_exit();
		}

		ccd_stat_time = stat_time;
	}
	catch(...) {
		//watchdog 功能并不是必须的
		fprintf(stderr, "Watchdog not enabled!\n");
		if ( wtg ) {
			delete wtg;
			wtg = NULL;
		}
	}

	//
	// fastmem
	//
	fastmem_init(1<<30);

	//
	//	main loop
	//
	int i, eventnum, listen_fd = -1;
	// unsigned long long flow;
	void *ev_data = NULL;
	fprintf(stderr, "ccd event_notify=%d,timeout=%u,stat_time=%u,max_conn=%u,rbsize=%u,wbsize=%u\n", event_notify, time_out, stat_time, max_conn, rbsize, wbsize);
	fprintf(stderr, "ccd started\n");
	
	while(!stop) {
		//处理网络超时与profile统计信息输出
		ccs->Watch(time_out, stat_time);	
		
		//处理网络事件与管道事件
		eventnum = epoll_wait(epfd, epev, max_conn, 1);	
		for(i = 0; i < eventnum; ++i)	{
			ev_data = epev[i].data.ptr;

			if ( ev_data >= ccs->BeginAddr() && ev_data <= ccs->EndAddr() ) {
				// Socket fd event.
				handle_socket((struct ConnCache *)ev_data, &epev[i]);
			} else if ( ev_data >= (void *)MIN_START_ADDR ) {
				// Mq event.
				handle_rsp_mq((CFifoSyncMQ *)ev_data);
			} else {
				// Listen fd event.
				listen_fd = (int)((long)ev_data);
				handle_accept(listen_fd);
			}
		}
		
		//这里要检查是否有mq没有被epoll激活，没有的话要扫描一次，避免mq通知机制的不足，详细参阅mq实现说明
		for(i = mq_mapping_min; i <= mq_mapping_max; ++i) {
			if(mq_mapping[i]._mq != NULL) {
				if(mq_mapping[i]._active)
					mq_mapping[i]._active = false;
				else
					handle_rsp_mq(mq_mapping[i]._mq, false);
			}
		}

#ifdef _SPEEDLIMIT_
		//处理上传下载限速回调
		handle_pending();
#endif		
		//维持与watchdog进程的心跳
		if(wdc)
			wdc->Touch();
	}

	fastmem_fini();
	
#ifdef _SHMMEM_ALLOC_
	myalloc_fini();
#endif
	fprintf(stderr, "ccd stopped\n");
	syslog(LOG_USER | LOG_CRIT | LOG_PID, "%s ccd stopped\n", argv[0]);
	exit(0);
}//main	
