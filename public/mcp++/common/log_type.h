#ifndef __LOG_TYPE_H__
#define __LOG_TYPE_H__

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <vector>
#include <string>
#include <stdio.h>

using namespace std;

namespace tools{

#define EVT_SFT     16  // 0x00FF0000
#define TRG_SFT     8   // 0x0000FF00

#define LEVEL_TRACE     0x00000000   // level 0, trace
#define LEVEL_ERROR     0x10000000   // level 1, error
#define LEVEL_FATAL     0x20000000   // level 2, fatal

/* basic event type */
enum tagBasicType
{
    LOG_NET_ESTAB = 1,      // establish connection
    LOG_NET_CLOSE,          // close connection
    LOG_NET_RECV,           // recv call
    LOG_NET_SEND,           // send call
    LOG_NET_DROP,           // drop packet/data
    LOG_NET_OVERLOAD,       // overload (load grid full)
    LOG_MEM_GROW,           // memory grow
    LOG_MEM_SHRINK,         // memory shrink
    LOG_MQ_ENQ,             // enqueue
    LOG_MQ_DEQ,             // dequeue
    LOG_NS_EVT,             // tns event
    LOG_EVT_MAX,
};

enum tagEventType
{
    LOG_EVT_ESTAB     = (LOG_NET_ESTAB  << EVT_SFT) + LEVEL_TRACE,
    LOG_EVT_CLOSE     = (LOG_NET_CLOSE  << EVT_SFT) + LEVEL_ERROR,
    LOG_EVT_RECV      = (LOG_NET_RECV   << EVT_SFT) + LEVEL_TRACE,
    LOG_EVT_DROP      = (LOG_NET_DROP   << EVT_SFT) + LEVEL_ERROR,
    LOG_EVT_SEND      = (LOG_NET_SEND   << EVT_SFT) + LEVEL_TRACE,
    LOG_EVT_ENQ       = (LOG_MQ_ENQ     << EVT_SFT) + LEVEL_TRACE,
    LOG_EVT_DEQ       = (LOG_MQ_DEQ     << EVT_SFT) + LEVEL_TRACE,
    LOG_EVT_MGROW     = (LOG_MEM_GROW   << EVT_SFT) + LEVEL_FATAL,
    LOG_EVT_MSHRNK    = (LOG_MEM_SHRINK << EVT_SFT) + LEVEL_FATAL,
    LOG_EVT_NS        = (LOG_NS_EVT     << EVT_SFT) + LEVEL_TRACE,
};

/* log type for ccd/dcc */
enum tagNetLog
{
    LOG4NET = 0x01000000,
    LOG_ESTAB_CONN           = LOG_EVT_ESTAB + 1 + LOG4NET,  // MCP++ CCD/DCC: Establish connection
    LOG_NO_FREE_FLOW         = LOG_EVT_CLOSE + (LOG_NET_ESTAB << TRG_SFT) + 1 + LOG4NET,  // MCP++ CCD: Find free flow fail
    LOG_NO_FREE_CC           = LOG_EVT_CLOSE + (LOG_NET_ESTAB << TRG_SFT) + 2 + LOG4NET,  // MCP++ CCD/DCC: No free conncache
    LOG_SOCK_CONN_FAIL       = LOG_EVT_DROP  + (LOG_NET_ESTAB << TRG_SFT) + 3 + LOG4NET,  // MCP++ DCC: connect() system call fail

    LOG_NO_COMP_FUNC         = LOG_EVT_CLOSE + 1 + LOG4NET,  // MCP++ DCC: No complete func for tcp
    LOG_SYNC_REQ_FAIL        = LOG_EVT_CLOSE + 2 + LOG4NET,  // MCP++ CCD: Sync request fail
    LOG_SYNC_RSP_FAIL        = LOG_EVT_CLOSE + 3 + LOG4NET,  // MCP++ CCD: Sync response fail
    LOG_DATA_TOO_LARGE       = LOG_EVT_CLOSE + 4 + LOG4NET,  // MCP++ CCD/DCC: Datalen > buffer_size after packet complete check
    LOG_COMP_CHK_FAIL        = LOG_EVT_CLOSE + 5 + LOG4NET,  // MCP++ CCD/DCC: Packet complete check fail
    LOG_EPOLL_ERR            = LOG_EVT_CLOSE + 6 + LOG4NET,  // MCP++ CCD/DCC: Events error when handle socket
    LOG_FLOW_CONFLICT        = LOG_EVT_CLOSE + 7 + LOG4NET,  // MCP++ DCC: flow conflict (ip/port not match)
    LOG_CONN_TYPE_CONFLICT   = LOG_EVT_CLOSE + 8 + LOG4NET,  // MCP++ DCC: connection type conflict

