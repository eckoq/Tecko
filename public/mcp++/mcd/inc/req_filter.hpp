#ifndef MCPPP_OVERLOAD_REQ_FILTER_H
#define MCPPP_OVERLOAD_REQ_FILTER_H

#include <netinet/in.h>
#include <ext/hash_map>
#include <map>
#include <iostream>
#include <string>

#include "mcd/inc/req_filter_helper.h"

/*
 * MCP++框架的过载保护库，对用户设置的key和addr进行过载判断。直接使用CheckKeyOverload接口即可完成所有过载判断。
 * 可以设置过载判断的周期（秒）、所有key的总处理次数、key与addr的最大处理个数。
 * 业务需要使用下面定义的宏来检测是否有过载情况的发生。其中很多过载情况可能同时发生。
 */

namespace mcppp {
namespace check_overload {

#define APPROACHING_COEFFICIENT 0.8

inline std::ostream& operator<<(std::ostream& os, const InfoItem& item) {
    os << "[" << item.m_count << ", " << *(item.m_info) << "]";
    return os;
}


enum OverloadFlags {
    KEY_COUNT_OVERLOAD = 1,
    KEY_COUNT_NEARLY_OVERLOAD = 2,
    KEY_AVG_COUNT_OVERLOAD = 4,
    KEY_NUM_OVERLOAD = 8,
    ADDR_NUM_OVERLOAD = 16,
};

class ReqFilter {
public:
    /*
     * @second:         过载保护的周期长度，秒
     * @count:          一个周期内所有key总的出现次数
     * @max_item_num:   过载保护库记录的key或者addr最大个数
     */
    ReqFilter(uint32_t period, uint64_t count, uint32_t max_item_num)
        : m_cycle_period(period),
        m_count(count),
        m_approaching_count(APPROACHING_COEFFICIENT * count),
        m_max_item_num(max_item_num),
        m_total_count(0),
        m_access_time(0) {
    }
    ~ReqFilter() {}
    /*
     * @key:            以请求包生成特性key（特性包括客户端ip、port、请求内容、请求类型等）
     * @addr:           请求包的源端地址
     * @key_count:      过载时当前key在本周期内出现次数
     * @total_count:    过载时本周期内所有的key次数
     * returns:         0   正常, 输出参数返回本周期key_count和total count, 下同
     *                  (业务需要使用下面的宏来检测返回值，以判断是否出现该种情况)
     *                  KEY_COUNT_OVERLOAD          key过载(本周期内处理的请求数超过了设定的总处理次数)时返回
     *                  KEY_COUNT_NEARLY_OVERLOAD   total_count超过总阈值时（80%）返回
     *                  KEY_AVG_COUNT_OVERLOAD      key重复次数超过平均key出现次数时返回
     *                  KEY_NUM_OVERLOAD            key的个数超过了max_item_num
     *                  ADDR_NUM_OVERLOAD           addr的个数超过了max_item_num
     */
    int32_t CheckKeyOverload(const std::string& key, const struct sockaddr_in& addr,
                             uint32_t *key_count, uint64_t *total_count) {
        // check time for cycle
        time_t cur_time = time(NULL);
        // TODO(huili): use Monotonic clock to avoid time falut
        if (cur_time - m_access_time >= m_cycle_period) { // at least one cycle end, reset
            m_total_count = 0;
            m_keys_hash_count_map.clear();
            m_addrs_hash_count_map.clear();
            m_access_time = cur_time;
        }

        int32_t ret = 0;
        // update key count and set info for user
        m_key_it = m_keys_hash_count_map.find(key);
        if (m_key_it != m_keys_hash_count_map.end()) {
            m_key_it->second++;
            *key_count = m_key_it->second;
        } else {
            if (m_keys_hash_count_map.size() >= m_max_item_num) {
                ret |= KEY_NUM_OVERLOAD;
            } else {
                m_keys_hash_count_map[key] = 1;
            }
            *key_count = 1;
        }
        // update addr count
        std::string addr_str;
        AddrToString(addr, &addr_str);
        m_addr_it = m_addrs_hash_count_map.find(addr_str);
        if (m_addr_it != m_addrs_hash_count_map.end()) {
            m_addr_it->second++;
        } else {
            if (m_addrs_hash_count_map.size() >= m_max_item_num) {
                ret |= ADDR_NUM_OVERLOAD;
            } else {
                m_addrs_hash_count_map[addr_str] = 1;
            }
        }

        m_total_count++;
        *total_count = m_total_count;

        // check whether overload or not
        if (m_total_count > m_count) { // overload occur
            ret |= KEY_COUNT_OVERLOAD;
        } else if (m_total_count > m_approaching_count) { // nearly overload
            ret |= KEY_COUNT_NEARLY_OVERLOAD;
        }
        uint32_t key_count_avg;
        GetKeyCountAvg(&key_count_avg);
        if (*key_count > key_count_avg) { // key count exceed key count average
            ret |= KEY_AVG_COUNT_OVERLOAD;
        }
        return ret;
    }

