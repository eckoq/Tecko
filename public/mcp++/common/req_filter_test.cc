#include <assert.h>
#include <iostream>
#include <sstream>
#include <string.h>
#include <unistd.h>

#include "mcd/inc/req_filter.hpp"
#include "sys/time.h"

using namespace std;
using namespace mcppp::check_overload;

void HashMapTest();
void HashFunTest();
void HeapSortTest();
void CheckOverLoadBasicTest0();
void CheckOverLoadBasicTest1();
void CheckOverLoadBasicTest_KeyAvgCountOverload();
void CheckOverLoadBasicTest_TooManyKeys();
void CheckOverLoadBasicTest_TimeWaitOk();
void CheckOverLoadTestTop();
void CheckOverLoadTestTop2();

int main(int argc, char *argv[]) {
    HashMapTest();
    HashFunTest();
    HeapSortTest();
    CheckOverLoadBasicTest0();
    CheckOverLoadBasicTest1();
    CheckOverLoadBasicTest_KeyAvgCountOverload();
    CheckOverLoadBasicTest_TooManyKeys();
    CheckOverLoadBasicTest_TimeWaitOk();
    CheckOverLoadTestTop();
    CheckOverLoadTestTop2();

    return 0;
}

void HashMapTest() {
    __gnu_cxx::hash_map<std::string, uint32_t, StrHash, StrEqual> hash;
    __gnu_cxx::hash_map<std::string, uint32_t, StrHash, StrEqual>::iterator it;

    for (int i = 0; i < 100; i++) {
        stringstream ss;
        ss << i;
        uint32_t& value = hash[ss.str()];
        // hash[ss.str()] = i;
        assert(value == 0);
        value = i;
    }

    for (it = hash.begin(); it != hash.end(); it++) {
        cout << it->first << ", " << it->second << endl;
    }

    cout << "add 100, hash size: " << hash.size() << endl;
}

void HashFunTest() {
    StrHash hash;
    for (int i = 0; i < 100; i++) {
        stringstream ss;
        ss << i;
        uint32_t result = hash.BKDRHash(ss.str());
        // cout << "i: " << i << ", result: " << result << endl;
    }
}

void Print(uint32_t *abc, uint32_t len) {
    for (uint32_t i = 0; i < len; i++) {
        cout << abc[i] << " ";
    }
    cout << endl;
}

void HeapSortTest() {
    HeapSort<uint32_t> sort(10);
    cout << "--------------------------\n";
    uint32_t abc[10];
    memset(abc, 0x0, 40);
    Print(abc, 10);
    sort.BuildHeap(abc, 10);
    Print(abc, 10);
    for (uint32_t i = 1; i < 101; i++){
        abc[0] = i;
        sort.AdjustHeap(abc, 10, 0);
        Print(abc, 10);
    }
    sort.AdjustSequence(abc, 10);
    Print(abc, 10);
    cout << "--------------------------\n";
    sort.HeapInit();
    for (uint32_t i = 1; i < 101; i++){
        sort.Update(i);
        sort.PrintArray();
    }
    sort.TopK();
    sort.PrintArray();

}

void CheckOverLoadBasicTest0() {
    cout << "begin CheckOverLoadBasicTest0()" << endl;
    ReqFilter filter(10, 100, 200);

    uint32_t count;
    filter.GetThreshold(&count);
    if (100 != count) {
        cout << "count number error: " << count << "\n";
        assert(false);
    }
    filter.SetThreshold(1000);
    filter.GetThreshold(&count);
    if (1000 != count) {
        cout << "count number error: " << count << "\n";
        assert(false);
    }

    uint32_t second;
    filter.GetPeriod(&second);
    if (10 != second) {
        cout << "period error: " << second << "\n";
        assert(false);
    }
    filter.SetPeriod(100);
    filter.GetPeriod(&second);
    if (100 != second) {
        cout << "period error: " << second << "\n";
        assert(false);
    }

    uint32_t max_item_num;
    filter.GetMaxItemNum(&max_item_num);
    if (200 != max_item_num) {
        cout << "max_item_num error: " << max_item_num << "\n";
        assert(false);
    }
    filter.SetMaxItemNum(2000);
    filter.GetMaxItemNum(&max_item_num);
    if (2000 != max_item_num) {
        cout << "set max_item_num error: " << max_item_num << "\n";
        assert(false);
    }
    cout << "end CheckOverLoadBasicTest0()" << endl;
}

