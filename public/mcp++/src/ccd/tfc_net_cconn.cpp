#include "tfc_net_cconn.h"
#include "tfc_net_socket_tcp.h"
#include <sys/time.h>
#include <assert.h>
#include <errno.h>
#include <syslog.h>
#include <stdio.h>
#ifdef _SHMMEM_ALLOC_
#include "myalloc.h"
#endif
#include "tfc_base_watchdog_client.h"
#include "wtg_client_api.h"
#include "dev_info.h"
#include "tfc_net_ccd_define.h"
#include "tfc_net_dcc_define.h"

#define SOCK_FLOW_MIN	2			//socket flow 最小
#define SOCK_FLOW_MAX	UINT_MAX 	//socket flow 最大
#define C_READ_BUFFER_SIZE (1<<16)
#define STAT_ATTR_NUM	7
#define STAT_ATTR_VAL_MAX	20
#define STAT_ATTR_KEY_LEN	64

using namespace tfc::net;
using namespace std;
using namespace tfc::watchdog;
using namespace tfc::wtgapi;

extern bool stop;			//true-运行，false-退出
extern CWatchdogClient		*wdc;		// watchdog client
extern CWtgClientApi		*wtg;		// Wtg client.
extern int					is_ccd;
#ifdef _OUTPUT_LOG
extern unsigned				output_log;	// 0 not output log, others output log.
#endif
extern unsigned 			ccd_stat_time;
extern unsigned				req_mq_num;
static int					dcc_report_inited = 0;
extern unsigned				send_buff_max;	// 当该项被配置时，连接的Sendbuf数据堆积超过该值则认为出错并关闭连接，为0时不启用该检测
extern unsigned				recv_buff_max;	// 当该项被配置时，连接的Recvbuf数据堆积超过该值则认为出错并关闭连接，为0时不启用该检测
extern bool					close_notify_details;	//启用连接关闭细节事件，etvent_notify启用时配置方有效

static wtg_api_attr_t ccd_wd_attrs[STAT_ATTR_NUM] = {
	{ATTR_TYPE_INT, "ccd#msgcount", {0}}, 
	{ATTR_TYPE_INT, "ccd#maxtime", {0}}, 
	{ATTR_TYPE_INT, "ccd#mintime", {0}}, 
	{ATTR_TYPE_INT, "ccd#avgtime", {0}}, 
	{ATTR_TYPE_INT, "ccd#recvsize", {0}}, 
	{ATTR_TYPE_INT, "ccd#sendsize", {0}},
	{ATTR_TYPE_INT, "ccd#connnum", {0}}
};

static wtg_api_attr_t dcc_wd_attrs[STAT_ATTR_NUM] = {
	{ATTR_TYPE_INT, NULL, {0}}, 
	{ATTR_TYPE_INT, NULL, {0}}, 
	{ATTR_TYPE_INT, NULL, {0}}, 
	{ATTR_TYPE_INT, NULL, {0}}, 
	{ATTR_TYPE_INT, NULL, {0}}, 
	{ATTR_TYPE_INT, NULL, {0}},
	{ATTR_TYPE_INT, NULL, {0}}
};

static char			dcc_attr_key[STAT_ATTR_NUM][STAT_ATTR_KEY_LEN];

struct timeval CConnSet::_nowtime;

CConnSet::CConnSet(check_complete func, unsigned max_conn, unsigned rbsize, unsigned wbsize,
	close_callback close_func, mcd_route mcd_route_func, check_complete *func_array)
