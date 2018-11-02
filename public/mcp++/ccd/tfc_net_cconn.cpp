#include <sys/time.h>
#include <sys/epoll.h>
#include <assert.h>
#include <errno.h>
#include <syslog.h>
#include <stdio.h>
#include <malloc.h>


#include "ccd/over_ctrl.h"
#include "ccd/tfc_net_cconn.h"
#include "ccd/tfc_net_socket_tcp.h"
#include "ccd/tfc_net_socket_udp.h"
#include "ccd/tfc_net_ccd_define.h"
#include "dcc/tfc_net_dcc_define.h"
#include "watchdog/tfc_base_watchdog_client.h"

#ifdef _SHMMEM_ALLOC_
#include "ccd/myalloc.h"
#endif
#include "ccd/mydaemon.h"

#include "common/log_client.h"
#include "ccd/app/server_net_handler.h"
#include "dcc/app/client_net_handler.h"


#define SOCK_FLOW_MIN       2           /* socket flow 最小 */
#define SOCK_FLOW_MAX       UINT_MAX    /* socket flow 最大 */
#define C_READ_BUFFER_SIZE  (1<<16)
#define STAT_ATTR_NUM       7
#define STAT_ATTR_VAL_MAX   20
#define STAT_ATTR_KEY_LEN   64

using namespace tfc::net;
using namespace std;
using namespace tfc::watchdog;

/* true-运行，false-退出 */
extern bool                 stop;
/* watchdog client */
extern CWatchdogClient     *wdc;
extern int                  is_ccd;
/* 0 not output log, others output log. */
extern unsigned             output_log;
extern unsigned             req_mq_num;
static int                  dcc_report_inited = 0;
int                         dcc_request_msg_timeout_thresh = 0;
/*
 * 当该项被配置时，连接的Sendbuf数据堆积超过该值则认为出错并关闭连接，
 * 为0时不启用该检测
 */
extern unsigned             send_buff_max;
/*
 * 当该项被配置时，连接的Recvbuf数据堆积超过该值则认为出错并关闭连接，
 * 为0时不启用该检测
 */
extern unsigned             recv_buff_max;
/* 启用连接关闭细节事件，etvent_notify启用时配置方有效 */
extern bool                 close_notify_details;
/* ms. */
extern unsigned long long   udp_msg_timeout;
extern unsigned             udp_send_buff_max;
extern unsigned             udp_send_count_max;

extern bool                 sync_enabled;
extern tools::CLogClient*   log_client;
extern tools::NetLogInfo    net_log_info;
extern tools::MemLogInfo    mem_log_info;
extern tools::CCDStatInfo   net_stat_info;

static char dcc_attr_key[STAT_ATTR_NUM][STAT_ATTR_KEY_LEN];

struct timeval CConnSet::_monotonic_clock_nowtime = tools::GET_MONOTONIC_CLOCK();
struct timeval CConnSet::_wall_clock_nowtime = tools::GET_WALL_CLOCK();

void write_net_log(unsigned event_type, ConnCache* cc,
                   unsigned data_len = 0, unsigned mq_index = -1,
                   unsigned short err = 0, unsigned wait_time = 0);
void write_net_log(unsigned event_type, unsigned long long flow,
                   unsigned data_len = 0, unsigned mq_index = -1,
                   unsigned short err = 0, unsigned wait_time = 0);
void write_mem_log(unsigned event_type, ConnCache* cc,
                   unsigned ip = 0, unsigned short port = 0);

static int cc_compare(const void *lhs, const void *rhs) {
    uint32_t lhs_buff_size =
        (*(ConnCache**)lhs)->_r->size() + (*(ConnCache**)lhs)->_w->size();
    uint32_t rhs_buff_size =
        (*(ConnCache**)rhs)->_r->size() + (*(ConnCache**)rhs)->_w->size();
    /* note: rhs - lhs, reverse sort */
    return rhs_buff_size - lhs_buff_size;
}