void CheckOverLoadBasicTest1() {
    cout << "begin CheckOverLoadBasicTest1()" << endl;
    int key_count_overload_num = 0;
    int key_count_nearly_overload_num = 0;
    ReqFilter filter(10, 100, 200);
    for (uint32_t i = 1; i <= 1000; i++) {
        std::stringstream ss;
        ss << i;
        struct sockaddr_in addr;
        uint32_t key_count;
        uint64_t total_count;
        std::string temp = ss.str();
        int ret;
        if (i % 2 == 0) {
            ret = filter.CheckKeyOverload(temp, addr, &key_count, &total_count);
        } else {
            ret = filter.CheckKeyOverload(temp.c_str(), temp.size(), addr, &key_count, &total_count);
        }
        if (ret & KEY_COUNT_OVERLOAD) {
            key_count_overload_num++;
            if (i <= 100) {
                cout << "key overload ret code error for count: " << i << endl;
                assert(false);
            }
        }
        if (ret & KEY_COUNT_NEARLY_OVERLOAD) {
            key_count_nearly_overload_num++;
            if (!(i >= 0.8 * 100 && i <= 100)) {
                cout << "key nearly overload ret code error for count: " << i << endl;
                assert(false);
            }
        }
    }
    assert(key_count_overload_num == 900);
    assert(key_count_nearly_overload_num == 20);
    cout << "end CheckOverLoadBasicTest1()" << endl;
}

void CheckOverLoadBasicTest_KeyAvgCountOverload() {
    cout << "begin CheckOverLoadBasicTest_KeyAvgCountOverload()" << endl;
    int key_avg_count_overload_num = 0;
    int real_key_avg_count_overload_num = 0;
    ReqFilter filter(10, 1000, 2000);
    for (uint32_t i = 1; i <= 100; i++) {
        std::stringstream ss;
        ss << i;
        struct sockaddr_in addr;
        uint32_t key_count;
        uint64_t total_count;
        for (uint32_t j = 1; j <= i; j++) {
            int ret = filter.CheckKeyOverload(ss.str(), addr, &key_count, &total_count);
            if (ret & KEY_AVG_COUNT_OVERLOAD) {
                key_avg_count_overload_num++;
            }

            int key_avg = total_count / i;
            if (j > key_avg) {
                real_key_avg_count_overload_num++;
            }
        }
    }
    assert(key_avg_count_overload_num == real_key_avg_count_overload_num);
    cout << "end CheckOverLoadBasicTest_KeyAvgCountOverload()" << endl;
}

void CheckOverLoadBasicTest_TooManyKeys() {
    cout << "begin CheckOverLoadBasicTest_TooManyKeys()" << endl;
    int key_count_overload_num = 0;
    int key_count_nearly_overload_num = 0;
    int key_num_overload_num = 0;
    int addr_num_overload_num = 0;
    ReqFilter filter(10, 1000, 200);
    for (uint32_t i = 1; i <= 10000; i++) {
        std::stringstream ss;
        ss << i;
        struct sockaddr_in addr;
        if (i > 10) {
            addr.sin_addr.s_addr = i;
        } else {
            addr.sin_addr.s_addr = 0;
        }
        uint32_t key_count;
        uint64_t total_count;
        int ret = filter.CheckKeyOverload(ss.str(), addr, &key_count, &total_count);
        if (ret & KEY_COUNT_OVERLOAD) {
            key_count_overload_num++;
            if (i <= 1000) {
                cout << "key overload ret code " << ret << " error for count: " << i << endl;
                assert(false);
            }
        }
        if (ret & KEY_COUNT_NEARLY_OVERLOAD) {
            key_count_nearly_overload_num++;
            if (!(i >= 0.8 * 1000 && i <= 1000)) {
                cout << "key nearly overload ret code " << ret << " error for count: " << i << endl;
                assert(false);
            }
        }
        if (ret & KEY_NUM_OVERLOAD) {
            key_num_overload_num++;
            if (!(i > 200)) {
                cout << "too many key ret code error for count: " << i << endl;
                assert(false);
            }
        }
        if (ret & ADDR_NUM_OVERLOAD) {
            addr_num_overload_num++;
            if (!(i > 209)) {
                cout << "key number overload ret code error for count: " << i << endl;
                assert(false);
            }
        }
    }
    assert(key_count_overload_num == 9000);
    assert(key_count_nearly_overload_num == 200);
    assert(key_num_overload_num == 9800);
    cout << addr_num_overload_num ;
    assert(addr_num_overload_num == 9791);
    cout << "end CheckOverLoadBasicTest_TooManyKeys()" << endl;
}

