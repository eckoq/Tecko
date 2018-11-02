#include <unistd.h>
#include <syslog.h>
#include <errno.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <map>
#include <string>

#include "base/tfc_base_config_file.h"
#include "base/tfc_base_str.h"
#include "base/tfc_base_so.h"
#include "ccd/tfc_net_epoll_flow.h"
#include "ccd/tfc_net_open_mq.h"
#include "ccd/tfc_net_cconn.h"
#include "ccd/tfc_net_socket_tcp.h"
#include "ccd/tfc_net_socket_udp.h"
#include "ccd/version.h"
#include "ccd/mydaemon.h"
#include "ccd/fastmem.h"
#ifdef _SHMMEM_ALLOC_
#include "ccd/tfc_net_open_shmalloc.h"
#include "ccd/myalloc.h"
#endif
#include "watchdog/tfc_base_watchdog_client.h"
#include "common/log_type.h"
#include "common/log_client.h"
#include "dcc/app/client_net_handler.h"
#include "dcc/app/compatible_dcc_net_handler.h"
#include "dcc/tfc_net_dcc_define.h"

using namespace tfc::base;
using namespace tfc::net;
using namespace tfc::watchdog;

/*
 * 1.dcc根据mcd传入的flow号唯一确定一个连接，不同的flow产生不同的
 *   连接(即使ip port一样),mcd有义务维护这个flow
 * 2.如果到同一个ip只有一个连接的话,flow=ip即可,flow不可以等于0或者1
 * 3.如果使用多mq的话，那么一个req_mq必须配对一个rsp_mq
 *   即req_mq对应rsp_mq，req_mq2对应rsp_mq2，依次类推
 */