    int32_t CheckKeyOverload(const char *key, int key_len, const struct sockaddr_in& addr,
                             uint32_t *key_count, uint64_t *total_count) {
        std::string key_str(key, key_len);
        return CheckKeyOverload(key_str, addr, key_count, total_count);
    }
    /*
     * 该接口操作耗时较久，频繁调用会影响性能
     * @num:            key个数
     * @keys:           返回本周期内出现次数最多的几个key
     */
    void GetTopKeys(uint32_t num, std::vector<std::string> *keys) {
#ifdef _OVERLOAD_DEBUG
        std::cout << "\nm_keys_hash_count_map size: " << m_keys_hash_count_map.size() << std::endl;
#endif
        if (num > m_keys_hash_count_map.size()) {
            num = m_keys_hash_count_map.size();
        }
        HeapSort<InfoItem> sort(num);
        sort.HeapInit();
        for (m_key_it = m_keys_hash_count_map.begin(); m_key_it != m_keys_hash_count_map.end(); m_key_it++) {
            InfoItem item(m_key_it->second, m_key_it->first);
            sort.Update(item);
        }
        sort.TopK();
#ifdef _OVERLOAD_DEBUG
        sort.PrintArray();
#endif
        for (uint32_t i = 0; i < num; i++) {
            keys->push_back(*(sort.m_array[i].m_info));
#ifdef _OVERLOAD_DEBUG
            std::cout << "[" << *(sort.m_array[i].m_info) << ", " << sort.m_array[i].m_count << "], ";
#endif
        }
    }
    /*
     * 该接口操作耗时较久，频繁调用会影响性能
     * @num:            地址个数
     * @addrs:          返回本周期内请求最多的几个源端地址
     */
    void GetTopReqAddrs(uint32_t num, std::vector<struct sockaddr_in> *addrs) {
#ifdef _OVERLOAD_DEBUG
        std::cout << "\nm_addrs_hash_count_map size: " << m_addrs_hash_count_map.size() << std::endl;
#endif
        if (num > m_addrs_hash_count_map.size()) {
            num = m_addrs_hash_count_map.size();
        }
        HeapSort<InfoItem> sort(num);
        sort.HeapInit();
        for (m_addr_it = m_addrs_hash_count_map.begin(); m_addr_it != m_addrs_hash_count_map.end(); m_addr_it++) {
            InfoItem item(m_addr_it->second, m_addr_it->first);
            sort.Update(item);
        }
        sort.TopK();
        for (uint32_t i = 0; i < num; i++) {
            struct sockaddr_in addr;
            memcpy(reinterpret_cast<char*>(&addr), sort.m_array[i].m_info->c_str(), sort.m_array[i].m_info->size());
            addrs->push_back(addr);
#ifdef _OVERLOAD_DEBUG
            std::cout << "[" << addr.sin_addr.s_addr << ", " << sort.m_array[i].m_count << "], ";
#endif
        }
    }
    /*
     * @key_count_avg:  key的平均重复次数
     */
    void GetKeyCountAvg(uint32_t *key_count_avg) {
        *key_count_avg = m_keys_hash_count_map.size();
        if (*key_count_avg != 0) {
            *key_count_avg = (uint32_t)(m_total_count / *key_count_avg);
        }
    }
    /*
     * @second:         过载保护的周期长度，秒
     */
    void SetPeriod(uint32_t second) {
        m_cycle_period = second;
    }
    void GetPeriod(uint32_t *second) {
        *second = m_cycle_period;
    }
    /*
     * @count:          一个周期内所有key总的出现次数阈值
     */
    void SetThreshold(uint32_t count) {
        m_count = count;
        m_approaching_count = (uint32_t)(APPROACHING_COEFFICIENT * count);
    }
    void GetThreshold(uint32_t *count) {
        *count = m_count;
    }
    /*
     * @max_item_num:   一个周期内key或者addr的最大个数
     */
    void SetMaxItemNum(uint32_t max_item_num) {
        m_max_item_num = max_item_num;
    }
    void GetMaxItemNum(uint32_t *max_item_num) {
        *max_item_num = m_max_item_num;
    }

private:
    void AddrToString(const struct sockaddr_in& addr, std::string *addr_str) {
        addr_str->clear();
        addr_str->append(reinterpret_cast<const char*>(&addr), sizeof(addr));
    }

private:
    uint32_t m_cycle_period;        // 周期
    uint64_t m_count;               // 周期内总处理的所有key的出现次数
    uint64_t m_approaching_count;   // 80% of m_count
    uint32_t m_max_item_num;        // 过载保护库最多处理的key个数

    uint64_t m_total_count;         // 当前周期内已记录的所有key出现次数
    time_t m_access_time;           // 记录上一次check时间

    __gnu_cxx::hash_map<std::string, uint32_t, StrHash, StrEqual> m_keys_hash_count_map;
    __gnu_cxx::hash_map<std::string, uint32_t, StrHash, StrEqual>::iterator m_key_it;
    __gnu_cxx::hash_map<std::string, uint32_t, StrHash, StrEqual> m_addrs_hash_count_map;
    __gnu_cxx::hash_map<std::string, uint32_t, StrHash, StrEqual>::iterator m_addr_it;
    /*
    std::map<std::string, uint32_t> m_keys_hash_count_map;
    std::map<std::string, uint32_t>::iterator m_key_it;
    std::map<std::string, uint32_t> m_addrs_hash_count_map;
    std::map<std::string, uint32_t>::iterator m_addr_it;
    */
};

} // namespace check_overload
} // namespace mcppp

#endif  // MCPPP_OVERLOAD_REQ_FILTER_H
