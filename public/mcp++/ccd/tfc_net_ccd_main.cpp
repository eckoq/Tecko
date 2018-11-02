#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <syslog.h>
#include <sched.h>
#include <errno.h>
#include <stddef.h>
#include <mcheck.h>
#include <set>
#include <map>
#include <string>

#include "base/tfc_base_config_file.h"
#include "base/tfc_base_str.h"
#include "base/tfc_base_so.h"
#include "base/tfc_load_grid.h"
#include "ccd/tfc_net_epoll_flow.h"
#include "ccd/tfc_net_open_mq.h"
#include "ccd/tfc_net_cconn.h"
#include "ccd/tfc_net_ccd_define.h"
#include "ccd/tfc_net_socket_tcp.h"
#include "ccd/tfc_net_socket_udp.h"
#include "ccd/version.h"
#include "ccd/mydaemon.h"
#include "ccd/fastmem.h"
#include "ccd/over_ctrl.h"
#include "ccd/app/server_net_handler.h"
#include "ccd/app/compatible_ccd_net_handler.h"
#include "watchdog/tfc_base_watchdog_client.h"

#ifdef _SHMMEM_ALLOC_
    #include "ccd/tfc_net_open_shmalloc.h"
    #include "ccd/myalloc.h"
#endif
#include "common/log_type.h"
#include "common/log_client.h"