#ifdef _DEBUG_
#define SAY(fmt, args...) \
    fprintf(stderr, "%s:%d,"fmt, __FILE__, __LINE__, ##args)
#else
#define SAY(fmt, args...)
#endif

/* 最大支持的mq对数 */
#define MAX_MQ_NUM          (1<<6)
/* fd->mq 映射数组最大长度，fd < FD_MQ_MAP_SIZE */
#define FD_MQ_MAP_SIZE      (1<<10)
/* 管道最大等待时间(10s)，超过这个时间就需要清管道 */
#define MAX_MQ_WAIT_TIME    10000

typedef struct {
    CFifoSyncMQ* _mq;        /* 管道 */
    bool         _active;    /* 是否被epoll激活 */
    int          _reqmqidx;  /* 标识mq在req_mq中的序号，
                              * 同时也是对应rsp_mq的序号
                              */
} MQINFO;

enum status_type {
    status_send_connecting = 0, /* 发送数据等待连接建立中 */
    status_connected = 1,       /* 已经建立连接 */
};

/* Dequeue time max. */
unsigned                mq_dequeue_max = 0;
/* ms. */
unsigned long long      udp_msg_timeout = 15000;
unsigned                udp_send_buff_max = (32<<20);
unsigned                udp_send_count_max = 200;
/* DCC not use. */
bool                    sync_enabled = false;
/* speed limit enable flag */
bool                    speed_limit  = false;
/* use monotonic time in CCD/DCC Header */
static bool             use_monotonic_time_in_header = false;
/*当 enqueue失败时是否关闭连接，启用后DCC为长连接时对MCD散列可能有影响 */
static bool             enqueue_fail_close = false;


bool                    tcp_disable = false;
/* 0 not output log, others output log. */
unsigned                output_log = 0;
/*
 * 当该项被配置时，连接的Sendbuf数据堆积超过该值则认为出错并关闭连接，
 * 为0时不启用该检测
 */
unsigned                send_buff_max = 0;
/*
 * 当该项被配置时，连接的Recvbuf数据堆积超过该值则认为出错并关闭连接，
 * 为0时不启用该检测
 */
unsigned                recv_buff_max = 0;
int                     is_ccd = 0;
typedef int (*protocol_convert)(const void* , unsigned, void* , unsigned &);
/* true，则DCC发送连接建立、断开、连接失败等通知给MCD，否则不发送 */
static bool             event_notify = false;
/* 启用连接关闭细节事件，event_notify启用时配置方有效 */
bool                    close_notify_details = false;
/* 启用包完整性检查事件、收发网络数据包事件、mq包和cc确认事件 */
static bool             conn_notify_details = false;
/* profile统计间隔时间 */
static unsigned         stat_time = 60;
/* 连接超时时间 */
static unsigned         time_out = 300;
/* second default = 0, defined in tfc_net_cconn.cpp */
extern int              dcc_request_msg_timeout_thresh;

/* 最大连接数 */
static unsigned         max_conn = 20000;
/* 是否启用socket的tcp_nodelay特性 */
static bool             tcp_nodelay = true;

/* mcd->dcc mq */
static CFifoSyncMQ*     req_mq[MAX_MQ_NUM];
/* dcc->mcd mq */
static CFifoSyncMQ*     rsp_mq[MAX_MQ_NUM];
/* req_mq数目 */
unsigned                req_mq_num;
/* rsp_mq数目 */
static unsigned         rsp_mq_num;
/* fd->mq映射 */
static MQINFO           mq_mapping[FD_MQ_MAP_SIZE];
/* req_mq fd min */
static int              mq_mapping_min = INT_MAX;
/* req_mq fd max */
static int              mq_mapping_max = 0;
/* 数据缓冲区，128M */
static const unsigned   C_TMP_BUF_LEN = (1<<27);
/* 头部+数据的buf */
static char             tmp_buffer[C_TMP_BUF_LEN];
/* dcc->mcd 消息包头部 */
static TDCCHeader      *dcc_header = (TDCCHeader*)tmp_buffer;
/* dcc->mcd 消息消息体 */
static char            *_buf = (char*)tmp_buffer + DCC_HEADER_LEN;
/* 可容纳最大消息体长度 */
static unsigned         _BUF_LEN = C_TMP_BUF_LEN - DCC_HEADER_LEN;
/* 连接对象 */
static CConnSet        *ccs = NULL;
/* epoll句柄 */
static int              epfd = -1;
/* epoll事件数组 */
static struct epoll_event* epev = NULL;
/* 毫秒 */
static unsigned         dcc_request_msg_mq_timeout_thresh = 0;

#ifdef _SHMMEM_ALLOC_
/*
 * 使用共享内存分配器存储数据的时候mq请求或回复包的格式如下：
 *  TDCCHeader + mem_handle + len
 * mem_handle是数据在内存分配器的地址，len是该数据的长度；
 * 且dccheader的_type值为dcc_req_send_shm或者dcc_rsp_data_shm
 * 该方式减少了数据的内存拷贝，但是需要enqueue的进程分配内存，
 * dequeue的进程释放内存
 */
/* true表示dcc->mcd使用共享内存分配器存储数据，否则不使用 */
static bool             enqueue_with_shm_alloc = false;
/* true表示mcd->dcc使用共享内存分配器存储数据，否则不使用 */
static bool             dequeue_with_shm_alloc = false;
#endif
/* watchdog client */
CWatchdogClient*        wdc = NULL;

tools::CLogClient      *log_client = NULL;
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

inline
void send_notify_2_mcd(int type, ConnCache* cc, unsigned short arg = 0,
                       const char *data = NULL, unsigned int data_len = 0)
{
    /* 
     * we use local buf to avoid overwrite the tmp_buffer via global
     * ccd_header variable
     */
    static char buf[512];
    /* dcc->mcd 消息包头部 */
    TDCCHeader *local_dcc_header = (TDCCHeader*)buf;
    local_dcc_header->_arg  = arg;
    local_dcc_header->_type = type;
    CConnSet::GetHeaderTimestamp(local_dcc_header->_timestamp,
                                 local_dcc_header->_timestamp_msec,
                                 use_monotonic_time_in_header);

    unsigned send_len = DCC_HEADER_LEN;
    if (0 < data_len && data_len <= 512 - DCC_HEADER_LEN && data != NULL) {
        memcpy(buf + send_len, data, data_len);
        send_len += data_len;
    }

    if (cc) {
        if (cc->_type == cc_tcp) {
            local_dcc_header->_ip = cc->_ip;
            local_dcc_header->_port = cc->_port;
        } else {
            local_dcc_header->_ip = 0;
            local_dcc_header->_port = 0;
        }
        rsp_mq[cc->_reqmqidx]->enqueue(buf, send_len, cc->_flow);
    } else {
        local_dcc_header->_ip = 0;
        local_dcc_header->_port = 0;
        for (unsigned i = 0; i < rsp_mq_num; ++i) {
            rsp_mq[i]->enqueue(buf, send_len, 0ULL);
        }
    }
}

void handle_cc_close(ConnCache* cc, unsigned short event) {
    static TDCCHeader header;
    if (cc->_type == cc_tcp) {
        header._ip = cc->_ip;
        header._port = cc->_port;
    } else {
        header._ip = 0;
        header._port = 0;
    }
    header._type = event;
    CConnSet::GetHeaderTimestamp(header._timestamp, header._timestamp_msec,
                                 use_monotonic_time_in_header);
    rsp_mq[cc->_reqmqidx]->enqueue(&header, DCC_HEADER_LEN, cc->_flow);
}

inline
ConnCache* make_new_cc_udp(CConnSet* ccs, unsigned long long flow, int& ret,
                           unsigned short local_port = 0)
{
    static CSocketUDP socket(-1, false);

    if ((ret = socket.create()) < 0) {
        return NULL;
    }
    if ((ret = socket.set_nonblock()) < 0) {
        return NULL;
    }

    if (local_port != 0) {
        /* bind port only; */
        string any_ip("");
        if ((ret = socket.bind(any_ip, local_port)) < 0) {
            return NULL;
        }
    }

    ConnCache* cc = ccs->AddConn(socket.fd(), flow, cc_client_udp);
    if (!cc) {
        socket.close();
        return NULL;
    }

    cc->_ip = 0;
    cc->_port = 0;

    return cc;
}

void handle_socket_message_udp(struct ConnCache* cc, char *data,
                               size_t data_len, struct sockaddr_in &addr)
{
    struct timeval *time_now = NULL;
    static time_t log_time = 0;

    time_now = CConnSet::GetMonotonicNowTime();

    dcc_header->_ip = addr.sin_addr.s_addr;
    dcc_header->_port = ntohs(addr.sin_port);
    CConnSet::GetHeaderTimestamp(dcc_header->_timestamp,
                                 dcc_header->_timestamp_msec,
                                 use_monotonic_time_in_header);
    dcc_header->_type = dcc_rsp_data_udp;

#ifdef _SHMMEM_ALLOC_
    memhead mhead;
    mhead.mem = NULL_HANDLE;
    if (enqueue_with_shm_alloc) {
        mhead.mem = myalloc_alloc(data_len);
        if (mhead.mem != NULL_HANDLE) {
            dcc_header->_type = dcc_rsp_data_shm_udp;
            mhead.len = data_len;
            memcpy(myalloc_addr(mhead.mem), data, data_len);
            memcpy(_buf, &mhead, sizeof(memhead));
            data_len = sizeof(memhead);
        }
    }
#endif

    data_len += DCC_HEADER_LEN;
    write_net_log(tools::LOG_ENQ_DATA, cc, data_len, (2 * cc->_reqmqidx + 1));
    if (rsp_mq[cc->_reqmqidx]->enqueue(tmp_buffer, data_len, cc->_flow)) {
#ifdef _OUTPUT_LOG_
        LOG_5MIN(output_log, time_now->tv_sec, log_time,
                 "MCP++ DCC: Enqueue to mq fail of UDP socket.\n");
#endif
        if (time_now->tv_sec > log_time) {
            write_net_log(tools::LOG_ENQ_FAIL_DROP, cc, data_len,
                          (2 * cc->_reqmqidx + 1));
            log_time = time_now->tv_sec + 300;
        }

#ifdef _SHMMEM_ALLOC_
        if (enqueue_with_shm_alloc && mhead.mem != NULL_HANDLE) {
            myalloc_free(mhead.mem);
        }
#endif
    }
}

bool handle_socket_recive_udp(struct ConnCache *cc)
{
    int ret;
    struct sockaddr_in addr;
    struct sockaddr *paddr = (struct sockaddr *)(&addr);
    socklen_t addr_len;
    size_t recvd_len;
    unsigned loop_cnt = 0;

    if (cc->_fd < 0 || cc->_type != cc_client_udp) {
        WRITE_LOG(output_log, "MCP++ DCC: Fd or type error of UDP socket recv! "
                  "fd - %d, type - %d, flow - %llu.\n",
                  cc->_fd, cc->_type, cc->_flow);

        return false;
    }

    do {
        loop_cnt++;
        if (loop_cnt > 100) {
            return true;
        }

        ret = ccs->RecvUDP(cc, _buf, _BUF_LEN, recvd_len, (*paddr), addr_len);
        const struct sockaddr_in *temp =
            reinterpret_cast<const struct sockaddr_in*>(&paddr);
        if (ret == -E_NEED_CLOSE) {
            ccs->CloseCC(cc, dcc_rsp_disconnect_peer_or_error);
            return false;
        } else if (ret == -E_NEED_RECV) {
            /* EAGAIN. */
            return true;
        } else if (ret == -E_RECVED) {
            /* Data recived. */
            ccs->EndStat(cc, temp);
            ccs->_remote_stat_info.UpdateSize(static_cast<uint32_t>(temp->sin_addr.s_addr),
                                              temp->sin_port, 0, recvd_len);

            handle_socket_message_udp(cc, _buf, recvd_len, addr);
            continue;
        } else if (ret == -E_TRUNC) {
            /* Not enqueue to mcd when data trunc. */
            ccs->EndStat(cc, temp);
            continue;
        } else {
            /* Other error. */
            ccs->CloseCC(cc, dcc_rsp_disconnect_peer_or_error);
            return false;
        }
    } while (!stop && (ret == -E_RECVED || ret == -E_TRUNC));

    return false;
}

bool handle_socket_send_udp(struct ConnCache *cc)
{
    int ret;

    if (cc->_fd < 0 || cc->_type != cc_client_udp) {
        WRITE_LOG(output_log, "MCP++ DCC: Fd or type error of UDP socket send! "
                  "fd - %d, type - %d, flow - %llu.\n",
                  cc->_fd, cc->_type, cc->_flow);

        return false;
    }

    if (ccs->GetSendBufferSize(cc)) {
        ccs->StartStat(cc);
    }

    ret = ccs->SendFromCacheUDP(cc);
    if (ret == -E_SEND_COMP) {
        epoll_mod(epfd, cc->_fd, cc, EPOLLIN);
        return true;
    } else if ( ret == -E_NEED_SEND ) {
        return true;
    } else if ( ret == -E_NEED_CLOSE ) {
        ccs->CloseCC(cc, dcc_rsp_disconnect_peer_or_error);
        return false;
    } else if ( ret == -E_FORCE_CLOSE ) {
        /* Do nothing. */
        return false;
    } else {
        /* Should not run here. ERROR. */
        ccs->CloseCC(cc, dcc_rsp_disconnect_peer_or_error);
        return false;
    }
}

void handle_socket_udp(struct ConnCache *cc, struct epoll_event* ev)
{
    int events = ev->events;

    /* Check cc by fd. */
    if (cc->_fd < 0) {
        WRITE_LOG(output_log, "MCP++ DCC: Client UPD socket fd error! "
                  "fd - %d, flow - %llu.\n", cc->_fd, cc->_flow);
        return;
    }

    if (!(events & (EPOLLIN | EPOLLOUT))) {
        /* Not close UDP server socket. */
#ifdef _OUTPUT_LOG_
        WRITE_LOG(output_log, "MCP++ DCC: Client UPD socket EPOLL event error! "
                  "Close cc. fd - %d, flow - %llu.\n", cc->_fd, cc->_flow);
#endif
        write_net_log(tools::LOG_EPOLL_ERR, cc, 0, -1, errno);
        ccs->CloseCC(cc, dcc_rsp_disconnect_error);
        return;
    }

    /*
     * 检查dcc的写RawCache中缓存的消息是否过期
     * 如果过期了，说明连接不正常，断开连接
     */
    const struct timeval* now =
        (use_monotonic_time_in_header ? CConnSet::GetMonotonicNowTime() : CConnSet::GetWallNowTime());
    if (dcc_request_msg_timeout_thresh > 0
        && cc->_w->is_msg_timeout(dcc_request_msg_timeout_thresh * 1000, now))
    {
        LOG_ONCE("MCP++ DCC: dcc buffered data timeout thresh:%d seconds, "
                 "fd:%d, flow:%lld, close connection\n",
                 dcc_request_msg_timeout_thresh, cc->_fd, cc->_flow);
        ccs->CloseCC(cc, dcc_rsp_disconnect_peer_or_error);
        return;
    }

    if (events & EPOLLIN) {
        if (!handle_socket_recive_udp(cc)) {
            return;
        }
    }

    if (events & EPOLLOUT) {
        if (!handle_socket_send_udp(cc)) {
            return;
        }
    }
}

inline
ConnCache* make_new_cc(CConnSet* ccs, unsigned long long flow, unsigned ip,
                       unsigned short port, int& ret)
{
    static CSocketTCP socket(-1, false);
    socket.create();
    socket.set_nonblock(tcp_nodelay);
    ret = socket.connect(ip, port);
    if ((ret != 0) && (ret != -EWOULDBLOCK) && (ret != -EINPROGRESS)) {
#ifdef _OUTPUT_LOG_
        WRITE_LOG(output_log, "MCP++ DCC: connect() system call fail in "
                  "make_new_cc! %m flow - %llu, ip - %u, port - %hu.\n",
                  flow, ip, port);
#endif
        close(socket.fd());
        return NULL;
    }

    /* 这里DCC的连接分配采用可淘汰方式，为了确保DCC总是连接目标成功 */
    ConnCache* cc = ccs->AddConn(socket.fd(), flow);
    if (!cc) {
        fprintf(stderr, "add conn fail, %m\n");
#ifdef _OUTPUT_LOG_
        WRITE_LOG(output_log, "MCP++ DCC: Add cc fail in make_new_cc! "
                  "flow - %llu, ip - %u, port - %hu.\n", flow, ip, port);
#endif
        close(socket.fd());
        return NULL;
    }

    /* 这里主动设置目标服务器的ip和port，否则如果连接没建立将获取不了ip和port */
    cc->_ip = ip;
    cc->_port = port;
    return cc;
}

int handle_socket_send(struct ConnCache* cc)
{
    if (cc->_fd < 0) {
        return -1;
    }

    if (cc->_connstatus != status_send_connecting
        && cc->_connstatus != status_connected)
    {
        return -1;
    }

    /* 之前是connecting状态，现在EPOLL_OUT被置位，socket可写，说明连接建立 */
    if (cc->_connstatus == status_send_connecting) {
        cc->_connstatus = status_connected;
        if (event_notify) {
            send_notify_2_mcd(dcc_rsp_connect_ok, cc);
        }
    }

    /*
     * 检查dcc的写RawCache中缓存的消息是否过期
     * 如果过期了，说明连接不正常，断开连接
     */
    const struct timeval* now =
        (use_monotonic_time_in_header ? CConnSet::GetMonotonicNowTime() : CConnSet::GetWallNowTime());
    if (dcc_request_msg_timeout_thresh > 0
        && cc->_w->is_msg_timeout(dcc_request_msg_timeout_thresh * 1000, now))
    {
        LOG_ONCE("MCP++ DCC: dcc buffered data timeout thresh:%d seconds, "
                 "fd:%d, flow:%lld, close connection\n",
                 dcc_request_msg_timeout_thresh, cc->_fd, cc->_flow);
        ccs->CloseCC(cc, dcc_rsp_disconnect_peer_or_error);
        return -1;
    }

    int ret = ccs->SendFromCache(cc);
    ccs->StartStat(cc);

    /* 发送完毕，需要发送send_ok通知 */
    if (ret == 0) {
        /* 缓存发送完毕,去除EPOLLOUT */
#ifdef _SPEEDLIMIT_
        if (speed_limit && ccs->is_send_speed_cfg(cc)) {
            unsigned send_speed =
                cc->_send_mon.touch(CConnSet::GetMonotonicNowTime());
            cc->_send_mon.reset_stat();
            send_notify_2_mcd(dcc_rsp_send_ok, cc, (send_speed >> 10));
        }
#endif

        cc->_epoll_flag = (cc->_epoll_flag & (~EPOLLOUT));
        epoll_mod(epfd, cc->_fd, cc, cc->_epoll_flag);
        return 0;
    }

    /* 没有限速，或者速度低于限速阈值 */
    if (ret == -E_NEED_SEND) {
        cc->_epoll_flag = (cc->_epoll_flag | EPOLLOUT);
        epoll_mod(epfd, cc->_fd, cc, cc->_epoll_flag);
        return 0;
    }


#ifdef _SPEEDLIMIT_
    /* 需要发送send_nearly_ok通知，同时需要pending，去掉epollout */
    if (ret == -E_NEED_PENDING_NOTIFY) {
        cc->_epoll_flag = (cc->_epoll_flag & (~EPOLLOUT));
        epoll_mod(epfd, cc->_fd, cc, cc->_epoll_flag);
        if (speed_limit && ccs->is_send_speed_cfg(cc)) {
            unsigned send_speed =
                cc->_send_mon.touch(CConnSet::GetMonotonicNowTime());
            /* 设置参数为已限速(1) */
            send_notify_2_mcd(dcc_rsp_send_nearly_ok, cc, (send_speed >> 10));
        }
        return 0;
    }

    /* 需要pending，去掉epollout */
    if (ret == -E_NEED_PENDING) {
        cc->_epoll_flag = (cc->_epoll_flag & (~EPOLLOUT));
        epoll_mod(epfd, cc->_fd, cc, cc->_epoll_flag);
        return 0;
    }
#endif

    /* 连接已经在SendFromCache中关闭 */
    if (ret == -E_FORCE_CLOSE) {
        return -1;
    }

    /* 发送失败,关闭连接 */
    if (ret == -E_NEED_CLOSE) {
        /*
         * 由于发送失败会关闭连接，这里就不发送“发送失败”通知，
         * 因为连接关闭会发送连接“连接关闭”通知
         */
#ifdef _OUTPUT_LOG_
        WRITE_LOG(output_log, "MCP++ DCC: SendFromCache failed in "
                  "handle_socket! flow - %llu, ip - %u, port - %hu.\n",
                  cc->_flow, cc->_ip, cc->_port);
#endif

        ccs->CloseCC(cc, dcc_rsp_disconnect_peer_or_error);
        return -1;
    }

    fprintf(stderr, "handle socket send should not come here\n");
    assert(false);
    return 0;
}

int handle_socket_recv(struct ConnCache* cc) {
    if (cc->_fd < 0) {
        return -1;
    }

    /*
     * 接收消息，如果发现对端断开连接但是读缓冲里还有未处理的数据
     * 则延迟关闭_finclose被设置
     */
    int ret = ccs->Recv(cc);

    /* ret == 0，遇到错误，finclose置位，不再接收 */
    if (ret == 0) {
        cc->_epoll_flag = (cc->_epoll_flag & (~EPOLLIN));
        epoll_mod(epfd, cc->_fd, cc, cc->_epoll_flag);
        return 0;
    }

    /* loop_cnt > 100，或者遇到-EAGAIN，监听epoll_in */
    if (ret == -E_NEED_RECV) {
        cc->_epoll_flag = (cc->_epoll_flag | EPOLLIN);
        epoll_mod(epfd, cc->_fd, cc, cc->_epoll_flag);
        return 0;
    }

    /* 需要挂起该连接，暂时去除epoll_in */
#ifdef _SPEEDLIMIT_
    if (ret == -E_NEED_PENDING) {
        cc->_epoll_flag = (cc->_epoll_flag & (~EPOLLIN));
        epoll_mod(epfd, cc->_fd, cc, cc->_epoll_flag);
        return 0;
    }
#endif

    /* 内存分配失败 */
    if (ret == -E_MEM_ALLOC) {
        fprintf(stderr, "alloc mem fail, %m\n");
#ifdef _OUTPUT_LOG_
        WRITE_LOG(output_log, "MCP++ DCC: Memory overload in handle_socket - "
                  "Recv! flow - %llu, ip - %u, port - %hu.\n",
                  cc->_flow, cc->_ip, cc->_port);
#endif

        if (event_notify) {
            send_notify_2_mcd(dcc_rsp_overload_mem, NULL);
        }

        /* 收缩缓冲内存池 */
        fastmem_shrink(mem_log_info.old_size, mem_log_info.new_size);
        write_mem_log(tools::LOG_RECV_OOM, cc);

        cc->_epoll_flag = (cc->_epoll_flag | EPOLLIN);
        epoll_mod(epfd, cc->_fd, cc, cc->_epoll_flag);
        return 0;
    }

    /* 关闭该连接，连接上已经没有缓存的接收数据 */
    if (ret == -E_NEED_CLOSE) {
        ccs->CloseCC(cc, dcc_rsp_disconnect_peer_or_error);
        return -1;
    }

    fprintf(stderr, "handle socket recv should not come here\n");
    assert(false);
    return 0;
}

void handle_socket_message(struct ConnCache* cc) {
    if (cc->_fd < 0) {
        return;
    }

    unsigned data_len = 0;
    int ret = 0;
    unsigned long long flow = cc->_flow;

#ifdef _SHMMEM_ALLOC_
    bool is_shm_alloc;
#endif
    /* 读出 */
    do {
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

#ifdef _SHMMEM_ALLOC_
            if (is_shm_alloc)
                dcc_header->_type = dcc_rsp_data_shm;
            else
#endif
                dcc_header->_type = dcc_rsp_data;

            dcc_header->_ip = cc->_ip;
            dcc_header->_port = cc->_port;
            CConnSet::GetHeaderTimestamp(dcc_header->_timestamp, dcc_header->_timestamp_msec, use_monotonic_time_in_header);

            data_len += DCC_HEADER_LEN;
            write_net_log(tools::LOG_ENQ_DATA, cc, data_len, (2 * cc->_reqmqidx + 1));
            if (rsp_mq[cc->_reqmqidx]->enqueue(tmp_buffer,
                                               data_len, flow) != 0)
            {
#ifdef _OUTPUT_LOG_
                WRITE_LOG(output_log, "MCP++ DCC: Enqueue fail in "
                          "handle_socket! flow - %llu.\n", flow);
#endif

#ifdef _SHMMEM_ALLOC_
                /* 这里要防止内存泄露 */
                if (is_shm_alloc) {
                    myalloc_free(*((unsigned long*)(tmp_buffer + DCC_HEADER_LEN)));
                }
#endif

                if (enqueue_fail_close) {
                    write_net_log(tools::LOG_ENQ_FAIL_CLOSE, cc, data_len,
                                  (2 * cc->_reqmqidx + 1));
                    LOG_ONCE("MCP++ DCC: Enqueue fail in handle_socket! Close "
                             "connection! flow - %llu.\n", flow);
                    ccs->CloseCC(cc, dcc_rsp_disconnect_overload);
                } else {
                    write_net_log(tools::LOG_ENQ_FAIL_DROP, cc, data_len,
                                  (2 * cc->_reqmqidx + 1));
                }
            }
            ccs->EndStat(cc);
        } else if (ret == -E_NEED_CLOSE) {
#ifdef _OUTPUT_LOG_
            WRITE_LOG(output_log, "MCP++ DCC: GetMessage fail in "
                      "handle_socket! flow - %llu, ip - %u, port - %hu.\n",
                      cc->_flow, cc->_ip, cc->_port);
#endif
            ccs->CloseCC(cc, dcc_rsp_disconnect_error);
            break;
        } else {
            break;
        }
    } while (!stop && ret == 0);

    /* 这里要检查一下是否在Recv的时候发现对端断开连接了，如果是则关闭 */
    if (cc->_finclose) {
        SAY("finclose cc, flow=%llu,ip=%u,port=%d\n", flow, cc->_ip, cc->_port);
        ccs->CloseCC(cc, dcc_rsp_disconnected);
    }
}