void CheckOverLoadBasicTest_TimeWaitOk() {
    cout << "begin CheckOverLoadBasicTest_TimeWaitOk()" << endl;
    int key_count_overload_num = 0;
    int key_count_nearly_overload_num = 0;
    ReqFilter filter(1, 10, 200);
    uint32_t step = 0;
    for (uint32_t i = 1; i <= 50; i++) {
        std::stringstream ss;
        ss << i;
        struct sockaddr_in addr;
        uint32_t key_count;
        uint64_t total_count;
        int ret = filter.CheckKeyOverload(ss.str(), addr, &key_count, &total_count);
        if (step == 1) {
            cout << "sleep ok for success access.\n";
            step = 0;
        }
        if (ret & KEY_COUNT_OVERLOAD) {
            sleep(1);
            step = 1;
            if (i != 11 && i != 22 && i != 33 && i != 44) {
                cout << "key overload ret code -1 error for count: " << i << endl;
                assert(false);
            }
            key_count_overload_num++;
        }
        if (ret & KEY_COUNT_NEARLY_OVERLOAD) {
            if (i != 9 && i != 10 && i != 20 && i != 21 && i != 31 && i != 32 && i != 42 && i != 43) {
                cout << "key nearly overload ret code -2 error for count: " << i << endl;
                assert(false);
            }
            key_count_nearly_overload_num++;
        }
    }
    assert(key_count_overload_num == 4);
    assert(key_count_nearly_overload_num == 8);
    cout << "end CheckOverLoadBasicTest_TimeWaitOk()" << endl;
}

void CheckOverLoadTestTop() {
    cout << "begin CheckOverLoadTestTop()" << endl;
    ReqFilter filter(100, 1000000000, 100000);
    cout << "check begin.\n";
    struct timeval tv1;
    gettimeofday(&tv1, NULL);
    uint64_t count = 0;
    for (uint32_t i = 0; i < 100000; i++) {
        std::stringstream ss;
        ss << i;
        struct sockaddr_in addr;
        addr.sin_addr.s_addr = 100000 - i;

        uint32_t key_count;
        uint64_t total_count;
        for (uint32_t j = 0; j < i / 1000 + 1; j++) {
            count++;
            int ret = filter.CheckKeyOverload(ss.str(), addr, &key_count, &total_count);
            if (ret != 0 && !(ret & KEY_AVG_COUNT_OVERLOAD)) {
                cout << "error ret code " << ret << endl;
                assert(false);
            }
        }
    }
    cout << "check finish, count: " << count << endl;
    struct timeval tv2;
    gettimeofday(&tv2, NULL);
    double us = (tv2.tv_sec - tv1.tv_sec) * 1000000 + (tv2.tv_usec - tv1.tv_usec);
    double us_per_check = us / count;
    cout << "insert count: " << count << ", consumes in ms: " << us / 1000 << ", us per check: " << us_per_check << "\n";

    std::vector<std::string> keys;
    gettimeofday(&tv1, NULL);
    filter.GetTopKeys(50, &keys);
    gettimeofday(&tv2, NULL);
    us = (tv2.tv_sec - tv1.tv_sec) * 1000000 + (tv2.tv_usec - tv1.tv_usec);
    cout << "Get Top Keys consumes time in us: " << us << endl;
    std::vector<std::string>::iterator it = keys.begin();
    for (; it != keys.end(); it++) {
        cout << *it << ", ";
        uint32_t key_int = (uint32_t)strtol(it->c_str(), NULL, 10);
        assert(key_int >= 99000);
    }
    cout << "ok\n";

    std::vector<struct sockaddr_in> addrs;
    gettimeofday(&tv1, NULL);
    filter.GetTopReqAddrs(50, &addrs);
    gettimeofday(&tv2, NULL);
    us = (tv2.tv_sec - tv1.tv_sec) * 1000000 + (tv2.tv_usec - tv1.tv_usec);
    cout << "Get Top Addrs consumes time in us: " << us << endl;
    std::vector<struct sockaddr_in>::iterator addr_it =  addrs.begin();
    for (; addr_it != addrs.end(); addr_it++) {
        cout << (*addr_it).sin_addr.s_addr << ", ";
        assert((*addr_it).sin_addr.s_addr <= 1000);
    }
    cout << "ok\n";
    cout << "end CheckOverLoadTestTop()" << endl;
}

void CheckOverLoadTestTop2() {
    cout << "begin CheckOverLoadTestTop2()" << endl;
    ReqFilter filter(10, 100, 20); // s, count, max num
    for (uint32_t i = 0; i < 100; i++) {
        struct sockaddr_in addr;
        std::stringstream ss;
        if (i > 9) {
            ss << 9;
        } else {
            ss << i;
        }
        addr.sin_addr.s_addr = i;
        uint32_t key_count;
        uint64_t total_count;
        filter.CheckKeyOverload(ss.str(), addr, &key_count, &total_count);
    }
    std::vector<std::string> keys;
    filter.GetTopKeys(50, &keys);
    assert(keys.size() == 10); // keys should only got 10

    std::vector<struct sockaddr_in> addrs;
    filter.GetTopReqAddrs(50, &addrs); // addrs should only got 20 max number
    assert(addrs.size() == 20);

    cout << "end CheckOverLoadTestTop2()" << endl;
}