    LOG_SEND_DATA            = LOG_EVT_SEND  + 1 + LOG4NET,  // MCP++ CCD/DCC: Send data
    LOG_SEND_ERR_RETRY       = LOG_EVT_SEND  + 2 + LOG4NET,  // MCP++ DCC: Send error, retry
    LOG_SEND_FORCE_ERR       = LOG_EVT_CLOSE + (LOG_NET_SEND << TRG_SFT) + 1 + LOG4NET,   // MCP++ CCD/DCC: SendForce fail
    LOG_SEND_ERR_CLOSE       = LOG_EVT_CLOSE + (LOG_NET_SEND << TRG_SFT) + 2 + LOG4NET,   // MCP++ CCD/DCC: Send error, close connection
    LOG_SEND_BUF_FULL_CLOSE  = LOG_EVT_CLOSE + (LOG_NET_SEND << TRG_SFT) + 3 + LOG4NET,   // MCP++ CCD/DCC: Send buffer max reach, close connection
    LOG_SEND_ERR_DROP        = LOG_EVT_DROP  + (LOG_NET_SEND << TRG_SFT) + 1 + LOG4NET,    // MCP++ CCD/DCC: UDP send error
    LOG_SEND_BUF_FULL_DROP   = LOG_EVT_DROP  + (LOG_NET_SEND << TRG_SFT) + 2 + LOG4NET,    // MCP++ CCD/DCC: Send buffer max reach

    LOG_RECV_DATA            = LOG_EVT_RECV  + 1 + LOG4NET,   // MCP++ CCD/DCC: Recv data
    LOG_RECV_ERR_CLOSE       = LOG_EVT_CLOSE + (LOG_NET_RECV << TRG_SFT) + 1 + LOG4NET,   // MCP++ CCD/DCC: TCP recv error, close connection
    LOG_RECV_BUF_FULL_CLOSE  = LOG_EVT_CLOSE + (LOG_NET_RECV << TRG_SFT) + 2 + LOG4NET,   // MCP++ CCD/DCC: recv buffer max reach, close connection
    LOG_RECV_ERR_DROP        = LOG_EVT_DROP  + (LOG_NET_RECV << TRG_SFT) + 1 + LOG4NET,    // MCP++ CCD/DCC: UDP recv error

    LOG_FLOW2CC_NOT_MATCH    = LOG_EVT_DROP  + 1 + LOG4NET,      // MCP++ CCD: No cc found
    LOG_INVALID_CONN_TYPE    = LOG_EVT_DROP  + 2 + LOG4NET,      // MCP++ CCD: Invalid connection type

    LOG_ENQ_DATA             = LOG_EVT_ENQ   + 1 + LOG4NET,        // MCP++ CCD/DCC: Enqueue data
    LOG_ENQ_FAIL_DROP        = LOG_EVT_DROP  + (LOG_MQ_ENQ << TRG_SFT) + 1 + LOG4NET,        // MCP++ CCD/DCC: Enqueue fail
    LOG_ENQ_FAIL_CLOSE       = LOG_EVT_CLOSE + (LOG_MQ_ENQ << TRG_SFT) + 1 + LOG4NET,       // MCP++ CCD/DCC: Enqueue fail, close connection

    LOG_DEQ_DATA             = LOG_EVT_DEQ   + 1 + LOG4NET,        // MCP++ CCD/DCC: Dequeue data
    LOG_DATA_EXPIRE          = LOG_EVT_DROP  + (LOG_MQ_DEQ << TRG_SFT) + 1 + LOG4NET,        // MCP++ CCD/DCC: Data expire in queue.

    LOG_OVERLOAD_RJT_CONN    = LOG_EVT_CLOSE + (LOG_NET_OVERLOAD << TRG_SFT) + 1 + LOG4NET, // MCP++ CCD: Loadgrid full, reject connection
    LOG_OVERLOAD_CLOSE       = LOG_EVT_CLOSE + (LOG_NET_OVERLOAD << TRG_SFT) + 2 + LOG4NET, // MCP++ CCD: Loadgrid full! Close connection. (ip,port,port,conn_num)
    LOG_OVERLOAD_DROP        = LOG_EVT_DROP  + (LOG_NET_OVERLOAD << TRG_SFT) + 1 + LOG4NET, // MCP++ CCD: Loadgrid full! Drop packet. (udp_port)
};

enum tagMemLog
{
    LOG4MEM = 0x02000000,
    LOG_RECV_OOM             = LOG_EVT_MSHRNK + (LOG_NET_RECV << TRG_SFT) + 1 + LOG4MEM,    // MCP++ CCD/DCC: Memory overload in recv
    LOG_SEND_OOM             = LOG_EVT_MSHRNK + (LOG_NET_SEND << TRG_SFT) + 1 + LOG4MEM,    // MCP++ CCD/DCC: Memory overload in send
    LOG_RECV_GROW            = LOG_EVT_MGROW  + (LOG_NET_RECV << TRG_SFT) + 1 + LOG4MEM,    // MCP++ CCD/DCC: Recv buffer larger than mempool chunk size
    LOG_SEND_GROW            = LOG_EVT_MGROW  + (LOG_NET_SEND << TRG_SFT) + 1 + LOG4MEM,    // MCP++ CCD/DCC: Send buffer larger than mempool chunk size
};

/* log type for mcd */
enum tagMCDLog
{
    LOG4MCD = 0x03000000,
    LOG_MCD_ENQ              = LOG_EVT_ENQ  + 1 + LOG4MCD,                            // MCP++ MCD: Enqueue data
    LOG_MCD_DEQ              = LOG_EVT_DEQ  + 1 + LOG4MCD,                            // MCP++ MCD: Dequeue data