void handle_socket(struct ConnCache *cc, struct epoll_event* ev) {
    int events = ev->events;

    /* Check cc by fd. */
    if (cc->_fd < 0) {
        fprintf(stderr,
                "DCC: handle_socket(): cc->fd < 0! cc->fd %d.\n", cc->_fd);
        return;
    }

    if (!(events & (EPOLLOUT|EPOLLIN))) {
#ifdef _OUTPUT_LOG_
        WRITE_LOG(output_log, "MCP++ DCC: Events error in handle_socket! "
                  "flow - %llu, ip - %u, port - %hu.\n",
                  cc->_flow, cc->_ip, cc->_port);
#endif
        write_net_log(tools::LOG_EPOLL_ERR, cc, 0, -1, errno);
        ccs->CloseCC(cc, dcc_rsp_disconnect_error);
        return;
    }

    if (events & EPOLLOUT) {
        if (handle_socket_send(cc) != 0) {
            /* 发送失败，cc状态不正确，或者连接关闭 */
            return;
        }
    }

    if (!(events & EPOLLIN)) {
        return;
    }

    if (handle_socket_recv(cc) != 0) {
        /* 接收失败，连接关闭 */
        return;
    }

    handle_socket_message(cc);
}

void reconnect_and_send_request(MQINFO* mi, memhead* head,
                                unsigned long long flow,
                                unsigned data_len);

