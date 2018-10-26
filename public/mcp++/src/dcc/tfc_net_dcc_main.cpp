#include "tfc_base_config_file.h"
#include "tfc_net_epoll_flow.h"
#include "tfc_net_open_mq.h"
#include "tfc_net_cconn.h"
#include "tfc_base_str.h"
#include "tfc_base_so.h"
#include "tfc_net_socket_tcp.h"
#include "tfc_net_dcc_define.h"
#include "tfc_net_open_shmalloc.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <syslog.h>
#include "version.h"
#include "mydaemon.h"
#include "fastmem.h"
#ifdef _SHMMEM_ALLOC_
#include "tfc_net_open_shmalloc.h"
#include "myalloc.h"
#endif
#include "tfc_base_watchdog_client.h"
#include "wtg_client_api.h"

using namespace tfc::base;
using namespace tfc::net;
using namespace tfc::watchdog;
using namespace tfc::wtgapi;

/*
 * 1.dcc����mcd�����flow��Ψһȷ��һ�����ӣ���ͬ��flow������ͬ������(��ʹip portһ��),mcd������ά�����flow
 * 2.�����ͬһ��ipֻ��һ�����ӵĻ�,flow=ip����,flow�����Ե���0����1
 * 3.���ʹ�ö�mq�Ļ�����ôһ��req_mq�������һ��rsp_mq����req_mq��Ӧrsp_mq��req_mq2��Ӧrsp_mq2���������� 
 */

#ifdef _DEBUG_
#define SAY(fmt, args...) fprintf(stderr, "%s:%d,"fmt, __FILE__, __LINE__, ##args)
#else
#define SAY(fmt, args...)
#endif

#define MAX_MQ_NUM		(1<<5)	//���֧�ֵ�mq����	
#define FD_MQ_MAP_SIZE	(1<<10)	//fd->mq ӳ��������󳤶ȣ�fd < FD_MQ_MAP_SIZE	

typedef struct {
	CFifoSyncMQ* _mq;			//�ܵ�
	bool _active;				//�Ƿ�epoll����
	int _reqmqidx;				//��ʶmq��req_mq�е���ţ�ͬʱҲ�Ƕ�Ӧrsp_mq�����
}MQINFO;

enum status_type {
	status_connecting = 0,		//���ӽ�����
	status_send_connecting = 1,	//�������ݵȴ����ӽ�����
	status_connected = 2,		//�Ѿ���������	
};