CConnSet::CConnSet(app::ServerNetHandler* ccd_net_handler,
                   app::ClientNetHandler* dcc_net_handler,
                   unsigned max_conn,
                   unsigned rbsize,
                   unsigned wbsize,
                   close_callback close_func,
                   send_notify_2_mcd_call_back send_notify_2_mcd):
               _close_func(close_func),
               _send_notify_2_mcd(send_notify_2_mcd),
               _max_conn(max_conn),
               _recv_buff_size(rbsize),
               _send_buff_size(wbsize),
               _ccd_net_handler(ccd_net_handler),
               _dcc_net_handler(dcc_net_handler)
{
    if (_max_conn < 1) {
        _max_conn = 1;
    }

    INIT_LIST_HEAD(&_free_ccs);
    INIT_LIST_HEAD(&_used_ccs);

    _ccs = new ConnCache[_max_conn];

    _cc_begin_addr = _ccs;
    _cc_end_addr = ((char *)((char *)_ccs)
                   + _max_conn * sizeof(struct ConnCache)) - 1;

    assert(_cc_begin_addr >= (void *)MIN_START_ADDR);

    _flow_slot_size = _max_conn < 1000000
                      ? ((unsigned)((double)_max_conn * 1.1415))
                      : _max_conn;
    _flow_2_cc = new ConnCache*[_flow_slot_size];
    memset(_flow_2_cc, 0x0, sizeof(ConnCache*) * _flow_slot_size);

    for (unsigned i = 0; i < _max_conn; ++i) {
        ConnCache* cc = &_ccs[i];

        cc->_r = new CRawCache();
        if (!is_ccd && dcc_request_msg_timeout_thresh > 0) {
            cc->_w = new CTimeRawCache();
        } else {
            cc->_w = new CRawCache();
        }

        cc->_r->_data = NULL;
        cc->_r->_size = 0;
        cc->_r->_len = 0;
        cc->_r->_offset = 0;

        cc->_w->_data = NULL;
        cc->_w->_size = 0;
        cc->_w->_len = 0;
        cc->_w->_offset = 0;

        cc->_type = cc_tcp;     /* Default set to TCP. */

        if (sync_enabled) {
            cc->_smachine = cc_stat_sync;
        } else {
            cc->_smachine = cc_stat_data;
        }

        /*
         * 这里只设置收发缓冲区的初始化大小，但是不分配内存，
         * 直到第一次使用到才分配内存
         */
        cc->_r->_buff_size = _recv_buff_size;
        cc->_w->_buff_size = _send_buff_size;
        cc->_start_time.tv_sec = 0;

#ifdef _SPEEDLIMIT_
        cc->_set_send_speed = 0;
        cc->_set_recv_speed = 0;
        cc->_recv_mon.init(20, 100);
        cc->_send_mon.init(20, 100);

        INIT_LIST_HEAD(&cc->_pending_send_next);
        INIT_LIST_HEAD(&cc->_pending_recv_next);
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
    _config_send_speed = 0;
    _config_recv_speed = 0;
    _low_buff_size = 0;
#endif

    /* for handling memory overload */
    _normal_ccs = new ConnCache*[_max_conn];
    memset(_normal_ccs, 0x0, sizeof(ConnCache*) * _max_conn);
    _enlarged_ccs = new ConnCache*[_max_conn];
    memset(_enlarged_ccs, 0x0, sizeof(ConnCache*) * _max_conn);
}

CConnSet::~CConnSet()
{
    ConnCache*   cc = NULL;
    list_head_t* tmp;

    list_for_each_entry_safe_l(cc, tmp, &_used_ccs, _next) {
        if (is_ccd) {
            CloseCC(cc, ccd_rsp_disconnect);
        } else {
            CloseCC(cc, dcc_rsp_disconnected);
        }
        delete cc->_r;
        delete cc->_w;
    }

#ifdef _SPEEDLIMIT_
    for (unsigned i = 0; i<_max_conn; i++) {
        _ccs[i]._send_mon.fini();
        _ccs[i]._recv_mon.fini();
    }
#endif

    delete [] _ccs;
    delete [] _flow_2_cc;
    delete [] _normal_ccs;
    delete [] _enlarged_ccs;
}

//////////////////////////////////////////////////////////////////////////
int CConnSet::GetCcdFlow(unsigned long long& flow)
{
    static const unsigned long long threshold =
        (unsigned long long)(SOCK_FLOW_MAX);

    static unsigned long long i_flow = (unsigned)time(0);
    if (i_flow + 2 >= threshold) {
        i_flow = SOCK_FLOW_MIN;
    } else {
        i_flow = i_flow + 1;
    }

    int i = 0;
    while (GetConnCache(i_flow) && ((++i) < 1000)) {
        if (i_flow + 1 >= threshold) {
            i_flow = SOCK_FLOW_MIN;
        } else {
            i_flow++;
        }
    }

    if (i >= 1000) {
        /* Find fail! */
        return -1;
    }

    flow = i_flow;
    return 0;
}

/*
 * ConnCache object, add conn success
 * NULL
 */
ConnCache* CConnSet::AddConn(int fd, unsigned long long &flow, int type)
{
    ConnCache* cc = NULL;
    unsigned long long tmp_flow = 0;

    /*
     * CCD只产生不大于UINT_MAX的flow，这是为了兼容32位flow，
     * 老的应用程序即可不修改代码
     */
    if (is_ccd) {
        if (GetCcdFlow(tmp_flow)) {
#ifdef _OUTPUT_LOG_
            WRITE_LOG(output_log, "MCP++ CCD: Find free flow fail!\n");
#endif
            write_net_log(tools::LOG_NO_FREE_FLOW, tmp_flow);
            return NULL;
        }

        flow = tmp_flow;
    }

    if (list_empty(&_free_ccs)) {
        fprintf(stderr, "no free conncache, fd - %d, _max_conn - %u, "
                "flow(ccd ignore this) - %llu\n", fd, _max_conn, flow);
#ifdef _OUTPUT_LOG_
        if (is_ccd) {
            WRITE_LOG(output_log,
                      "MCP++ CCD: No free connection in AddConn!\n");
        } else {
            WRITE_LOG(output_log,
                      "MCP++ DCC: No free connection in AddConn!\n");
        }
#endif
        write_net_log(tools::LOG_NO_FREE_CC, flow);

        return NULL;
    }

    cc = list_entry(_free_ccs.next, struct ConnCache, _next);

    if (_flow_2_cc[flow % _flow_slot_size] != NULL) {
        ConnCache* prev_cc = _flow_2_cc[flow % _flow_slot_size];
        while ((prev_cc != NULL) && (prev_cc->_next_cc != NULL)) {
            prev_cc = prev_cc->_next_cc;
        }

        if (prev_cc) {
            prev_cc->_next_cc = cc;
        }
    } else {
        _flow_2_cc[flow % _flow_slot_size] = cc;
    }

    cc->_next_cc = NULL;
    cc->_access = _monotonic_clock_nowtime.tv_sec;
    cc->_flow   = flow;
    cc->_fd     = fd;
    cc->_flag   = 0;
    cc->_type   = type;

    if (is_ccd) {
        cc->_epoll_flag = (EPOLLIN);
    } else {
        cc->_epoll_flag = (EPOLLIN | EPOLLOUT);
    }

    if (is_ccd && type == cc_tcp) {
        static struct sockaddr_in _address = {0};
        static socklen_t _address_len = sizeof(struct sockaddr_in);
        getpeername(fd, (sockaddr*)&_address, &_address_len);

        cc->_ip   = _address.sin_addr.s_addr;
        cc->_port = _address.sin_port;
    }

    if (sync_enabled) {
        cc->_smachine = cc_stat_sync;
    } else {
        cc->_smachine = cc_stat_data;
    }

    list_move_tail(&cc->_next, &_used_ccs);
    _stat._conn_num++;

    return cc;
}

inline
int CConnSet::RecvCC(ConnCache* cc, char* buff,
                     size_t buff_size, size_t& recvd_len)
{
    static CSocketTCP sock(-1, false);
    sock.attach(cc->_fd);
    int ret = 0;

    cc->_access = _monotonic_clock_nowtime.tv_sec;
    list_move_tail(&cc->_next, &_used_ccs);

#ifdef _SPEEDLIMIT_
    list_del_init(&cc->_pending_recv_next);

    unsigned recv_speed = get_recv_speed(cc);
    if (recv_speed) {
        size_t seg_size = ((recv_speed / 10) > SEGMENT_SIZE)
                          ? SEGMENT_SIZE
                          : SMALL_SEG_SIZE;

        /* assert SEGMENT_SIZE >= buff_size */
        ret = sock.receive(buff, seg_size, recvd_len);
        unsigned speed = cc->_recv_mon.touch(GetNowTick(), recvd_len);
        if (recvd_len == seg_size) {
            if (recv_speed < speed) {
                list_move_tail(&cc->_pending_recv_next, &_pending_recv);
                ret = 1;
            }
        }
    } else {
#endif
    ret = sock.receive(buff, buff_size, recvd_len);
#ifdef _SPEEDLIMIT_
    }
#endif

    if (recvd_len > 0) {
        _stat._total_recv_size += recvd_len;
        this->_remote_stat_info.UpdateSize(cc->_ip, cc->_port, 0, recvd_len);
        if (_send_notify_2_mcd != NULL) {
            unsigned int recvlen = recvd_len;
            if (is_ccd) {
                _send_notify_2_mcd(ccd_rsp_recv_data, cc, 0,
                                   reinterpret_cast<const char*>(&recvlen),
                                   sizeof(recvlen));
            } else {
                _send_notify_2_mcd(dcc_rsp_recv_data, cc, 0,
                                   reinterpret_cast<const char*>(&recvlen),
                                   sizeof(recvlen));
            }
        }
    }
    return ret;
}

/*
 * 0, recv some data
 * -E_NEED_CLOSE
 * -E_NEED_RECV
 */
int CConnSet::Recv(ConnCache* cc)
{
    static char buffer[C_READ_BUFFER_SIZE];
    size_t recvd_len;
    int ret;
    unsigned loop_cnt = 0;

    while (!stop) {
        if (recv_buff_max && cc->_r->data_len() > recv_buff_max) {
#ifdef _OUTPUT_LOG_
            if (is_ccd) {
                WRITE_LOG(output_log, "MCP++ CCD: recive buffer max reached! "
                          "Close cc. cc->_fd - %d, cc->_flow - %llu, "
                          "cc->_ip - %u, cc->_r->data_len() - %u.\n", cc->_fd,
                          cc->_flow, cc->_ip, cc->_r->data_len());
            } else {
                WRITE_LOG(output_log, "MCP++ DCC: recive buffer max reached! "
                          "Close cc. cc->_fd - %d, cc->_flow - %llu, "
                          "cc->_ip - %u, cc->_r->data_len() - %u.\n", cc->_fd,
                          cc->_flow, cc->_ip, cc->_r->data_len());
            }
#endif
            write_net_log(tools::LOG_RECV_BUF_FULL_CLOSE, cc,
                          cc->_r->data_len());
            _stat.recv_buff_max_count++;
            return -E_NEED_CLOSE;
        }

        loop_cnt++;
        if (loop_cnt > 100) {
            return -E_NEED_RECV;
        }

        recvd_len = 0;
        ret = RecvCC(cc, buffer, C_READ_BUFFER_SIZE, recvd_len);

        if (ret == 0) {
            if (recvd_len > 0) {
                if (cc->_r->append(buffer, recvd_len) < 0) {
                    return -E_MEM_ALLOC;
                }

                if (recvd_len < C_READ_BUFFER_SIZE) {
                    continue;
                }
            } else {
                /* 如果读缓存还有未处理的数据则需要延迟关闭 */
                if (cc->_r->data_len() > 0) {
                    cc->_finclose = 1;
                    return 0;
                } else {
                    return -E_NEED_CLOSE;
                }
            }
        }

#ifdef _SPEEDLIMIT_
        else if (ret == 1) {
            if (cc->_r->append(buffer, recvd_len) < 0) {
                return -E_MEM_ALLOC;
            }
            return -E_NEED_PENDING;
        }
#endif
        else {
            if (ret != -EAGAIN) {
                /* 如果读缓存还有未处理的数据则需要延迟关闭 */
                if (cc->_r->data_len() > 0) {
                    cc->_finclose = 1;
#ifdef _OUTPUT_LOG_
                    if (is_ccd) {
                        WRITE_LOG(output_log, "MCP++ CCD: RecvCC error in "
                                  "Recv! Data left, so close later. "
                                  "flow - %llu, ip - %u, port - %hu. %m\n",
                                  cc->_flow, cc->_ip, cc->_port);
                    } else {
                        WRITE_LOG(output_log, "MCP++ DCC: RecvCC error in "
                                  "Recv! Data left, so close later. "
                                  "flow - %llu, ip - %u, port - %hu. %m\n",
                                  cc->_flow, cc->_ip, cc->_port);
                    }
#endif
                    write_net_log(tools::LOG_RECV_ERR_CLOSE, cc,
                                  cc->_r->data_len(), -1, -ret);
                    return 0;
                } else {
#ifdef _OUTPUT_LOG_
                    if (is_ccd) {
                        WRITE_LOG(output_log, "MCP++ CCD: RecvCC error in "
                                  "Recv! flow - %llu, ip - %u, port - %hu. "
                                  "%m\n", cc->_flow, cc->_ip, cc->_port);
                    } else {
                        WRITE_LOG(output_log, "MCP++ DCC: RecvCC error in "
                                  "Recv! flow - %llu, ip - %u, port - %hu. "
                                  "%m\n", cc->_flow, cc->_ip, cc->_port);
                    }
#endif
                    write_net_log(tools::LOG_RECV_ERR_CLOSE, cc, 0, -1, -ret);
                    return -E_NEED_CLOSE;
                }
            } else {
                return -E_NEED_RECV;
            }
        }
    }

    return -E_NEED_CLOSE;
}

inline
int CConnSet::SendCC(ConnCache* cc, const char* data,
                     size_t data_len, size_t& sent_len)
{
    static CSocketTCP sock(-1, false);
    sock.attach(cc->_fd);
    int ret;

    cc->_access = _monotonic_clock_nowtime.tv_sec;
    list_move_tail(&cc->_next, &_used_ccs);

#ifdef _SPEEDLIMIT_
    list_del_init(&cc->_pending_send_next);

    unsigned send_speed = get_send_speed(cc);

    if (send_speed) {
        size_t seg_size = ((send_speed / 10) > SEGMENT_SIZE)
                          ? SEGMENT_SIZE
                          : SMALL_SEG_SIZE;
        size_t len = data_len > seg_size ? seg_size : data_len;

        ret = sock.send(data, len, sent_len);
        unsigned speed = cc->_send_mon.touch(GetNowTick(), sent_len);
        if (len == seg_size) {
            if (send_speed < speed) {
                list_move_tail(&cc->_pending_send_next, &_pending_send);
                ret = 1;
            }
        }
    } else {
#endif
        ret = sock.send(data, data_len, sent_len);
#ifdef _SPEEDLIMIT_
    }
#endif

    if (sent_len > 0) {
        _stat._total_send_size += sent_len;
        this->_remote_stat_info.UpdateSize(cc->_ip, cc->_port, sent_len, 0);
        if (_send_notify_2_mcd != NULL) {
            unsigned int sendlen = sent_len;
            if (is_ccd) {
                _send_notify_2_mcd(ccd_rsp_send_data, cc, 0,
                                   reinterpret_cast<const char*>(&sendlen),
                                   sizeof(sendlen));
            } else {
                _send_notify_2_mcd(dcc_rsp_send_data, cc, 0,
                                   reinterpret_cast<const char*>(&sendlen),
                                   sizeof(sendlen));
            }
        }
    }
    return ret;
}

/*
 * 0, send failed or send not complete, add epollout
 * 1, send complete
 */
int CConnSet::SendForce(ConnCache* cc, const char* data, size_t data_len)
{
    size_t sent_len = 0;
    int ret = 0;

    if (send_buff_max && cc->_w->data_len() > send_buff_max) {
#ifdef _OUTPUT_LOG_
        if (is_ccd) {
            WRITE_LOG(output_log, "MCP++ CCD: send buffer max reached! Close "
                      "cc. cc->_fd - %d, cc->_flow - %llu, cc->_ip - %u, "
                      "cc->_w->data_len() - %u.\n", cc->_fd, cc->_flow,
                      cc->_ip, cc->_w->data_len());
        } else {
            WRITE_LOG(output_log, "MCP++ DCC: send buffer max reached! Close "
                      "cc. cc->_fd - %d, cc->_flow - %llu, cc->_ip - %u, "
                      "cc->_w->data_len() - %u.\n", cc->_fd, cc->_flow,
                      cc->_ip, cc->_w->data_len());
        }
#endif
        write_net_log(tools::LOG_SEND_BUF_FULL_CLOSE, cc,
                      (cc->_w->data_len() + data_len));
        _stat.send_buff_max_count++;
        return -E_NEED_CLOSE;
    }

    ret = SendCC(cc, data, data_len, sent_len);

    if (ret < 0) {
        if (ret == -EAGAIN) {
            if (cc->_w->append(data, data_len) < 0)
                return -E_MEM_ALLOC;
        } else {
            write_net_log(tools::LOG_SEND_FORCE_ERR, cc,
                          (cc->_w->data_len() + data_len), -1, -ret);
            return -E_NEED_CLOSE;
        }
    } else if (ret == 0 && sent_len < data_len) {
        if (cc->_w->append(data + sent_len, data_len - sent_len) < 0)
            return -E_MEM_ALLOC;
    } else if (ret == 0 && sent_len == data_len) {
        return 1;
    }

#ifdef _SPEEDLIMIT_
    else if (ret == 1) {
        if (sent_len < data_len) {
            if (cc->_w->append(data + sent_len, data_len - sent_len) < 0) {
                return -E_MEM_ALLOC;
            }
        }
        return -E_NEED_PENDING;
    }
#endif

    return 0;
}

/*
 * 1 send completely
 * 0 send part
 * -E_NEED_CLOSE
 */
int CConnSet::Send(ConnCache* cc, const char* data, size_t data_len)
{
    size_t sent_len = 0;
    int ret = 0;

    if (send_buff_max && cc->_w->data_len() > send_buff_max) {
#ifdef _OUTPUT_LOG_
        if (is_ccd) {
            WRITE_LOG(output_log, "MCP++ CCD: send buffer max reached! Close "
                      "cc. cc->_fd - %d, cc->_flow - %llu, cc->_ip - %u, "
                      "cc->_w->data_len() - %u.\n", cc->_fd, cc->_flow,
                      cc->_ip, cc->_w->data_len());
        } else {
            WRITE_LOG(output_log, "MCP++ DCC: send buffer max reached! Close "
                      "cc. cc->_fd - %d, cc->_flow - %llu, cc->_ip - %u, "
                      "cc->_w->data_len() - %u.\n", cc->_fd, cc->_flow,
                      cc->_ip, cc->_w->data_len());
        }
#endif
        write_net_log(tools::LOG_SEND_BUF_FULL_CLOSE, cc,
                      (cc->_w->data_len()+data_len));
        _stat.send_buff_max_count++;
        return -E_NEED_CLOSE;
    }

    if (cc->_w->data_len() != 0) {
        ret = SendCC(cc, cc->_w->data(), (size_t)cc->_w->data_len(), sent_len);
        if (ret == 0) {
            if (sent_len < cc->_w->data_len()) {
                cc->_w->skip(sent_len);
                if (cc->_w->append(data, data_len) < 0) {
                    return -E_MEM_ALLOC;
                }

                return 0;
            } else {
                cc->_w->skip(cc->_w->data_len());
            }
        }

#ifdef _SPEEDLIMIT_
        else if (ret == 1) {
            cc->_w->skip(sent_len);
            if (cc->_w->append(data, data_len) < 0) {
                return -E_MEM_ALLOC;
            }
            return -E_NEED_PENDING;
        }
#endif
        else if (ret == -EAGAIN) {
            if (cc->_w->append(data, data_len) < 0) {
                return -E_MEM_ALLOC;
            }
            return 0;
        } else {
            write_net_log(tools::LOG_SEND_ERR_CLOSE, cc,
                          (cc->_w->data_len()+data_len), -1, -ret);
            return -E_NEED_CLOSE;
        }
    }

    sent_len = 0;
    ret = SendCC(cc, data, data_len, sent_len);
    if (ret < 0 && ret != -EAGAIN) {
        write_net_log(tools::LOG_SEND_ERR_CLOSE, cc, data_len, -1, -ret);
        return -E_NEED_CLOSE;
    }

    if (sent_len < data_len) {
        if (cc->_w->append(data + sent_len, data_len - sent_len) < 0) {
            return -E_MEM_ALLOC;
        }
#ifdef _SPEEDLIMIT_
        if (ret == 1) {
            return -E_NEED_PENDING;
        }
#endif
        return 0;
    } else {
        return 1;
    }
}

/*
 * 0, send complete
 * -E_FORCE_CLOSE, send complete and close
 * -E_NEED_SEND
 * -E_NEED_CLOSE
 */
int CConnSet::SendFromCache(ConnCache* cc)
{
    if (send_buff_max && cc->_w->data_len() > send_buff_max) {
#ifdef _OUTPUT_LOG_
        if (is_ccd) {
            WRITE_LOG(output_log, "MCP++ CCD: send buffer max reached! Close "
                      "cc. cc->_fd - %d, cc->_flow - %llu, cc->_ip - %u, "
                      "cc->_w->data_len() - %u.\n", cc->_fd, cc->_flow,
                      cc->_ip, cc->_w->data_len());
        } else {
            WRITE_LOG(output_log, "MCP++ DCC: send buffer max reached! Close "
                      "cc. cc->_fd - %d, cc->_flow - %llu, cc->_ip - %u, "
                      "cc->_w->data_len() - %u.\n", cc->_fd, cc->_flow, cc->_ip,
                      cc->_w->data_len());
        }
#endif
        write_net_log(tools::LOG_SEND_BUF_FULL_CLOSE, cc, cc->_w->data_len());
        _stat.send_buff_max_count++;
        return -E_NEED_CLOSE;
    }

    if (cc->_w->data_len() == 0) {
        return 0;
    }

    size_t sent_len = 0;
    int ret = SendCC(cc, cc->_w->data(), cc->_w->data_len(), sent_len);

#ifdef _SPEEDLIMIT_
    if (ret >= 0)
#else
    if (ret == 0)
#endif
    {
        if (sent_len == 0) {
            return -E_NEED_SEND;
        } else {
            if (sent_len == cc->_w->data_len()) {
                cc->_w->skip(sent_len);
                /* 发送完成时检查是否有主动关闭连接标识 */
                if (cc->_finclose) {
                    if (is_ccd) {
                        CloseCC(cc, ccd_rsp_disconnect);
                    } else {
                        CloseCC(cc, dcc_rsp_disconnected);
                    }
                    return -E_FORCE_CLOSE;
                }

#ifdef _SPEEDLIMIT_
                /*
                 * 在ret == 1的条件下确保返回E_NEED_PENDING，
                 * ret == 1时cc被放入了pending队列
                 * 这样可以避免在限速条件下发送两次重复的send_ok
                 */
                if (ret == 1) {
                    return -E_NEED_PENDING;
                }
#endif
                return 0;
            } else {
                cc->_w->skip(sent_len);
#ifdef _SPEEDLIMIT_
                if (ret == 1) {
                    if (cc->_w->data_len() < _low_buff_size) {
                        return -E_NEED_PENDING_NOTIFY;
                    } else {
                        return -E_NEED_PENDING;
                    }
                } else
#endif
                    return -E_NEED_SEND;
            }
        }
    } else if (ret == -EAGAIN) {
        /* 继续发 */
        return -E_NEED_SEND;
    } else {
        write_net_log(tools::LOG_SEND_ERR_CLOSE, cc,
                      cc->_w->data_len(), -1, -ret);
        return -E_NEED_CLOSE;
    }
}

/*
 * 0, data complete
 * -E_NEED_CLOSE
 * -E_NEED_RECV
 */
#ifdef _SHMMEM_ALLOC_
int CConnSet::GetMessage(ConnCache* cc, void* buf, unsigned buf_size,
                         unsigned& data_len, bool* is_shm_alloc)
#else
int CConnSet::GetMessage(ConnCache* cc, void* buf, unsigned buf_size,
                         unsigned& data_len)
#endif
{
    int ret = 0, rsp_ret = 0, rsp_len = 0;
    void *user = NULL;

    if (!is_ccd) {
        ret = _dcc_net_handler->check_complete(cc->_r->data(),
                                               cc->_r->data_len(),
                                               cc->_ip, cc->_port);
    } else {
        if (sync_enabled && (cc->_smachine == cc_stat_sync)) {
            ret = _ccd_net_handler->sync_request(cc->_r->data(),
                                                 cc->_r->data_len(), &user);
            if (ret == 0) {
                return -E_NEED_RECV;
            } else if (ret < 0) {
                write_net_log(tools::LOG_SYNC_REQ_FAIL, cc,
                              cc->_r->data_len(), -1, -ret);
                return -E_NEED_CLOSE;
            } else {
                cc->_smachine = cc_stat_data;
                cc->_r->skip(ret);

                rsp_len = _ccd_net_handler->sync_response((char*)buf, buf_size,
                                                          user);
                if (rsp_len <= 0) {
                    write_net_log(tools::LOG_SYNC_RSP_FAIL, cc);
                    return -E_NEED_CLOSE;
                }

                rsp_ret = this->SendForce(cc, (char*)buf, (size_t)rsp_len);
                if (rsp_ret == 1) {
                    return -E_NEED_RECV;
                } else if (rsp_ret == 0) {
                    /* monitor EPOLLOUT here. */
                    return -E_NEED_SEND;
                } else {
                    return -E_NEED_CLOSE;
                }
            }
        } else {
            ret = _ccd_net_handler->check_complete(cc->_r->data(),
                                                   cc->_r->data_len(), cc->_ip,
                                                   cc->_port, cc->_listen_port);
        }
    }

    unsigned mr_ret;

    if (ret > 0) {
        if ((unsigned)ret > buf_size) {
            /* 数据异常（需要的请求包长度比实际收到的要长） */
            fprintf(stderr, "need close: ret > buf_size, %u, %u\n",
                    (unsigned) ret, buf_size);
            _stat.check_complete_err_count++;

#ifdef _OUTPUT_LOG_
            if (is_ccd) {
                WRITE_LOG(output_log, "MCP++ CCD: Datalen > buffer size after "
                          "packet complete check! flow - %llu, ip - %u, "
                          "port - %hu.\n", cc->_flow, cc->_ip, cc->_port);
            } else {
                WRITE_LOG(output_log, "MCP++ DCC: Datalen > buffer size after "
                          "packet complete check! flow - %llu, ip - %u, "
                          "port - %hu.\n", cc->_flow, cc->_ip, cc->_port);
            }
#endif
            if (_send_notify_2_mcd != NULL) {
                if (is_ccd) {
                    _send_notify_2_mcd(ccd_rsp_check_complete_error, cc, 0,
                                       reinterpret_cast<const char*>(&ret),
                                       sizeof(ret));
                } else {
                    _send_notify_2_mcd(dcc_rsp_check_complete_error, cc, 0,
                                       reinterpret_cast<const char*>(&ret),
                                       sizeof(ret));
                }
            }
            write_net_log(tools::LOG_DATA_TOO_LARGE, cc, ret);
            return -E_NEED_CLOSE;
        } else {
            /* 数据完整 */
            if (_send_notify_2_mcd != NULL) {
                if (is_ccd) {
                    _send_notify_2_mcd(ccd_rsp_check_complete_ok, cc, 0,
                                       reinterpret_cast<const char*>(&ret),
                                       sizeof(ret));
                } else {
                    _send_notify_2_mcd(dcc_rsp_check_complete_ok, cc, 0,
                                       reinterpret_cast<const char*>(&ret),
                                       sizeof(ret));
                }
            }
            /* MCD route: */
            if (is_ccd) {
                /*
                 * The default mcd_route_func will return INT_MAX,
                 * which is compatible with old mcp
                 */
                mr_ret =
                    _ccd_net_handler->route_packet(cc->_r->data(), ret, cc->_ip,
                                                   cc->_port, cc->_flow,
                                                   cc->_listen_port,
                                                   req_mq_num);
                if (mr_ret < req_mq_num) {
                    cc->_reqmqidx = mr_ret;
                }
                /* else use route function of mcd_pre_route */
            } else {
                /* dcc notify */
            }
            /////////////
#ifdef _SHMMEM_ALLOC_
            if (is_shm_alloc != NULL) {
                memhead* head = (memhead*)buf;
                head->mem = myalloc_alloc(ret);
                if (head->mem != NULL_HANDLE) {
                    /* 分配共享内存成功 */
                    *is_shm_alloc = true;
                    memcpy(myalloc_addr(head->mem), cc->_r->data(), ret);
                    head->len = ret;
                    data_len = sizeof(memhead);
                } else {
                    /* 分配失败，使用本地内存 */
                    *is_shm_alloc = false;
                    memcpy(buf, cc->_r->data(), ret);
                    data_len = ret;
                }
            } else {
#endif
                memcpy(buf, cc->_r->data(), ret);
                data_len = ret;
#ifdef _SHMMEM_ALLOC_
            }
#endif
            cc->_r->skip(ret);
            return 0;
        }
    } else if (ret == 0) {
        /* 数据未完整 */
        data_len = 0;
        return -E_NEED_RECV;
    } else {
        /* 数据异常 */
        _stat.check_complete_err_count++;
#ifdef _OUTPUT_LOG_
        if (is_ccd) {
            WRITE_LOG(output_log, "MCP++ CCD: Packet complete check fail! "
                      "flow - %llu, ip - %u, port - %hu.\n", cc->_flow, cc->_ip,
                      cc->_port);
        } else {
            WRITE_LOG(output_log, "MCP++ DCC: Packet complete check fail! "
                      "flow - %llu, ip - %u, port - %hu.\n", cc->_flow, cc->_ip,
                      cc->_port);
        }
#endif
        if (_send_notify_2_mcd != NULL) {
            if (is_ccd) {
                _send_notify_2_mcd(ccd_rsp_check_complete_error, cc, 0,
                                   reinterpret_cast<const char*>(&ret),
                                   sizeof(ret));
            } else {
                _send_notify_2_mcd(dcc_rsp_check_complete_error, cc, 0,
                                   reinterpret_cast<const char*>(&ret),
                                   sizeof(ret));
            }
        }
        write_net_log(tools::LOG_COMP_CHK_FAIL, cc);
        return -E_NEED_CLOSE;
    }
}

void CConnSet::CloseCC(ConnCache* cc, unsigned short event)
{
    if (_close_func) {
        if (close_notify_details) {
            _close_func(cc, event);
        } else {
            if (is_ccd) {
                _close_func(cc, ccd_rsp_disconnect);
            } else {
                _close_func(cc, dcc_rsp_disconnected);
            }
        }
    }

    if (cc->_fd > 0) {
        close(cc->_fd);
        cc->_fd = -1;
    }

    cc->_w->reinit();
    cc->_r->reinit();

    cc->_type = cc_tcp;     /* Reset to default. */
    cc->_flag = 0;  /* Reset finclose, connstatus, reqmqidx, spdlmt */

    if (sync_enabled) {
        cc->_smachine = cc_stat_sync;
    } else {
        cc->_smachine = cc_stat_data;
    }

    cc->_start_time.tv_sec = 0;
#ifdef _SPEEDLIMIT_
    cc->_set_send_speed = 0;
    cc->_set_recv_speed = 0;
    cc->_send_mon.reset_stat();
    cc->_recv_mon.reset_stat();
#endif

    ConnCache* prev_cc = _flow_2_cc[cc->_flow % _flow_slot_size];
    if (prev_cc == cc) {
        _flow_2_cc[cc->_flow % _flow_slot_size] = cc->_next_cc;
    } else {
        while ((prev_cc != NULL) && (prev_cc->_next_cc != cc)) {
            prev_cc = prev_cc->_next_cc;
        }

        if (prev_cc) {
            prev_cc->_next_cc = cc->_next_cc;
        }
    }
    cc->_next_cc = NULL;

    list_move(&cc->_next, &_free_ccs);
#ifdef _SPEEDLIMIT_
    list_del_init(&cc->_pending_send_next);
    list_del_init(&cc->_pending_recv_next);
#endif

    _stat._conn_num--;
}

void CConnSet::CheckTimeout(time_t access_deadline)
{
    ConnCache*   cc = NULL;
    list_head_t* tmp;

    list_for_each_entry_safe_l(cc, tmp, &_used_ccs, _next) {
        if (cc->_type != cc_server_udp) {
            if (cc->_access < access_deadline) {
                if (is_ccd) {
                    CloseCC(cc, ccd_rsp_disconnect_timeout);
                } else {
                    CloseCC(cc, dcc_rsp_disconnect_timeout);
                }
                _stat.cc_timeout_close_count++;
            } else {
                break;
            }
        }
    }
}

/*
 * 0, delay close
 * 1, closed
 */
int CConnSet::TryCloseCC(ConnCache* cc, unsigned short event)
{
    if (cc->_w->data_len() > 0) {
        /* 发送缓冲区还有数据未发送完，标识延迟关闭标识 */
        cc->_finclose = 1;
        return 0;
    } else {
        CloseCC(cc, event);
        return 1;
    }
}

void CConnSet::Watch(unsigned cc_timeout, unsigned cc_stattime,
                     tfc::base::CLoadGrid* pload)
{
    static time_t  cur_time = GetMonotonicNowTimeSec();
    static time_t  last_check_time = cur_time;
    static time_t  last_stat_time = cur_time;
    static CCSStat stat;
    int            dcc_idx = 0;

    /* refresh _monotonic_clock_nowtime in GetNowTick */
    GetNowTick();

    cur_time = GetMonotonicNowTimeSec();

    if ((unsigned)(cur_time - last_check_time) > cc_timeout / 5) {
        CheckTimeout(cur_time - cc_timeout);
        last_check_time = cur_time;
    }


    if ((unsigned)(cur_time - last_stat_time) >= cc_stattime) {
        /* save the statisitcs data into shared memory */
        GetMqStat(&net_stat_info);
        /*
         * The pload is a grid to handle requests over a period of time.
         * pload is a global variable in ccd/dcc main file.
         */
        if (pload) {
            unsigned milli_sec, req_cnt;
            pload->fetch_new_load(_monotonic_clock_nowtime, milli_sec, req_cnt);
            if (milli_sec) {
                net_stat_info.load =
                    (unsigned)((double)req_cnt / milli_sec * 1000);
            } else {
                net_stat_info.load = req_cnt / 10;
            }
        }
        net_stat_info.is_ccd = is_ccd;
        net_stat_info.sample_gap = (unsigned)(cur_time - last_stat_time);
        log_client->write_stat(tools::STAT_NET, &net_stat_info);

        if (is_ccd != 1) {
            uint32_t byte_size = this->_remote_stat_info.ByteSize();
            char *buf = new(std::nothrow) char[byte_size];
            if (buf != NULL) {
                uint32_t data_len = 0;
                uint32_t item_num = 0;
                this->_remote_stat_info.ToString(buf, byte_size, &data_len,
                                                 &item_num);
                log_client->write_remote_info(buf, byte_size, item_num);
                delete[] buf;
            }
        }

        GetStatResult(&stat);

        ConnCache*   cc = NULL;
        list_head_t* tmp;
        unsigned long long r_buf_total = 0;
        unsigned long long r_buf_used = 0;
        unsigned long long r_buf_max = 0;
        unsigned long long r_buf_min = UINT_MAX;
        unsigned long long w_buf_total = 0;
        unsigned long long w_buf_used = 0;
        unsigned long long w_buf_max = 0;
        unsigned long long w_buf_min = UINT_MAX;
        unsigned cur_r_size, cur_w_size;
        unsigned cc_cnt = 0;

        if (list_empty(&_used_ccs)) {
            r_buf_min = 0;
            w_buf_min = 0;
        } else {
            list_for_each_entry_safe_l(cc, tmp, &_used_ccs, _next) {
                cc_cnt++;
                r_buf_used += cc->_r->data_len();
                w_buf_used += cc->_w->data_len();
                cur_r_size = cc->_r->size();
                cur_w_size = cc->_w->size();
                r_buf_total += cur_r_size;
                w_buf_total += cur_w_size;
                if (r_buf_max < cur_r_size) {
                    r_buf_max = cur_r_size;
                }
                if (r_buf_min > cur_r_size) {
                    r_buf_min = cur_r_size;
                }
                if (w_buf_max < cur_w_size) {
                    w_buf_max = cur_w_size;
                }
                if (w_buf_min > cur_w_size) {
                    w_buf_min = cur_w_size;
                }
            }
        }

        unsigned used_size = 0;
        fastmem_used_size(used_size);
		if(is_ccd) {

        syslog(LOG_USER | LOG_CRIT | LOG_PID,
            "STAT conn_num=%u,msg_count=%u,"
            "max_time=%u,min_time=%u,avg_time=%u,"
            "recv_size=%llu,send_size=%llu,"
            "recv_buff_max_count=%u,send_buff_max_count=%u,"
			"overload:%u, "
			"proc cpurate:%d, "
			"total cpurate:%d, "
            "read_buf_total:%llu, read_buf_used:%llu,"
            "read_buf_min:%llu, read_buf_max:%llu,"
            "write_buf_total:%llu, write_buf_used:%llu,"
            "write_buf_min:%llu, write_buf_max:%llu,"
            "fastmem_used_size:%dk, cc_cnt:%u\n",
            stat._conn_num,
            stat._msg_count,
            stat._max_time,
            stat._min_time,
            stat._avg_time,
            stat._total_recv_size,
            stat._total_send_size,
            stat.recv_buff_max_count,
            stat.send_buff_max_count,
			stat._overload,
			OVER_CTRL->get_proc_cpu_rate(3),
		    OVER_CTRL->get_cpu_rate("cpu", 3),
            r_buf_total, r_buf_used,
            r_buf_min, r_buf_max,
            w_buf_total, w_buf_used,
            w_buf_min, w_buf_max,
            used_size, cc_cnt
           );
		}
		else {
			syslog(LOG_USER | LOG_CRIT | LOG_PID,
					"STAT conn_num=%u,msg_count=%u,"
					"max_time=%u,min_time=%u,avg_time=%u,"
					"recv_size=%llu,send_size=%llu,"
					"recv_buff_max_count=%u,send_buff_max_count=%u,"
					"read_buf_total:%llu, read_buf_used:%llu,"
					"read_buf_min:%llu, read_buf_max:%llu,"
					"write_buf_total:%llu, write_buf_used:%llu,"
					"write_buf_min:%llu, write_buf_max:%llu,"
					"fastmem_used_size:%dk, cc_cnt:%u\n",
					stat._conn_num,
					stat._msg_count,
					stat._max_time,
					stat._min_time,
					stat._avg_time,
					stat._total_recv_size,
					stat._total_send_size,
					stat.recv_buff_max_count,
					stat.send_buff_max_count,
					r_buf_total, r_buf_used,
					r_buf_min, r_buf_max,
					w_buf_total, w_buf_used,
					w_buf_min, w_buf_max,
					used_size, cc_cnt);

		}
        last_stat_time = cur_time;

        ResetStat();
    }
}

void CConnSet::HandleMemOverload(long long delta_bytes)
{
    WRITE_LOG(output_log, "MCP++ CCD: memory overloaded, delta bytes:%lld\n",
              delta_bytes);

    unsigned long cleaned_size = 0;
    fastmem_clean(cleaned_size);
#ifdef _OUTPUT_LOG_
    WRITE_LOG(output_log, "MCP++ CCD: fastmem_clean() cleaned size:%ld bytes\n",
              cleaned_size);
#endif
    if (cleaned_size >= (unsigned long)delta_bytes) {
        /* fastmem free enough memory */
        return;
    }

    /* there is no active connection, return */
    if (list_empty(&_used_ccs)) {
        if (is_ccd) {
            WRITE_LOG(output_log, "MCP++ CCD Error: memory over load, but "
                      "there is no active connection.\n");
        }
        return;
    }

    int normal_cnt = 0;
    int enlarged_cnt = 0;
    ConnCache* cc = NULL;
    list_head_t* tmp;
    list_for_each_entry_safe_l(cc, tmp, &_used_ccs, _next) {
        if ( cc->_type != cc_server_udp ) {
            if (cc->_r->size() > cc->_r->_buff_size
                || cc->_w->size() > cc->_w->_buff_size)
            {
                _enlarged_ccs[enlarged_cnt++] = cc;
            } else if (cc->_r->size() == cc->_r->_buff_size
                       || cc->_w->size() == cc->_w->_buff_size)
            {
                _normal_ccs[normal_cnt++] = cc;
            }
        }
    }

    WRITE_LOG(output_log, "MCP++ CCD: enlarged_cnt:%d, normal_cnt:%d",
              enlarged_cnt, normal_cnt);

    /* sort enlarged cc fist */
    if (enlarged_cnt != 0) {
        /* reverse sort, close connection from which take up most memory */
        qsort(_enlarged_ccs, enlarged_cnt, sizeof(ConnCache*), cc_compare);
        int i = 0;
        while (delta_bytes > 0 && i < enlarged_cnt) {
            delta_bytes -= _enlarged_ccs[i]->_r->size()
                           + _enlarged_ccs[i]->_w->size();
            CloseCC(_enlarged_ccs[i], ccd_rsp_overload_mem);
            i++;
        }
        fastmem_clean(cleaned_size);
#ifdef _OUTPUT_LOG_
        WRITE_LOG(output_log, "MCP++ CCD: closed enlarged cc cnt:%d, "
                  "fastmem_clean cleaned size:%ld bytes\n", i, cleaned_size);
#endif
    }

    // close normal cc is necessary
    if (delta_bytes > 0 && normal_cnt != 0) {
        int i = 0;
        while (delta_bytes > 0 && i < normal_cnt) {
            delta_bytes -= _normal_ccs[i]->_r->size()
                           + _normal_ccs[i]->_w->size();
            CloseCC(_normal_ccs[i], ccd_rsp_overload_mem);
            i++;
        }
        fastmem_clean(cleaned_size);
#ifdef _OUTPUT_LOG_
        WRITE_LOG(output_log, "MCP++ CCD: closed normal cc cnt:%d, "
                  "fastmem_clean cleaned size:%ld bytes\n", i, cleaned_size);
#endif
    }

    if (delta_bytes > 0) {
        if (is_ccd) {
            fprintf(stderr, "MCP++ CCD ERROR: all connections closed, memory "
                    "still overloaded!\n");
#ifdef _OUTPUT_LOG_
            WRITE_LOG(output_log, "MCP++ CCD ERROR: all connections closed, "
                      "memory still overloaded!\n");
#endif
        }
    }
    return;
}

#ifdef _SPEEDLIMIT_
void CConnSet::SetSpeedLimit(unsigned download_speed, unsigned upload_speed,
                             unsigned low_buff_size) {
    if (is_ccd) {
        if (download_speed > 0) {
            _config_send_speed = download_speed;
        }

        if (upload_speed > 0) {
            _config_recv_speed = upload_speed;
        }
    } else {
        if (download_speed > 0) {
            _config_recv_speed = download_speed;
        }

        if (upload_speed > 0) {
            _config_send_speed = upload_speed;
        }
    }

    if (low_buff_size > 0) {
        _low_buff_size = low_buff_size << 10; /* kbytes-->bytes */
    }
}
#endif

inline
int CConnSet::RecvCCUDP(ConnCache* cc, char* buff, size_t buff_size,
                        size_t& recvd_len, struct sockaddr &from,
                        socklen_t &fromlen)
{
    static CSocketUDP sock(-1, false);
    sock.attach(cc->_fd);

    cc->_access = _monotonic_clock_nowtime.tv_sec;
    list_move_tail(&cc->_next, &_used_ccs);

    fromlen = sizeof(struct sockaddr);

    recvd_len = 0;
    int ret = sock.recvfrom(buff, buff_size, recvd_len, 0, from, fromlen);
    if (recvd_len > 0) {
        _stat._total_recv_size += recvd_len;
        const struct sockaddr_in *temp =
            reinterpret_cast<const struct sockaddr_in*>(&from);
        this->_remote_stat_info.UpdateSize(static_cast<uint32_t>(temp->sin_addr.s_addr),
                                           temp->sin_port, 0, recvd_len);
        if (_send_notify_2_mcd != NULL) {
            unsigned int recvlen = recvd_len;
            if (is_ccd) {
                _send_notify_2_mcd(ccd_rsp_recv_data, cc, 0,
                                   reinterpret_cast<const char*>(&recvlen),
                                   sizeof(recvlen));
            } else {
                _send_notify_2_mcd(dcc_rsp_recv_data, cc, 0,
                                   reinterpret_cast<const char*>(&recvlen),
                                   sizeof(recvlen));
            }
        }
    }

    return ret;
}

inline
int CConnSet::SendCCUDP(ConnCache* cc, const char* data, size_t data_len,
                        size_t& sent_len, const struct sockaddr &to,
                        socklen_t tolen)
{
    static CSocketUDP sock(-1, false);
    sock.attach(cc->_fd);

    cc->_access = _monotonic_clock_nowtime.tv_sec;
    list_move_tail(&cc->_next, &_used_ccs);

    sent_len = 0;
    int ret = sock.sendto(data, data_len, sent_len, 0, to, tolen);
    if (sent_len > 0) {
        _stat._total_send_size += sent_len;
        const struct sockaddr_in *temp =
            reinterpret_cast<const struct sockaddr_in*>(&to);
        this->_remote_stat_info.UpdateSize(static_cast<uint32_t>(temp->sin_addr.s_addr),
                                           temp->sin_port, sent_len, 0);
        if (_send_notify_2_mcd != NULL) {
            unsigned int sendlen = sent_len;
            if (is_ccd) {
                _send_notify_2_mcd(ccd_rsp_send_data, cc, 0,
                                   reinterpret_cast<const char*>(&sendlen),
                                   sizeof(sendlen));
            } else {
                _send_notify_2_mcd(dcc_rsp_send_data, cc, 0,
                                   reinterpret_cast<const char*>(&sendlen),
                                   sizeof(sendlen));
            }
        }
    }
    return ret;
}

/*
 * -E_NEED_CLOSE, For the caller, CCD not close and only exit the recive loop.
 * -E_NEED_RECV
 * -E_RECVED
 * -E_TRUNC
 */
int CConnSet::RecvUDP(ConnCache* cc, char* buff, size_t buff_size,
                      size_t& recvd_len, struct sockaddr &from,
                      socklen_t &fromlen)
{
    recvd_len = 0;

    int ret = RecvCCUDP(cc, buff, buff_size, recvd_len, from, fromlen);
    if (ret == 0) {
        /* recvd_len >= 0. 0 means empty UDP packet. */
        if (recvd_len == buff_size) {
            return -E_TRUNC;
        } else {
            return -E_RECVED;
        }
    } else {
        if (ret != -EAGAIN) {
            /* For the caller, CCD not close and only exit the recive loop. */
            return -E_NEED_CLOSE;
        } else {
            return -E_NEED_RECV;
        }
    }

    /* Actuly never run here. */
    return -E_NEED_CLOSE;
}

/*
 * -E_SEND_COMP send complete
 * -E_FORCE_CLOSE, send complete and close
 * -E_NEED_SEND
 * -E_NEED_CLOSE
 */
int CConnSet::SendFromCacheUDP(ConnCache * cc)
{
    size_t sent_len = 0;
    int ret = 0;
    CUDPSendHeader *send_header = NULL;
    unsigned long long msg_tm, cur_tm;
    unsigned msg_len;

    if (cc->_type == cc_server_udp) {
        if (cc->_w->data_len() > udp_send_buff_max
            || cc->_w->size() > (udp_send_buff_max + (4<<20)))
        {
#ifdef _OUTPUT_LOG_
            WRITE_LOG(output_log, "MCP++ CCD: Server UDP socket send buffer max "
                      "reached! cc->_fd - %d, cc->_flow - %llu, "
                      "cc->_w->data_len() - %u.\n", cc->_fd, cc->_flow,
                      cc->_w->data_len());
#endif
            write_net_log(tools::LOG_SEND_BUF_FULL_DROP, cc,
                          cc->_w->data_len());

            if (cc->_w->data_len() > udp_send_buff_max) {
                cc->_w->clean_data();
            } else {
                cc->_w->reinit();
            }
        }
    } else {
        if (cc->_w->data_len() > udp_send_buff_max) {
#ifdef _OUTPUT_LOG_
            if (is_ccd) {
                WRITE_LOG(output_log, "MCP++ CCD: UDP send buffer max reached! "
                          "Close cc. cc->_fd - %d, cc->_flow - %llu, "
                          "cc->_ip - %u, cc->_w->data_len() - %u.\n", cc->_fd,
                          cc->_flow, cc->_ip, cc->_w->data_len());
            } else {
                WRITE_LOG(output_log, "MCP++ DCC: UDP send buffer max reached! "
                          "Close cc. cc->_fd - %d, cc->_flow - %llu, "
                          "cc->_ip - %u, cc->_w->data_len() - %u.\n", cc->_fd,
                          cc->_flow, cc->_ip, cc->_w->data_len());
            }
#endif
            write_net_log(tools::LOG_SEND_BUF_FULL_CLOSE, cc,
                          cc->_w->data_len());
            return -E_NEED_CLOSE;
        }
    }

    unsigned i = 0;
    while (!stop && i < udp_send_count_max) {
        if (cc->_w->data_len() == 0) {
            return -E_SEND_COMP;
        }

        if (cc->_w->data_len() < sizeof(CUDPSendHeader)) {
            cc->_w->reinit();
            /* Server UDP socket return -E_SEND_COMP ??? */
            return -E_NEED_CLOSE;
        }

        send_header = (CUDPSendHeader*)cc->_w->data();
        msg_len = send_header->_msglen;
        if (cc->_w->data_len() < (sizeof(CUDPSendHeader) + msg_len)) {
            cc->_w->reinit();
            /* Server UDP socket return -E_SEND_COMP ??? */
            return -E_NEED_CLOSE;
        }

        /* Check message timeout. */
        msg_tm = ((unsigned long long)(send_header->_tm.tv_sec)) * 1000
            + ((unsigned long long)(send_header->_tm.tv_usec)) / 1000;
        cur_tm = ((unsigned long long)(_monotonic_clock_nowtime.tv_sec)) * 1000
            + ((unsigned long long)(_monotonic_clock_nowtime.tv_usec)) / 1000;
        if (cur_tm - msg_tm > udp_msg_timeout) {
            /* Timeout message. Not inc i. */
            cc->_w->skip(sizeof(CUDPSendHeader) + msg_len);
            continue;
        }

        sent_len = 0;
        ret = SendCCUDP(cc, cc->_w->data() + sizeof(CUDPSendHeader),
                        msg_len, sent_len,
                        *((struct sockaddr *)(&send_header->_addr)),
                        sizeof(struct sockaddr_in));
        if (ret == 0) {
            /* Always skip whole message. */
            cc->_w->skip(msg_len);

            if (sent_len != msg_len) {
                if (cc->_w->data_len() == 0) {
                    return -E_SEND_COMP;
                } else {
                    i++;
                    continue;
                }
            }

            if (cc->_w->data_len() == 0) {
                /* Send complete. */
                if (cc->_finclose && cc->_type != cc_server_udp) {
                    if (is_ccd) {
                        CloseCC(cc, ccd_rsp_disconnect);
                    } else {
                        CloseCC(cc, dcc_rsp_disconnected);
                    }
                    return -E_FORCE_CLOSE;
                }

                return -E_SEND_COMP;
            }
        } else if ( ret == -EAGAIN ) {
            return -E_NEED_SEND;
        } else {
            /*
             * Other error skip current data. Error maybe cause by current data.
             * Such as data too large and so on.
             */
            cc->_w->skip(msg_len);
            return -E_NEED_CLOSE;
        }

        i++;
    }

    return -E_NEED_SEND;
}

/*
 * -E_SEND_COMP
 * -E_NEED_SEND
 * -E_NEED_CLOSE CCD server UDP socket caller should not close.
 * -E_FORCE_CLOSE
 * -E_MEM_ALLOC
 */
int CConnSet::SendUDP(ConnCache* cc, const char* data, size_t data_len,
                      const struct sockaddr &to, socklen_t tolen)
{
    int ret = 0, inter_ret = 0;
    size_t sent_len;
    CUDPSendHeader header;

    if (cc->_type == cc_server_udp) {
        if (cc->_w->data_len() > udp_send_buff_max
            || cc->_w->size() > (udp_send_buff_max + (4<<20)))
        {
#ifdef _OUTPUT_LOG_
            WRITE_LOG(output_log, "MCP++ CCD: Server UDP socket send buffer max "
                      "reached! cc->_fd - %d, cc->_flow - %llu, "
                      "cc->_w->data_len() - %u.\n", cc->_fd, cc->_flow,
                      cc->_w->data_len());
#endif
            write_net_log(tools::LOG_SEND_BUF_FULL_DROP, cc,
                          cc->_w->data_len());
            if (cc->_w->data_len() > udp_send_buff_max) {
                cc->_w->clean_data();
            } else {
                cc->_w->reinit();
            }
        }
    } else {
        if (cc->_w->data_len() > udp_send_buff_max) {
#ifdef _OUTPUT_LOG_
            if (is_ccd) {
                WRITE_LOG(output_log, "MCP++ CCD: UDP send buffer max reached! "
                          "Close cc. cc->_fd - %d, cc->_flow - %llu, "
                          "cc->_ip - %u, cc->_w->data_len() - %u.\n",
                          cc->_fd, cc->_flow, cc->_ip, cc->_w->data_len());
            } else {
                WRITE_LOG(output_log, "MCP++ DCC: UDP send buffer max reached! "
                          "Close cc. cc->_fd - %d, cc->_flow - %llu, "
                          "cc->_ip - %u, cc->_w->data_len() - %u.\n", cc->_fd,
                          cc->_flow, cc->_ip, cc->_w->data_len());
            }
#endif
            write_net_log(tools::LOG_SEND_BUF_FULL_CLOSE, cc,
                          cc->_w->data_len());
            return -E_NEED_CLOSE;
        }
    }

    ret = SendFromCacheUDP(cc);
    if (ret == -E_SEND_COMP) {
        inter_ret = SendCCUDP(cc, data, data_len, sent_len, to, tolen);
        if (inter_ret == 0) {
            /* Ignore data_len != sent_len. */
            return -E_SEND_COMP;
        } else if ( inter_ret == -EAGAIN ) {
            /* Append. */
            header._msglen = data_len;
            memcpy(&header._addr, (struct sockaddr_in *)(&to),
                   sizeof(struct sockaddr_in));
            memcpy(&header._tm, &_monotonic_clock_nowtime,
                   sizeof(struct timeval));

            if (cc->_w->append(((char*)(&header)), sizeof(CUDPSendHeader),
                               data, data_len) < 0)
            {
                return -E_MEM_ALLOC;
            }
            return -E_NEED_SEND;
        } else {
            return -E_NEED_CLOSE;
        }
    } else if ( ret == -E_NEED_SEND ) {
        header._msglen = data_len;
        memcpy(&header._addr, (struct sockaddr_in *)(&to),
               sizeof(struct sockaddr_in));
        memcpy(&header._tm, &_monotonic_clock_nowtime, sizeof(struct timeval));

        if (cc->_w->append(((char*)(&header)), sizeof(CUDPSendHeader),
                           data, data_len) < 0)
        {
            return -E_MEM_ALLOC;
        }
        return -E_NEED_SEND;
    } else if (ret == -E_NEED_CLOSE) {
        if (cc->_type == cc_server_udp) {
            /* Try next time. */
            header._msglen = data_len;
            memcpy(&header._addr, (struct sockaddr_in *)(&to),
                   sizeof(struct sockaddr_in));
            memcpy(&header._tm, &_monotonic_clock_nowtime,
                   sizeof(struct timeval));

            if (cc->_w->append(((char*)(&header)), sizeof(CUDPSendHeader),
                               data, data_len) < 0)
            {
                return -E_MEM_ALLOC;
            }
            /* Also return -E_NEED_CLOSE. */
            return -E_NEED_CLOSE;
        } else {
            return -E_NEED_CLOSE;
        }
    } else if ( ret == -E_FORCE_CLOSE) {
        return -E_FORCE_CLOSE;
    } else {
        /* Never run here. */
        return -E_NEED_CLOSE;
    }
}

void CConnSet::UpdateDelay(const ConnCache *cc, const sockaddr_in *add_in,
                           uint32_t use_time)
{
    if (is_ccd == 1) {
        return;
    }
    /* only use in dcc */
    if (add_in == NULL) { /* tcp */
        this->_remote_stat_info.UpdateDelay(cc->_ip, cc->_port, use_time);
    } else { /* udp */
        this->_remote_stat_info.UpdateDelay(static_cast<uint32_t>(add_in->sin_addr.s_addr),
                                            add_in->sin_port, use_time);
    }
}