void send_mq_request_by_tcp(struct ConnCache* cc, MQINFO* mi,
                            unsigned long long flow, unsigned data_len)
{
    memhead* head = NULL;
#ifdef _SHMMEM_ALLOC_
    if (dequeue_with_shm_alloc && (dcc_header->_type == dcc_req_send_shm)) {
        head = (memhead*)(tmp_buffer + DCC_HEADER_LEN);
    }
#endif

    if (cc == NULL || cc->_fd < 0) {
        reconnect_and_send_request(mi, head, flow, data_len);
        return;
    }

    /*
     * 检查dcc的写RawCache中缓存的消息是否过期
     * 如果过期了，说明连接不正常，断开连接
     */
    const struct timeval* now =
        (use_monotonic_time_in_header ? CConnSet::GetMonotonicNowTime() : CConnSet::GetWallNowTime());
    if (dcc_request_msg_timeout_thresh > 0 && cc != NULL
        && cc->_w->is_msg_timeout(dcc_request_msg_timeout_thresh * 1000, now))
    {
        LOG_ONCE("MCP++ DCC: dcc buffered data timeout thresh:%d seconds, "
                 "fd:%d, flow:%lld, close connection\n",
                 dcc_request_msg_timeout_thresh, cc->_fd, cc->_flow);
        ccs->CloseCC(cc, dcc_rsp_disconnect_peer_or_error);
        return;
    }

    /* 重用已有连接 */
    ccs->StartStat(cc);
    cc->_w->add_new_msg_time(dcc_header->_timestamp,
                             dcc_header->_timestamp_msec);

    ccs->_remote_stat_info.UpdateReq(dcc_header->_ip, dcc_header->_port, true);

    int ret = 0;
#ifdef _SHMMEM_ALLOC_
    if (head) {
        ret = ccs->Send(cc, (char*)myalloc_addr(head->mem), head->len);
        /* 如果返回值是E_NEED_CLOSE，需要重试 */
        if (ret != -E_NEED_CLOSE) {
            myalloc_free(head->mem);
            head = NULL;
        }
    } else {
#endif
        ret = ccs->Send(cc, tmp_buffer + DCC_HEADER_LEN,
                        data_len - DCC_HEADER_LEN);
#ifdef _SHMMEM_ALLOC_
    }
#endif

    /* 未发送完，监控EPOLL_OUT，以免此前没有监控EPOLL_OUT */
    if (ret == 0) {
        cc->_epoll_flag = (cc->_epoll_flag | EPOLLOUT);
        epoll_mod(epfd, cc->_fd, cc, cc->_epoll_flag);
        return;
    }

    /* 发送完毕，不监控EPOLL_OUT，需要发送send_ok通知 */
    if (ret == 1) {
#ifdef _SPEEDLIMIT_
        if (speed_limit && ccs->is_send_speed_cfg(cc)) {
            unsigned send_speed =
                cc->_send_mon.touch(CConnSet::GetMonotonicNowTime());
            cc->_send_mon.reset_stat();
            send_notify_2_mcd(dcc_rsp_send_ok, cc, (send_speed >> 10));
        }
#endif

        cc->_epoll_flag = (cc->_epoll_flag & (~EPOLLOUT));
        epoll_mod(epfd, cc->_fd, cc, cc->_epoll_flag);
        return;
    }

#ifdef _SPEEDLIMIT_
    /* 需要限速，不监控EPOLL_OUT */
    if (ret == -E_NEED_PENDING) {
        cc->_epoll_flag = (cc->_epoll_flag & (~EPOLLOUT));
        epoll_mod(epfd, cc->_fd, cc, cc->_epoll_flag);
        return;
    }
#endif

    if (ret == -E_MEM_ALLOC) {
        fprintf(stderr, "alloc mem fail, %m\n");
#ifdef _OUTPUT_LOG_
        WRITE_LOG(output_log, "MCP++ DCC: Memory overload in handle_req_mq - "
                  "Send! flow - %llu, ip - %u, port - %hu.\n",
                  cc->_flow, cc->_ip, cc->_port);
#endif
        if (event_notify) {
            send_notify_2_mcd(dcc_rsp_overload_mem, NULL);
        }
        /* 收缩缓冲内存池 */
        fastmem_shrink(mem_log_info.old_size, mem_log_info.new_size);

        write_mem_log(tools::LOG_SEND_OOM, cc);
        ccs->_remote_stat_info.UpdateReq(dcc_header->_ip, dcc_header->_port,
                                         false);

        /* 监控EPOLL_OUT，以免此前没有监控EPOLL_OUT */
        cc->_epoll_flag = (cc->_epoll_flag | EPOLLOUT);
        epoll_mod(epfd, cc->_fd, cc, cc->_epoll_flag);
        return;
    }

    if (ret == -E_NEED_CLOSE) {
#ifdef _OUTPUT_LOG_
        WRITE_LOG(output_log, "MCP++ DCC: Send failed in handle_req_mq - Send! "
                  "Will retry! %m flow - %llu, ip - %u, port - %hu.\n",
                  cc->_flow, cc->_ip, cc->_port);
#endif
        write_mem_log(tools::LOG_SEND_ERR_RETRY, cc);
        ccs->_remote_stat_info.UpdateReq(dcc_header->_ip, dcc_header->_port,
                                         false);

        ccs->CloseCC(cc, dcc_rsp_disconnect_peer_or_error);

        /* 有可能是远端ccd主动关闭了连接，所以这里重试一下 */
        reconnect_and_send_request(mi, head, flow, data_len);
        return;
    }

    fprintf(stderr, "send_mq_req should not come here\n");
    assert(false);

    return;
}