#ifdef _OUTPUT_LOG
unsigned output_log = 0;				// 0 not output log, others output log.
#endif
unsigned send_buff_max = 0;				// ���������ʱ�����ӵ�Sendbuf���ݶѻ�������ֵ����Ϊ�����ر����ӣ�Ϊ0ʱ�����øü��
unsigned recv_buff_max = 0;				// ���������ʱ�����ӵ�Recvbuf���ݶѻ�������ֵ����Ϊ�����ر����ӣ�Ϊ0ʱ�����øü��
int			is_ccd = 0;
unsigned	ccd_stat_time;
typedef int (*protocol_convert)(const void* , unsigned, void* , unsigned &);
static bool event_notify = false; 		//true����DCC�������ӽ������Ͽ�������ʧ�ܵ�֪ͨ��MCD�����򲻷���	
bool close_notify_details = false;		//�������ӹر�ϸ���¼���event_notify����ʱ���÷���Ч
static unsigned stat_time = 60; 		//profileͳ�Ƽ��ʱ��
static unsigned time_out = 300;			//���ӳ�ʱʱ��
static unsigned max_conn = 20000;		//���������
static bool tcp_nodelay = true;			//�Ƿ�����socket��tcp_nodelay����
static CFifoSyncMQ* req_mq[MAX_MQ_NUM];	//mcd->dcc mq
static CFifoSyncMQ* rsp_mq[MAX_MQ_NUM];	//dcc->mcd mq
unsigned req_mq_num;				//req_mq��Ŀ	
static unsigned rsp_mq_num;				//rsp_mq��Ŀ
static MQINFO mq_mapping[FD_MQ_MAP_SIZE];//fd->mqӳ��
static int mq_mapping_min = INT_MAX;	//req_mq fd min
static int mq_mapping_max = 0;			//req_mq fd max
static check_complete cc_func = NULL;	//�������Լ��
static const unsigned C_TMP_BUF_LEN = (1<<27); 				//���ݻ�������128M              
static char tmp_buffer[C_TMP_BUF_LEN];						//ͷ��+���ݵ�buf
static TDCCHeader *dcc_header = (TDCCHeader*)tmp_buffer;	//dcc->mcd ��Ϣ��ͷ��	
static char* _buf = (char*)tmp_buffer + DCC_HEADER_LEN;		//dcc->mcd ��Ϣ��Ϣ��
static unsigned _BUF_LEN = C_TMP_BUF_LEN - DCC_HEADER_LEN;	//�����������Ϣ�峤��
static CConnSet* ccs = NULL;			//���Ӷ���
static int epfd = -1;					//epoll���
static struct epoll_event* epev = NULL;	//epoll�¼�����
#ifdef _SHMMEM_ALLOC_
//ʹ�ù����ڴ�������洢���ݵ�ʱ��mq�����ظ����ĸ�ʽ���£�
//  TDCCHeader + mem_handle + len
//mem_handle���������ڴ�������ĵ�ַ��len�Ǹ����ݵĳ��ȣ���dccheader��_typeֵΪdcc_req_send_shm����dcc_rsp_data_shm
//�÷�ʽ���������ݵ��ڴ濽����������Ҫenqueue�Ľ��̷����ڴ棬dequeue�Ľ����ͷ��ڴ�
static bool enqueue_with_shm_alloc = false;	//true��ʾdcc->mcdʹ�ù����ڴ�������洢���ݣ�����ʹ��
static bool dequeue_with_shm_alloc = false;	//true��ʾmcd->dccʹ�ù����ڴ�������洢���ݣ�����ʹ��
#endif
CWatchdogClient* 	wdc = NULL;		//watchdog client 
CWtgClientApi*		wtg = NULL;		// Wtg client.

/*
inline void send_notify_2_mcd(int type, unsigned long long flow) {
	dcc_header->_type = type;
	CConnSet::GetNowTimeVal(dcc_header->_timestamp, dcc_header->_timestamp_msec);
	//for(unsigned i = 0; i < req_mq_num; ++i) {
	for(unsigned i = 0; i < rsp_mq_num; ++i) {
		//req_mq[i]->enqueue(dcc_header, DCC_HEADER_LEN, flow);
		rsp_mq[i]->enqueue(dcc_header, DCC_HEADER_LEN, flow);
	}
}*/

inline void send_notify_2_mcd(int type, ConnCache* cc) {
	dcc_header->_type = type;
	CConnSet::GetNowTimeVal(dcc_header->_timestamp, dcc_header->_timestamp_msec);

	if ( cc ) {
		dcc_header->_ip = cc->_ip;
		dcc_header->_port = cc->_port;
		rsp_mq[cc->_reqmqidx]->enqueue(dcc_header, DCC_HEADER_LEN, cc->_flow);
	} else {
		dcc_header->_ip = 0;
		dcc_header->_port = 0;
		for(unsigned i = 0; i < rsp_mq_num; ++i) {
			rsp_mq[i]->enqueue(dcc_header, DCC_HEADER_LEN, (unsigned long long)0);
		}
	}
}

void handle_cc_close(ConnCache* cc, unsigned short event) {
	static TDCCHeader header;
	header._ip = cc->_ip;
	header._port = cc->_port;
	header._type = event;
	CConnSet::GetNowTimeVal(header._timestamp, header._timestamp_msec);
	rsp_mq[cc->_reqmqidx]->enqueue(&header, DCC_HEADER_LEN, cc->_flow);
}