: _func(func), _func_array(func_array), _mr_func(mcd_route_func), _max_conn(max_conn), _recv_buff_size(rbsize), _send_buff_size(wbsize), _close_func(close_func) {
	
	if(_max_conn < 1)
		_max_conn = 1;
	
	INIT_LIST_HEAD(&_free_ccs);
	INIT_LIST_HEAD(&_used_ccs);
			
	_ccs = new ConnCache[_max_conn];

	_cc_begin_addr = _ccs;
	_cc_end_addr = ( (char *)((char *)_ccs) + _max_conn * sizeof(struct ConnCache) ) - 1;

	assert(_cc_begin_addr >= (void *)MIN_START_ADDR);
	
	_flow_slot_size = _max_conn < 1000000 ? ((unsigned)((double)_max_conn * 1.1415)) : _max_conn;
	_flow_2_cc = new ConnCache*[_flow_slot_size];
	memset(_flow_2_cc, 0x0, sizeof(ConnCache*) * _flow_slot_size);
	ConnCache* cc = NULL;
	for(unsigned i = 0; i < _max_conn; ++i) {
		cc = &_ccs[i];
		
		cc->_r._data = NULL;
		cc->_r._size = 0;
		cc->_r._len = 0;
		cc->_r._offset = 0;
		
		cc->_w._data = NULL;
		cc->_w._size = 0;
		cc->_w._len = 0;
		cc->_w._offset = 0;
		
		//这里只设置收发缓冲区的初始化大小，但是不分配内存，直到第一次使用到才分配内存
		cc->_r._buff_size = _recv_buff_size;
		cc->_w._buff_size = _send_buff_size;
		cc->_start_time.tv_sec = 0;		
		
#ifdef _SPEEDLIMIT_
		cc->_du_ticks = 0;
#endif		
		INIT_LIST_HEAD(&cc->_next);	
		list_add_tail(&cc->_next, &_free_ccs);
		
		cc->_next_cc = NULL;	
	}
	
	memset(&_stat, 0x0, sizeof(CCSStat));
	_stat._min_time = UINT_MAX;

#ifdef _SPEEDLIMIT_
	INIT_LIST_HEAD(&_pending_recv);
	INIT_LIST_HEAD(&_pending_send);
	_download_ticks = 0;
	_upload_ticks = 0;
	_low_buff_size = 0;
#endif
}
CConnSet::~CConnSet() {
	ConnCache* cc = NULL;
	list_head_t *tmp;

	list_for_each_entry_safe_l(cc, tmp, &_used_ccs, _next) {
		if ( is_ccd ) {
			CloseCC(cc, ccd_rsp_disconnect);
		} else {
			CloseCC(cc, dcc_rsp_disconnected);
		}		
	}
	delete [] _ccs;
	delete [] _flow_2_cc;    	
}
//////////////////////////////////////////////////////////////////////////

// ConnCache object, add conn success
// NULL
ConnCache* CConnSet::AddConn(int fd, unsigned long long &flow) {
	static unsigned long long i_flow = (unsigned)time(0);
	ConnCache* cc = NULL;
	
	if(list_empty(&_free_ccs)) {		
		fprintf(stderr, "no free conncache, fd - %d, _max_conn - %u, flow(ccd ignore this) - %llu\n",
				fd, _max_conn, flow);
#ifdef _OUTPUT_LOG
		if ( is_ccd ) {
			if ( output_log ) {
				syslog(LOG_USER | LOG_CRIT | LOG_PID, "MCP++ CCD: No free connection in AddConn!\n");
			}
		} else {
			if ( output_log ) {
				syslog(LOG_USER | LOG_CRIT | LOG_PID, "MCP++ DCC: No free connection in AddConn!\n");
			}
		}
#endif
		return NULL;
	}

	cc = list_entry(_free_ccs.next, struct ConnCache, _next);

	// CCD只产生不大于UINT_MAX的flow，这是为了兼容32位flow，老的应用程序即可不修改代码
	if ( is_ccd ) {
		if(i_flow != SOCK_FLOW_MAX) {
			flow = ++i_flow;
		} else {
			flow = i_flow = SOCK_FLOW_MIN;
		}
	}

	if(_flow_2_cc[flow % _flow_slot_size] != NULL) {
		ConnCache* prev_cc = _flow_2_cc[flow % _flow_slot_size];
		while((prev_cc != NULL) && (prev_cc->_next_cc != NULL))
			prev_cc = prev_cc->_next_cc;
		if(prev_cc)
			prev_cc->_next_cc = cc;
	}
	else {
		_flow_2_cc[flow % _flow_slot_size] = cc;
	}
	
	cc->_next_cc = NULL;
	cc->_access = _nowtime.tv_sec;
	cc->_flow = flow;
	cc->_fd = fd;
	cc->_flag = 0;
	if ( is_ccd ) {
		static struct sockaddr_in _address = {0};
		static socklen_t _address_len = sizeof(struct sockaddr_in);
		getpeername(fd, (sockaddr*)&_address, &_address_len);
		
		cc->_ip = _address.sin_addr.s_addr;
		cc->_port = _address.sin_port;
	}
	
	list_move_tail(&cc->_next, &_used_ccs);
	_stat._conn_num++;
	
	return cc;
}

