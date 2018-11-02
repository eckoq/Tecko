// Copyright 2012, Tencent Inc.
// Author: Hui Li <huili@tencent.com>
//
// 2013-01-28: modified by saintvliu
//
#include <arpa/inet.h>
#include <assert.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <sstream>
#include <stdio.h>
#include <string>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

#include "mcppp_reportor.h"
#include "tfc_base_config_file.h"
#include "tfc_base_str.h"
#include "tfc_base_so.h"
#include "tfc_cache_proc.h"
#include "version.h"

#if defined(__GNUC__)
#define UNLIKELYDO(x) (__builtin_expect(x, 0))
#else
#define UNLIKELYDO(x) (x)
#endif

using namespace std;

bool err_quit(int fd1, int fd2, int len) {
    close(fd1);
    if (len == 2) {
        close(fd2);
    }
    return false;
}

namespace reportor { namespace utils {

bool shell_exec(const std::string& cmd, std::string *output)
{
    FILE *fp = popen(cmd.c_str(), "r");
    if (fp == NULL)
    {
        fprintf(stderr, "popen error in shell_exec().\n");
        return false;
    }
    *output = "";
    char result[1024];
    while (fgets(result, 1024, fp) != 0)
    {
        *output += result;
    }
    pclose(fp);
    return true;
}

bool cal_md5(const std::string& file, std::string *md5)
{
    std::string command = "md5sum " + file;
    if (!utils::shell_exec(command, md5))
    {
        fprintf(stderr, "shell_exec for cmd[%s] error.\n", command.c_str());
        return false;
    }
    std::vector<std::string> words;
    utils::split_string(*md5, ' ', &words);
    *md5 = words[0];
    return true;
}

bool get_local_ip(const std::string& net_frame_name, std::string *ip, bool is_ipv4)
{
    struct ifaddrs *ptr = NULL;
    if (getifaddrs(&ptr) != 0)
    {
        fprintf(stderr, "getifaddrs() error: %m.\n");
        return false;
    }

    bool is_find = false;
    *ip = "";
    for (struct ifaddrs *i = ptr; i != NULL; i = i->ifa_next)
    {
        if (strcmp(net_frame_name.c_str(), i->ifa_name) != 0)
        { // check the net frame name
            continue;
        }
        if (i->ifa_addr->sa_family == AF_INET && is_ipv4)
        { // ipv4
            void *addr_ptr = &((struct sockaddr_in*)i->ifa_addr)->sin_addr;
            char ip_buf[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, addr_ptr, ip_buf, INET_ADDRSTRLEN);
            *ip = ip_buf;
            is_find = true;
        }
        else if (i->ifa_addr->sa_family == AF_INET6 && !is_ipv4)
        { // ipv6
            void *addr_ptr = &((struct sockaddr_in6*)i->ifa_addr)->sin6_addr;
            char ip_buf[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, addr_ptr, ip_buf, INET6_ADDRSTRLEN);
            *ip = ip_buf;
            is_find = true;
        }
        if (is_find)
        {
            break;
        }
    }
    if (ptr != NULL)
    {
        freeifaddrs(ptr);
    }
    return is_find;
}

void split_string(const std::string& line, char sep, std::vector<std::string> *temp)
{
    uint32_t i, p;
    for (i = 0, p = 0; i < line.length() && p < line.length(); ++i)
    {
        if (line[i] == sep)
        {
            if ((i - 1) < p)
            {
                p = i + 1;
                continue;
            }
            std::string word;
            word.assign(line, p, i - p);
            temp->push_back(word);
            p = i + 1;
        }
    }
    if (i > p)
    {
        std::string word;
        word.assign(line, p, i - p);
        temp->push_back(word);
    }
}

void get_date_and_time(std::string *date, std::string *time)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
    struct tm *tmm = localtime(&(tv.tv_sec));
    char buf[1024];
    bzero(buf, 1024);
    strftime(buf, 1024, "%Y-%m-%d", tmm);
    *date = buf;
    bzero(buf, 1024);
    strftime(buf, 1024, "%H:%M:%S", tmm);
    *time = buf;
}

void url_encode(const std::string& input, std::string *escaped) {
    escaped->clear();
    for(uint32_t i = 0; i < input.length(); i++) {
        if ((35 <= input[i] && input[i] <= 36) || // #$
            input[i] == '&' ||
            // (38 <= input[i] && input[i] <= 47) || // &'()*+,-./
            (48 <= input[i] && input[i] <= 57) || // 0-9
            (58 <= input[i] && input[i] <= 59) || // :;
            (63 <= input[i] && input[i] <= 64) || // ?@
            (65 <= input[i] && input[i] <= 90) || // A...Z
            (97 <= input[i] && input[i] <= 122) || // a...z
            (input[i] == '~' || input[i] == '!' || input[i] == '*' ||
             input[i] == '(' || input[i] == ')' || input[i] == '\'' ||
             input[i] == '=' || input[i] == '_' || input[i] == '~')) {
            escaped->push_back(input[i]);
        } else {
            escaped->push_back('%');
            char dig1 = (input[i] & 0xF0) >> 4;
            if (0 <= dig1 && dig1 <= 9) {
                dig1 += 48; // 0, 48 in ascii
            } else if (10 <= dig1 && dig1 <= 15) {
                dig1 += (97 - 10); // a, 97 in ascii
            }
            escaped->push_back(dig1);
            char dig2 = (input[i] & 0x0F);
            if (0 <= dig2 && dig2 <= 9) {
                dig2 += 48;
            } else if (10 <= dig2 && dig2 <= 15) {
                dig2 += (97 - 10);
            }
            escaped->push_back(dig2);
        }
    }
}
} // namespace utils