void reconnect_and_send_request(MQINFO* mi, memhead* head,
                                unsigned long long flow, unsigned data_len)
{
    /* 建立新连接 */
    int ret = 0;
    struct ConnCache* cc = make_new_cc(ccs, flow, dcc_header->_ip,
                                       dcc_header->_port, ret);
    if (!cc) {
        /* 响应 */
        if (event_notify) {
            dcc_header->_type = dcc_rsp_connect_failed;
            CConnSet::GetHeaderTimestamp(dcc_header->_timestamp,
                                         dcc_header->_timestamp_msec,
                                         use_monotonic_time_in_header);
            rsp_mq[mi->_reqmqidx]->enqueue(dcc_header, DCC_HEADER_LEN, flow);
        }
#ifdef _OUTPUT_LOG_
        WRITE_LOG(output_log, "MCP++ DCC: Connect failed in handle_req_mq! "
                  "flow - %llu.\n", flow);
#endif
        write_net_log(tools::LOG_SOCK_CONN_FAIL, flow, data_len,
                      (2 * mi->_reqmqidx), -ret);
        ccs->_remote_stat_info.UpdateReq(dcc_header->_ip, dcc_header->_port,
                                         false);
#ifdef _SHMMEM_ALLOC_
        /* 释放内存，防止内存泄露 */
        if (head) {
            myalloc_free(head->mem);
        }
#endif
        return;
    }

    cc->_reqmqidx = mi->_reqmqidx;
    cc->_connstatus = status_send_connecting;

    write_net_log(tools::LOG_ESTAB_CONN, cc, 0, -1);
    ccs->_remote_stat_info.UpdateReq(dcc_header->_ip, dcc_header->_port, true);

    ccs->StartStat(cc);
    cc->_w->add_new_msg_time(dcc_header->_timestamp,
                             dcc_header->_timestamp_msec);

    /*
     * 放入缓存
     * 0, send failed or send not complete, add epollout
     * 1, send complete
     */
#ifdef _SHMMEM_ALLOC_
    if (head) {
        ret = ccs->SendForce(cc, (char*)myalloc_addr(head->mem), head->len);
        myalloc_free(head->mem);
    } else {
#endif
        ret = ccs->SendForce(cc, tmp_buffer + DCC_HEADER_LEN,
                             data_len - DCC_HEADER_LEN);
#ifdef _SHMMEM_ALLOC_
    }
#endif

    /* 未发送完毕，监听EPOLL_OUT */
    if (ret == 0) {
        cc->_epoll_flag = (cc->_epoll_flag | EPOLLOUT);
        epoll_add(epfd, cc->_fd, cc, cc->_epoll_flag);
        return;
    }

    /* 发送完毕，不监听EPOLL_OUT */
    if (ret == 1) {
#ifdef _SPEEDLIMIT_
        if (speed_limit && ccs->is_send_speed_cfg(cc)) {
            unsigned send_speed =
                cc->_send_mon.touch(CConnSet::GetMonotonicNowTime());
            cc->_send_mon.reset_stat();
            send_notify_2_mcd(dcc_rsp_send_ok, cc, (send_speed >> 10));
        }

#endif
        if (cc->_connstatus == status_send_connecting) {
            cc->_connstatus = status_connected;
            if (event_notify) {
                send_notify_2_mcd(dcc_rsp_connect_ok, cc);
            }
        }

        cc->_epoll_flag = (cc->_epoll_flag & (~EPOLLOUT));
        epoll_add(epfd, cc->_fd, cc, cc->_epoll_flag);
        return;
    }

    /* 内存分配失败 */
    if (ret == -E_MEM_ALLOC) {
        fprintf(stderr, "alloc mem fail, %m\n");
#ifdef _OUTPUT_LOG_
        WRITE_LOG(output_log, "MCP++ DCC: Memory overload in handle_req_mq - "
                  "SendForce! flow - %llu, ip - %u, port - %hu.\n",
                  cc->_flow, cc->_ip, cc->_port);
#endif
        if (event_notify) {
            send_notify_2_mcd(dcc_rsp_overload_mem, NULL);
        }
        /* 收缩缓冲内存池 */
        fastmem_shrink(mem_log_info.old_size, mem_log_info.new_size);
        write_mem_log(tools::LOG_SEND_OOM, cc);
        ccs->_remote_stat_info.UpdateReq(dcc_header->_ip, dcc_header->_port,
                                         false);

        /* 数据放入缓冲内存池失败，根本没有数据缓冲起来，不需要EPOLL_OUT */
        cc->_epoll_flag = (cc->_epoll_flag & (~EPOLLOUT));
        epoll_add(epfd, cc->_fd, cc, cc->_epoll_flag);
        return;
    }

#ifdef _SPEEDLIMIT_
    if (ret == -E_NEED_PENDING) {
        cc->_epoll_flag = (cc->_epoll_flag & (~EPOLLOUT));
        epoll_add(epfd, cc->_fd, cc, cc->_epoll_flag);
        return;
    }
#endif

    /* 连接关闭 */
    if (ret == -E_NEED_CLOSE) {
        /*
         *由于发送失败会关闭连接，这里就不发送“发送失败”通知，
         * 因为连接关闭会发送连接“连接关闭”通知
         */
#ifdef _OUTPUT_LOG_
        WRITE_LOG(output_log, "MCP++ DCC: send fail in handle_req_mq - "
                  "SendForce! %m flow - %llu, ip - %u, port - %hu.\n",
                  cc->_flow, cc->_ip, cc->_port);
#endif
        ccs->CloseCC(cc, dcc_rsp_disconnect_peer_or_error);
        ccs->_remote_stat_info.UpdateReq(dcc_header->_ip, dcc_header->_port,
                                         false);
        return;
    }

    /* SendForce返回0或者E_NEED_PENDING，继续监听该socket */
    fprintf(stderr, "reconnect should not come here\n");
    assert(false);
    return;
}

