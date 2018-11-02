#ifndef SATA_H
#define SATA_H

#include <limits.h>
#include <stdint.h>
#include <string>
#include <map>

#ifndef MAX_REMOTE_INFO_SIZE
#define MAX_REMOTE_INFO_SIZE 100
#endif
namespace tools {

#pragma pack(1)
class RemoteInfo {
public:
    RemoteInfo();
    static uint32_t ByteSize();
    bool ToString(char *buf, uint32_t buf_len, uint32_t *data_len);
    bool ParserFromString(const char *buf, uint32_t data_len);
    void DebugString();

public:
    uint64_t key;           // Remote IP:port
    uint32_t min_delay;     // 最小延时
    uint32_t max_delay;     // 最大延时
    uint32_t avg_delay;     // 平均延时
    uint32_t req_count;     // 请求数
    uint32_t fail_req_count; // 超时数
    uint32_t consecutive_fail_req_count; // 连续超时失败数目
    uint64_t send_size;     // 流量 send
    uint64_t recv_size;     // 流量 recv
    uint32_t sample_gap;    // sample gap
};
#pragma pack()

class RemoteStatInfo {
public:
    RemoteStatInfo() {
        start_stat_timestamp = time(NULL);
    }
    ~RemoteStatInfo() {}
    void UpdateDelay(uint32_t ip, uint32_t port, uint32_t delay);
    void UpdateReq(uint32_t ip, uint32_t port, bool success=true);
    void UpdateSize(uint32_t ip, uint32_t port, uint64_t send, uint64_t recv);
    bool ToString(char *buf, uint32_t buf_len, uint32_t *data_len, uint32_t *item_num);
    uint32_t ByteSize() { return remote_info.size() * RemoteInfo::ByteSize(); }
private:
    inline uint64_t IpPort2Key(uint32_t ip, uint32_t port) {
        uint64_t key = ip;
        key <<= 32;
        key |= port;
        return key;
    }
private:
    struct Info {
        RemoteInfo info;
        uint64_t total_delay;
        uint32_t temp_consecutive_fail_req_count;
        Info() : total_delay(0), temp_consecutive_fail_req_count(0) {}
    };
    std::map<uint64_t, Info> remote_info; // remote ip+port info, only for dcc
    time_t start_stat_timestamp;
};

#pragma pack(1)
/* network statistics info structure */
class CCDStatInfo {
public:
    CCDStatInfo();
    void GetByteSize(uint32_t *data_len);
    bool ToString(char *buf, uint32_t buf_len, uint32_t *data_len);
    bool ParserFromString(const char *buf, uint32_t data_len);
    static void printTitle();
    void printItem();
    void DebugString();

public:
    // 消息延时
    uint32_t mq_wait_avg;       // average mq wait time
    uint32_t mq_wait_min;       // minimum mq wait time, from ccsstat
    uint32_t mq_wait_max;       // maximum mq wait time, from ccsstat
    uint32_t mq_msg_timeout_count;   // msg timeout while in mq count.
    uint32_t mq_msg_count;      // 取管道消息次数
    uint64_t mq_wait_total;     // 消息在管道中总时间
    // Mq消息延时、管道统计
    uint32_t max_time;          // 处理消息最大时间
    uint32_t min_time;          // 处理消息最小时间
    uint32_t avg_time;          // 处理消息平均时间
    uint64_t total_time;        // 处理消息总时间
    // 连接统计
    uint32_t conn_num;          // connection number, from ccsstat
    uint32_t msg_count;         // 消息数目
    uint32_t load;              // load
    uint32_t complete_err_count; // complete function check error count.
    uint32_t cc_timeout_close_count; // cc timeout close count
    uint64_t total_recv_size;   // 接收字节数
    uint64_t total_send_size;   // 发送字节数
    // 其它
    uint32_t is_ccd;
    uint32_t sample_gap;        // above data is sampling during gap seconds.
};

/* mcd statistics info structure */
class MCDStatInfo {
public:
    void GetByteSize(uint32_t *data_len);
    bool ToString(char *buf, uint32_t buf_len, uint32_t *data_len);
    bool ParserFromString(const char *buf, uint32_t data_len);
    static void printTitle();
    void printItem();

public:
    unsigned load;        // load
    unsigned mq_wait_avg; // average mq wait time
    unsigned mq_wait_min; // minimum mq wait time
    unsigned mq_wait_max; // maximum mq wait time
    unsigned mq_ccd_discard_package_count; // discard timeout pakcage count in ccd mq
    unsigned mq_dcc_discard_package_count; // discard timeout pakcage count in dcc mq
    unsigned mq_ccd_package_count;         // pakcage count in ccd mq
    unsigned mq_dcc_package_count;         // pakcage count in dcc mq
    uint64_t mq_ccd_total_recv_size;
    uint64_t mq_ccd_total_send_size;
    uint64_t mq_dcc_total_recv_size;
    uint64_t mq_dcc_total_send_size;
    uint32_t sample_gap; // above data is sampling during gap seconds.
};
#pragma pack()

} // namespace tools

#endif  // SATA_H