static void *worker(void *args)
{
    CMcpppReportor *ptr = reinterpret_cast<CMcpppReportor*>(args);

    if (ptr)
    {
        ptr->run();
    }
    pthread_exit(NULL);
}

CMsgQ::CMsgQ(int size)
: _buf_size(size)
{
    _ring_buf = new RptMsg[_buf_size];
    memset(_ring_buf, 0, (sizeof(RptMsg)) * _buf_size);
    _head     = 0;
    _tail     = 0;
}

CMsgQ::~CMsgQ()
{
    if(NULL != _ring_buf)
    {
        delete[]  _ring_buf;
        _ring_buf = NULL;
    }
}

int CMsgQ::post(void* msg, unsigned arg1)
{
    RptMsg* pObj = &_ring_buf[_tail];
    if(NULL != pObj->msg)
    {
        // full
        return -1;
    }

    _tail = (_tail + 1) % _buf_size;

    pObj->arg1 = arg1;
    pObj->msg  = msg;    // should be set at last for lock-free

    return 0;
}

int CMsgQ::get(RptMsg& msg)
{
    RptMsg* pObj = &_ring_buf[_head];
    if(NULL == pObj->msg)
    {
        return -1;
    }

    // get one from head of the queue
    _head = (_head + 1) % _buf_size;

    // reset slot
    msg = *pObj;
    pObj->msg = NULL;

    return 0;
}

const uint32_t seconds_per_day = (24 * 60 * 60);
CMcpppReportor *CMcpppReportor::_instance = NULL;

CMcpppReportor *CMcpppReportor::instance()
{
    // need double check?
    if (_instance == NULL)
    {
        _instance = new CMcpppReportor();
        atexit(destroy);
    }
    return _instance;
}

void CMcpppReportor::destroy()
{
    if (_instance != NULL)
    {
        _instance->stop();
        delete _instance;
        _instance = NULL;
    }
}