void handle_req_mq(CFifoSyncMQ* mq, bool is_epoll_event = true)
{
    unsigned long long flow;
    unsigned data_len;
    int ret;
    ConnCache* cc = NULL;
    unsigned mq_wait_time = 0;

    MQINFO* mi = &mq_mapping[mq->fd()];
    if (mi->_mq != mq) {
        fprintf(stderr, "handle_req_mq(): MQ check fail!\n");
        return;
    }

    if (is_epoll_event) {
        /* 设置mq激活标志 */
        mi->_active = true;
        /* 清除mq通知 */
        mq->clear_flag();
    }

    /* dequeue at most 100 timeout messages once */
    int dequeue_timeout_max_count = 100;
    unsigned dequeue_cnt = 0;
    struct timeval now =
        (use_monotonic_time_in_header ? tools::GET_MONOTONIC_CLOCK() : tools::GET_WALL_CLOCK());

    while (!stop) {
        if (mq_dequeue_max && ((dequeue_cnt++) > mq_dequeue_max)) {
            SAY("DCC dequeue message max reached!\n");
            break;
        }

        /* flow = UINT_MAX; */
        flow = ULLONG_MAX;
        data_len = 0;
        ret = mq->try_dequeue(tmp_buffer, C_TMP_BUF_LEN, data_len, flow);
        if (ret < 0 || data_len == 0) {
            break;
        }

        cc = ccs->GetConnCache(flow);

        mq_wait_time = ccs->CheckWaitTime(dcc_header->_timestamp,
                                          dcc_header->_timestamp_msec, now);
        write_net_log(tools::LOG_DEQ_DATA, flow, data_len, (2 * mi->_reqmqidx),
                      0, mq_wait_time);

        if (dcc_request_msg_mq_timeout_thresh != 0
            && mq_wait_time > dcc_request_msg_mq_timeout_thresh) {
            /* 发生异常，需要清管道 */
            ccs->IncMQMsgTimeoutCount();

            LOG_ONCE("MCP++ DCC: dropped a packet from mcd because packet "
                     "timeout, wait time:%u, header:%ld, now:%ld, thresh:%u\n",
                     mq_wait_time, dcc_header->_timestamp, now.tv_sec,
                     dcc_request_msg_mq_timeout_thresh);

            /* close connection */
            if (cc != NULL) {
                write_net_log(tools::LOG_OVERLOAD_CLOSE, cc);
                LOG_ONCE("MCP++ DCC: overload protect will close connection");
                ccs->CloseCC(cc, dcc_rsp_disconnect_overload);
            }

#ifdef _SHMMEM_ALLOC_
            /* release myalloc mem if any */
            if (dequeue_with_shm_alloc
                && (dcc_header->_type == dcc_req_send_shm
                    || dcc_header->_type == dcc_req_send_shm_udp))
            {
                memhead* head = (memhead*)(tmp_buffer + DCC_HEADER_LEN);
                myalloc_free(head->mem);
            }
#endif

            if (--dequeue_timeout_max_count >= 0) {
                continue;
            } else {
                break;
            }
        }

        if (cc != NULL) {
#ifdef _SPEEDLIMIT_
            if (speed_limit
                && ((dcc_header->_type == dcc_req_set_duspeed)
                    || (dcc_header->_type == dcc_req_set_dspeed)
                    || (dcc_header->_type == dcc_req_set_uspeed)))
            {
                if (cc->_type == cc_tcp) {
                    if (dcc_header->_type == dcc_req_set_dspeed) {
                        cc->_set_recv_speed = dcc_header->_arg;
                    } else if (dcc_header->_type == dcc_req_set_uspeed) {
                        cc->_set_send_speed = dcc_header->_arg;
                    } else {
                        cc->_set_send_speed = dcc_header->_arg;
                        cc->_set_recv_speed = dcc_header->_arg;
                    }
                }
                continue;
            }
#endif

            if (cc->_type == cc_tcp
               && ((cc->_ip != dcc_header->_ip)
                   || (cc->_port != dcc_header->_port)))
            {
                /*
                 * 存在风险：
                 * 如果flow到cc的哈希算法有冲突可能导致连接被强制关闭。
                 */
#ifdef _OUTPUT_LOG_
                WRITE_LOG(output_log, "MCP++ DCC: ip or port not match, "
                          "flow number confict. close old cc! ori_flow - %llu, "
                          "ori_ip - %u, ori_port - %hu, flow - %llu, ip - %u, "
                          "port - %hu, cc->_type - %d, header->_type - %hu.\n",
                          flow, dcc_header->_ip, dcc_header->_port, cc->_flow,
                          cc->_ip, cc->_port, cc->_type, dcc_header->_type);
#endif
                write_net_log(tools::LOG_FLOW_CONFLICT, cc, data_len,
                              (2 * mi->_reqmqidx));
                ccs->CloseCC(cc, dcc_rsp_disconnect_error);
                cc = NULL;
            } else {
                if ((dcc_header->_type == dcc_req_send_udp
                    || dcc_header->_type == dcc_req_send_shm_udp
                    || dcc_header->_type == dcc_req_send_udp_bindport)
                    && cc->_type != cc_client_udp )
                {
#ifdef _OUTPUT_LOG_
                    WRITE_LOG(output_log, "MCP++ DCC: connection type confict. "
                              "close old cc! ori_flow - %llu, ori_ip - %u, "
                              "ori_port - %hu, ori_type - %d, flow - %llu, "
                              "ip - %u, port - %hu.\n", cc->_flow, cc->_ip,
                              cc->_port, cc->_type, flow, dcc_header->_ip,
                              dcc_header->_port);
#endif
                    write_net_log(tools::LOG_CONN_TYPE_CONFLICT, cc, data_len,
                                  (2 * mi->_reqmqidx));
                    ccs->CloseCC(cc, dcc_rsp_disconnect_error);
                    cc = NULL;
                } else if ((dcc_header->_type == dcc_req_send
                           || dcc_header->_type == dcc_req_send_shm)
                           && cc->_type != cc_tcp )
                {
#ifdef _OUTPUT_LOG_
                    WRITE_LOG(output_log, "MCP++ DCC: connection type confict. "
                              "close old cc! ori_flow - %llu, ori_ip - %u, "
                              "ori_port - %hu, ori_type - %d, flow - %llu, "
                              "ip - %u, port - %hu.\n", cc->_flow, cc->_ip,
                              cc->_port, cc->_type, flow, dcc_header->_ip,
                              dcc_header->_port);
#endif
                    write_net_log(tools::LOG_CONN_TYPE_CONFLICT, cc, data_len,
                                  (2 * mi->_reqmqidx));
                    ccs->CloseCC(cc, dcc_rsp_disconnect_error);
                    cc = NULL;
                } else {
                    SAY("reuse cc, flow=%llu,ip=%u,port=%d\n",
                        flow, cc->_ip, cc->_port);
                }
            }

        }

        if (dcc_header->_type == dcc_req_disconnect) {
            if (cc) {
                ccs->CloseCC(cc, dcc_rsp_disconnect_local);
            }
        } else if (dcc_header->_type == dcc_req_send
                   || dcc_header->_type == dcc_req_send_shm)
        {
            if (tcp_disable) {
                if (event_notify) {
                    send_notify_2_mcd(dcc_rsp_connect_failed, NULL);
                }
                continue;
            }
            send_mq_request_by_tcp(cc, mi, flow, data_len);
        } else if (dcc_header->_type == dcc_req_send_udp
                   || dcc_header->_type == dcc_req_send_udp_bindport
                   || dcc_header->_type == dcc_req_send_shm_udp )
        {
            struct sockaddr_in addr;
            struct sockaddr *paddr = (struct sockaddr *)(&addr);
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = dcc_header->_ip;
            addr.sin_port = htons(dcc_header->_port);

#ifdef _SHMMEM_ALLOC_
            memhead* head = NULL;
            if (dequeue_with_shm_alloc
                && (dcc_header->_type == dcc_req_send_shm_udp))
            {
                head = (memhead*)(tmp_buffer + DCC_HEADER_LEN);
            }
#endif

            if (cc) {
                ccs->StartStat(cc);
                cc->_w->add_new_msg_time(dcc_header->_timestamp,
                                         dcc_header->_timestamp_msec);
                ccs->_remote_stat_info.UpdateReq(dcc_header->_ip,
                                                 dcc_header->_port, true);
#ifdef _SHMMEM_ALLOC_
                if (head) {
                    ret = ccs->SendUDP(cc, (char*)myalloc_addr(head->mem),
                                       head->len, *paddr,
                                       sizeof(struct sockaddr_in));
                    myalloc_free(head->mem);
                } else {
#endif
                    ret = ccs->SendUDP(cc, tmp_buffer + DCC_HEADER_LEN,
                                       data_len - DCC_HEADER_LEN, *paddr,
                                       sizeof(struct sockaddr_in));
#ifdef _SHMMEM_ALLOC_
                }
#endif

                if (ret == -E_NEED_SEND) {
                    epoll_mod(epfd, cc->_fd, cc, EPOLLIN | EPOLLOUT);
                } else if (ret == -E_SEND_COMP) {
                    epoll_mod(epfd, cc->_fd, cc, EPOLLIN);
                } else if (ret == -E_MEM_ALLOC) {
#ifdef _OUTPUT_LOG_
                    WRITE_LOG(output_log, "MCP++ DCC: Memory overload in "
                              "handle_req_mq UDP Send! flow - %llu, "
                              "ip - %u, port - %hu.\n", cc->_flow,
                              dcc_header->_ip, dcc_header->_port);
#endif
                    if (event_notify) {
                        send_notify_2_mcd(dcc_rsp_overload_mem, NULL);
                    }
                    fastmem_shrink(mem_log_info.old_size, mem_log_info.new_size);
                    write_mem_log(tools::LOG_SEND_OOM, cc);
                    ccs->_remote_stat_info.UpdateReq(dcc_header->_ip,
                                                     dcc_header->_port, false);
                } else if (ret == -E_FORCE_CLOSE) {
                    /* Not run here. */
                    WRITE_LOG(output_log,
                        "MCP++ DCC: Invalid send return code %d. flow - %llu, ip - %u, port - %hu.\n",
                        ret, cc->_flow, dcc_header->_ip, dcc_header->_port);
                    ccs->_remote_stat_info.UpdateReq(dcc_header->_ip, dcc_header->_port, false);
                } else {
                    /* Actually -E_NEED_CLOSE. Close anyway. */
                    WRITE_LOG(output_log, "MCP++ DCC: Send failed in "
                              "handle_req_mq - UDP Send! %m flow - %llu, "
                              "ip - %u, port - %hu.\n", cc->_flow,
                              dcc_header->_ip, dcc_header->_port); 
                    ccs->CloseCC(cc, dcc_rsp_disconnect_peer_or_error);
                    ccs->_remote_stat_info.UpdateReq(dcc_header->_ip,
                                                     dcc_header->_port, false);
                }
            } else {
                if (dcc_header->_type == dcc_req_send_udp_bindport) {
                    unsigned short local_port = dcc_header->_arg;
                    cc = make_new_cc_udp(ccs, flow, ret, local_port);
                } else {
                    cc = make_new_cc_udp(ccs, flow, ret);
                }
                if (!cc) {
#ifdef _OUTPUT_LOG_
                    WRITE_LOG(output_log, "MCP++ DCC: Connect failed in "
                              "handle_req_mq UDP! flow - %llu.\n", flow);
#endif
                    write_net_log(tools::LOG_SOCK_CONN_FAIL, flow, data_len,
                                  (2 * mi->_reqmqidx), -ret);
                    ccs->_remote_stat_info.UpdateReq(dcc_header->_ip,
                                                     dcc_header->_port, false);
                    if (event_notify) {
                        dcc_header->_type = dcc_rsp_connect_failed;
                        CConnSet::GetHeaderTimestamp(dcc_header->_timestamp,
                                                     dcc_header->_timestamp_msec,
                                                     use_monotonic_time_in_header);
                        rsp_mq[mi->_reqmqidx]->enqueue(dcc_header,
                                                       DCC_HEADER_LEN, flow);
                    }

#ifdef _SHMMEM_ALLOC_
                    if (head) {
                        myalloc_free(head->mem);
                    }
#endif
                    continue;
                }

                cc->_reqmqidx = mi->_reqmqidx;
                cc->_connstatus = status_connected;
                write_net_log(tools::LOG_ESTAB_CONN, cc, 0, -1);

                ccs->StartStat(cc);
                cc->_w->add_new_msg_time(dcc_header->_timestamp,
                                         dcc_header->_timestamp_msec);
                ccs->_remote_stat_info.UpdateReq(dcc_header->_ip,
                                                 dcc_header->_port, true);

#ifdef _SHMMEM_ALLOC_
                if (head) {
                    ret = ccs->SendUDP(cc, (char*)myalloc_addr(head->mem),
                                       head->len, *paddr,
                                       sizeof(struct sockaddr_in));
                    myalloc_free(head->mem);
                } else {
#endif
                    ret = ccs->SendUDP(cc, tmp_buffer + DCC_HEADER_LEN,
                                       data_len - DCC_HEADER_LEN, *paddr,
                                       sizeof(struct sockaddr_in));
#ifdef _SHMMEM_ALLOC_
                }
#endif

                if (ret == -E_NEED_SEND) {
                    epoll_add(epfd, cc->_fd, cc, EPOLLIN | EPOLLOUT);
                } else if (ret == -E_SEND_COMP) {
                    epoll_add(epfd, cc->_fd, cc, EPOLLIN);
                } else if (ret == -E_MEM_ALLOC) {
#ifdef _OUTPUT_LOG_
                    WRITE_LOG(output_log, "MCP++ DCC: Memory overload in "
                              "handle_req_mq UDP Send 1! flow - %llu, "
                              "ip - %u, port - %hu.\n", cc->_flow,
                              dcc_header->_ip, dcc_header->_port);
#endif
                    if (event_notify) {
                        send_notify_2_mcd(dcc_rsp_overload_mem, NULL);
                    }

                    fastmem_shrink(mem_log_info.old_size, mem_log_info.new_size);

                    write_mem_log(tools::LOG_SEND_OOM, cc);
                    ccs->_remote_stat_info.UpdateReq(dcc_header->_ip,
                                                     dcc_header->_port, false);
                } else if (ret == -E_FORCE_CLOSE) {
                    /* Not run here. */
                    WRITE_LOG(output_log, "MCP++ DCC: Invalid send return "
                              "code %d. flow - %llu, ip - %u, port - %hu.\n",
                              ret, cc->_flow, dcc_header->_ip,
                              dcc_header->_port);

                    ccs->_remote_stat_info.UpdateReq(dcc_header->_ip,
                                                     dcc_header->_port, false);
                } else {
                    /* Actually -E_NEED_CLOSE. Close anyway. */
                    WRITE_LOG(output_log, "MCP++ DCC: Send failed in "
                              "handle_req_mq - UDP Send! %m flow - %llu, "
                              "ip - %u, port - %hu.\n", cc->_flow,
                              dcc_header->_ip, dcc_header->_port);

                    ccs->CloseCC(cc, dcc_rsp_disconnect_peer_or_error);
                    ccs->_remote_stat_info.UpdateReq(dcc_header->_ip,
                                                     dcc_header->_port, false);
                }
            }
        }
    }/* mq loop */
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
            handle_socket_send(cc);
            ++proc_cnt;
        }
    }

    if (proc_cnt) {
        /* update now time */
        nowtime = CConnSet::GetNowTick();
    }

    list_for_each_entry_safe_l(cc, tmp, &ccs->_pending_recv, _pending_recv_next)
    {
        if (ccs->get_recv_speed(cc) == 0
            || ccs->get_recv_speed(cc) > cc->_recv_mon.touch(nowtime))
        {
            list_del_init(&cc->_pending_recv_next);
            if (handle_socket_recv(cc) == 0) {
                handle_socket_message(cc);
            }
        }
    }
}
#endif