    LOG_MCD_ENQ_FAIL         = LOG_EVT_DROP + (LOG_MQ_ENQ << TRG_SFT) + 1 + LOG4MCD,  // MCP++ MCD: Enqueue fail
    LOG_MCD_DEQ_DATA_EXPIRE  = LOG_EVT_DROP + (LOG_MQ_DEQ << TRG_SFT) + 1 + LOG4MCD,  // MCP++ MCD: Dequeue data expire
    LOG_MCD_FIND_NO_TIMER    = LOG_EVT_DROP + (LOG_MQ_DEQ << TRG_SFT) + 2 + LOG4MCD,  // MCP++ MCD: Find timer obj fail
};

enum tagNSLog
{
    LOG4NS  = 0x04000000,
    LOG_MCD_NS_GET          = LOG_EVT_NS +  1 + LOG4NS,  // MCP++ MCD: TNS get event
    LOG_MCD_NS_REG_DATA_CHG = LOG_EVT_NS +  2 + LOG4NS,  // MCP++ MCD: TNS register data change event
    LOG_MCD_NS_DATA_CHG     = LOG_EVT_NS +  3 + LOG4NS,  // MCP++ MCD: TNS data change event
    LOG_MCD_NS_GET_FAIL     = LOG_EVT_NS +  4 + LOG4NS,  // MCP++ MCD: TNS get failed
    LOG_MCD_NS_DEC_FAIL     = LOG_EVT_NS +  5 + LOG4NS,  // MCP++ MCD: decode hashmap failed
    LOG_MCD_NS_CHK_FAIL     = LOG_EVT_NS +  6 + LOG4NS,  // MCP++ MCD: Check hashmap header failed
    LOG_MCD_NS_FIND_NO_SVR  = LOG_EVT_NS +  7 + LOG4NS,  // MCP++ MCD: Refresh hashmap, but cannot find server
    LOG_MCD_NS_FIND_NO_PRX  = LOG_EVT_NS +  8 + LOG4NS,  // MCP++ MCD: Search proxy failed
    LOG_MCD_NS_RETRY_REG    = LOG_EVT_NS +  9 + LOG4NS,  // MCP++ MCD: Retry register failed
    LOG_MCD_NS_NO_PRX_ID    = LOG_EVT_NS + 10 + LOG4NS,  // MCP++ MCD: Alloc proxy id failed
};

enum tagStatType
{
    STAT_NET = 1,
    STAT_MCD,
    STAT_UNKOWN,
};

///////////////////////////////////////////////////

/* event macros */
#define EVENT_MATCH(event_type,event_bit)      ((((event_type) >> EVT_SFT) & 0xFF) == (event_bit))
#define TRIGGER_MATCH(event_type,trigger_bit)  ((((event_type) >> TRG_SFT) & 0xFF) == (trigger_bit))
#define GET_LOGTYPE(event_type)                ((event_type) & 0x0F000000)
#define GET_LOGLEVEL(event_type)               (((event_type) & 0xF0000000) >> 28)

///////////////////////////////////////////////////
/* net log info structure */
#pragma pack(1)
class NetLogInfo {
public:
    unsigned long long flow;        // connection flow number
    unsigned remote_ip;
    unsigned local_ip;
    unsigned short remote_port;
    unsigned short local_port;
    unsigned data_len;              // buffer length or recv/send data length, it should be related to log type
    short mq_index;
    unsigned short err;             // errno or retcode. when it is less than 256, it is errno, otherwise it is retcode
    unsigned wait_time;             // minisecond of data stuck in mq

public:
    static void printTitle() {
        printf("%17s%22s%22s%10s%16s%6s%10s", "flow", "remoteHost", "localHost", "dataLen", "mq", "errno", "waitTime");
    }