bool CMcpppReportor::init(const std::string& mcp_conf, const std::string& my_conf)
{
    try
    {
        tfc::base::CFileConfig page1;
        page1.Init(mcp_conf);
        _app_so_file = page1["root\\app_so_file"];
        if (!utils::cal_md5(_app_so_file, &_app_so_file_md5))
        {
            fprintf(stderr, "CMcpppReportor::init_report_request()::cal_md5() error.\n");
            return false;
        }

        void *so_handler = dlopen(_app_so_file.c_str(), RTLD_LAZY);
        if (so_handler == NULL) {
            fprintf(stderr, "dlopen error: %s\n", dlerror());
            return false;
        }
        char *error;
        tfc::cache::read_so_info get_so_information = (tfc::cache::read_so_info)dlsym(so_handler, "get_so_information");
        if ((error = dlerror()) != NULL) {
            fprintf(stderr, "dlsym error: %s\nThe SO should implement get_so_information func\n", error);
            return false;
        }
        tfc::cache::ReportInfoMap so_info;
        get_so_information(so_info);
        _bussiness_name = so_info[tfc::cache::BUSSINESS_NAME];
        _rtx_linkman = so_info[tfc::cache::RTX];
#ifdef __REPORT_DEBUG
        fprintf(stdout, "BUSSINESS_NAME: %s\n", _bussiness_name.c_str());
        fprintf(stdout, "RTX: %s\n", _rtx_linkman.c_str());
#endif
        dlclose(so_handler);
        if (_bussiness_name.size() < 1 || _rtx_linkman.size() < 1)
        {
            fprintf(stderr, "the bussiness name and rtx linkman in SO should not be empty!\n");
            return false;
        }
    }
    catch(...)
    {
        fprintf(stderr, "read config file [%s] error.\n", mcp_conf.c_str());
        return false;
    }

    _mcppp_ver = version_string;

    if (my_conf.size() < 1) {
        fprintf(stdout, "no my conf to use.\n");
        return true;
    }

    tfc::base::CFileConfig page2;
    try
    {
        page2.Init(my_conf);
    }
    catch(...)
    {
        fprintf(stderr, "open config file [%s] error.\n", my_conf.c_str());
        return false;
    }

    try
    {
        _report_cgi_name = page2["root\\report_server\\cgi_name"];
        if (_report_cgi_name.size() < 1)
        {
            _report_cgi_name = "report_server.cgi";
        }

        std::string local_net_frame;
        try {
            local_net_frame = page2["root\\bind_net_frame"];
        } catch (...) {
        }

        if (!get_local_ip_address(local_net_frame)) {
            fprintf(stderr, "CMcpppReportor::init()::get_local_ip() error.\n");
            return false;
        }

        char buf[128];
        std::string report_ip, report_port;
        for (uint32_t i = 1; i <= 2; i++)
        {
            snprintf(buf, 128, "root\\report_server\\ip%d", i);
            report_ip = page2[buf];
            snprintf(buf, 128, "root\\report_server\\port%d", i);
            report_port = page2[buf];
            if (report_ip.size() > 0 && report_port.size() > 0)
            {
                SHostInfo host_info;
                host_info.ip = report_ip;
                char *endptr;
                host_info.port = (uint16_t)strtol(report_port.c_str(), &endptr, 10);
                _hosts.push_back(host_info);
            }
        }
        try
        {
            _monitor.ip.assign(page2["root\\monitor_center\\ip"]);
            _monitor.port = tfc::base::from_str<unsigned short>(page2["root\\monitor_center\\short"]);
        }
        catch(...)
        {
            fprintf(stderr, "monitor center ip(%s), port(%d)\n",_monitor.ip.c_str(),_monitor.port);
        }
    }
    catch(...)
    {
        fprintf(stderr, "read config file [%s] error.\n", my_conf.c_str());
        return false;
    }

    return true;
}

bool CMcpppReportor::start()
{
    _stop = false;
    if (pthread_create(&_pid, NULL, worker, this) != 0)
    {
        fprintf(stderr, "start worker error.\n");
        return false;
    }
    _is_thread_alive = true;
    return true;
}

void CMcpppReportor::stop()
{
    _stop = true;
    pthread_mutex_lock(&_cond_lock);
    pthread_cond_signal(&_cond);
    pthread_mutex_unlock(&_cond_lock); // make _stop = true first!!
    if (_is_thread_alive) {
        pthread_join(_pid, NULL);
        _is_thread_alive = false;
    }
}

void CMcpppReportor::set_sleep_duration(uint32_t sleep_duration)
{
	if (sleep_duration < seconds_per_day / 2)
	{
    	_sleep_duration = sleep_duration;
	}
}

