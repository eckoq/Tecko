#include <stdio.h>
#include <limits.h>
#include "stat.h"

namespace tools {

RemoteInfo::RemoteInfo()
    :key(0),
    min_delay(INT_MAX),
    max_delay(0),
    avg_delay(0),
    req_count(0),
    fail_req_count(0),
    consecutive_fail_req_count(0),
    send_size(0),
    recv_size(0) {
}

uint32_t RemoteInfo::ByteSize() {
    return sizeof(RemoteInfo);
}

bool RemoteInfo::ToString(char *buf, uint32_t buf_len, uint32_t *data_len) {
    if (buf_len < ByteSize()) {
        // fprintf(stderr, "buffer is too small to store data.\n");
        return false;
    }
    *data_len = ByteSize();
    *reinterpret_cast<RemoteInfo*>(buf) = *this;
    // memcpy(buf, this, ByteSize());
    return true;
}

bool RemoteInfo::ParserFromString(const char *buf, uint32_t data_len) {
    if (data_len < ByteSize()) {
        // fprintf(stderr, "buffer is too small to parse data.\n");
        return false;
    }
    *this = *reinterpret_cast<const RemoteInfo*>(buf);
    // memcpy(this, buf, ByteSize());
    return true;
}

void RemoteInfo::DebugString() {
    printf("key: %ld\t\tmin_delay: %d\n", key, min_delay);
    printf("max_delay: %d\t\tavg_delay: %d\n", max_delay, avg_delay);
    printf("req_count: %d\t\tfail_req_count: %d\n", req_count, fail_req_count);
    printf("consecutive_fail_req_count: %d\t\tsend_size: %ld\n", consecutive_fail_req_count, send_size);
    printf("recv_size: %ld\n", recv_size);
}

void RemoteStatInfo::UpdateDelay(uint32_t ip, uint32_t port, uint32_t delay) {
    uint64_t key = IpPort2Key(ip, port);
    std::map<uint64_t, Info>::iterator it = remote_info.find(key);
    if (it != remote_info.end()) {
        if (it->second.info.min_delay > delay) {
            it->second.info.min_delay = delay;
        }
        if (it->second.info.max_delay < delay) {
            it->second.info.max_delay = delay;
        }
        it->second.total_delay += delay;
    } else {
        if (remote_info.size() >= MAX_REMOTE_INFO_SIZE) {
            return;
        }
        Info info;
        info.info.key = key;
        info.info.min_delay = delay;
        info.info.max_delay = delay;
        info.total_delay += delay;
        remote_info[key] = info;
    }
}

void RemoteStatInfo::UpdateReq(uint32_t ip, uint32_t port, bool success) {
    uint64_t key = IpPort2Key(ip, port);
    std::map<uint64_t, Info>::iterator it = remote_info.find(key);
    if (it != remote_info.end()) {
        if (!success) {
            it->second.info.fail_req_count++;
            it->second.temp_consecutive_fail_req_count++;
        } else {
            it->second.info.req_count++;
            if (it->second.temp_consecutive_fail_req_count > it->second.info.consecutive_fail_req_count) {
                it->second.info.consecutive_fail_req_count = it->second.temp_consecutive_fail_req_count;
            }
            it->second.temp_consecutive_fail_req_count = 0;
        }
    } else {
        if (remote_info.size() >= MAX_REMOTE_INFO_SIZE) {
            return;
        }
        Info info;
        info.info.key = key;
        if (!success) {
            info.info.fail_req_count++;
            info.temp_consecutive_fail_req_count++;
        } else {
            info.info.req_count++;
        }
        remote_info[key] = info;
    }
}

void RemoteStatInfo::UpdateSize(uint32_t ip, uint32_t port, uint64_t send, uint64_t recv) {
    uint64_t key = IpPort2Key(ip, port);
    std::map<uint64_t, Info>::iterator it = remote_info.find(key);
    if (it != remote_info.end()) {
        it->second.info.send_size += send;
        it->second.info.recv_size += recv;
    } else {
        if (remote_info.size() >= MAX_REMOTE_INFO_SIZE) {
            return;
        }
        Info info;
        info.info.key = key;
        info.info.send_size = send;
        info.info.recv_size = recv;
        remote_info[key] = info;
    }
}

bool RemoteStatInfo::ToString(char *buf, uint32_t buf_len, uint32_t *data_len, uint32_t *item_num) {
    uint32_t sample_gap = time(NULL) - start_stat_timestamp;
    uint32_t total_item_len = remote_info.size() * RemoteInfo::ByteSize();
    if (total_item_len > buf_len) {
        // fprintf(stderr, "buffer is small, need: %d.\n", total_item_len);
        // fprintf(stderr, "remote info size: %d\n", remote_info.size());
        return false;
    }
    uint32_t write_len = 0;
    for (std::map<uint64_t, Info>::iterator it = remote_info.begin(); it != remote_info.end(); it++) {
        uint32_t one_item_len = 0;
        it->second.info.sample_gap = sample_gap;
        if (it->second.info.req_count > 0) {
            it->second.info.avg_delay = it->second.total_delay / it->second.info.req_count;
        } else {
            it->second.info.avg_delay = 0;
        }
        if (it->second.temp_consecutive_fail_req_count > it->second.info.consecutive_fail_req_count) {
            it->second.info.consecutive_fail_req_count = it->second.temp_consecutive_fail_req_count;
        }
        it->second.info.ToString(buf + write_len, buf_len - write_len, &one_item_len);
        write_len += one_item_len;
    }
    *data_len = write_len;
    *item_num = remote_info.size();

    remote_info.clear();
    start_stat_timestamp = time(NULL);
    return (write_len == total_item_len);
}

CCDStatInfo::CCDStatInfo() {
}

void CCDStatInfo::GetByteSize(uint32_t *data_len) {
    *data_len = sizeof(CCDStatInfo);
}

bool CCDStatInfo::ToString(char *buf, uint32_t buf_len, uint32_t *data_len) {
    uint32_t to_string_size = 0;
    GetByteSize(&to_string_size);
    if (buf_len < to_string_size) {
        return false;
    }
    *reinterpret_cast<CCDStatInfo*>(buf) = *this;
    *data_len = to_string_size;
    return true;
}

bool CCDStatInfo::ParserFromString(const char *buf, uint32_t data_len) {
    uint32_t to_string_size = 0;
    GetByteSize(&to_string_size);
    if (data_len < to_string_size) {
        // fprintf(stderr, "data_len: %d, CCDStatInfo standard size: %d\n", data_len, to_string_size);
        // fprintf(stderr, "CCDStatInfo min size: %d\n", common_size);
        return false;
    }
    *this = *reinterpret_cast<const CCDStatInfo*>(buf);
    return true;
}

void CCDStatInfo::printTitle() {
    printf("%10s%10s%12s%12s%12s\n", "load", "connNum", "mqWaitAvgMs", "mqWaitMinMs", "mqWaitMaxMs");
}

void CCDStatInfo::printItem() {
    printf("%10u%10u%12u%12u%12u\n", load, conn_num, mq_wait_avg, mq_wait_min, mq_wait_max);
}

void CCDStatInfo::DebugString() {
    printf("----------------------------------------------------\n");
    printf("Mq消息延时、管道统计\n");
    printf("mq_wait_avg: %d\t\tmq_wait_min: %d\n", mq_wait_avg, mq_wait_min);
    printf("mq_wait_max: %d\t\tmq_msg_timeout_count: %d\n", mq_wait_max, mq_msg_timeout_count);
    printf("mq_msg_count: %d\t\tmq_wait_total: %ld\n", mq_msg_count, mq_wait_total);
    printf("消息延时\n");
    printf("max_time: %d\t\tmin_time: %d\n", max_time, min_time);
    printf("total_time: %ld\n", total_time);
    printf("连接统计\n");
    printf("conn_num: %d\t\tmsg_count: %d\n", conn_num, msg_count);
    printf("load: %d\t\tcomplete_err_count: %d\n", load, complete_err_count);
    printf("cc_timeout_close_count: %d\n", cc_timeout_close_count);
    printf("total_recv_size: %ld\t\ttotal_send_size: %ld\n", total_recv_size, total_send_size);
    printf("其它\n");
    printf("is_ccd: %d\t\tsample_gap: %d\n", is_ccd, sample_gap);
    printf("----------------------------------------------------\n");
}

void MCDStatInfo::GetByteSize(uint32_t *data_len) {
    *data_len = sizeof(MCDStatInfo);
}

bool MCDStatInfo::ToString(char *buf, uint32_t buf_len, uint32_t *data_len) {
    uint32_t to_string_size = 0;
    GetByteSize(&to_string_size);
    if (buf_len < to_string_size) {
        return false;
    }
    *reinterpret_cast<MCDStatInfo*>(buf) = *this;
    *data_len = to_string_size;
    return true;
}
bool MCDStatInfo::ParserFromString(const char *buf, uint32_t data_len) {
    uint32_t to_string_size = 0;
    GetByteSize(&to_string_size);
    if (data_len < to_string_size) {
        return false;
    }
    *this = *reinterpret_cast<const MCDStatInfo*>(buf);
    return true;
}

void MCDStatInfo::printTitle() {
    printf("%10s%12s%12s%12s\n", "load", "mqWaitAvgMs", "mqWaitMinMs", "mqWaitMaxMs");
}

void MCDStatInfo::printItem() {
    printf("%10u%12u%12u%12u\n", load, mq_wait_avg, mq_wait_min, mq_wait_max);
}

} // namespace tools