void init_global_cfg(CFileConfig& page)
{
    try {
        use_monotonic_time_in_header =
            from_str<bool>(page["root\\use_monotonic_time_in_header"]);
    } catch(...) {
        use_monotonic_time_in_header = false;
    }

    try {
        enqueue_fail_close = from_str<bool>(page["root\\enqueue_fail_close"]);
    } catch(...) {
        enqueue_fail_close = false;
    }
    if (enqueue_fail_close) {
        fprintf(stderr, "DCC enqueue_fail_close enabled!\n");
    } else {
        fprintf(stderr, "DCC enqueue_fail_close not enabled!\n");
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
        output_log = from_str<unsigned>(page["root\\output_log"]);
    } catch (...) {
        output_log = 0;
    }
    if (output_log) {
        fprintf(stderr, "DCC log open!\n");
    } else {
        fprintf(stderr, "DCC log close!\n");
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
        fprintf(stderr, "DCC send buffer max check enabled, buffer max "
                "length - %u.\n", send_buff_max);
    } else {
        fprintf(stderr, "DCC send buffer max is 0\n");
    }

    try {
        recv_buff_max = from_str<unsigned>(page["root\\recv_buff_max"]);
    } catch (...) {
        recv_buff_max = 0;
    }
    if ( recv_buff_max ) {
        fprintf(stderr, "DCC recive buffer max check enabled, buffer max "
                "length - %u.\n", recv_buff_max);
    } else {
        fprintf(stderr, "DCC recive buffer max is 0\n");
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
        dcc_request_msg_timeout_thresh =
            from_str<unsigned>(page["root\\dcc_request_msg_timeout_thresh"]);
        fprintf(stderr, "dcc_request_msg_timeout_thresh:%d\n",
                dcc_request_msg_timeout_thresh);
        if (dcc_request_msg_timeout_thresh > CTimeRawCache::MAX_MSG_TIMEOUT) {
            fprintf(stderr, "Max Dcc request msg timeout is:%d\n",
                    CTimeRawCache::MAX_MSG_TIMEOUT);
            assert(false);
        }
    } catch (...) {
        /* nothing */
    }

    try {
        dcc_request_msg_mq_timeout_thresh =
            from_str<unsigned>(page["root\\dcc_request_msg_mq_timeout_thresh"]);
    } catch(...) {
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
        } catch(...) {
            break;
        }

        fprintf(stderr, "dcc get mq from path:%s\n", mq_path.c_str());
        try {
            mq = GetMQ(mq_path);
        } catch(...) {
            fprintf(stderr, "get mq fail, %s, %m\n", mq_path.c_str());
            err_exit();
        }

        if (is_req) {
            req_mq[req_mq_num++] = mq;
            /* req mq需要登记映射表和加入epoll监控 */
            fd = mq->fd();
            if (fd < FD_MQ_MAP_SIZE) {
                mq_mapping[fd]._mq = mq;
                mq_mapping[fd]._active = false;
                mq_mapping[fd]._reqmqidx = req_mq_num - 1;
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
        } else {
            rsp_mq[rsp_mq_num++] = mq;
        }
    }
}