CMcpppReportor::CMcpppReportor()
: _sleep_duration(10)
, _report_done(false)
, _stop(true)
, _is_thread_alive(false)
{
    _report_cgi_name = "report_server.cgi";

    // get the report time stamp
    struct timeval tv;
    gettimeofday(&tv, NULL);
    uint32_t seed = tv.tv_usec;;
    std::string ip;
    if (utils::get_local_ip("eth1", &ip))
    {
        seed <<= 16;
        uint32_t int_ip = (uint32_t)inet_addr(ip.c_str());
        seed |= (0x0000ffff & int_ip);
        seed ^= (0xffff0000 & int_ip);
    }
    srand(seed);
    _report_timestamp = rand() % seconds_per_day;
    if (((seconds_per_day - _report_timestamp) <= _sleep_duration) ||
        (_report_timestamp < _sleep_duration))
    {
        _report_timestamp -= _sleep_duration;
        _report_timestamp = _report_timestamp > 0 ? _report_timestamp : _sleep_duration;
    }
#ifdef __REPORT_DEBUG
    fprintf(stdout, "seed: %u, ts: %ld\n", seed, _report_timestamp);
#endif
    pthread_cond_init(&_cond, NULL);
    pthread_mutex_init(&_cond_lock, NULL);

    _monitor.ip.assign("10.198.0.169");
    _monitor.port = 40000;
}

CMcpppReportor::~CMcpppReportor()
{
}

int CMcpppReportor::post_report(string& data)
{
    if (data.empty())
    {
        return 0;
    }

    string *msg = new (nothrow) string(data);
    if (NULL == msg)
    {
        return -1;
    }

    return _msg_queue.post((void*)msg, msg->length());
}

void CMcpppReportor::run()
{
    struct timeval tv;
    struct timespec outts;

    pthread_mutex_lock(&_cond_lock);
    while (!_stop)
    {
        check_report_info();
        report_stat();

        gettimeofday(&tv, NULL);
        outts.tv_sec = tv.tv_sec + _sleep_duration;
        outts.tv_nsec = tv.tv_usec * 1000;
        pthread_cond_timedwait(&_cond, &_cond_lock, &outts);
    }
    pthread_mutex_unlock(&_cond_lock);
}

void CMcpppReportor::check_report_info()
{
    if (UNLIKELYDO(need_do_report()))
    {
        try
        {
            if (!report_info())
            {
                fprintf(stderr, "CMcpppReportor do_report() error.\n");
            }
        }
        catch(...)
        {
            fprintf(stderr, "CMcpppReportor DoReport error @ line %d.\n", __LINE__);
        }
    }

}

void CMcpppReportor::report_stat()
{
    RptMsg data;
    if (_msg_queue.get(data))
    {
        return;
    }

    string* rpt_stat = reinterpret_cast<string*>(data.msg);
    // TODO: get monitor center ip by tns

    stringstream ss;
    // header
    ss << "POST / HTTP/1.1\r\nAccept:*/*\r\nConnection:close\r\nHost: "
       << _monitor.ip
       << "\r\nContent-Type: text/plain\r\nContent-Length: "
       << rpt_stat->length()
       << "\r\n\r\n"
       << *rpt_stat;
    string report_data = ss.str();
    delete rpt_stat;

    std::string rsp;
    if (!http_visit(_monitor.ip, _monitor.port, report_data, &rsp))
    {
        fprintf(stderr, "report mcp++ information to server [%s:%d] error.\n", _monitor.ip.c_str(), _monitor.port);
        return;
    }

    if (!analyse_report_response(rsp))
    {
        fprintf(stderr, "report response from server [%s:%d] error.\n", _monitor.ip.c_str(), _monitor.port);
        return;
    }
}

bool CMcpppReportor::need_do_report()
{
    // get current time stamp
    struct timeval tv;
	gettimeofday(&tv, NULL);
    time_t time_stamp = tv.tv_sec % seconds_per_day;
#ifdef __REPORT_DEBUG
    fprintf(stdout, "cur: %ld, ts: %ld, prev-done: %d\n", time_stamp, _report_timestamp, _report_done);
#endif
    if (time_stamp > _report_timestamp)
    {
        if (!_report_done)
        {
            _report_done = true;
            return true;
        }
    }
    else
    {
        _report_done = false;
    }
    return false;
}