    bool filter(unsigned remote_ip_, unsigned remote_port_, unsigned local_ip_, unsigned local_port_, short mq_index_) {
        if(remote_ip_!=0 && remote_ip_!=remote_ip)
            return false;
        else if(remote_port_!=0 && remote_port_!=remote_port)
            return false;
        else if(local_ip_!= 0 && local_ip_!=local_ip)
            return false;
        else if(local_port_!=0 && local_port_!= local_port)
            return false;
        else if(mq_index_!=-1 && mq_index_!=mq_index)
            return false;

        return true;
    }

    void printItem(vector<string>& mq_names) {
        struct in_addr addr;
        addr.s_addr = remote_ip; string remote_ip = inet_ntoa(addr);
        addr.s_addr = local_ip;  string local_ip = inet_ntoa(addr);
        string mq="NA";
        if(mq_names.size() !=0 && (short)mq_names.size() > mq_index && mq_index>=0) mq = mq_names[mq_index];
        printf("%17llX%16s:%5u%16s:%5u%10u%16s%6u%10u", flow, remote_ip.c_str(), remote_port, local_ip.c_str(), local_port,
            data_len, mq.c_str(), err, wait_time);
    }
};
#pragma pack()

#pragma pack(1)
/* mem log info structure */
class MemLogInfo {
public:
    unsigned long long flow;        // connection flow number
    unsigned remote_ip;
    unsigned local_ip;
    unsigned short remote_port;
    unsigned short local_port;
    unsigned old_size;              // original memory size (in KB)
    unsigned new_size;              // new memory size (in KB)

public:
    static void printTitle() {
        printf("%17s%22s%22s%10s%10s", "flow", "remoteHost", "localHost", "oldSizeK", "newSizeK");
    }
    bool filter(unsigned remote_ip_, unsigned remote_port_, unsigned local_ip_, unsigned local_port_, short mq_index_) {
        if(remote_ip_!=0 && remote_ip_!=remote_ip)
            return false;
        else if(remote_port_!=0 && remote_port_!=remote_port)
            return false;
        else if(local_ip_!= 0 && local_ip_!=local_ip)
            return false;
        else if(local_port_!=0 && local_port_!= local_port)
            return false;

        return true;
    }
    void printItem(vector<string>& mq_names) {
        struct in_addr addr;
        addr.s_addr = remote_ip; string remote_ip = inet_ntoa(addr);
        addr.s_addr = local_ip;  string local_ip = inet_ntoa(addr);
        printf("%17llX%16s:%5u%16s:%5u%10u%10u", flow, remote_ip.c_str(), remote_port, local_ip.c_str(), local_port, old_size, new_size);
    }
};
#pragma pack()

#pragma pack(1)
/* mcd log info structure */
class MCDLogInfo {
public:
    unsigned long long flow;        // connection flow number
    unsigned ip;
    unsigned short port;
    unsigned short local_port;
    unsigned wait_time;             // minisecond of data stuck in mq
    short mq_index;
    unsigned short err;             // errno or retcode. when it is less than 256, it is errno, otherwise it is retcode
    unsigned seq;                   // msg seq

public:
    static void printTitle() {
        printf("%17s%22s%12s%10s%16s%10s", "flow", "remoteHost", "localPort", "waitTime", "mq", "errno");
    }
    bool filter(unsigned remote_ip_, unsigned remote_port_, unsigned local_ip_, unsigned local_port_, short mq_index_) {
        if(remote_ip_!=0 && remote_ip_!=ip)
            return false;
        else if(remote_port_!=0 && remote_port_!=port)
            return false;
        else if(local_port_!=0 && local_port_!= local_port)
            return false;
        else if(mq_index_!=-1 && mq_index_!=mq_index)
            return false;

        return true;
    }
    void printItem(vector<string>& mq_names) {
        struct in_addr addr;
        addr.s_addr = ip; string remote_ip_str = inet_ntoa(addr);
        string mq="NA";
        if(mq_names.size() !=0 && (short)mq_names.size() > mq_index && mq_index>=0) mq = mq_names[mq_index];
        printf("%17llX%16s:%5u%12u%10u%16s%10u", flow, remote_ip_str.c_str(), port, local_port, wait_time, mq.c_str(), err);
    }
};
#pragma pack()

#pragma pack(1)
/* mcd tns log info structure */
class NSLogInfo {
public:
    int retcode;        // return code
    int event_id;       // TNS event id

public:
    static void printTitle() {
        printf("%10s%10s", "retcode", "eventId");
    }
	bool filter(unsigned remote_ip_, unsigned remote_port_, unsigned local_ip_, unsigned local_port_, short mq_index_) {
		return true;
	}
    void printItem(vector<string> mq_names) {
        printf("%10d%10d", retcode, event_id);
    }
};
#pragma pack()

}

#endif //__LOG_TYPE_H__
///:~