int init_wdc(CFileConfig& page, CSOFile& so_file)
{
    /* watchdog client */

    try {
        string wdc_conf = page["root\\watchdog_conf_file"];
        try {
            wdc = new CWatchdogClient;
        } catch (...) {
            fprintf(stderr, "Out of memory for watchdog client!\n");
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

        if (wdc->Init(wdc_conf.c_str(), PROC_TYPE_DCC, frame_version,
                      plugin_version, NULL, add_0, add_1))
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

app::ClientNetHandler* init_dcc_net_handler(CFileConfig& page,
                                            CFileConfig& service_config_page,
                                            const std::string& complete_so_file,
                                            CSOFile& so_file,
                                            bool so_config,
                                            bool start_as_new_interface)
{
    std::map<std::string, std::string> service_config_map;
    app::ClientNetHandler* dcc_net_handler = NULL;

    service_config_page.GetConfigMap(&service_config_map);
    if (so_config) {
        /* so config, has tcp */
        if (so_file.open(complete_so_file)) {
            fprintf(stderr, "so_file open fail, %s, %m\n",
                    complete_so_file.c_str());
            err_exit();
        }

        if (start_as_new_interface == true) {
            /* The method for tae initialize */
            typedef app::ClientNetHandler* (* create_net_handler)();
            std::string name;

            try {
                name =
                    service_config_page["root\\create_client_net_handler_func"];
            } catch (...) {
                name = "create_client_net_handler";
            }

            create_net_handler constructor =
                (create_net_handler)so_file.get_func(name.c_str());
            if (constructor == NULL) {
                fprintf(stderr, "cannot find create_client_net_handler_func in "
                        "so file, dcc init failed\n");
                err_exit();
            }

            dcc_net_handler = constructor();
            if (dcc_net_handler->init(service_config_map) != 0) {
                fprintf(stderr, "client_net_handler init failed\n");
                err_exit();
            }
        } else {
            /*
             * The method to compatible with old mcp++ initialize
             * If user do not define check_complete, dcc MUST exist
             */
            app::CompatibleDCCNetHandler* handler =
                new app::CompatibleDCCNetHandler();
            if (handler->compatible_init(page, so_file) != 0) {
                fprintf(stderr, "compatible client_net_handler init failed\n");
                err_exit();
            }
            dcc_net_handler = handler;
        }
    } else {
        /*
         * no so config, it is only for udp,
         * just create a dcc_net_handler obj, in fact it is not used in udp
         */
        dcc_net_handler = new app::ClientNetHandler();
        dcc_net_handler->init(service_config_map);
    }
    return dcc_net_handler;
}

int main(int argc, char* argv[])
{
    if (argc < 2) {
        printf("%s conf_file [non-daemon]\n", argv[0]);
        err_exit();
    }

    if (!strncasecmp(argv[1], "-v", 2)) {
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

    fprintf(stderr, "dcc max_open_file_num:%d\n", max_open_file_num);
    int ret = 0;

    if (argc == 2) {
        ret = mydaemon(argv[0], max_open_file_num);
    } else {
        ret = initenv(argv[0], max_open_file_num);
    }
    if (ret != 0) {
        fprintf(stderr, "dcc initenv failed\n");
    }

    CFileConfig service_config_page;
    bool start_as_new_interface = false;
    try {
        std::string service_config_file = page["root\\service_config"];
        service_config_page.Init(service_config_file);
        start_as_new_interface = true;
    } catch (...) {
        fprintf(stderr, "no service_config find, use dcc.conf to init\n");
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

    /* epoll */
    epfd = epoll_create(max_conn);
    if (epfd < 0) {
        fprintf(stderr, "create epoll fail, %u,%m\n", max_conn);
        err_exit();
    }
    epev = new struct epoll_event[max_conn];

    /* open mq */
    memset(mq_mapping, 0x0, sizeof(MQINFO) * FD_MQ_MAP_SIZE);
    init_mq_conf(page, true);
    init_mq_conf(page, false);
    if (req_mq_num < 1 || rsp_mq_num < 1 || req_mq_num != rsp_mq_num) {
        fprintf(stderr, "no req mq or rsp mq, %u,%u\n", req_mq_num, rsp_mq_num);
        err_exit();
    }

    /* load check_complete */
    bool so_config = false;
    string complete_so_file;
    try {
        if (start_as_new_interface == false) {
            complete_so_file = page["root\\complete_so_file"];
        } else {
            complete_so_file =
                service_config_page["root\\client_net_handler_so_file"];
        }
        so_config = true;
    } catch (...) {
        fprintf(stderr, "no complete_so_file or net_handler_so_file specific "
                "in config\n");
        so_config = false;
    }

    CSOFile so_file;
    app::ClientNetHandler* dcc_net_handler =
        init_dcc_net_handler(page, service_config_page, complete_so_file,
                             so_file, so_config, start_as_new_interface);

    if (so_config) {
        tcp_disable = false;
    } else {
        tcp_disable = true;
        fprintf(stderr, "TCP disabled!\n");
    }

    /* conn set */
    unsigned rbsize = 1<<16; /* 64kb */
    unsigned wbsize = 1<<14; /* 16kb */
    try {
        rbsize = from_str<unsigned>(page["root\\recv_buff_size"]);
        wbsize = from_str<unsigned>(page["root\\send_buff_size"]);
    } catch(...) {
        /* nothing */
    }
    if (event_notify) {
        if (conn_notify_details) {
            ccs = new CConnSet(NULL, dcc_net_handler, max_conn, rbsize,
                               wbsize, handle_cc_close, send_notify_2_mcd);
        } else {
            ccs = new CConnSet(NULL, dcc_net_handler, max_conn, rbsize,
                               wbsize, handle_cc_close, NULL);
        }
    } else {
        ccs = new CConnSet(NULL, dcc_net_handler, max_conn, rbsize, wbsize,
                           NULL, NULL);
    }

    if (init_wdc(page, so_file)) {
        err_exit();
    }

#ifdef _SPEEDLIMIT_
    try {
        speed_limit = from_str<bool>(page["root\\speed_limit"]);
    } catch (...) {
        /* nothing */
    }

    if (speed_limit) {
        if ( sync_enabled ) {
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
        fprintf(stderr, "speed limit:%d, download:%d, upload:%d, low_buff:%d\n",
                speed_limit, download_speed, upload_speed, low_buff_size);
    }
#endif

#ifdef _SHMMEM_ALLOC_
    /* share memory allocator */
    try {
        enqueue_with_shm_alloc =
            from_str<bool>(page["root\\shmalloc\\enqueue_enable"]);
    } catch(...) {
        /* nothing */
    }
    try {
        dequeue_with_shm_alloc = from_str<bool>(page["root\\shmalloc\\dequeue_enable"]);
    } catch(...) {
        /* nothing */
    }
    if (enqueue_with_shm_alloc || dequeue_with_shm_alloc) {
        try {
            if (OpenShmAlloc(page["root\\shmalloc\\shmalloc_conf_file"])) {
                fprintf(stderr, "shmalloc init fail, %m\n");
                err_exit();
            }
        } catch (...) {
            fprintf(stderr, "shmalloc config error\n");
            err_exit();
        }
    }
    fprintf(stderr, "dcc shmalloc enqueue=%d, dequeue=%d\n",
            (int)enqueue_with_shm_alloc, (int)dequeue_with_shm_alloc);
#endif

    memset(&net_log_info, 0, sizeof(net_log_info));
    memset(&mem_log_info, 0, sizeof(mem_log_info));
    memset(&net_stat_info, 0, sizeof(net_stat_info));
    log_client = new (nothrow) tools::CLogClient();
    if (NULL == log_client) {
        fprintf(stderr, "alloc log_client api failed\n");
        err_exit();
    }
    if (log_client->init(argv[1]) < 0) {
        fprintf(stderr, "dcc init log_client failed\n");
        err_exit();
    }
    log_client->SetIpWithInnerIp();
    log_client->SetMcpppVersion(version_string, strlen(version_string));
    log_client->SetMcpppCompilingDate(compiling_date, strlen(compiling_date));

    // fastmem
    unsigned long mem_thresh_size = 1<<30;
    try {
        mem_thresh_size = from_str<unsigned long>(page["root\\mem_thresh_size"]);
        if ((mem_thresh_size > ((unsigned long)2 << 30))
            || (mem_thresh_size < ((unsigned long)1 << 27)))
        {
            mem_thresh_size = 1<<30;
        }
    } catch(...) {
        /* nothing */
    }

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
        fprintf(stderr, "DCC exp_max_pkt_size(%lu) too large, or larger than "
                "recv_buff_max/send_buff_max.\n", exp_max_pkt_size);
        err_exit();
    }

    if (exp_max_pkt_size) {
        fprintf(stderr, "DCC exp_max_pkt_size enabled, expect max "
                "size - %lu.\n", exp_max_pkt_size);
    }

    fastmem_init(mem_thresh_size, exp_max_pkt_size);

    /* main loop */
    int i, eventnum;
    void *ev_data = NULL;
    fprintf(stderr, "dcc event_notify=%d,timeout=%u,stat_time=%u,max_conn=%u,"
            "rbsize=%u,wbsize=%u\n", event_notify, time_out, stat_time,
            max_conn, rbsize, wbsize);
    fprintf(stderr, "dcc started\n");

    ConnCache *ev_cc = NULL;

    while (!stop) {
        /* 处理网络超时与profile统计信息输出 */
        ccs->Watch(time_out, stat_time);

        /* 处理网络事件与管道事件 */
        eventnum = epoll_wait(epfd, epev, max_conn, 1);
        for (i = 0; i < eventnum; ++i) {
            ev_data = epev[i].data.ptr;

            if (ev_data >= ccs->BeginAddr() && ev_data <= ccs->EndAddr()) {
                /* Socket fd event. */
                ev_cc = (struct ConnCache *)ev_data;
                if ( ev_cc->_type == cc_tcp ) {
                    handle_socket(ev_cc, &epev[i]);
                } else if ( ev_cc->_type == cc_client_udp ) {
                    handle_socket_udp(ev_cc, &epev[i]);
                } else {
                    /* Never here. */
                    WRITE_LOG(output_log, "MCP++ DCC: Invalid connection "
                              "type %d!\n", ev_cc->_type);
                }
            } else {
                /* Mq event. */
                handle_req_mq((CFifoSyncMQ *)ev_data);
            }
        }

        /*
         * 这里要检查是否有mq没有被epoll激活，没有的话要再扫描一次，
         * 避免mq通知机制的不足
         */
        for (i = mq_mapping_min; i <= mq_mapping_max; ++i) {
            if (mq_mapping[i]._mq != NULL) {
                if (mq_mapping[i]._active) {
                    mq_mapping[i]._active = false;
                } else {
                    handle_req_mq(mq_mapping[i]._mq, false);
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
    fprintf(stderr, "dcc stopped\n");
    syslog(LOG_USER | LOG_CRIT | LOG_PID, "%s dcc stopped\n", argv[0]);
    exit(0);
}