bool CMcpppReportor::report_info()
{
    // check report server
    if (_hosts.size() < 1)
    {
        fprintf(stderr, "No report server is specified. Report to default servers.\n");
        struct SHostInfo hi;
        hi.ip = "10.198.0.169";
        hi.port = 80;
        _hosts.push_back(hi);
        hi.ip = "10.130.83.146";
        hi.port = 80;
        _hosts.push_back(hi);
    }
    // do send
    bool success = false;
    for (uint32_t i = 0; i < _hosts.size(); i++)
    {
        std::string server_addr = _hosts[i].ip;
        uint16_t port = _hosts[i].port;

        std::string report_content;
        if (!init_report_request(server_addr, &report_content))
        {
            fprintf(stderr, "CMcpppReportor init_report_request() error.\n");
            continue;
        }
#ifdef __REPORT_DEBUG
        else
        {
            fprintf(stdout, "report request: \n--------\n%s\n---------\n", report_content.c_str());
        }
#endif

        std::string rsp;
        if (!http_visit(server_addr, port, report_content, &rsp))
        {
            fprintf(stderr, "report mcp++ information to server [%s:%d] error.\n", server_addr.c_str(), port);
            continue;
        }
#ifdef __REPORT_DEBUG
        else
        {
            fprintf(stdout, "report response: \n--------\n%s\n---------\n", rsp.c_str());
        }
#endif

        if (!analyse_report_response(rsp))
        {
            fprintf(stderr, "report response from server [%s:%d] error.\n", server_addr.c_str(), port);
            continue;
        }
        success = true;
        break;
    }

    return success;
}

bool CMcpppReportor::init_report_request(const std::string& host, std::string *content)
{
    std::string cur_date, cur_time;
    utils::get_date_and_time(&cur_date, &cur_time);
    std::string ports = "";
    // Content:
    // Separated by & character
    std::string info_sep = "&";
    std::stringstream ss;
    ss << "date=" << cur_date << info_sep;
    ss << "time=" << cur_time << info_sep;
    ss << "ip=" << _cur_ip << info_sep;
    ss << "port=" << ports << info_sep;
    ss << "so_name=" << _app_so_file << info_sep;
    ss << "so_md5=" << _app_so_file_md5 << info_sep;
    ss << "rtx=" << _rtx_linkman << info_sep;
    ss << "so_ver=" << _mcppp_ver << info_sep;
    ss << "bussiness_name=" << _bussiness_name;
    std::string report_content = "";
    utils::url_encode(ss.str(), &report_content);
    // reset stringstream
    ss.str("");
    ss.clear();
    // header
    ss << "POST /cgi-bin/" << _report_cgi_name << " HTTP/1.1\r\n";
    ss << "Accept:*/*\r\n";
    ss << "Connection:close\r\n";
    ss << "Host: " << host << "\r\n";
    ss << "Content-Type: application/x-www-form-urlencoded\r\n";
    ss << "Content-Length: ";
    ss << report_content.size();
    ss << "\r\n\r\n";
    // add content
    ss << report_content;
    *content = ss.str();
    return true;
}