#ifdef _DEBUG_
    #define SAY(fmt, args...) \
    fprintf(stderr, "%s:%d,"fmt, __FILE__, __LINE__, ##args)
#else
    #define SAY(fmt, args...)
#endif

/* 最大支持的mq对数 */
#define MAX_MQ_NUM          (1<<6)
/* fd->mq 映射数组最大长度，mq'fd < FD_MQ_MAP_SIZE */
#define FD_MQ_MAP_SIZE      (1<<10)
/* fd->listen port 映射数组最大长度, listen socket'fd < FD_LISTEN_MAP_SIZE */
#define FD_LISTEN_MAP_SIZE  MIN_START_ADDR
/* 管道最大等待时间(10s)，超过这个时间就需要清管道 */
#define MAX_MQ_WAIT_TIME    10000

using namespace tfc::base;
using namespace tfc::net;
using namespace tfc::watchdog;

int      is_ccd = 1;

typedef struct {
    CFifoSyncMQ* _mq;       /* 管道 */
    bool         _active;   /* 是否被epoll激活 */
    int          _rspmqidx;
} MQINFO;

enum status_type {
    status_normal = 0,      //连接正常
    status_pending = 1,     //被限速
};

/* Dequeue time max. */
unsigned               mq_dequeue_max = 0;
/* ms. CCD default more strict. */
unsigned long long     udp_msg_timeout = 2000;
/* CCD default more large. */
unsigned               udp_send_buff_max = (64<<20);
/* CCD default much more. */
unsigned               udp_send_count_max = 1000;
/* CCD connect sync model enable flag. */
bool                   sync_enabled = false;
/* speed limit enable flag */
bool                   speed_limit  = false;
/* use monotonic time in CCD/DCC Header */
static bool            use_monotonic_time_in_header = false;
/* backlog argument of listen() */
static int             listen_backlog = 1024;
/* SO_REUSEPORT enable flag */
bool                   enable_reuse_port = false;

/* 0 not output log, others output log. */
unsigned               output_log = 0;
/*
 * 2016/1/22 多CCD向同一个MCD发送数据时，
 * 需要该字段区分消息来源，默认为0，
 * 需业务保证该配置不冲突
 */
unsigned short         ccd_id = 0;
/* Net complete function array size. */
#define CC_ARRAY_SIZE  65536
/* Specific net complete function array. */
check_complete         cc_func_array[CC_ARRAY_SIZE];
/*
 * 当该项被配置时，连接的Sendbuf数据堆积超过
 * 该值则认为出错并关闭连接，
 * 为0时不启用该检测 
 */
unsigned               send_buff_max = 0;
/*
 * 当该项被配置时，连接的Recvbuf数据堆积超过
 * 该值则认为出错并关闭连接，
 * 为0时不启用该检测
 */
unsigned               recv_buff_max = 0;

/*
 * 当该项被配置时，CCD进程整体内存使用量超过该值
 * 则关闭使用内存最多的苦干连接
 * 直到CCD进程整体内存使用量低于该值
 * 为0时不启用该检测
 */
unsigned long long     overall_mem_max = 0;
/* true，则CCD发送连接建立、断开等通知给MCD，否则不发送 */
static bool            event_notify = false;
/* 启用连接关闭细节事件，event_notify启用时配置方有效 */
bool                   close_notify_details = false;
/* 启用包完整性检查事件、收发网络数据包事件、mq包和cc确认事件 */
static bool            conn_notify_details = false;
/* profile统计间隔时间 */
static unsigned        stat_time = 60;
/* 连接超时时间 */
static unsigned        time_out = 60;
/* 最大连接数 */
static unsigned        max_conn = 40000;
/* 最大监听端口数，TCP, UDP分别计算 */
static unsigned        max_listen_port = 100;
/* 是否启用listen socket的defer accept特性 */
static bool            defer_accept = true;
/* 是否启用socket的tcp_nodelay特性 */
static bool            tcp_nodelay = true;
/* 是否启用远程获取ccd实时负载功能 */
static bool            fetch_load_enable = true;
/* 当enqueue失败时是否关闭连接，启用后CCD为长连接时对MCD散列可能有影响 */
static bool            enqueue_fail_close = false;
/* ccd->mcd mq */
static CFifoSyncMQ*    req_mq[MAX_MQ_NUM];
/* mcd->ccd mq */
static CFifoSyncMQ*    rsp_mq[MAX_MQ_NUM];
/* Listen socket fd -> listen port. */
static unsigned short  listenfd2port[FD_LISTEN_MAP_SIZE];
/* req_mq数目 */
unsigned               req_mq_num;
/* rsp_mq数目 */
static unsigned        rsp_mq_num;
/* fd->mq映射 */
static MQINFO          mq_mapping[FD_MQ_MAP_SIZE];
/* rsp_mq fd min */
static int             mq_mapping_min = INT_MAX;
/* rsp_mq fd max */
static int             mq_mapping_max = 0;
/* 负载检测 */
static CLoadGrid*      pload_grid = NULL;
/* 数据缓冲区，128M */
static const unsigned  C_TMP_BUF_LEN = (1<<27);
/* 头部+数据的buf */
static char            tmp_buffer[C_TMP_BUF_LEN];
/* ccd->mcd 消息包头部 */
static TCCDHeader*     ccd_header = (TCCDHeader*)tmp_buffer;
/* ccd->mcd 消息消息体 */
static char*           _buf = (char*)tmp_buffer + CCD_HEADER_LEN;
/* 可容纳最大消息体长度 */
static unsigned        _BUF_LEN = C_TMP_BUF_LEN - CCD_HEADER_LEN;
/* 连接池 */
static CConnSet*       ccs = NULL;
/* epoll句柄 */
static int             epfd = -1;
/* epoll事件数组 */
static struct          epoll_event* epev = NULL;
/* 毫秒 */
static unsigned        ccd_response_msg_mq_timeout_thresh = 0;

/*
 * 使用共享内存分配器存储数据的时候mq请求或回复包的格式如下：
 * TCCDHeader + mem_handle + len
 * mem_handle是数据在内存分配器的地址，len是该数据的长度；
 * 且ccdheader里面_type为ccd_req_data_shm或者ccd_rsp_data_shm
 * 该方式减少了数据的内存拷贝，但是需要enqueue的进程分配内存
 * dequeue的进程释放内存
 */
#ifdef _SHMMEM_ALLOC_
/* true表示ccd->mcd使用共享内存分配器存储数据，否则不使用 */
static bool            enqueue_with_shm_alloc = false;
/* true表示mcd->ccd使用共享内存分配器存储数据，否则不使用 */
static bool            dequeue_with_shm_alloc = false;
/*
 * 在下载类业务中，往往只开启dequeue_with_shm_alloc
 * 而在上传类业务中往往只开启enqueue_with_shm_alloc
 */
#endif

/* watchdog client */
CWatchdogClient*        wdc = NULL;

tools::CLogClient*      log_client = NULL;
tools::NetLogInfo       net_log_info;
tools::MemLogInfo       mem_log_info;
tools::CCDStatInfo      net_stat_info;

void write_net_log(unsigned event_type, ConnCache* cc,
                   unsigned data_len = 0, unsigned mq_index = -1,
                   unsigned short err = 0, unsigned wait_time = 0)
{
    if (cc) {
        net_log_info.flow        = cc->_flow;
        net_log_info.remote_ip   = cc->_ip;
        net_log_info.remote_port = cc->_port;
        net_log_info.local_port  = cc->_listen_port;
    }
    net_log_info.data_len    = data_len;
    net_log_info.mq_index    = (unsigned short)mq_index;
    net_log_info.err         = err;
    net_log_info.wait_time   = wait_time;
    log_client->write_log(event_type, ccs->GetWallNowTime(), &net_log_info);
}

void write_net_log(unsigned event_type, unsigned long long flow,
                   unsigned data_len = 0, unsigned mq_index = -1,
                   unsigned short err = 0, unsigned wait_time = 0)
{
    net_log_info.flow        = flow;
    net_log_info.remote_ip   = 0;
    net_log_info.remote_port = 0;
    net_log_info.local_port  = 0;
    net_log_info.data_len    = data_len;
    net_log_info.mq_index    = (unsigned short)mq_index;
    net_log_info.err         = err;
    net_log_info.wait_time   = wait_time;
    log_client->write_log(event_type, ccs->GetWallNowTime(), &net_log_info);
}

void write_mem_log(unsigned event_type, ConnCache* cc,
                   unsigned ip = 0, unsigned short port = 0)
{
    if (cc) {
        mem_log_info.flow        = cc->_flow;
        mem_log_info.local_port  = cc->_listen_port;
        if (ip && port) {
            mem_log_info.remote_ip   = ip;
            mem_log_info.remote_port = port;
        } else {
            mem_log_info.remote_ip   = cc->_ip;
            mem_log_info.remote_port = cc->_port;
        }
    }
    log_client->write_log(event_type, ccs->GetWallNowTime(), &mem_log_info);
}

inline void send_notify_2_mcd(int type, ConnCache* cc, unsigned short arg = 0, 
                              const char *data = NULL,
                              unsigned int data_len = 0)
{
    /*
     * we use own buf to avoid overwrite the data in tmp_buffer via global
     * ccd_header variable.
     */
    static char buf[512];
    /* ccd->mcd 消息包头部 */
    TCCDHeader* local_ccd_header = (TCCDHeader*)buf;
    local_ccd_header->_arg  = arg;
    local_ccd_header->_type = type;
    local_ccd_header->_ccd_id = ccd_id;
    CConnSet::GetHeaderTimestamp(local_ccd_header->_timestamp,
                                 local_ccd_header->_timestamp_msec,
                                 use_monotonic_time_in_header);

    unsigned send_len = CCD_HEADER_LEN;
    if (0 < data_len && data_len <= 512 - CCD_HEADER_LEN && data != NULL) {
        memcpy(buf + send_len, data, data_len);
        send_len += data_len;
    }

    if (cc) {
        if ( cc->_type == cc_tcp ) {
            local_ccd_header->_ip = cc->_ip;
            local_ccd_header->_port = cc->_port;
        } else {
            local_ccd_header->_ip = 0;
            local_ccd_header->_port = 0;
        }
        local_ccd_header->_listen_port = cc->_listen_port;
        req_mq[cc->_reqmqidx]->enqueue(buf, send_len, cc->_flow);
    } else {
        local_ccd_header->_ip = 0;
        local_ccd_header->_port = 0;
        local_ccd_header->_listen_port = 0;
        for (unsigned i = 0; i < req_mq_num; ++i) {
            req_mq[i]->enqueue(buf, send_len, 0ULL);
        }
    }
}

void handle_cc_close(ConnCache* cc, unsigned short event)
{
    static TCCDHeader header;
    if ( cc->_type == cc_tcp ) {
        header._ip = cc->_ip;
        header._port = cc->_port;
    } else {
        header._ip = 0;
        header._port = 0;
    }

    header._listen_port = cc->_listen_port;
    header._type = event;
    header._ccd_id = ccd_id;
    CConnSet::GetHeaderTimestamp(header._timestamp, header._timestamp_msec,
                                 use_monotonic_time_in_header);
    req_mq[cc->_reqmqidx]->enqueue(&header, CCD_HEADER_LEN, cc->_flow);
}

void handle_socket_message(ConnCache* cc)
{
    int ret;
    unsigned data_len;
    struct timeval* time_now = NULL;
    static time_t log_time = 0;

#ifdef _SHMMEM_ALLOC_
    bool is_shm_alloc;
#endif

    while (!stop) {
        data_len = 0;
#ifdef _SHMMEM_ALLOC_
        is_shm_alloc = false;
        if (enqueue_with_shm_alloc) {
            ret = ccs->GetMessage(cc, _buf, _BUF_LEN, data_len, &is_shm_alloc);
        } else {
            ret = ccs->GetMessage(cc, _buf, _BUF_LEN, data_len, NULL);
        }
#else
        ret = ccs->GetMessage(cc, _buf, _BUF_LEN, data_len);
#endif
        if (ret == 0 && data_len > 0) {
            time_now = CConnSet::GetMonotonicNowTime();
            ret = pload_grid->check_load(*time_now);

            /*
             * 这是一个负载探测包, 不够优雅，硬编码了长度，且是asn协议
             * 由于探测请求比较小，不会采用shm alloc方式，所以这里无需释放shm内存
             * 如果配置了fetch_load为false，则不进行判断，以避免与业务数据包的冲突
             */
            if ((data_len == 10) && fetch_load_enable) {
                unsigned milliseconds;
                unsigned req_cnt;
                pload_grid->fetch_load(milliseconds, req_cnt);
                *(unsigned*)(_buf + 2) = milliseconds;
                *(unsigned*)(_buf + 6) = req_cnt;

                /*
                 * 如果rsp_mq没有进行互斥，这里会导致ccd和mcd可能同时enqueue同
                 * 一个mq导致程序crash，所以这里直接调用Send发送回包，因为回包
                 * 很短，这里简化处理，不判断发送结果
                 */
                ccs->Send(cc, _buf, 10);
                break;
            }

			int  refuse_rate = 0;
			bool is_over_load = OVER_CTRL->is_over_load(refuse_rate);

            if (ret == CLoadGrid::LR_FULL||is_over_load) {
				ccs->OverLoadStat();
//                fprintf(stderr, "loadgrid full. close.\n");

#ifdef _OUTPUT_LOG_
                LOG_5MIN(output_log, time_now->tv_sec, log_time,
                         "MCP++ CCD: Loadgrid full! Close connection.\n");
#endif
                if (time_now->tv_sec > log_time) {
                    write_net_log(tools::LOG_OVERLOAD_CLOSE, cc);
                    log_time = time_now->tv_sec + 300;
                }

                if (event_notify) {
                    /* 发送过载通知给所有MCD */
                    send_notify_2_mcd(ccd_rsp_overload, cc);
                }
                ccs->CloseCC(cc, ccd_rsp_disconnect_overload);
#ifdef _SHMMEM_ALLOC_
                /* 此处要防止内存泄露 */
                if (is_shm_alloc) {
                    myalloc_free(*((unsigned long*)_buf));
                }
#endif
                break;
            }

            ccd_header->_ip = cc->_ip;
            ccd_header->_port = cc->_port;
            ccd_header->_listen_port = cc->_listen_port;
            ccd_header->_ccd_id = ccd_id;
#ifdef _SHMMEM_ALLOC_
            if (is_shm_alloc)
                ccd_header->_type = ccd_rsp_data_shm;
            else
#endif
                ccd_header->_type = ccd_rsp_data;

            data_len += CCD_HEADER_LEN;
            write_net_log(tools::LOG_ENQ_DATA, cc, data_len,
                          (2 * cc->_reqmqidx));
            CConnSet::GetHeaderTimestamp(ccd_header->_timestamp,
                                         ccd_header->_timestamp_msec,
                                         use_monotonic_time_in_header);
            ret = req_mq[cc->_reqmqidx]->enqueue(tmp_buffer, data_len,
                                                 cc->_flow);
            if (ret) {
                fprintf(stderr, "ccd enqueue failed, close, ret=%d,"
                        "data_len=%u\n", ret, data_len);
#ifdef _OUTPUT_LOG_
                WRITE_LOG(output_log, "MCP++ CCD: Enqueue fail in"
                          "handle_socket_message! Close connection."
                          "ret - %d, data_len - %u, flow - %llu, ip - %u, "
                          "port - %hu.\n",
                          ret, data_len, cc->_flow, cc->_ip, cc->_port);
#endif

                if (enqueue_fail_close) {
                    write_net_log(tools::LOG_ENQ_FAIL_CLOSE, cc, data_len,
                                  (2 * cc->_reqmqidx));
                    LOG_ONCE("MCP++ CCD: Enqueue fail in handle_socket_message!"
                             " Close connection. ret - %d, data_len - %u, "
                             "flow - %llu, ip - %u, port - %hu.\n",
                             ret, data_len, cc->_flow, cc->_ip, cc->_port);
                    ccs->CloseCC(cc, ccd_rsp_disconnect_overload);
                } else {
                    write_net_log(tools::LOG_ENQ_FAIL_DROP, cc, data_len,
                                  (2 * cc->_reqmqidx));
                }
#ifdef _SHMMEM_ALLOC_
                /* 此处要防止内存泄露 */
                if (is_shm_alloc) {
                    myalloc_free(*((unsigned long*)_buf));
                }
#endif
                break;
            }
        } else if (ret == -E_NEED_CLOSE) {
#ifdef _OUTPUT_LOG_
            WRITE_LOG(output_log, "MCP++ CCD: GetMessage error in "
                      "handle_socket_message! flow - %llu, "
                      "ip - %u, port - %hu.\n",
                      cc->_flow, cc->_ip, cc->_port);
#endif

            ccs->CloseCC(cc, ccd_rsp_disconnect_error);
            break;
        } else if (ret == -E_NEED_SEND) {
            /*
             * 用来支持sync_request和sync_response接口，
             * 两个接口是为了兼容poppy的，今后考虑去掉
             */
            cc->_epoll_flag = (cc->_epoll_flag | EPOLLOUT);
            epoll_mod(epfd, cc->_fd, cc, cc->_epoll_flag);
            break;
        } else {
            /* ret == -E_NEED_RECV */
            break;
        }
    }

    /* 这里要检查一下是否在Recv的时候发现对端断开连接了，如果是则关闭 */
    if (cc->_finclose) {
        SAY("fin close cc, flow=%llu,ip=%u,port=%d\n",
            cc->_flow, cc->_ip, cc->_port);
        ccs->CloseCC(cc, ccd_rsp_disconnect);
    }
}

inline bool handle_socket_recv(ConnCache* cc)
{
    if (cc->_fd < 0) {
        return false;
    }

    /*
     * 接收消息，如果发现对端断开连接但是读缓冲里还有未处理的数据
     * 则延迟关闭_finclose被设置
     */
    int ret = ccs->Recv(cc);
    if (ret == -E_NEED_CLOSE) {
        ccs->CloseCC(cc, ccd_rsp_disconnect_peer_or_error);
        return false;
    }

#ifdef _SPEEDLIMIT_
    else if (ret == -E_NEED_PENDING) {
        /* 将连接放入接收等待队列，去掉EPOLLIN */
        if (speed_limit) {
            cc->_epoll_flag = (cc->_epoll_flag & (~EPOLLIN));
            epoll_mod(epfd, cc->_fd, cc, cc->_epoll_flag);
            cc->_connstatus = status_pending;
        }
        return true;
    }
#endif
    else if (ret == -E_MEM_ALLOC) {
        fprintf(stderr, "alloc mem fail, %m\n");
#ifdef _OUTPUT_LOG_
        WRITE_LOG(output_log, "MCP++ CCD: Memory overload in "
                  "handle_socket_recv! flow - %llu, ip - %u, port - %hu.\n",
                  cc->_flow, cc->_ip, cc->_port);
#endif

        if (event_notify) {
            /* 通知MCD内存过载了 */
            send_notify_2_mcd(ccd_rsp_overload_mem, NULL);
        }
        /* 收缩缓冲内存池 */
        fastmem_shrink(mem_log_info.old_size, mem_log_info.new_size);
        write_mem_log(tools::LOG_RECV_OOM, cc);

        /* 加上EPOLLIN，避免连接从等待队列中取出后碰到E_MEM_ALLOC */
        cc->_epoll_flag = (cc->_epoll_flag | EPOLLIN);
        epoll_mod(epfd, cc->_fd, cc, cc->_epoll_flag);
        return true;
    } else {
        /* 
         *( ret == 0 || ret == -E_NEED_RECV)
         * ret == 0时， finclose置为，连接即将关闭
         */
#ifdef _SPEEDLIMIT_
        /*
         * 这里之所以要重新加入EPOLLIN监控，
         *原因是可能在上一次限速的时候把EPOLLIN给去掉了
         */
        if (speed_limit) {
            cc->_epoll_flag = (cc->_epoll_flag | EPOLLIN);
            epoll_mod(epfd, cc->_fd, cc, cc->_epoll_flag);

            if (status_pending == cc->_connstatus) {
                cc->_connstatus = status_normal;
            }
        }
#endif
        return true;
    }
}

inline bool handle_socket_send(ConnCache* cc, const char* data,
                               unsigned data_len)
{
    int ret;

    if (cc->_fd < 0) {
        return false;
    }

    if (data) {
        ret = ccs->Send(cc, data, data_len);
        if (ret != -E_NEED_CLOSE) {
            if (ret == 0) {
                /* 未发送完，继续监视EPOLLOUT */
                cc->_epoll_flag = (cc->_epoll_flag | EPOLLOUT);
                epoll_mod(epfd, cc->_fd, cc, cc->_epoll_flag);
            }
#ifdef _SPEEDLIMIT_
            else if(ret == -E_NEED_PENDING) {
                /* 被限速挂起了, 不再监控EPOLLOUT */
                if (speed_limit) {
                    cc->_epoll_flag = (cc->_epoll_flag & (~EPOLLOUT));
                    epoll_mod(epfd, cc->_fd, cc, cc->_epoll_flag);
                    cc->_connstatus = status_pending;
                }
            }
#endif
            else if (ret == -E_MEM_ALLOC) {
                fprintf(stderr, "alloc mem fail, %m\n");
#ifdef _OUTPUT_LOG_
                WRITE_LOG(output_log, "MCP++ CCD: Memory overload in "
                          "handle_socket_send! flow - %llu, ip - %u, "
                          "port - %hu.\n",
                          cc->_flow, cc->_ip, cc->_port);
#endif

                if (event_notify) {
                    /* 通知MCD内存过载了 */
                    send_notify_2_mcd(ccd_rsp_overload_mem, NULL);
                }
                /* 收缩缓冲内存池 */
                fastmem_shrink(mem_log_info.old_size, mem_log_info.new_size);
                write_mem_log(tools::LOG_SEND_OOM, cc);

                /*
                 * 需要监控EPOLLOUT，
                 * 避免连接从等待队列里取出后遇到E_MEM_ALLOC
                 */
                cc->_epoll_flag = (cc->_epoll_flag | EPOLLOUT);
                epoll_mod(epfd, cc->_fd, cc, cc->_epoll_flag);
            } else {
                /*
                 * ret==1,发送完毕
                 * 请求计时结束
                 * fix bug of tapd:5519282
                */
                cc->_epoll_flag = (cc->_epoll_flag & (~EPOLLOUT));
                epoll_mod(epfd, cc->_fd, cc, cc->_epoll_flag);
                ccs->EndStat(cc);
#ifdef _SPEEDLIMIT_
                if (speed_limit) {
                    unsigned send_speed =
                        cc->_send_mon.touch(CConnSet::GetMonotonicNowTime());
                    cc->_send_mon.reset_stat();
                    if (ccs->is_send_speed_cfg(cc)) {
                        send_notify_2_mcd(ccd_rsp_send_ok, cc,
                                          (send_speed >> 10));
                    }
                }
#endif
            }
            return true;
        } else {
#ifdef _OUTPUT_LOG_
            WRITE_LOG(output_log, "MCP++ CCD: Send error in handle_socket_send!"
                      " Connection will be close! flow - %llu, "
                      "ip - %u, port - %hu.\n",
                      cc->_flow, cc->_ip, cc->_port);
#endif

            ccs->CloseCC(cc, ccd_rsp_disconnect_peer_or_error);
            return false;
        }
    } else {
        ret = ccs->SendFromCache(cc);
        if (ret == 0) {
            /* 缓存发送完毕,去除EPOLLOUT */
            cc->_epoll_flag = (cc->_epoll_flag & (~EPOLLOUT));
            epoll_mod(epfd, cc->_fd, cc, cc->_epoll_flag);
            ccs->EndStat(cc);
#ifdef _SPEEDLIMIT_
            if (speed_limit) {
                unsigned send_speed =
                    cc->_send_mon.touch(CConnSet::GetMonotonicNowTime());
                cc->_send_mon.reset_stat();
                if (ccs->is_send_speed_cfg(cc)) {
                    send_notify_2_mcd(ccd_rsp_send_ok, cc, (send_speed >> 10));
                }
            }
#endif
            return true;
        } else if (ret == -E_NEED_CLOSE) {
#ifdef _OUTPUT_LOG_
            WRITE_LOG(output_log, "MCP++ CCD: SendFromCache error in "
                      "handle_socket_Send! Connection will be close! "
                      "flow - %llu, ip - %u, port - %hu.\n",
                      cc->_flow, cc->_ip, cc->_port);
#endif

            ccs->CloseCC(cc, ccd_rsp_disconnect_peer_or_error);
            return false;
        }
#ifdef _SPEEDLIMIT_
        else if (ret == -E_NEED_PENDING || ret == -E_NEED_PENDING_NOTIFY) {
            if (speed_limit) {
                cc->_connstatus = status_pending;
                /* 放入限速等待队列，不需要监控EPOLLOUT */
                cc->_epoll_flag = (cc->_epoll_flag & (~EPOLLOUT));
                epoll_mod(epfd, cc->_fd, cc, cc->_epoll_flag);

                if (ret == -E_NEED_PENDING_NOTIFY) {
                    unsigned send_speed =
                        cc->_send_mon.touch(CConnSet::GetMonotonicNowTime());
                    /* 设置参数为已限速(1) */
                    send_notify_2_mcd(ccd_rsp_send_nearly_ok, cc,
                                      (send_speed >> 10));
                }
            }
            return true;
        }
#endif
        else if (ret != -E_FORCE_CLOSE) {
            /* 也就是 (ret == E_NEED_SEND) */
            /*
             *由于可能上一轮发送被限速了没有监控EPOLLOUT事件，
             * 所以这轮发送如果没被限速需要重新加入EPOLLOUT监控
             */
            if (speed_limit) {
                cc->_epoll_flag = (cc->_epoll_flag | EPOLLOUT);
                epoll_mod(epfd, cc->_fd, cc, cc->_epoll_flag);
#ifdef _SPEEDLIMIT_
                if (status_pending == cc->_connstatus) {
                    cc->_connstatus = status_normal;
                }
#endif
            }
            return true;
        } else {
            /* 这里 ret == E_FORCE_CLOSE，连接已经关闭，不需要其他动作 */
            return false;
        }
    }
}

void handle_accept(int listenfd)
{
    unsigned long long flow;
    static CSocketTCP sock(-1, false);
    ConnCache* cc = NULL;
    int ret;

    while (true) {
        ret = ::accept(listenfd, NULL, NULL);
        if (ret >= 0) {
            cc = ccs->AddConn(ret, flow);
            if (cc == NULL) {
                close(ret);
                fprintf(stderr, "add cc fail, %m\n");
#ifdef _OUTPUT_LOG_
                WRITE_LOG(output_log, "MCP++ CCD: Add connection fail when "
                          "accept! Maybe no free connection or flow map "
                          "too crowd!\n");
#endif
                if (event_notify) {
                    /* 通知MCD连接过载了 */
                    send_notify_2_mcd(ccd_rsp_overload_conn, NULL);
                }
                break;
            }

            sock.attach(ret);
            sock.set_nonblock(tcp_nodelay);
            /* 只有EPOLLIN */
            epoll_add(epfd, ret, cc, cc->_epoll_flag);
            cc->_listen_port = listenfd2port[listenfd];

            /* 选择mq */
            cc->_reqmqidx =
                ccs->_ccd_net_handler->route_connection(cc->_ip,
                                                        cc->_port,
                                                        cc->_listen_port,
                                                        flow, req_mq_num);
            if (cc->_reqmqidx >= req_mq_num) {
                cc->_reqmqidx = flow % req_mq_num;
            }

            write_net_log(tools::LOG_ESTAB_CONN, cc);

            if (event_notify) {
                /* 通知MCD有新连接建立 */
                ccd_header->_ip = cc->_ip;
                ccd_header->_port = cc->_port;
                ccd_header->_listen_port = cc->_listen_port;
                ccd_header->_type = ccd_rsp_connect;
                ccd_header->_ccd_id = ccd_id;
                CConnSet::GetHeaderTimestamp(ccd_header->_timestamp,
                                             ccd_header->_timestamp_msec,
                                             use_monotonic_time_in_header);
                req_mq[cc->_reqmqidx]->enqueue(tmp_buffer, CCD_HEADER_LEN,
                                               flow);
            }
        } else {
            break;
        }
    }
}

void handle_socket(struct ConnCache *cc, struct epoll_event* ev)
{
    int events = ev->events;

    /* Check cc by fd. */
    if (cc->_fd < 0) {
        fprintf(stderr, "handle_socket(): cc->fd < 0! cc->fd %d.\n", cc->_fd);
        return;
    }

    if (!(events & (EPOLLIN | EPOLLOUT))) {
#ifdef _OUTPUT_LOG_
        WRITE_LOG(output_log, "MCP++ CCD: Events error when handle socket! "
                  "flow - %llu, ip - %u, port - %hu.\n",
                  cc->_flow, cc->_ip, cc->_port);
#endif
        write_net_log(tools::LOG_EPOLL_ERR, cc, 0, -1, errno);
        ccs->CloseCC(cc, ccd_rsp_disconnect_error);
        return;
    }

    if (events & EPOLLOUT) {
        if (!handle_socket_send(cc, NULL, 0)) {
            return;
        }
    }

    if (!(events & EPOLLIN)) {
        return;
    }

    /* 从收请求开始计时 */
    ccs->StartStat(cc);

    if (!handle_socket_recv(cc)) {
        return;
    }

    /* 处理收到的socket数据 */
    handle_socket_message(cc);
}

#ifdef _SPEEDLIMIT_
void handle_pending()
{
    ConnCache* cc = NULL;
    list_head_t *tmp;
    struct timeval* nowtime = CConnSet::GetNowTick();
    int proc_cnt = 0;

    list_for_each_entry_safe_l(cc, tmp, &ccs->_pending_send, _pending_send_next)
    {
        if (ccs->get_send_speed(cc) == 0
            || ccs->get_send_speed(cc) > cc->_send_mon.touch(nowtime))
        {
            list_del_init(&cc->_pending_send_next);
            handle_socket_send(cc, NULL, 0);
            ++proc_cnt;
        }
    }

    if (proc_cnt) {
        /* update now time */
        nowtime = CConnSet::GetNowTick();
    }

    list_for_each_entry_safe_l(cc, tmp, &ccs->_pending_recv, _pending_recv_next)
    {
        if (ccs->get_send_speed(cc) == 0
            || ccs->get_recv_speed(cc) > cc->_recv_mon.touch(nowtime))
        {
            list_del_init(&cc->_pending_recv_next);
            if (handle_socket_recv(cc)) {
                handle_socket_message(cc);
            }
        }
    }
}
#endif

void handle_socket_message_udp(struct ConnCache* cc,
                               char *data,
                               size_t data_len,
                               struct sockaddr_in &addr)
{
    struct timeval *time_now = NULL;
    int ret;
    static time_t log_time = 0;

    time_now = CConnSet::GetMonotonicNowTime();
    ret = pload_grid->check_load(*time_now);

    if ((data_len == 10) && fetch_load_enable) {
        unsigned milliseconds;
        unsigned req_cnt;
        pload_grid->fetch_load(milliseconds, req_cnt);
        *(unsigned*)(data + 2) = milliseconds;
        *(unsigned*)(data + 6) = req_cnt;

        struct sockaddr *paddr = (struct sockaddr *)(&addr);
        ccs->SendUDP(cc, data, 10, (*paddr), sizeof(struct sockaddr_in));

        return;
    }

	int  refuse_rate = 0;
    bool is_over_load = OVER_CTRL->is_over_load(refuse_rate);

    if (ret == CLoadGrid::LR_FULL||is_over_load) {
		ccs->OverLoadStat();
#ifdef _OUTPUT_LOG_
        LOG_5MIN(output_log, time_now->tv_sec, log_time,
                 "MCP++ CCD: Loadgrid full! UDP server socket.\n");
#endif
        if (time_now->tv_sec > log_time) {
            write_net_log(tools::LOG_OVERLOAD_DROP, cc);
            log_time = time_now->tv_sec + 300;
        }

        if (event_notify) {
            send_notify_2_mcd(ccd_rsp_overload, cc);
        }

        return;
    }

    unsigned mr_ret = ccs->_ccd_net_handler->route_packet(data, data_len,
                                                          cc->_ip, cc->_port,
                                                          cc->_flow,
                                                          cc->_listen_port,
                                                          req_mq_num);
    if (mr_ret < req_mq_num) {
        cc->_reqmqidx = mr_ret;
    } else {
        cc->_reqmqidx = (cc->_reqmqidx + 1) % req_mq_num;
    }

    ccd_header->_ip = addr.sin_addr.s_addr;
    ccd_header->_port = ntohs(addr.sin_port);
    ccd_header->_listen_port = cc->_listen_port;
    ccd_header->_ccd_id = ccd_id;
    CConnSet::GetHeaderTimestamp(ccd_header->_timestamp,
                                 ccd_header->_timestamp_msec,
                                 use_monotonic_time_in_header);

    ccd_header->_type = ccd_rsp_data_udp;

#ifdef _SHMMEM_ALLOC_
    memhead mhead;
    mhead.mem = NULL_HANDLE;
    if (enqueue_with_shm_alloc) {
        mhead.mem = myalloc_alloc(data_len);
        if (mhead.mem != NULL_HANDLE) {
            ccd_header->_type = ccd_rsp_data_shm_udp;
            mhead.len = data_len;
            memcpy(myalloc_addr(mhead.mem), data, data_len);
            memcpy(_buf, &mhead, sizeof(memhead));
            data_len = sizeof(memhead);
        }
    }
#endif
    data_len += CCD_HEADER_LEN;
    write_net_log(tools::LOG_ENQ_DATA, cc, data_len, (2 * cc->_reqmqidx));
    if (req_mq[cc->_reqmqidx]->enqueue(tmp_buffer, data_len, cc->_flow)) {
#ifdef _OUTPUT_LOG_
        LOG_5MIN(output_log, time_now->tv_sec, log_time,
                 "MCP++ CCD: Enqueue to mq fail of UDP socket.\n");
#endif
        if (time_now->tv_sec > log_time) {
            write_net_log(tools::LOG_ENQ_FAIL_DROP, cc, data_len,
                          (2 * cc->_reqmqidx));
            log_time = time_now->tv_sec + 300;
        }

#ifdef _SHMMEM_ALLOC_
        if (enqueue_with_shm_alloc && mhead.mem != NULL_HANDLE) {
            myalloc_free(mhead.mem);
        }
#endif
    }
}

bool handle_socket_recive_udp(struct ConnCache* cc)
{
    int ret;
    struct sockaddr_in addr;
    struct sockaddr *paddr = (struct sockaddr *)(&addr);
    socklen_t addr_len;
    size_t recvd_len;
    unsigned loop_cnt = 0;

    if (cc->_fd < 0) {
        return false;
    }

    if (cc->_type != cc_server_udp) {
#ifdef _OUTPUT_LOG_
        WRITE_LOG(output_log, "MCP++ CCD: Invalid connection type %d in "
                  "handle UDP recive!\n",
                  cc->_type);
#endif
        return false;
    }

    do {
        loop_cnt++;
        if (loop_cnt > 100) {
            return true;
        }

        ret = ccs->RecvUDP(cc, _buf, _BUF_LEN, recvd_len, (*paddr), addr_len);
        if (ret == -E_NEED_CLOSE) {
            /* Do nothing for server UDP. */
            return false;
        } else if (ret == -E_NEED_RECV) {
            /* EAGAIN */
            return true;
        } else if (ret == -E_RECVED) {
            /* Data recived */
            handle_socket_message_udp(cc, _buf, recvd_len, addr);
            continue;
        } else if (ret == -E_TRUNC) {
            /* Not enqueue to mcd when data trunc */
            continue;
        } else {
            /* Other error */
            return false;
        }
    } while (!stop && (ret == -E_RECVED || ret == -E_TRUNC));

    return false;
}

bool handle_socket_send_udp(struct ConnCache* cc, const char* data,
                            unsigned data_len, unsigned ip,
                            unsigned short port)
{
    int ret;
    unsigned left_len;

    if (cc->_fd < 0) {
        return false;
    }

    if (cc->_type != cc_server_udp) {
#ifdef _OUTPUT_LOG_
        WRITE_LOG(output_log, "MCP++ CCD: Invalid connection type %d in "
                  "handle UDP send!\n", cc->_type);
#endif
        return false;
    }

    if (data) {
        struct sockaddr addr;
        struct sockaddr_in *paddr = (struct sockaddr_in*)(&addr);

        paddr->sin_family = AF_INET;
        paddr->sin_addr.s_addr = ip;
        paddr->sin_port = htons(port);

        ret = ccs->SendUDP(cc, data, data_len, addr,
                           sizeof(struct sockaddr_in));
        if (ret == -E_SEND_COMP) {
            /* Call from hand mq, so always update endstat. */
            ccs->EndStat(cc);
            epoll_mod(epfd, cc->_fd, cc, EPOLLIN);
            return true;
        } else if (ret == -E_NEED_SEND) {
            epoll_mod(epfd, cc->_fd, cc, EPOLLIN | EPOLLOUT);
            return true;
        } else if (ret == -E_MEM_ALLOC) {
#ifdef _OUTPUT_LOG_
            WRITE_LOG(output_log, "MCP++ CCD: UDP send buffer append fail! "
                      "flow - %llu, listenport - %hu, destip - %u, "
                      "destport - %hu.\n",
                      cc->_flow, cc->_listen_port, ip, port);
#endif

            if (event_notify) {
                send_notify_2_mcd(ccd_rsp_overload_mem, NULL);
            }

            fastmem_shrink(mem_log_info.old_size, mem_log_info.new_size);
            write_mem_log(tools::LOG_SEND_OOM, cc, ip, port);

            return false;
        } else {
            /* -E_NEED_CLOSE or -E_FORCE_CLOSE */
            /* Do nothing. */
            return false;
        }
    } else {
        left_len = ccs->GetSendBufferSize(cc);
        ret = ccs->SendFromCacheUDP(cc);
        if (ret == -E_SEND_COMP) {
            if (left_len) {
                /* Not Recv in the same loop. */
                ccs->EndStat(cc);
            }
            epoll_mod(epfd, cc->_fd, cc, EPOLLIN);
            return true;
        } else if (ret == -E_NEED_SEND) {
            return true;
        } else {
            /* -E_NEED_CLOSE or -E_FORCE_CLOSE */
            /* Do nothing. */
            return false;
        }
    }
}

void handle_socket_udp(struct ConnCache *cc, struct epoll_event* ev)
{
    int events = ev->events;

    /* Check cc by fd. */
    if (cc->_fd < 0) {
        /* Should not occur in ccd UDP server socket. Serious error. */
        WRITE_LOG(output_log, "MCP++ CCD: Server UPD socket fd error! fd - %d, "
                  "flow - %llu.\n", cc->_fd, cc->_flow);
        return;
    }

    if (!(events & (EPOLLIN | EPOLLOUT))) {
        /* Not close UDP server socket. */
        return;
    }

    if (events & EPOLLIN) {
        ccs->StartStat(cc);
        if (!handle_socket_recive_udp(cc)) {
            return;
        }
    }

    if (events & EPOLLOUT) {
        if (!handle_socket_send_udp(cc, NULL, 0, 0, 0)) {
            return;
        }
    }
}

void handle_rsp_mq(CFifoSyncMQ* mq, bool is_epoll_event = true)
{
    unsigned data_len;
    unsigned long long flow;
    int ret;
    ConnCache* cc = NULL;
    unsigned mq_wait_time = 0;

    MQINFO *mi = &mq_mapping[mq->fd()];
    if (mi->_mq != mq) {
        fprintf(stderr, "handle_rsp_mq(): MQ check fail!\n");
        return;
    }

    if (is_epoll_event) {
        /* 设置mq激活标志 */
        mi->_active = true;
        /* 清除mq通知 */
        mq->clear_flag();
    }

    set<unsigned long long> flow_set;
    /* dequeue at most 100 timeout messages once */
    int dequeue_timeout_max_count = 100;
    unsigned dequeue_cnt = 0;
    struct timeval now =
        (use_monotonic_time_in_header ? tools::GET_MONOTONIC_CLOCK() :
        tools::GET_WALL_CLOCK());

    while (!stop) {
        if (mq_dequeue_max && ((dequeue_cnt++) > mq_dequeue_max)) {
            SAY("CCD dequeue message max reached!\n");
            break;
        }

        flow = ULLONG_MAX;
        data_len = 0;
        ret = mq->try_dequeue(tmp_buffer, _BUF_LEN, data_len, flow);

        if (ret < 0 || data_len == 0) { /* 没有数据 */
            break;
        }

        cc = ccs->GetConnCache(flow);

        mq_wait_time = ccs->CheckWaitTime(ccd_header->_timestamp,
                                          ccd_header->_timestamp_msec, now);
        write_net_log(tools::LOG_DEQ_DATA, flow, data_len,
                      (2 * mi->_rspmqidx + 1), 0, mq_wait_time);

        if (ccd_response_msg_mq_timeout_thresh != 0
            && mq_wait_time > ccd_response_msg_mq_timeout_thresh)
        {
            /* 发生异常，需要清管道 */
            ccs->IncMQMsgTimeoutCount();

            LOG_ONCE("MCP++ CCD: dropped a packet from mcd because packet "
                     "timeout,wait time:%u, header:%ld, now:%ld, thresh:%u\n",
                     mq_wait_time, ccd_header->_timestamp, now.tv_sec,
                     ccd_response_msg_mq_timeout_thresh);

            /* close connection */
            if (cc != NULL) {
                write_net_log(tools::LOG_OVERLOAD_CLOSE, cc);
                LOG_ONCE("MCP++ CCD: overload protect will close connection");
                ccs->CloseCC(cc, ccd_rsp_disconnect_overload);
            }

#ifdef _SHMMEM_ALLOC_
            /* release myalloc mem if any */
            if (dequeue_with_shm_alloc
                && (ccd_header->_type == ccd_req_data_shm
                    || ccd_header->_type == ccd_req_data_shm_udp))
            {
                memhead* head = (memhead*)(tmp_buffer + CCD_HEADER_LEN);
                myalloc_free(head->mem);
            }
#endif

            if (--dequeue_timeout_max_count >= 0) {
                continue;
            } else {
                break;
            }
        }

        if (cc == NULL) {
            if (conn_notify_details) { /* only notify the send data mq */
                send_notify_2_mcd(ccd_rsp_cc_closed, NULL, 0);
            }

            if (flow_set.find(flow) == flow_set.end()) {
                flow_set.insert(flow);
                /*
                 *此种情况可能由于MCD处理超时，导致CCD把超时的连接已经关闭了；
                 * 或者同一个连接之前的处理导致连接关闭
                 */
#ifdef _OUTPUT_LOG_
                WRITE_LOG(output_log, "MCP++ CCD: No cc found when "
                          "handle_rsp_mq! flow - %llu.\n", flow);
#endif
                write_net_log(tools::LOG_FLOW2CC_NOT_MATCH, flow, data_len,
                              (2 * mi->_rspmqidx + 1));
            }

#ifdef _SHMMEM_ALLOC_
            /* 这里要释放在shm分配的内存，否则会导致内存泄露 */
            if (dequeue_with_shm_alloc
               && (ccd_header->_type == ccd_req_data_shm
                   || ccd_header->_type == ccd_req_data_shm_udp))
            {
                myalloc_free(*((unsigned long*)_buf));
            }
#endif
            continue;
        } else {
            if (conn_notify_details) {
                send_notify_2_mcd(ccd_rsp_cc_ok, cc, 0);
            }
        }

        /*
         * ccd暂时不对mcd发过来的消息包的timestamp进行处理，
         * 因为mcd可能不会对timestamp赋值
         */

        if (ccd_header->_type == ccd_req_disconnect) {
            if (cc->_type != cc_server_udp) {
                /*
                 * MCD要求数据发送完成后主动关闭连接
                 * 这里尝试立即关闭连接，如果不成功，
                 * 则记下标识以便数据发送完成关闭连接
                 */
                ret = ccs->TryCloseCC(cc, ccd_rsp_disconnect_local);
            }
            continue;
        }

        if (ccd_header->_type == ccd_req_force_disconnect) {
            if (cc->_type != cc_server_udp) {
                /* MCD强制要求关闭连接 */
                ccs->CloseCC(cc, ccd_rsp_disconnect_local);
            }
            continue;
        }

#ifdef _SPEEDLIMIT_
        /* 设置该连接的上传或者下载速率 */
        if (speed_limit
            && ((ccd_header->_type == ccd_req_set_dspeed)
                 || (ccd_header->_type == ccd_req_set_uspeed)
                 || (ccd_header->_type == ccd_req_set_duspeed)))
        {
            if (cc->_type == cc_tcp) {
                if (ccd_header->_type == ccd_req_set_duspeed) {
                    cc->_set_send_speed = (unsigned)ccd_header->_arg;
                    cc->_set_recv_speed = (unsigned)ccd_header->_arg;
                } else if (ccd_header->_type == ccd_req_set_dspeed) {
                    cc->_set_send_speed = ccd_header->_arg;
                } else if (ccd_header->_type == ccd_req_set_uspeed) {
                    cc->_set_recv_speed = ccd_header->_arg;
                }
            }
            continue;
        }
#endif

#ifdef _SHMMEM_ALLOC_
        if (dequeue_with_shm_alloc
           && (ccd_header->_type == ccd_req_data_shm
               || ccd_header->_type == ccd_req_data_shm_udp))
        {
            memhead *head = (memhead*)_buf;
            char    *req_data = (char*)myalloc_addr(head->mem);

            if (conn_notify_details) {
                unsigned int real_data_len = head->len;
                if (real_data_len > 0) {
                    send_notify_2_mcd(ccd_rsp_reqdata_recved, cc, 0,
                                      reinterpret_cast<const char*>(&real_data_len),
                                      sizeof(real_data_len));
                }
            }

            if (cc->_type == cc_tcp && ccd_header->_type == ccd_req_data_shm) {
                handle_socket_send(cc, req_data, head->len);
            } else if (cc->_type == cc_server_udp
                       && ccd_header->_type == ccd_req_data_shm_udp)
            {
                handle_socket_send_udp(cc, req_data, head->len,
                                       ccd_header->_ip, ccd_header->_port);
            } else {
#ifdef _OUTPUT_LOG_
                WRITE_LOG(output_log,
                    "MCP++ CCD: Invalid connection type %d or header type %hu "
                    "in handle rsp_mq!\n", cc->_type, ccd_header->_type);
#endif
                write_net_log(tools::LOG_INVALID_CONN_TYPE, cc, data_len,
                              (2 * mi->_rspmqidx + 1));
            }
            /* 这里使用完需要释放内存，否则会造成内存泄露 */
            myalloc_free(head->mem);
        } else {
#endif
            if (conn_notify_details) {
                unsigned int real_data_len = data_len - CCD_HEADER_LEN;
                if (real_data_len > 0) {
                    send_notify_2_mcd(ccd_rsp_reqdata_recved, cc, 0,
                                      reinterpret_cast<const char*>(&real_data_len),
                                      sizeof(real_data_len));
                }
            }

            if (cc->_type == cc_tcp && ccd_header->_type == ccd_req_data) {
                handle_socket_send(cc, _buf, data_len - CCD_HEADER_LEN);
            } else if (cc->_type == cc_server_udp
                       && ccd_header->_type == ccd_req_data_udp)
            {
                handle_socket_send_udp(cc, _buf, data_len - CCD_HEADER_LEN,
                                       ccd_header->_ip, ccd_header->_port);
            } else {
#ifdef _OUTPUT_LOG_
                WRITE_LOG(output_log,
                    "MCP++ CCD: Invalid connection type %d or header type %hu "
                    "in handle rsp_mq!\n", cc->_type, ccd_header->_type);
#endif
                write_net_log(tools::LOG_INVALID_CONN_TYPE, cc, data_len,
                              (2 * mi->_rspmqidx + 1));
            }

#ifdef _SHMMEM_ALLOC_
        }
#endif
    }
}

void init_global_cfg(CFileConfig& page)
{
    try {
        listen_backlog = from_str<int>(page["root\\listen_backlog"]);
    } catch(...) {
        listen_backlog = 1024;
    }

    try {
        use_monotonic_time_in_header =
            from_str<bool>(page["root\\use_monotonic_time_in_header"]);
    } catch(...) {
        use_monotonic_time_in_header = false;
    }

    try {
        event_notify = from_str<bool>(page["root\\event_notify"]);
    } catch(...) {
        /* nothing */
    }

    try {
        close_notify_details =
            from_str<bool>(page["root\\close_notify_details"]);
    } catch (...) {
        close_notify_details = false;
    }

    try {
        conn_notify_details = from_str<bool>(page["root\\conn_notify_details"]);
    } catch(...) {
        conn_notify_details = false;
    }

    try {
        stat_time = from_str<unsigned>(page["root\\stat_time"]);
    } catch(...) {
        /* nothing */
    }

    try {
        time_out = from_str<unsigned>(page["root\\time_out"]);
    } catch(...) {
        /* nothing */
    }
    try {
        max_conn = from_str<unsigned>(page["root\\max_conn"]);
    } catch(...) {
        /* nothing */
    }

    try {
        max_listen_port = from_str<unsigned>(page["root\\max_listen_port"]);
    } catch(...) {
        /* nothing */
    }

    try {
        enable_reuse_port = from_str<bool>(page["root\\enable_reuse_port"]);
    } catch (...) {
        /* nothing */
    }

    try {
        fetch_load_enable = from_str<bool>(page["root\\fetch_load"]);
    } catch(...) {
        /* nothing */
    }

    try {
        enqueue_fail_close = from_str<bool>(page["root\\enqueue_fail_close"]);
    } catch(...) {
        /* nothing */
    }

    if (enqueue_fail_close) {
        fprintf(stderr, "CCD enqueue_fail_close enabled!\n");
    } else {
        fprintf(stderr, "CCD enqueue_fail_close not enabled!\n");
    }

    try {
        output_log = from_str<unsigned>(page["root\\output_log"]);
    } catch (...) {
        output_log = 0;
    }

    if (output_log) {
        fprintf(stderr, "CCD log open!\n");
    } else {
        fprintf(stderr, "CCD log close!\n");
    }

    try {
        ccd_id = from_str<unsigned>(page["root\\ccd_id"]);
    } catch (...) {
        ccd_id = 0;
    }
    fprintf(stderr, "CCD ID:%d\n", ccd_id);

    try {
        defer_accept = from_str<bool>(page["root\\defer_accept"]);
    } catch(...) {
        /* nothing */
    }

    try {
        tcp_nodelay = from_str<bool>(page["root\\tcp_nodelay"]);
    } catch(...) {
        /* nothing */
    }

    try {
        send_buff_max = from_str<unsigned>(page["root\\send_buff_max"]);
    } catch (...) {
        send_buff_max = 0;
    }

    if (send_buff_max) {
        fprintf(stderr, "CCD send buffer max check enabled, buffer max "
                "length - %u.\n", send_buff_max);
    } else {
        fprintf(stderr, "CCD send buffer max is 0\n");
    }

    try {
        recv_buff_max = from_str<unsigned>(page["root\\recv_buff_max"]);
    } catch (...) {
        recv_buff_max = 0;
    }
    if (recv_buff_max) {
        fprintf(stderr, "CCD recive buffer max check enabled, buffer max "
                "length - %u.\n", recv_buff_max);
    } else {
        fprintf(stderr, "CCD recive buffer max is 0\n");
    }

    try {
        overall_mem_max =
            from_str<unsigned long long>(page["root\\overall_mem_max"]);
    } catch (...) {
        overall_mem_max = 0;
    }
    if (overall_mem_max) {
        unsigned long long mem_total = 0;
        if (!get_mem_total(&mem_total)) {
            fprintf(stderr, "get mem total failed.\n");
            err_exit();
        } else {
            fprintf(stderr, "mem_total:%lld\n", mem_total);
        }
        if (mem_total && overall_mem_max > mem_total) {
            fprintf(stderr, "overall_mem_max [%lld] larger than "
                    "mem_total [%lld]\n", overall_mem_max, mem_total);
            err_exit();
        }
        /* 512M mem */
        if (overall_mem_max < ((unsigned long)1 << 29)) {
            fprintf(stderr, "overall_mem_max [%lld] too small, use new value "
                    "instead.\n", overall_mem_max);
            overall_mem_max = 1<<29;
        }
        fprintf(stderr, "CCD overall memory max check enabled, memory max "
                "size: %lld.\n", overall_mem_max);
    } else {
        fprintf(stderr, "CCD overall memory max check disable.\n");
    }

    try {
        mq_dequeue_max = from_str<unsigned>(page["root\\mq_dequeue_max"]);
    } catch (...) {
        /* nothing */
    }

    try {
        udp_msg_timeout = from_str<unsigned>(page["root\\udp\\msg_timeout"]);
    } catch (...) {
        /* nothing */
    }

    try {
        udp_send_buff_max =
            from_str<unsigned>(page["root\\udp\\send_buff_max"]);
    } catch (...) {
        /* nothing */
    }

    try {
        udp_send_count_max =
            from_str<unsigned>(page["root\\udp\\send_count_max"]);
    } catch (...) {
        /* nothing */
    }

    try {
        ccd_response_msg_mq_timeout_thresh =
            from_str<unsigned>(page["root\\ccd_response_msg_mq_timeout_thresh"]);
    } catch (...) {
        /* nothing */
    }
}

void init_mq_conf(CFileConfig& page, bool is_req)
{
    char mq_name[64] = {0};
    string mq_path;
    CFifoSyncMQ* mq;
    int fd;

    for (unsigned i = 0; i < MAX_MQ_NUM; ++i) {
        if (i == 0) {
            if (is_req) {
                sprintf(mq_name, "root\\req_mq_conf");
            } else {
                sprintf(mq_name, "root\\rsp_mq_conf");
            }
        } else {
            if (is_req) {
                sprintf(mq_name, "root\\req_mq%u_conf", i + 1);
            } else {
                sprintf(mq_name, "root\\rsp_mq%u_conf", i + 1);
            }
        }

        try {
            mq_path = page[mq_name];
        } catch (...) {
            break;
        }

        try {
            mq = GetMQ(mq_path);
        } catch (...) {
            fprintf(stderr, "get mq fail, %s, %m\n", mq_path.c_str());
            err_exit();
        }

        if (is_req) {
            req_mq[req_mq_num++] = mq;
        } else {
            /* rsp mq需要登记映射表和加入epoll监控 */
            rsp_mq[rsp_mq_num++] = mq;
            fd = mq->fd();

            if (fd < FD_MQ_MAP_SIZE) {
                mq_mapping[fd]._mq = mq;
                mq_mapping[fd]._active = false;
                mq_mapping[fd]._rspmqidx = rsp_mq_num - 1;
                epoll_add(epfd, fd, mq, EPOLLIN);
                if (fd < mq_mapping_min) {
                    mq_mapping_min = fd;
                }
                if (fd > mq_mapping_max) {
                    mq_mapping_max = fd;
                }
            } else {
                fprintf(stderr, "%s mq's fd is too large, %d >= %d\n",
                        mq_name, fd, FD_MQ_MAP_SIZE);
                err_exit();
            }
        }
    }
}

/*
 *  TCP acceptor
 *支持多ip多端口侦听，至少要侦听一个地址
 */
int init_tcp_listener(CFileConfig& page)
{
    unsigned tcp_listen_cnt = 0;
    CSocketTCP acceptor(-1, false);
    string bind_ip;
    unsigned short bind_port;
    char tmp[64] = {0};
    memset(listenfd2port, 0, sizeof(unsigned short) * FD_LISTEN_MAP_SIZE);
    for (unsigned i = 0; i < max_listen_port; ++i) {
        try {
            if (i == 0) {
                sprintf(tmp, "root\\bind_ip");
                bind_ip = page[tmp];
                sprintf(tmp, "root\\bind_port");
                bind_port = from_str<unsigned short>(page[tmp]);
            } else {
                sprintf(tmp, "root\\bind_ip%d", i + 1);
                bind_ip = page[tmp];
                sprintf(tmp, "root\\bind_port%d", i + 1);
                bind_port = from_str<unsigned short>(page[tmp]);
            }

            tcp_listen_cnt++;
        } catch(...) {
            if (i == 0) {
                fprintf(stderr, "No TCP port bind!\n");
            }
            break;
        }

        if (acceptor.create()) {
            fprintf(stderr, "create acceptor fail, %m\n");
            return -1;
        }
        if (acceptor.set_reuseaddr()) {
            fprintf(stderr, "Reuse TCP socket address fail, %m\n");
            return -1;
        }
        if (enable_reuse_port) {
            if (acceptor.set_reuseport()) {
                fprintf(stderr, "Reuse TCP socket port fail, %m\n");
                return -1;
            }
        }
        if (acceptor.bind(bind_ip, bind_port)) {
            fprintf(stderr, "bind port fail, %m\n");
            return -1;
        }
        acceptor.set_nonblock(tcp_nodelay);
        acceptor.listen(defer_accept, listen_backlog);
        if (acceptor.fd() >= FD_LISTEN_MAP_SIZE) {
            fprintf(stderr, "Too large listen fd %d.\n", acceptor.fd());
            return -1;
        }
        listenfd2port[acceptor.fd()] = bind_port;
        /* For listen socket, add fd to epoll data. */
        epoll_add(epfd, acceptor.fd(), (void *)(acceptor.fd()), EPOLLIN);
        fprintf(stdout, "===> ccd listen %s:%d\n", bind_ip.c_str(), bind_port);
    }

    return tcp_listen_cnt;
}

int init_udp_listener(CFileConfig& page)
{
    unsigned            udp_listen_cnt = 0;
    string              udp_bind_ip;
    unsigned short      udp_bind_port;
    CSocketUDP          udp_server(-1, false);
    unsigned long long  udp_server_flow;
    ConnCache          *udp_server_cc = NULL;
    char tmp[64] = {0};
    for (udp_listen_cnt = 0; udp_listen_cnt < max_listen_port; udp_listen_cnt++) {
        try {
            if (udp_listen_cnt == 0) {
                sprintf(tmp, "root\\udp\\bind_ip");
                udp_bind_ip = page[tmp];
                sprintf(tmp, "root\\udp\\bind_port");
                udp_bind_port = from_str<unsigned short>(page[tmp]);
            } else {
                sprintf(tmp, "root\\udp\\bind_ip%d", udp_listen_cnt + 1);
                udp_bind_ip = page[tmp];
                sprintf(tmp, "root\\udp\\bind_port%d", udp_listen_cnt + 1);
                udp_bind_port = from_str<unsigned short>(page[tmp]);
            }
        } catch (...) {
            if ( udp_listen_cnt == 0 ) {
                fprintf(stderr, "No UDP port bind!\n");
            }
            break;
        }

        if (udp_server.create()) {
            fprintf(stderr, "create UDP server socket fail! %m\n");
            return -1;
        }
        if (udp_server.set_reuseaddr()) {
            fprintf(stderr, "Reuse UDP server socket address fail! %m\n");
            return -1;
        }
        if (enable_reuse_port) {
            if (udp_server.set_reuseport()) {
                fprintf(stderr, "Reuse UDP server socket port fail! %m\n");
                return -1;
            }
        }
        if (udp_server.bind(udp_bind_ip, udp_bind_port)) {
            fprintf(stderr, "UDP bind port fail, %m\n");
            return -1;
        }
        if (udp_server.set_nonblock()) {
            fprintf(stderr, "Set UDP server socket to nonblock fail! %m\n");
            return -1;
        }

        udp_server_cc = ccs->AddConn(udp_server.fd(), udp_server_flow,
                                     cc_server_udp);
        if (!udp_server_cc) {
            fprintf(stderr,
                    "Get connection cache for UDP server socket fail!\n");
            return -1;
        }
        udp_server_cc->_listen_port = udp_bind_port;
        udp_server_cc->_reqmqidx = 0;
        epoll_add(epfd, udp_server.fd(), udp_server_cc, EPOLLIN);
    }

    return udp_listen_cnt;
}

int init_conncache(CFileConfig& ccd_page, CFileConfig& service_config_page,
                   bool so_opened, CSOFile& so_file,
                   unsigned tcp_listen_cnt, bool start_as_new_interface)
{
    try {
        sync_enabled = from_str<bool>(ccd_page["root\\sync_enable"]);
    } catch (...) {
        /* nothing */
    }

    std::map<std::string, std::string> service_config_map;
    app::ServerNetHandler* ccd_net_handler = NULL;

    service_config_page.GetConfigMap(&service_config_map);

    if (so_opened) {
        /* so open, has tcp */
        if (start_as_new_interface == true)
        {
            /* The method for tae new api initalize */
            typedef app::ServerNetHandler* (* create_net_handler)();

            std::string name;
            try {
                name =
                    service_config_page["root\\create_server_net_handler_func"];
            } catch(...) {
                name = "create_server_net_handler";
            }

            create_net_handler constructor =
                (create_net_handler)so_file.get_func(name.c_str());
            if (constructor == NULL) {
                fprintf(stderr, "no constructor function for "
                        "create_net_handler is found\n");
                return -1;
            }

            ccd_net_handler = constructor();
            if (ccd_net_handler->init(service_config_map) != 0) {
                fprintf(stderr,
                        "init user defined server_net_handler failed\n");
                return -1;
            }
        } else {
            /* This is for old mcp++ initialize */
            app::CompatibleCCDNetHandler* handler =
                new app::CompatibleCCDNetHandler();
            if (handler->compatible_init(ccd_page, so_file,
                                         tcp_listen_cnt, event_notify) != 0)
            {
                fprintf(stderr, "compatible init ccd net_handler failed\n");
                return -1;
            }
            ccd_net_handler = handler;
        }
    } else {
        /* so not open, only for udp */
        std::map<std::string, std::string> ccd_config_map;
        ccd_page.GetConfigMap(&ccd_config_map);

        ccd_net_handler = new app::ServerNetHandler();
        if (start_as_new_interface) {
            ccd_net_handler->init(service_config_map);
        } else {
            ccd_net_handler->init(ccd_config_map);
        }
    }

    /*
     *  cached conn set
     */
    /* 1kb，接收初始缓冲区大小，这个要根据业务实际情况和内存使用酌情配置 */
    unsigned rbsize = 1<<10;
    /* 16kb，发送初始缓冲区大小，这个要根据业务实际情况和内存使用酌情配置 */
    unsigned wbsize = 1<<14;
    try {
        rbsize = from_str<unsigned>(ccd_page["root\\recv_buff_size"]);
        wbsize = from_str<unsigned>(ccd_page["root\\send_buff_size"]);
    } catch(...) {
        /* nothing */
    }
	
    if (event_notify) {
        if (conn_notify_details) {
            ccs = new CConnSet(ccd_net_handler, NULL, max_conn, rbsize,
                               wbsize, handle_cc_close, send_notify_2_mcd);
        } else {
            ccs = new CConnSet(ccd_net_handler, NULL, max_conn, rbsize,
                               wbsize, handle_cc_close, NULL);
        }
    } else {
        ccs = new CConnSet(ccd_net_handler, NULL, max_conn, rbsize,
                           wbsize, NULL, NULL);
    }

    fprintf(stderr, "ccd event_notify=%d,timeout=%u,stat_time=%u,max_conn=%u,"
            "rbsize=%u,wbsize=%u\n", event_notify, time_out, stat_time,
            max_conn, rbsize, wbsize);

    return 0;
}

int init_wdc(CFileConfig& page, CSOFile& so_file)
{
    /*
     * watchdog client
     */
    
    try {
        string wdc_conf = page["root\\watchdog_conf_file"];
        try {
            wdc = new CWatchdogClient;
        } catch (...) {
            fprintf(stderr, "Watchdog client alloc fail, %m\n");
            return -1;
        }

        /* Get frame version. */
        char *frame_version =
            (strlen(version_string) > 0 ? version_string : NULL);

        /* Get plugin version. */
        const char *plugin_version = NULL;
        get_plugin_version pv_func =
            (get_plugin_version)so_file.get_func("get_plugin_version_func");
        if (pv_func) {
            plugin_version = pv_func();
        } else {
            plugin_version = NULL;
        }

        /* Get addition 0. */
        const char *add_0 = NULL;
        get_addinfo_0 add0_func =
            (get_addinfo_0)so_file.get_func("get_addinfo_0_func");
        if (add0_func) {
            add_0 = add0_func();
        } else {
            add_0 = NULL;
        }

        /* Get addition 1. */
        const char *add_1 = NULL;
        get_addinfo_1 add1_func =
            (get_addinfo_1)so_file.get_func("get_addinfo_1_func");
        if (add1_func) {
            add_1 = add1_func();
        } else {
            add_1 = NULL;
        }

        if (wdc->Init(wdc_conf.c_str(), PROC_TYPE_CCD, frame_version,
                      plugin_version,
                      NULL,
                      add_0, add_1))
        {
            fprintf(stderr, "watchdog client init fail, %s,%m\n",
                    wdc_conf.c_str());
            return -1;
        }
    } catch(...) {
        /* watchdog 功能并不是必须的 */
        fprintf(stderr, "Watchdog not enabled!\n");
    }

    return 0;
}

int main(int argc, char* argv[])
{
    if (argc < 2) {
        printf("%s conf_file [non-daemon]\n", argv[0]);
        err_exit();
    }

    if (!strncasecmp(argv[1], "-v", 2)) {
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
    }

    if (!one_instance(argv[1])) {
        err_exit();
    }

    CFileConfig page;
    try {
        page.Init(argv[1]);
    } catch(...) {
        fprintf(stderr, "open config fail, %s, %m\n", argv[1]);
        err_exit();
    }

    int max_open_file_num = kDefaultMaxOpenFileNum;  /* default 1M */
    try {
        max_open_file_num = from_str<int>(page["root\\max_open_file_num"]);
    } catch (...) {
        /* nothing */
    }

    fprintf(stderr, "ccd max_open_file_num:%d\n", max_open_file_num);

    int ret = 0;
    if (argc == 2) {
        ret = mydaemon(argv[0], max_open_file_num);
    } else {
        ret = initenv(argv[0], max_open_file_num);
    }

    /*
     * For mcp++ Easy use, interface and configs are modified,
     * so we have to judge which version we are using
     * by whether ccd.conf has root\\service_config
     */
    CFileConfig service_config_page;
    bool start_as_new_interface = false;
    try {
        std::string service_config_file = page["root\\service_config"];
        try {
            service_config_page.Init(service_config_file.c_str());
        } catch (...) {
            fprintf(stderr, "load service config file:%s failed\n",
                    service_config_file.c_str());
            err_exit();
        }
        fprintf(stderr,
                "service config file find, using service.conf to init\n");
        start_as_new_interface = true;
    } catch (...) {
        fprintf(stderr,
                "no service config file find, using ccd.conf to init\n");
        service_config_page = page;
        start_as_new_interface = false;
    }

    init_global_cfg(page);

    /* Bind CPU. */
    try {
        int bind_cpu = from_str<int>(service_config_page["root\\bind_cpu"]);
        cpubind(argv[0], bind_cpu);
    } catch (...) {
        /* nothing */
    }

    /*
     *  epoll
     */
    epfd = epoll_create(max_conn);
    if (epfd < 0) {
        fprintf(stderr, "create epoll fail, %u,%m\n", max_conn);
        err_exit();
    }
    epev = new struct epoll_event[max_conn];

    /*
     *  open mq
     */
    memset(mq_mapping, 0x0, sizeof(MQINFO) * FD_MQ_MAP_SIZE);
    init_mq_conf(page, true);
    init_mq_conf(page, false);
    if (req_mq_num < 1 || rsp_mq_num < 1) {
        fprintf(stderr, "no req mq or rsp mq, %u,%u\n", req_mq_num, rsp_mq_num);
        err_exit();
    }

    /*
     *  net load
     */

    /* init over ctrl model */
      int use_ctrl = 0;    
      try {
          use_ctrl = from_str<int>(page["root\\use_over_ctrl"]);
       } catch(...) {
           use_ctrl = 0;
       }
	OVER_CTRL->init(getpid(), use_ctrl);
	OVER_CTRL->start_thread();

	int net_work_overflow_rate = 0, cpu_overload_rate = 0;
	try {
		net_work_overflow_rate = from_str<int>(page["root\\network_overflow_rate"]);
		cpu_overload_rate      = from_str<int>(page["root\\cpu_overload_rate"]);

		fprintf(stderr, "read config: cpu overload rate:%d,network overload rate:%d\n"
				,net_work_overflow_rate
				,cpu_overload_rate);

		OVER_CTRL->set_cpu_overload_rate(cpu_overload_rate);
		OVER_CTRL->set_network_over_flow_rate(net_work_overflow_rate);
	} catch(...) {
	}

    unsigned grid_count = 100, grid_distant = 100, req_water_mark = 1000000;
    try {
        /* 这里兼容老mcp使用习惯，需要配置这三个参数，参数意义见文档 */
        grid_count = from_str<unsigned>(page["root\\grid_num"]);
        grid_distant = from_str<unsigned>(page["root\\grid_distant"]);
        req_water_mark = from_str<unsigned>(page["root\\req_water_mark"]);
    } catch(...) {
        /* 这里是mcp++新配置方式，只配置req_water_mark，表示每秒最大请求数 */
        try {
            req_water_mark = from_str<unsigned>(page["root\\req_water_mark"]);
            if (req_water_mark < 100000000) {
                /*
                 * 这里req_water_mark*10是因为
                 * grid_count*grid_distant=10000ms=10s
                 */
                req_water_mark *= 10;
            } else {
                fprintf(stderr,
                        "req_water_mark is too large, %u\n", req_water_mark);
            }
        } catch(...) {
            /* nothing */
        }
    }
    struct timeval time_now = *CConnSet::GetMonotonicNowTime();
    pload_grid = new CLoadGrid(grid_count, grid_distant,
                               req_water_mark, time_now);

    int tcp_listen_cnt = init_tcp_listener(service_config_page);
    if (0 > tcp_listen_cnt) {
        err_exit();
    }

    bool so_config = false;
    bool so_opened = false;
    string complete_so_file;

    try {
        if (start_as_new_interface == false) {
            /* Start as old, load from ccd.conf */
            complete_so_file = page["root\\complete_so_file"];
        } else {
            /* Start as new interface, load from service_config_file.conf */
            complete_so_file =
                service_config_page["root\\server_net_handler_so_file"];
        }
        so_config = true;
    } catch (...) {
        so_config = false;
        if (tcp_listen_cnt) {
            fprintf(stderr, "tcp_listen_cnt > 0, but no complete_so_file or "
                    "net_handler_so_file is found in config\n");
            err_exit();
        }
    }

    CSOFile so_file;
    if (so_config) {
        if (so_file.open(complete_so_file) != 0) {
            so_opened = false;
            if (tcp_listen_cnt) {
                fprintf(stderr, "so_file open fail, %s, %m\n",
                        complete_so_file.c_str());
                err_exit();
            }
        } else {
            so_opened = true;
        }
    }

    if (init_conncache(page, service_config_page, so_opened, so_file,
                       tcp_listen_cnt, start_as_new_interface))
    {
        err_exit();
    }

    /* initialize udp listener */
    int udp_listen_cnt = init_udp_listener(service_config_page);
    if (0 > udp_listen_cnt) {
        err_exit();
    }

    if (tcp_listen_cnt == 0 && udp_listen_cnt == 0) {
        fprintf(stderr, "No TCP or UDP to bind!\n");
        err_exit();
    }

#ifdef _SPEEDLIMIT_
    try {
        speed_limit = from_str<bool>(page["root\\speed_limit"]);
    } catch (...) {
        /* nothing */
    }

    if (speed_limit) {
        if (sync_enabled) {
            fprintf(stderr, "sync enable not support in speed limited!\n");
            err_exit();
        }

        unsigned download_speed = 0;
        unsigned upload_speed = 0;
        unsigned low_buff_size = 0;
        try {
            download_speed = from_str<unsigned>(page["root\\download_speed"]);
        } catch(...) {
            /* nothing */
        }
        try {
            upload_speed = from_str<unsigned>(page["root\\upload_speed"]);
        } catch(...) {
            /* nothing */
        }
        try {
            low_buff_size = from_str<unsigned>(page["root\\low_buff_size"]);
        } catch(...) {
            /* nothing */
        }
        /* 这里设置全局的上传下载速率，0值表示不限制 */
        ccs->SetSpeedLimit(download_speed, upload_speed, low_buff_size);
    }
#endif

    unsigned long exp_max_pkt_size = 0;
    try {
        exp_max_pkt_size = from_str<unsigned>(page["root\\exp_max_pkt_size"]);
    } catch(...) {
        /* nothing */
    }

    if ((_BUF_LEN <= exp_max_pkt_size)
        || ((send_buff_max && (send_buff_max < exp_max_pkt_size))
            && (recv_buff_max && (recv_buff_max < exp_max_pkt_size))))
    {
        fprintf(stderr, "CCD exp_max_pkt_size(%lu) too large, or larger than "
                "recv_buff_max/send_buff_max.\n", exp_max_pkt_size);
        err_exit();
    }

    if (exp_max_pkt_size) {
        fprintf(stderr, "CCD exp_max_pkt_size enabled, expect max "
                "size - %lu.\n", exp_max_pkt_size);
    }

#ifdef _SHMMEM_ALLOC_
    /*
     * share memory allocator
     */
    try {
        enqueue_with_shm_alloc =
            from_str<bool>(page["root\\shmalloc\\enqueue_enable"]);
    } catch(...) {
        /* nothing */
    }
    try {
        dequeue_with_shm_alloc =
            from_str<bool>(page["root\\shmalloc\\dequeue_enable"]);
    } catch(...) {
        /* nothing */
    }
    if (enqueue_with_shm_alloc || dequeue_with_shm_alloc) {
        try {
            if (OpenShmAlloc(page["root\\shmalloc\\shmalloc_conf_file"])) {
                fprintf(stderr, "shmalloc init fail, %m\n");
                err_exit();
            }
        } catch(...) {
            fprintf(stderr, "shmalloc config error\n");
            err_exit();
        }
    }
    fprintf(stderr, "ccd shmalloc enqueue=%d, dequeue=%d\n",
            (int)enqueue_with_shm_alloc, (int)dequeue_with_shm_alloc);
#endif

    if (init_wdc(page, so_file)) {
        err_exit();
    }

    memset(&net_log_info, 0, sizeof(net_log_info));
    memset(&mem_log_info, 0, sizeof(mem_log_info));
    memset(&net_stat_info, 0, sizeof(net_stat_info));
    log_client = new (nothrow) tools::CLogClient();
    if (NULL == log_client) {
        fprintf(stderr, "alloc log_client api failed\n");
        err_exit();
    }
    if (log_client->init(argv[1]) < 0) {
        fprintf(stderr, "ccd init log_client failed\n");
        err_exit();
    }

    log_client->SetCCDPort(service_config_page);
    log_client->SetIpWithInnerIp();
    log_client->SetMcpppVersion(version_string, strlen(version_string));
    log_client->SetMcpppCompilingDate(compiling_date, strlen(compiling_date));

    unsigned long mem_thresh_size = 1<<30;
    try {
        mem_thresh_size =
            from_str<unsigned long>(page["root\\mem_thresh_size"]);
        if ((mem_thresh_size > ((unsigned long)2 << 30))
            || (mem_thresh_size < ((unsigned long)1 << 27)))
        {
            mem_thresh_size = 1<<30;
        }
    } catch(...) {
        /* nothing */
    }

    fastmem_init(mem_thresh_size, exp_max_pkt_size);

    /*  main loop */
    int   i, eventnum, listen_fd = -1;
    void *ev_data = NULL;
    fprintf(stderr, "ccd started\n");

    ConnCache* ev_cc = NULL;

    while (!stop) {
        /* 处理网络超时与profile统计信息输出 */
        ccs->Watch(time_out, stat_time, pload_grid);

        /* 处理进程整体内存使用量限制 */
        if (overall_mem_max) {
            unsigned long long cur_mem_bytes;
            if (!get_mem_usage(&cur_mem_bytes)) {
                fprintf(stderr, "get current memory usage failed!\n");
#ifdef _OUTPUT_LOG_
                WRITE_LOG(output_log,
                          "MCP++ CCD: get current memory usage failed.\n");
#endif
            } else if (cur_mem_bytes > overall_mem_max) {
                WRITE_LOG(output_log, "MCP++ CCD: cur_mem_bytes:%lld, "
                          "overall_mem_max:%lld\n",
                          cur_mem_bytes, overall_mem_max);
                ccs->HandleMemOverload(cur_mem_bytes - overall_mem_max);
            }
        }

        /* 处理网络事件与管道事件 */
        eventnum = epoll_wait(epfd, epev, max_conn, 1);
        for (i = 0; i < eventnum; ++i)   {
            ev_data = epev[i].data.ptr;

            if ( ev_data >= ccs->BeginAddr() && ev_data <= ccs->EndAddr() ) {
                /* Socket fd event. */
                ev_cc = (struct ConnCache *)ev_data;
                if (ev_cc->_type == cc_tcp) {
                    /* TCP Socket. */
                    handle_socket(ev_cc, &epev[i]);
                } else if (ev_cc->_type == cc_server_udp) {
                    /* UDP server socket. */
                    handle_socket_udp(ev_cc, &epev[i]);
                } else {
                    /* Never here. */
                    WRITE_LOG(output_log,
                        "MCP++ CCD: Invalid connection type %d!\n",
                        ev_cc->_type);
                }
            } else if (ev_data >= (void *)MIN_START_ADDR) {
                /* Mq event. */
                handle_rsp_mq((CFifoSyncMQ *)ev_data);
            } else {
                /* Listen fd event. */
                listen_fd = (int)((long)ev_data);
                handle_accept(listen_fd);
            }
        }

        /*
         * 这里要检查是否有mq没有被epoll激活，
         * 没有的话要扫描一次，避免mq通知机制的不足，详细参阅mq实现说明
         */
        for (i = mq_mapping_min; i <= mq_mapping_max; ++i) {
            if (mq_mapping[i]._mq != NULL) {
                if (mq_mapping[i]._active) {
                    mq_mapping[i]._active = false;
                } else {
                    handle_rsp_mq(mq_mapping[i]._mq, false);
                }
            }
        }

#ifdef _SPEEDLIMIT_
        /* 处理上传下载限速回调 */
        handle_pending();
#endif

        /* 维持与watchdog进程的心跳 */
        if (wdc) {
            wdc->Touch();
        }
    }

    fastmem_fini();

#ifdef _SHMMEM_ALLOC_
    myalloc_fini();
#endif
    fprintf(stderr, "ccd stopped\n");
    syslog(LOG_USER | LOG_CRIT | LOG_PID, "%s ccd stopped\n", argv[0]);
    exit(0);
} /* main */