inline int CConnSet::RecvCC(ConnCache* cc, char* buff, size_t buff_size, size_t& recvd_len) {
	
	static CSocketTCP sock(-1, false);
	sock.attach(cc->_fd);
	int ret;
	
	cc->_access = _nowtime.tv_sec;
	list_move_tail(&cc->_next, &_used_ccs);

#ifdef _SPEEDLIMIT_
	unsigned u_ticks = cc->_du_ticks ? cc->_du_ticks : _upload_ticks;
	if(u_ticks) {
		unsigned long long nowticks = GetNowTick();
		//assert SEGMENT_SIZE >= buff_size
		ret = sock.receive(buff, SEGMENT_SIZE, recvd_len);		
		if(recvd_len == SEGMENT_SIZE) {
			unsigned useticks = (unsigned)(GetNowTick() - nowticks);
			if(useticks < u_ticks) {
				cc->_deadline_tick = nowticks + u_ticks - 1000;//这个是微调因子，使得限速更精准
				list_move_tail(&cc->_next, &_pending_recv);
				ret = 1;
			}	
		}
		return ret;
	}
	else {
#endif	
	ret = sock.receive(buff, buff_size, recvd_len);
	if(recvd_len > 0)
		_stat._total_recv_size += recvd_len;
	return ret;

#ifdef _SPEEDLIMIT_
	}
#endif
}
// 0, recv some data
// -E_NEED_CLOSE
// -E_NEED_RECV
int CConnSet::Recv(ConnCache* cc) {
	
	static char buffer[C_READ_BUFFER_SIZE];
	size_t recvd_len; 
	int ret;

	//////// For debug next FIN ///////
	// int c_flag = 0;

	while( !stop ) {
		if ( recv_buff_max && cc->_r.data_len() > recv_buff_max ) {
			if ( is_ccd ) {
				if ( output_log ) {
					syslog(LOG_USER | LOG_CRIT | LOG_PID, "MCP++ CCD: recive buffer max reached! Close cc. cc->_fd - %d, cc->_flow - %llu, cc->_ip - %u, cc->_r.data_len() - %u.\n",
						cc->_fd, cc->_flow, cc->_ip, cc->_r.data_len());
				}
			} else {
				if ( output_log ) {
					syslog(LOG_USER | LOG_CRIT | LOG_PID, "MCP++ DCC: recive buffer max reached! Close cc. cc->_fd - %d, cc->_flow - %llu, cc->_ip - %u, cc->_r.data_len() - %u.\n",
						cc->_fd, cc->_flow, cc->_ip, cc->_r.data_len());
				}
			}
			return -E_NEED_CLOSE;
		}
		
		recvd_len = 0;
		ret = RecvCC(cc, buffer, C_READ_BUFFER_SIZE, recvd_len);

		if(ret == 0) {
			if (recvd_len > 0) {
				if(cc->_r.append(buffer, recvd_len) < 0)
					return -E_MEM_ALLOC;
				if(recvd_len < C_READ_BUFFER_SIZE) {
					// Next FIN?
					////////
					// c_flag = 1;
					continue;
					// return 0;
				}
			}
			else {
				// if ( c_flag ) {
				//	fprintf(stderr, "FIN debug: Close after recive datalen < bufsize!\n");
				// }
				//如果读缓存还有未处理的数据则需要延迟关闭
				if(cc->_r.data_len() > 0) {
					cc->_finclose = 1;
					return 0;
				}
				else
					return -E_NEED_CLOSE;
			}
		}
#ifdef _SPEEDLIMIT_
		else if(ret == 1) {
			if(cc->_r.append(buffer, recvd_len) < 0)
				return -E_MEM_ALLOC;
			return -E_NEED_PENDING;			
		}
#endif				
		else {
			if (ret != -EAGAIN) {	
				//如果读缓存还有未处理的数据则需要延迟关闭
				if(cc->_r.data_len() > 0) {
					cc->_finclose = 1;
#ifdef _OUTPUT_LOG
					if ( is_ccd ) {
						if ( output_log ) {
							syslog(LOG_USER | LOG_CRIT | LOG_PID, "MCP++ CCD: RecvCC error in Recv! Data left, so close later. flow - %llu, ip - %u, port - %hu. %m\n",
								cc->_flow, cc->_ip, cc->_port);
						}
					} else {
						if ( output_log ) {
							syslog(LOG_USER | LOG_CRIT | LOG_PID, "MCP++ DCC: RecvCC error in Recv! Data left, so close later. flow - %llu, ip - %u, port - %hu. %m\n",
								cc->_flow, cc->_ip, cc->_port);
						}
					}
#endif
					return 0;
				}
				else {
#ifdef _OUTPUT_LOG
					if ( is_ccd ) {
						if ( output_log ) {
							syslog(LOG_USER | LOG_CRIT | LOG_PID, "MCP++ CCD: RecvCC error in Recv! flow - %llu, ip - %u, port - %hu. %m\n",
								cc->_flow, cc->_ip, cc->_port);
						}
					} else {
						if ( output_log ) {
							syslog(LOG_USER | LOG_CRIT | LOG_PID, "MCP++ DCC: RecvCC error in Recv! flow - %llu, ip - %u, port - %hu. %m\n",
								cc->_flow, cc->_ip, cc->_port);
						}
					}
#endif

					return -E_NEED_CLOSE;
				}
			}
			else {	
				return -E_NEED_RECV;
			}
		}
	}

	return -E_NEED_CLOSE;
}
inline int CConnSet::SendCC(ConnCache* cc, const char* data, size_t data_len, size_t& sent_len) {
	static CSocketTCP sock(-1, false);
	sock.attach(cc->_fd);
	int ret;
	
	cc->_access = _nowtime.tv_sec;
	list_move_tail(&cc->_next, &_used_ccs);

#ifdef _SPEEDLIMIT_
	unsigned d_ticks = cc->_du_ticks ? cc->_du_ticks : _download_ticks;
	if(d_ticks) {	
		int len = data_len > SEGMENT_SIZE ? SEGMENT_SIZE : data_len;
		unsigned long long nowticks = GetNowTick();
		ret = sock.send(data, len, sent_len);
		if(ret == 0) {
			unsigned useticks = (unsigned)(GetNowTick() - nowticks);
			if((useticks < d_ticks) && (len == SEGMENT_SIZE)) {
				cc->_deadline_tick = nowticks + d_ticks - 1000;//这个是微调因子，使得限速更精准
				list_move_tail(&cc->_next, &_pending_send);
				ret = 1;
			}
			_stat._total_send_size += sent_len;
		}
		return ret;
	}
	else {
#endif			
	ret = sock.send(data, data_len, sent_len);
	if(ret == 0)
		_stat._total_send_size += sent_len;
	return ret;

#ifdef _SPEEDLIMIT_
	}
#endif	
}
// 0, send failed or send not complete, add epollout
// 1, send complete
int CConnSet::SendForce(ConnCache* cc, const char* data, size_t data_len) {
	
	size_t sent_len = 0; 
	int ret = 0;

	if ( send_buff_max && cc->_w.data_len() > send_buff_max ) {
		if ( is_ccd ) {
			if ( output_log ) {
				syslog(LOG_USER | LOG_CRIT | LOG_PID, "MCP++ CCD: send buffer max reached! Close cc. cc->_fd - %d, cc->_flow - %llu, cc->_ip - %u, cc->_w.data_len() - %u.\n",
		  			cc->_fd, cc->_flow, cc->_ip, cc->_w.data_len());
			}
		} else {
			if ( output_log ) {
				syslog(LOG_USER | LOG_CRIT | LOG_PID, "MCP++ DCC: send buffer max reached! Close cc. cc->_fd - %d, cc->_flow - %llu, cc->_ip - %u, cc->_w.data_len() - %u.\n",
		  			cc->_fd, cc->_flow, cc->_ip, cc->_w.data_len());
			}
		}
		return -E_NEED_CLOSE;
	}

	ret = SendCC(cc, data, data_len, sent_len);
		
	if (ret < 0) {
		if(ret == -EAGAIN) {
			if(cc->_w.append(data, data_len) < 0)
				return -E_MEM_ALLOC;
		}
		else {
			return -E_NEED_CLOSE;
		}
	}
	else if(ret == 0 && sent_len < data_len) {
		if(cc->_w.append(data + sent_len, data_len - sent_len) < 0)
			return -E_MEM_ALLOC;
	}
	else if(ret == 0 && sent_len == data_len) {
		return 1;
	}
#ifdef _SPEEDLIMIT_
	else if(ret == 1) {
		if(sent_len < data_len)
			if(cc->_w.append(data + sent_len, data_len - sent_len) < 0)
				return -E_MEM_ALLOC;
		return -E_NEED_PENDING;		
	}
#endif		
	return 0;
}
// 1 send completely
// 0 send part
// -E_NEED_CLOSE
int CConnSet::Send(ConnCache* cc, const char* data, size_t data_len) {
	size_t sent_len = 0; 
	int ret = 0;

	if ( send_buff_max && cc->_w.data_len() > send_buff_max ) {
		if ( is_ccd ) {
			if ( output_log ) {
				syslog(LOG_USER | LOG_CRIT | LOG_PID, "MCP++ CCD: send buffer max reached! Close cc. cc->_fd - %d, cc->_flow - %llu, cc->_ip - %u, cc->_w.data_len() - %u.\n",
		   			cc->_fd, cc->_flow, cc->_ip, cc->_w.data_len());
			}
		} else {
			if ( output_log ) {
				syslog(LOG_USER | LOG_CRIT | LOG_PID, "MCP++ DCC: send buffer max reached! Close cc. cc->_fd - %d, cc->_flow - %llu, cc->_ip - %u, cc->_w.data_len() - %u.\n",
		   			cc->_fd, cc->_flow, cc->_ip, cc->_w.data_len());
			}
		}
		return -E_NEED_CLOSE;
	}
	
	if (cc->_w.data_len() != 0) {
		ret = SendCC(cc, cc->_w.data(), (size_t)cc->_w.data_len(), sent_len);
		if (ret == 0) {
			if(sent_len < cc->_w.data_len()) {
				cc->_w.skip(sent_len);
				if(cc->_w.append(data, data_len) < 0)
					return -E_MEM_ALLOC;
			
				return 0;
			}
			else {
				cc->_w.skip(cc->_w.data_len());
			}
		}
#ifdef _SPEEDLIMIT_
		else if(ret == 1) {
			cc->_w.skip(sent_len);
			if(cc->_w.append(data, data_len) < 0)
				return -E_MEM_ALLOC;
			return -E_NEED_PENDING;
		}
#endif		
		else if(ret == -EAGAIN) {
			if(cc->_w.append(data, data_len) < 0)
				return -E_MEM_ALLOC;
			return 0;
		}			
		else
			return -E_NEED_CLOSE;
	}

	sent_len = 0;
	ret = SendCC(cc, data, data_len, sent_len);
	if (ret < 0 && ret != -EAGAIN) {
		return -E_NEED_CLOSE;
	}

	if (sent_len < data_len ) {
		if(cc->_w.append(data + sent_len, data_len - sent_len) < 0)
			return -E_MEM_ALLOC;
#ifdef _SPEEDLIMIT_
		if(ret == 1)
			return -E_NEED_PENDING;
#endif
		return 0;
	}
	else {
		return 1;
	}
}
// 0, send complete
// -E_FORCE_CLOSE, send complete and close
// -E_NEED_SEND 
// -E_NEED_CLOSE
int CConnSet::SendFromCache(ConnCache* cc) {
	if ( send_buff_max && cc->_w.data_len() > send_buff_max ) {
		if ( is_ccd ) {
			if ( output_log ) {
				syslog(LOG_USER | LOG_CRIT | LOG_PID, "MCP++ CCD: send buffer max reached! Close cc. cc->_fd - %d, cc->_flow - %llu, cc->_ip - %u, cc->_w.data_len() - %u.\n",
		   			cc->_fd, cc->_flow, cc->_ip, cc->_w.data_len());
			}
		} else {
			if ( output_log ) {
				syslog(LOG_USER | LOG_CRIT | LOG_PID, "MCP++ DCC: send buffer max reached! Close cc. cc->_fd - %d, cc->_flow - %llu, cc->_ip - %u, cc->_w.data_len() - %u.\n",
		   			cc->_fd, cc->_flow, cc->_ip, cc->_w.data_len());
			}
		}
		return -E_NEED_CLOSE;
	}
	
	if (cc->_w.data_len() == 0)
		return 0;
	
	size_t sent_len = 0; 
	int ret = SendCC(cc, cc->_w.data(), cc->_w.data_len(), sent_len);
	 
#ifdef _SPEEDLIMIT_
	if(ret >= 0) {
#else	 
	if(ret == 0) {
#endif
	    if(sent_len == 0) {
            return -E_NEED_SEND;
        }
        else {
    		if(sent_len == cc->_w.data_len()) {
    			cc->_w.skip(sent_len);	
				//发送完成时检查是否有主动关闭连接标识
				if(cc->_finclose) {
					if ( is_ccd ) {
						CloseCC(cc, ccd_rsp_disconnect);
					} else {
						CloseCC(cc, dcc_rsp_disconnected);
					}
					return -E_FORCE_CLOSE;
				}
				else 
					return 0;
    		}
    		else {
    			cc->_w.skip(sent_len);
#ifdef _SPEEDLIMIT_
				if(ret == 1) {
					if(cc->_w.data_len() < _low_buff_size)
						return -E_NEED_PENDING_NOTIFY;
					else		
						return -E_NEED_PENDING;
				}	
				else				  
#endif    			
					return -E_NEED_SEND;
    		}
    	}
	}
	else if (ret == -EAGAIN) {
		// 继续发
		return -E_NEED_SEND;
	}
	else {
		return -E_NEED_CLOSE;
	}
}
// 0, data complete
// -E_NEED_CLOSE
// -E_NEED_RECV
#ifdef _SHMMEM_ALLOC_
int CConnSet::GetMessage(ConnCache* cc, void* buf, unsigned buf_size, unsigned& data_len, bool* is_shm_alloc) {
#else
int CConnSet::GetMessage(ConnCache* cc, void* buf, unsigned buf_size, unsigned& data_len) {
#endif
	int ret = 0;

	if ( !is_ccd ) {
		ret = _func(cc->_r.data(), cc->_r.data_len());
	} else {
		if ( _func_array ) {
			if ( _func_array[cc->_listen_port] ) {
				ret = _func_array[cc->_listen_port](cc->_r.data(), cc->_r.data_len());
			} else {
				ret = _func(cc->_r.data(), cc->_r.data_len());
			}
		} else {
			ret = _func(cc->_r.data(), cc->_r.data_len());
		}
	}
	
	unsigned mr_ret;

	if(ret > 0) {
		if ((unsigned)ret > buf_size) {	//数据异常（需要的请求包长度比实际收到的要长） 
			fprintf(stderr, "need close: ret > buf_size, %u, %u\n", (unsigned) ret, buf_size);
#ifdef _OUTPUT_LOG
			if ( is_ccd ) {
				if ( output_log ) {
					syslog(LOG_USER | LOG_CRIT | LOG_PID, "MCP++ CCD: Datalen > buffer size after packet complete check! flow - %llu, ip - %u, port - %hu.\n",
						cc->_flow, cc->_ip, cc->_port);
				}
			} else {
				if ( output_log ) {
					syslog(LOG_USER | LOG_CRIT | LOG_PID, "MCP++ DCC: Datalen > buffer size after packet complete check! flow - %llu, ip - %u, port - %hu.\n",
						cc->_flow, cc->_ip, cc->_port);
				}
			}
#endif
			return -E_NEED_CLOSE;
		}
		else {							//数据完整
			// MCD route:
			if(is_ccd && _mr_func) {
				mr_ret = _mr_func(cc->_r.data(), ret, cc->_flow, cc->_listen_port, req_mq_num);
				if ( mr_ret < req_mq_num ) {
					cc->_reqmqidx = mr_ret;
				} else {
					// Use last mq index.
				}
			}
			/////////////
#ifdef _SHMMEM_ALLOC_
			if(is_shm_alloc != NULL) {
				memhead* head = (memhead*)buf;		
				head->mem = myalloc_alloc(ret);
				if(head->mem != NULL_HANDLE) {	//分配共享内存成功
					*is_shm_alloc = true;
					memcpy(myalloc_addr(head->mem), cc->_r.data(), ret);
					head->len = ret;
					data_len = sizeof(memhead);
				}
				else {							//分配失败，使用本地内存
					*is_shm_alloc = false;
					memcpy(buf, cc->_r.data(), ret);
					data_len = ret;
				}
			}
			else {		
#endif
				memcpy(buf, cc->_r.data(), ret);
				data_len = ret;
#ifdef _SHMMEM_ALLOC_		
			}
#endif			
			cc->_r.skip(ret);
			return 0;
		}
	}
	else if(ret == 0) {					//数据未完整
		data_len = 0;
		return -E_NEED_RECV;
	}
	else {								//数据异常
#ifdef _OUTPUT_LOG
		if ( is_ccd ) {
			if ( output_log ) {
				syslog(LOG_USER | LOG_CRIT | LOG_PID, "MCP++ CCD: Packet complete check fail! flow - %llu, ip - %u, port - %hu.\n",
					cc->_flow, cc->_ip, cc->_port);
			}
		} else {
			if ( output_log ) {
				syslog(LOG_USER | LOG_CRIT | LOG_PID, "MCP++ DCC: Packet complete check fail! flow - %llu, ip - %u, port - %hu.\n",
					cc->_flow, cc->_ip, cc->_port);
			}
		}
#endif
		return -E_NEED_CLOSE;
	}
}
void CConnSet::CloseCC(ConnCache* cc, unsigned short event) {
	if(_close_func) {
		if ( close_notify_details ) {
			_close_func(cc, event);
		} else {
			if ( is_ccd ) {
				_close_func(cc, ccd_rsp_disconnect);
			} else {
				_close_func(cc, dcc_rsp_disconnected);
			}
		}
	}

	if(cc->_fd > 0) {
		close(cc->_fd);
		cc->_fd = -1;
	}
		
	cc->_w.reinit();
	cc->_r.reinit();
	
	cc->_start_time.tv_sec = 0;
#ifdef _SPEEDLIMIT_
	cc->_du_ticks = 0;
#endif
	
	ConnCache* prev_cc = _flow_2_cc[cc->_flow % _flow_slot_size];
	if(prev_cc == cc) {
		_flow_2_cc[cc->_flow % _flow_slot_size] = cc->_next_cc;
	} else {
		while((prev_cc != NULL) && (prev_cc->_next_cc != cc))
			prev_cc = prev_cc->_next_cc;
		if(prev_cc)
			prev_cc->_next_cc = cc->_next_cc;
	}
	cc->_next_cc = NULL;

	list_move(&cc->_next, &_free_ccs);		
	_stat._conn_num--;
}
void CConnSet::CheckTimeout(time_t access_deadline) {
	ConnCache* cc = NULL;
	list_head_t *tmp;

	list_for_each_entry_safe_l(cc, tmp, &_used_ccs, _next) {
		if(cc->_access < access_deadline) {
			if ( is_ccd ) {
				CloseCC(cc, ccd_rsp_disconnect_timeout);
			} else {
				CloseCC(cc, dcc_rsp_disconnect_timeout);
			}
		}
		else
			break;
	}    	
}
// 0, delay close
// 1, closed
int CConnSet::TryCloseCC(ConnCache* cc, unsigned short event) {
	if(cc->_w.data_len() > 0) {
		//发送缓冲区还有数据未发送完，标识延迟关闭标识
		cc->_finclose = 1;
		return 0;
	}
	else {
		CloseCC(cc, event);
		return 1;
	}
}

void CConnSet::Watch(unsigned cc_timeout, unsigned cc_stattime) {
	static time_t cur_time = time(0);
	static time_t last_check_time = cur_time;
	static time_t last_stat_time = cur_time;
	static CCSStat stat;
	int			dcc_idx = 0;
	
#ifndef _SPEEDLIMIT_
	gettimeofday(&_nowtime, NULL);
#endif

	cur_time = _nowtime.tv_sec;

	if((unsigned)(cur_time - last_check_time) > cc_timeout / 5) {
		CheckTimeout(cur_time - cc_timeout);
		last_check_time = cur_time;
	}

	if((unsigned)(cur_time - last_stat_time) >= cc_stattime) {
		GetStatResult(&stat);
	
		//统计信息上报给watchdog
		if ( wtg ) {
			if ( is_ccd ) {
				ccd_wd_attrs[0].i_val = stat._msg_count;
				ccd_wd_attrs[1].i_val = stat._max_time;
				ccd_wd_attrs[2].i_val = stat._min_time;
				ccd_wd_attrs[3].i_val = stat._avg_time;
				ccd_wd_attrs[4].i_val = (unsigned long)(stat._total_recv_size >> 10);
				ccd_wd_attrs[5].i_val = (unsigned long)(stat._total_send_size >> 10);
				ccd_wd_attrs[6].i_val = stat._conn_num;
				wtg->ReportAttr(ccd_wd_attrs, STAT_ATTR_NUM);
				eth_flow_report((ccd_stat_time / 10 ) != 0 ? (ccd_stat_time / 10 ) : 1);
			} else {
				if ( !dcc_report_inited ) {			
					if ( wdc && wdc->IsInited() ) {
						dcc_idx = wdc->Index();
					}
				
					memset(dcc_attr_key, 0, STAT_ATTR_NUM * STAT_ATTR_KEY_LEN * sizeof(char));

					snprintf(dcc_attr_key[0], STAT_ATTR_KEY_LEN - 1, "dcc#msgcount_%d", dcc_idx);
					snprintf(dcc_attr_key[1], STAT_ATTR_KEY_LEN - 1, "dcc#maxtime_%d", dcc_idx);
					snprintf(dcc_attr_key[2], STAT_ATTR_KEY_LEN - 1, "dcc#mintime_%d", dcc_idx);
					snprintf(dcc_attr_key[3], STAT_ATTR_KEY_LEN - 1, "dcc#avgtime_%d", dcc_idx);
					snprintf(dcc_attr_key[4], STAT_ATTR_KEY_LEN - 1, "dcc#recvsize_%d", dcc_idx);
					snprintf(dcc_attr_key[5], STAT_ATTR_KEY_LEN - 1, "dcc#sendsize_%d", dcc_idx);
					snprintf(dcc_attr_key[6], STAT_ATTR_KEY_LEN - 1, "dcc#connnum_%d", dcc_idx);

					dcc_wd_attrs[0].key = dcc_attr_key[0];
					dcc_wd_attrs[1].key = dcc_attr_key[1];
					dcc_wd_attrs[2].key = dcc_attr_key[2];
					dcc_wd_attrs[3].key = dcc_attr_key[3];
					dcc_wd_attrs[4].key = dcc_attr_key[4];
					dcc_wd_attrs[5].key = dcc_attr_key[5];
					dcc_wd_attrs[6].key = dcc_attr_key[6];

					dcc_report_inited = 1;
				}
			
				dcc_wd_attrs[0].i_val = stat._msg_count;
				dcc_wd_attrs[1].i_val = stat._max_time;
				dcc_wd_attrs[2].i_val = stat._min_time;
				dcc_wd_attrs[3].i_val = stat._avg_time;
				dcc_wd_attrs[4].i_val = (unsigned long)(stat._total_recv_size >> 10);
				dcc_wd_attrs[5].i_val = (unsigned long)(stat._total_send_size >> 10);
				dcc_wd_attrs[6].i_val = stat._conn_num;
				
				wtg->ReportAttr(dcc_wd_attrs, STAT_ATTR_NUM);
			}
		}
		
		syslog(LOG_USER | LOG_CRIT | LOG_PID, "STAT conn_num=%u,msg_count=%u,max_time=%u,min_time=%u,avg_time=%u,recv_size=%llu,send_size=%llu\n",
			stat._conn_num,
			stat._msg_count,
			stat._max_time,
			stat._min_time,
			stat._avg_time,
			stat._total_recv_size,
			stat._total_send_size
		   );
		last_stat_time = cur_time; 
	}
}
#ifdef _SPEEDLIMIT_
void CConnSet::SetSpeedLimit(unsigned download_speed, unsigned upload_speed, unsigned low_buff_size) {
	
	if(download_speed > 0)
		_download_ticks = (unsigned)(((double)SEGMENT_SIZE / (double)download_speed) * 1000); 
	if(upload_speed > 0)
		_upload_ticks = (unsigned)(((double)SEGMENT_SIZE / (double)upload_speed) * 1000); 
	if(low_buff_size > 0)
		_low_buff_size = low_buff_size << 10;//kbytes-->bytes
}
#endif