bool CMcpppReportor::http_visit(const std::string& host, uint16_t port, const std::string& http_req, std::string *rsp)
{
    // construct socket
    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        fprintf(stderr, "create socket to report mcp++ information error.\n");
        return false;
    }
    struct sockaddr_in servaddr;
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    if (inet_pton(AF_INET, host.c_str(), &servaddr.sin_addr) <= 0)
    {
        fprintf(stderr, "the server address to report mcp++ information is error.\n");
        return err_quit(sockfd, 0, 1);
    }
    // set socket nonblocking
    int flags;
    if ((flags = fcntl(sockfd, F_GETFL, 0)) == -1)
    {
        fprintf(stderr, "get the file descriptor flags of socket fd error.\n");
        return err_quit(sockfd, 0, 1);
    }
    if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        fprintf(stderr, "set socket fd to nonblock error.\n");
        return err_quit(sockfd, 0, 1);
    }
    // create epoll to control socket timeout
    int epfd;
    if ((epfd = epoll_create(10)) == -1)
    {
        fprintf(stderr, "epoll_create error.\n");
        return err_quit(sockfd, 0, 1);
    }
    struct epoll_event ev;
    ev.events = EPOLLOUT | EPOLLONESHOT;
    ev.data.fd = sockfd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &ev) < 0)
    {
        fprintf(stderr, "epoll ctl add sockfd error.\n");
        return err_quit(sockfd, epfd, 2);
    }
    // do connect
    if (connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0)
    {
        if (errno != EINPROGRESS) {
            fprintf(stderr, "connect to server to report mcp++ information error.errno:%d, %s\n", errno, strerror(errno));
            return err_quit(sockfd, epfd, 2);
        }
    }
    // judge socket timeout
    struct epoll_event events[10];
    int nfds = epoll_wait(epfd, events, 10, 5000);
    if (nfds < 0)
    {
        fprintf(stderr, "epoll wait error.\n");
        return err_quit(sockfd, epfd, 2);
    } else if (nfds == 0) {
        fprintf(stderr, "socket timeout.\n");
        return err_quit(sockfd, epfd, 2);
    } else {
        int i;
        for (i = 0; i < nfds; i++)
        {
            if (events[i].data.fd == sockfd) {
                if ((events[i].events & EPOLLOUT) && !(events[i].events & EPOLLERR))
                {
                    int so_error = -1;
                    socklen_t len = sizeof(so_error);
                    if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &so_error, &len) < 0)
                    {
                        fprintf(stderr, "getsockopt to judge connect error.\n");
                        return err_quit(sockfd, epfd, 2);
                    }
                    if (so_error != 0)
                    {
                        fprintf(stderr, "epoll find the connect error.\n");
                        return err_quit(sockfd, epfd, 2);
                    }
                    break;
                } else {
                    fprintf(stderr, "socket fd is not ready!\n");
                    return err_quit(sockfd, epfd, 2);
                }
            }
        }
        if (i == nfds)
        {
            fprintf(stderr, "this cannot happen!\n");
            return err_quit(sockfd, epfd, 2);
        }
        int flags;
        if ((flags = fcntl(sockfd, F_GETFL, 0)) == -1)
        {
            fprintf(stderr, "get the file descriptor flags of socket fd error 2.\n");
            return err_quit(sockfd, epfd, 2);
        }
        if (fcntl(sockfd, F_SETFL, flags & ~O_NONBLOCK) == -1)
        {
            fprintf(stderr, "set socket fd to blocking error.\n");
            return err_quit(sockfd, epfd, 2);
        }
        struct timeval tmo;
        tmo.tv_sec  = 0;
        tmo.tv_usec = 500000;
        setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &tmo, sizeof(tmo));
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tmo, sizeof(tmo));
    }

    // send
    uint32_t sent_len = 0;
    while (sent_len < http_req.size())
    {
        int ret = send(sockfd, http_req.c_str() + sent_len, http_req.size() - sent_len, 0);
        if (ret < 0)
        {
            fprintf(stderr, "send mcp++ information error.\n");
            return err_quit(sockfd, epfd, 2);
        }
        sent_len += ret;
    }

    // read rsp
    char rsp_buf[1024];
    int32_t read_len = 0;
    while ((read_len = recv(sockfd, rsp_buf, 1024, 0)) > 0)
    {
#ifdef __REPORT_DEBUG
        fprintf(stdout, "read_len:%d\n", read_len);
#endif
        if (read_len > 0)
        {
            rsp->append(rsp_buf, read_len);
            bzero(rsp_buf, 1024);
        }
    }
    close(epfd);
    close(sockfd);
    return true;
}

bool CMcpppReportor::analyse_report_response(const std::string& rsp)
{
    std::vector<std::string> words;
    utils::split_string(rsp, ' ', &words);
    if (words.size() < 2)
    {
        return false;
    }
    if (words[1] != "200")
    {
        return false;
    }
    return true;
}

bool CMcpppReportor::get_local_ip_address(const std::string& local_net_frame) {
    // First get local ip from config
    if (local_net_frame.size() > 0 && utils::get_local_ip(local_net_frame.c_str(), &_cur_ip, true))
        return true;

    if (local_net_frame.size() > 0)
        fprintf(stderr, "get local_ip from bind_net_frame:%s failed, "
                        "try to get from eth0, eth1 or br1\n", local_net_frame.c_str());

    // Then get local ip by default
    if (utils::get_local_ip("eth0", &_cur_ip, true))
        return true;
    if (utils::get_local_ip("eth1", &_cur_ip, true))
        return true;
    return utils::get_local_ip("br1", &_cur_ip, true);
}

} // namespace reportor