inline ConnCache* make_new_cc(CConnSet* ccs, unsigned long long flow, unsigned ip, unsigned short port) {
	static CSocketTCP socket(-1, false);
	socket.create();
	socket.set_nonblock(tcp_nodelay);
	int ret = socket.connect(ip, port);
	if((ret != 0 )&& (ret != -EWOULDBLOCK) && (ret != -EINPROGRESS)) {
#ifdef _OUTPUT_LOG
		if ( output_log ) {
			syslog(LOG_USER | LOG_CRIT | LOG_PID, "MCP++ DCC: connect() system call fail in make_new_cc! %m flow - %llu, ip - %u, port - %hu.\n",
				flow, ip, port);
		}
#endif
		close(socket.fd());
		return NULL;
	}

	//����DCC�����ӷ�����ÿ���̭��ʽ��Ϊ��ȷ��DCC��������Ŀ��ɹ�
	ConnCache* cc = ccs->AddConn(socket.fd(), flow);
	if(!cc) {
		fprintf(stderr, "add conn fail, %m\n");
#ifdef _OUTPUT_LOG
		if ( output_log ) {
			syslog(LOG_USER | LOG_CRIT | LOG_PID, "MCP++ DCC: Add cc fail in make_new_cc! flow - %llu, ip - %u, port - %hu.\n",
				flow, ip, port);
		}
#endif
		close(socket.fd());
		return NULL;
	}
	//������������Ŀ���������ip��port�������������û��������ȡ����ip��port
	cc->_ip = ip;
	cc->_port = port;
	return cc;
}
//void handle_socket(int eventfd, struct epoll_event* ev) {
void handle_socket(struct ConnCache *cc, struct epoll_event* ev) {
	
	unsigned data_len;
	int ret;
	int events = ev->events;
	unsigned long long flow;

	// Check cc by fd.
	if ( cc->_fd < 0 ) {
		fprintf(stderr, "DCC: handle_socket(): cc->fd < 0! cc->fd %d.\n", cc->_fd);
		return;
	}
	
	flow = cc->_flow;

	if (!(events & (EPOLLOUT|EPOLLIN))) {
#ifdef _OUTPUT_LOG
		if ( output_log ) {
			syslog(LOG_USER | LOG_CRIT | LOG_PID, "MCP++ DCC: Events error in handle_socket! flow - %llu, ip - %u, port - %hu.\n",
				cc->_flow, cc->_ip, cc->_port);
		}
#endif
		ccs->CloseCC(cc, dcc_rsp_disconnect_error);
		SAY("events error, close, flow=%llu,ip=%u,port=%d\n", flow, cc->_ip, cc->_port);
		return;
	}

	if (events & EPOLLOUT) {	
		if((cc->_connstatus == status_send_connecting) || (cc->_connstatus == status_connected)) {
			if(cc->_connstatus == status_send_connecting) {
				cc->_connstatus = status_connected;
				SAY("flow %llu send_connect success\n", flow);
				if(event_notify) {
					send_notify_2_mcd(dcc_rsp_connect_ok, cc);
				}
			}

			ret = ccs->SendFromCache(cc);
			if(ret == 0) {
				//���淢�����,ȥ��EPOLLOUT
				epoll_mod(epfd, cc->_fd, cc, EPOLLIN | EPOLLET);
			}
			else if (ret == -E_NEED_CLOSE) {
				//���ڷ���ʧ�ܻ�ر����ӣ�����Ͳ����͡�����ʧ�ܡ�֪ͨ����Ϊ���ӹرջᷢ�����ӡ����ӹرա�֪ͨ
				//if(event_notify) {
				//	send_notify_2_mcd(dcc_rsp_send_failed, flow);
				//}
				SAY("SendFromCache failed, close, flow=%llu,ip=%u,port=%d\n", flow, cc->_ip, cc->_port);
#ifdef _OUTPUT_LOG
				if ( output_log ) {
					syslog(LOG_USER | LOG_CRIT | LOG_PID, "MCP++ DCC: SendFromCache failed in handle_socket! flow - %llu, ip - %u, port - %hu.\n",
						cc->_flow, cc->_ip, cc->_port);
				}
#endif
				ccs->CloseCC(cc, dcc_rsp_disconnect_peer_or_error);
				return;
			}
			//else
			//{
			//ret==-E_NEED_SEND ���淢��δ���,EPOLLOUT��������
			//}					
		}
		else {	
			return;
		}	
	}

	if (!(events & EPOLLIN)) {
		return;
	}

	//������Ϣ��������ֶԶ˶Ͽ����ӵ��Ƕ������ﻹ��δ������������ӳٹر�_finclose������
	ret = ccs->Recv(cc);
	SAY("Recv flow=%llu,ret=%d\n", flow, ret);			
	if (ret == -E_NEED_CLOSE) {	
		SAY("Recv err, close, flow=%llu,ip=%u,port=%d\n", flow, cc->_ip, cc->_port);
		ccs->CloseCC(cc, dcc_rsp_disconnect_peer_or_error);
		return;
	}
	else if(ret == -E_MEM_ALLOC) {
		fprintf(stderr, "alloc mem fail, %m\n");
#ifdef _OUTPUT_LOG
		if ( output_log ) {
			syslog(LOG_USER | LOG_CRIT | LOG_PID, "MCP++ DCC: Memory overload in handle_socket - Recv! flow - %llu, ip - %u, port - %hu.\n",
				cc->_flow, cc->_ip, cc->_port);
		}
#endif
		if(event_notify) {
			send_notify_2_mcd(dcc_rsp_overload_mem, NULL);
		}
		//���������ڴ��
		fastmem_shrink();
	}
	//else
	//{
	//recv some data
	//}

#ifdef _SHMMEM_ALLOC_
	bool is_shm_alloc;	
#endif	
	//����
	do {
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
		SAY("GetMessage data_len=%d,flow=%llu,ret=%d\n", data_len, flow, ret);
		if (ret == 0 && data_len > 0) {
			
#ifdef _SHMMEM_ALLOC_
			if(is_shm_alloc)
				dcc_header->_type = dcc_rsp_data_shm;
			else
#endif					
				dcc_header->_type = dcc_rsp_data;

			dcc_header->_ip = cc->_ip;
			dcc_header->_port = cc->_port;
			CConnSet::GetNowTimeVal(dcc_header->_timestamp, dcc_header->_timestamp_msec);

			if(rsp_mq[cc->_reqmqidx]->enqueue(tmp_buffer, data_len + DCC_HEADER_LEN, flow) != 0) {
#ifdef _OUTPUT_LOG
				if ( output_log ) {
					syslog(LOG_USER | LOG_CRIT | LOG_PID, "MCP++ DCC: Enqueue fail in handle_socket! flow - %llu.\n",
						flow);
				}
#endif
#ifdef _SHMMEM_ALLOC_
				//����Ҫ��ֹ�ڴ�й¶
				if(is_shm_alloc) {
					myalloc_free(*((unsigned long*)(tmp_buffer + DCC_HEADER_LEN)));
				}
#endif								
			}
			ccs->EndStat(cc);
		}
		else if(ret == -E_NEED_CLOSE) {
#ifdef _OUTPUT_LOG
			if ( output_log ) {
				syslog(LOG_USER | LOG_CRIT | LOG_PID, "MCP++ DCC: GetMessage fail in handle_socket! flow - %llu, ip - %u, port - %hu.\n",
					cc->_flow, cc->_ip, cc->_port);
			}
#endif
			SAY("get message from failed, flow=%llu,ip=%u,port=%d\n", flow, cc->_ip, cc->_port);	
			ccs->CloseCC(cc, dcc_rsp_disconnect_error);
			break;
		}
		else {
			break;	
		}
	}
	while( !stop && ret == 0 );

	//����Ҫ���һ���Ƿ���Recv��ʱ���ֶԶ˶Ͽ������ˣ��������ر�
	if(cc->_finclose) {
		SAY("finclose cc, flow=%llu,ip=%u,port=%d\n", flow, cc->_ip, cc->_port);
		ccs->CloseCC(cc, dcc_rsp_disconnected);					
	}
}
void handle_req_mq(CFifoSyncMQ* mq, bool is_epoll_event = true) {
	unsigned long long flow;
	unsigned data_len;
	int ret;
	ConnCache* cc = NULL;

	MQINFO* mi = &mq_mapping[mq->fd()];
	if ( mi->_mq != mq ) {
		fprintf(stderr, "handle_req_mq(): MQ check fail!\n");
		return;
	}
	
	if(is_epoll_event) {
		//����mq�����־
		mi->_active = true;
		//���mq֪ͨ
		mq->clear_flag();
	}

	while( !stop ) {
		// flow = UINT_MAX;
		flow = ULLONG_MAX;
		data_len = 0;
		ret = mq->try_dequeue(tmp_buffer, C_TMP_BUF_LEN, data_len, flow);
		if (data_len == 0)
			break;
		SAY("dequeue message, flow=%llu,data_len=%u\n", flow, data_len);
		
		cc = ccs->GetConnCache(flow);
		if(cc != NULL) {
			if((cc->_ip != dcc_header->_ip) || (cc->_port != dcc_header->_port)) {
				//���ڷ��գ����flow��cc�Ĺ�ϣ�㷨�г�ͻ���ܵ������ӱ�ǿ�ƹرա�
				SAY("ip or port not match, close cc, flow=%llu,ip=%u,port=%d\n", flow, cc->_ip, cc->_port);
#ifdef _OUTPUT_LOG
				if ( output_log ) {
					syslog(LOG_USER | LOG_CRIT | LOG_PID, "MCP++ DCC: ip or port not match, flow number confict. close old cc! ori_flow - %llu, ori_ip - %u, ori_port - %hu, flow - %llu, ip - %u, port - %hu.\n",
						flow, dcc_header->_ip, dcc_header->_port, cc->_flow, cc->_ip, cc->_port);
				}
#endif
				ccs->CloseCC(cc, dcc_rsp_disconnect_error);
				cc = NULL;
			}
			else {
				SAY("reuse cc, flow=%llu,ip=%u,port=%d\n", flow, cc->_ip, cc->_port);
			}
		}
		else {
			SAY("create new cc,flow=%llu\n", flow);
		}

		if(dcc_header->_type == dcc_req_disconnect) {
			if(cc) {
				SAY("req to disconnect ip=%u,port=%d\n", dcc_header->_ip, dcc_header->_port);
				ccs->CloseCC(cc, dcc_rsp_disconnect_local);
			}
		}
		else if(dcc_header->_type == dcc_req_send || dcc_header->_type == dcc_req_send_shm) {
#ifdef _SHMMEM_ALLOC_
			memhead* head = NULL;
			if(dequeue_with_shm_alloc && (dcc_header->_type == dcc_req_send_shm)) { 
				head = (memhead*)(tmp_buffer + DCC_HEADER_LEN);
			}
#endif	
			//������������
			if (cc) {
				ccs->StartStat(cc);

				if (cc->_connstatus != status_connected)
					SAY("%d:%d in connecting, data push to cache to wait...\n",dcc_header->_ip, dcc_header->_port);
				else
					SAY("req to send on a connect link %d:%d\n", dcc_header->_ip, dcc_header->_port);

#ifdef _SHMMEM_ALLOC_
				if(head) {
					ret = ccs->Send(cc, (char*)myalloc_addr(head->mem), head->len);
					myalloc_free(head->mem);
				}
				else {
#endif
					ret = ccs->Send(cc, tmp_buffer + DCC_HEADER_LEN, data_len - DCC_HEADER_LEN);
#ifdef _SHMMEM_ALLOC_
				}
#endif				
				if(ret != -E_NEED_CLOSE && ret != -E_MEM_ALLOC) {
					if(ret == 0)
					{
						//δ�����꣬��������EPOLLOUT
						epoll_mod(epfd, cc->_fd, cc, EPOLLIN | EPOLLOUT | EPOLLET);
					}
					//else
					//{
					//ret==1,�������
					//}
				}
				else if(ret == -E_MEM_ALLOC) {
					fprintf(stderr, "alloc mem fail, %m\n");
#ifdef _OUTPUT_LOG
					if ( output_log ) {
						syslog(LOG_USER | LOG_CRIT | LOG_PID, "MCP++ DCC: Memory overload in handle_req_mq - Send! flow - %llu, ip - %u, port - %hu.\n",
							cc->_flow, cc->_ip, cc->_port);
					}
#endif
					if(event_notify) {
						send_notify_2_mcd(dcc_rsp_overload_mem, NULL);
					}
					//���������ڴ��
					fastmem_shrink();
				}
				else {
					SAY("send failed, close, ret=%d,flow=%llu,fd=%d\n", ret, flow, cc->_fd);
#ifdef _OUTPUT_LOG
					if ( output_log ) {
						syslog(LOG_USER | LOG_CRIT | LOG_PID, "MCP++ DCC: Send failed in handle_req_mq - Send! Will retry! %m flow - %llu, ip - %u, port - %hu.\n",
							cc->_flow, cc->_ip, cc->_port);
					}
#endif
					ccs->CloseCC(cc, dcc_rsp_disconnect_peer_or_error);
					goto retry;
				}
			}
			//����������
			else {
retry:				
				cc = make_new_cc(ccs, flow, dcc_header->_ip, dcc_header->_port);
				if(!cc) {
					//��Ӧ
					if(event_notify) {
						dcc_header->_type = dcc_rsp_connect_failed;
						//dcc_header->_timestamp = CConnSet::GetNowTimeSec();
						CConnSet::GetNowTimeVal(dcc_header->_timestamp, dcc_header->_timestamp_msec);
						rsp_mq[mi->_reqmqidx]->enqueue(dcc_header, DCC_HEADER_LEN, flow);
					}
					SAY("connect failed, ip=%u,port=%d\n", dcc_header->_ip, dcc_header->_port);
#ifdef _OUTPUT_LOG
					if ( output_log ) {
						syslog(LOG_USER | LOG_CRIT | LOG_PID, "MCP++ DCC: Connect failed in handle_req_mq! flow - %llu.\n",
							flow);
					}
#endif
#ifdef _SHMMEM_ALLOC_
					//�ͷ��ڴ棬��ֹ�ڴ�й¶
					if(head)
						myalloc_free(head->mem);
#endif								
					continue;
				}
				cc->_reqmqidx = mi->_reqmqidx;
				cc->_connstatus = status_send_connecting;

				ccs->StartStat(cc);

				//���뻺��
				// 0, send failed or send not complete, add epollout
				// 1, send complete
#ifdef _SHMMEM_ALLOC_
				if(head) {
					ret = ccs->SendForce(cc, (char*)myalloc_addr(head->mem), head->len);
					myalloc_free(head->mem);
				}
				else {
#endif
					ret = ccs->SendForce(cc, tmp_buffer + DCC_HEADER_LEN, data_len - DCC_HEADER_LEN);
#ifdef _SHMMEM_ALLOC_
				}
#endif				
				if(ret == 1) {	
					epoll_add(epfd, cc->_fd, cc, EPOLLIN | EPOLLET);
					SAY("sendforce finished, data_len=%u\n", data_len - DCC_HEADER_LEN);
				}
				else {
					if(ret == -E_MEM_ALLOC) {
						fprintf(stderr, "alloc mem fail, %m\n");
#ifdef _OUTPUT_LOG
						if ( output_log ) {
							syslog(LOG_USER | LOG_CRIT | LOG_PID, "MCP++ DCC: Memory overload in handle_req_mq - SendForce! flow - %llu, ip - %u, port - %hu.\n",
								cc->_flow, cc->_ip, cc->_port);
						}
#endif
						if(event_notify) {
							send_notify_2_mcd(dcc_rsp_overload_mem, NULL);
						}
						//���������ڴ��
						fastmem_shrink();
					}
					else if(ret == -E_NEED_CLOSE) {
						//���ڷ���ʧ�ܻ�ر����ӣ�����Ͳ����͡�����ʧ�ܡ�֪ͨ����Ϊ���ӹرջᷢ�����ӡ����ӹرա�֪ͨ
						//if(event_notify) {
						//	send_notify_2_mcd(dcc_rsp_send_failed, cc->_flow);
						//}
						SAY("send fail, flow=%llu,ip=%u,port=%d, %m\n", flow, cc->_ip, cc->_port);
#ifdef _OUTPUT_LOG
						if ( output_log ) {
							syslog(LOG_USER | LOG_CRIT | LOG_PID, "MCP++ DCC: send fail in handle_req_mq - SendForce! %m flow - %llu, ip - %u, port - %hu.\n",
								cc->_flow, cc->_ip, cc->_port);
						}
#endif
						ccs->CloseCC(cc, dcc_rsp_disconnect_peer_or_error);
					}
					else {
						epoll_add(epfd, cc->_fd, cc, EPOLLIN | EPOLLOUT | EPOLLET);
						SAY("sendforce unfinished, data_len=%u\n", data_len - DCC_HEADER_LEN);
					}
				}
			}
		}
	}//mq loop
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
			//req mq��Ҫ�Ǽ�ӳ���ͼ���epoll���
			fd = mq->fd();
			if(fd < FD_MQ_MAP_SIZE) {
				mq_mapping[fd]._mq = mq;
				mq_mapping[fd]._active = false;
				mq_mapping[fd]._reqmqidx = req_mq_num - 1;
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
		else {
			rsp_mq[rsp_mq_num++] = mq;
		}
	}
}
int main(int argc, char* argv[]) {
	if (argc < 2) {
		printf("%s conf_file [non-daemon]\n", argv[0]);
		err_exit();
		// return 0;
	}

	if(!strncasecmp(argv[1], "-v", 2)) {
		printf("dcc\n");
		printf("%s\n", version_string);
		printf("%s\n", compiling_date);
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
		time_out = from_str<unsigned>(page["root\\time_out"]);
	}
	catch(...) {
	}
	try {
		stat_time = from_str<unsigned>(page["root\\stat_time"]);
	}   
	catch(...) {   
	}
	try {
		max_conn = from_str<unsigned>(page["root\\max_conn"]);
	}
	catch(...) {
	}
	try {
		tcp_nodelay = from_str<bool>(page["root\\tcp_nodelay"]);
	}
	catch(...) {
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
		fprintf(stderr, "DCC log open!\n");
	} else {
		fprintf(stderr, "DCC log close!\n");
	}
#endif

	//
	// epoll
	//
	epfd = epoll_create(max_conn);
	if(epfd < 0) {
		fprintf(stderr, "create epoll fail, %u,%m\n", max_conn);
		err_exit();
	}
	epev = new struct epoll_event[max_conn];

	//
	// open mq
	//
	memset(mq_mapping, 0x0, sizeof(MQINFO) * FD_MQ_MAP_SIZE);
	init_mq_conf(page, true);
	init_mq_conf(page, false);
	if(req_mq_num < 1 || rsp_mq_num < 1 || req_mq_num != rsp_mq_num) {
		fprintf(stderr, "no req mq or rsp mq, %u,%u\n", req_mq_num, rsp_mq_num);
		err_exit();
	}
	//fprintf(stderr, "req_mq_num=%u,rsp_mq_num=%u,mq_mapping_min=%d,mq_mapping_max=%d\n", req_mq_num, rsp_mq_num, mq_mapping_min, mq_mapping_max);
	
	//
	// load check_complete
	//
	const string comeplete_so_file = page["root\\complete_so_file"];
	CSOFile so_file;
	if(so_file.open(comeplete_so_file)) {
		fprintf(stderr, "so_file open fail, %s, %m\n", comeplete_so_file.c_str());
		err_exit();
	}
	cc_func = (check_complete) so_file.get_func("net_complete_func");
	if(cc_func == NULL) {
		fprintf(stderr, "check_complete func is NULL, %m\n");
		err_exit();
	}

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
			fprintf(stderr, "Out of memory for watchdog client!\n");
			err_exit();
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
		
		if( wdc->Init(wdc_conf.c_str(), PROC_TYPE_DCC, frame_version, plugin_version,
			NULL, add_0, add_1) ) {
			fprintf(stderr, "watchdog client init fail, %s,%m\n", wdc_conf.c_str());
			err_exit();
		}

		if ( wtg->Init(WTG_API_TYPE_MCP, 0, NULL, NULL)  ) {
			fprintf(stderr, "Wtg client init fail, %s,%m\n", wdc_conf.c_str());
			err_exit();
		}
	}
	catch(...) {
		//watchdog ���ܲ����Ǳ����
		fprintf(stderr, "Watchdog not enabled!\n");
		if ( wtg ) {
			delete wtg;
			wtg = NULL;
		}
	}

	//
	// conn set
	//
	unsigned rbsize = 1<<16;//64kb
	unsigned wbsize = 1<<14;//16kb
	try {
		rbsize = from_str<unsigned>(page["root\\recv_buff_size"]);
		wbsize = from_str<unsigned>(page["root\\send_buff_size"]);
	}
	catch(...) {
	}
	if(event_notify)
		ccs = new CConnSet(cc_func, max_conn, rbsize, wbsize, handle_cc_close);
	else
		ccs = new CConnSet(cc_func, max_conn, rbsize, wbsize, NULL);

	try {
		send_buff_max = from_str<unsigned>(page["root\\send_buff_max"]);
	} catch (...) {
		send_buff_max = 0;
	}
	if ( send_buff_max ) {
		fprintf(stderr, "DCC send buffer max check enabled, buffer max length - %u.\n", send_buff_max);
	}

	try {
		recv_buff_max = from_str<unsigned>(page["root\\recv_buff_max"]);
	} catch (...) {
		recv_buff_max = 0;
	}
	if ( recv_buff_max ) {
		fprintf(stderr, "DCC recive buffer max check enabled, buffer max length - %u.\n", recv_buff_max);
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
	fprintf(stderr, "dcc shmalloc enqueue=%d, dequeue=%d\n", (int)enqueue_with_shm_alloc, (int)dequeue_with_shm_alloc);
#endif	

	//
	// fastmem
	//
	fastmem_init(1<<30);
		
	//
	// main loop
	//
	// int i, eventfd, eventnum, flow;
	int i, eventnum;
	void *ev_data = NULL;
	fprintf(stderr, "dcc event_notify=%d,timeout=%u,stat_time=%u,max_conn=%u,rbsize=%u,wbsize=%u\n", event_notify, time_out, stat_time, max_conn, rbsize, wbsize);
	fprintf(stderr, "dcc started\n");
	while(!stop) {
		//�������糬ʱ��profileͳ����Ϣ���
		ccs->Watch(time_out, stat_time);
			
		//���������¼���ܵ��¼�
		eventnum = epoll_wait(epfd, epev, max_conn, 1);	
		for(i = 0; i < eventnum; ++i)	{
			ev_data = epev[i].data.ptr;

			if ( ev_data >= ccs->BeginAddr() && ev_data <= ccs->EndAddr() ) {
				// Socket fd event.
				handle_socket((struct ConnCache *)ev_data, &epev[i]);
			} else {
				// Mq event.
				handle_req_mq((CFifoSyncMQ *)ev_data);
			}
		}

		//����Ҫ����Ƿ���mqû�б�epoll���û�еĻ�Ҫ��ɨ��һ�Σ�����mq֪ͨ���ƵĲ���	
		for(i = mq_mapping_min; i <= mq_mapping_max; ++i) {
			if(mq_mapping[i]._mq != NULL) {
				if(mq_mapping[i]._active)
					mq_mapping[i]._active = false;
				else
					handle_req_mq(mq_mapping[i]._mq, false);
			}
		}

		//ά����watchdog���̵�����
		if(wdc)
			wdc->Touch();
	}

	fastmem_fini();

#ifdef _SHMMEM_ALLOC_
	myalloc_fini();
#endif
	fprintf(stderr, "dcc stopped\n");
	syslog(LOG_USER | LOG_CRIT | LOG_PID, "%s dcc stopped\n", argv[0]);
	exit(0);
}
