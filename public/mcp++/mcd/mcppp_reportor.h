// Copyright 2012, Tencent Inc.
// Author: Hui Li <huili@tencent.com>
//
//
#ifndef __MCPPP_REPORTOR_H__
#define __MCPPP_REPORTOR_H__

#include <map>
#include <string>
#include <vector>
#include <stdint.h>

namespace reportor
{
    namespace utils
    {
        bool shell_exec(const std::string& cmd, std::string *output);
        bool cal_md5(const std::string& file, std::string *md5);
        bool get_local_ip(const std::string& net_frame_name, std::string *ip, bool is_ipv4 = true);
        void split_string(const std::string& line, char sep, std::vector<std::string> *temp);
        void get_date_and_time(std::string *date, std::string *time);
        void url_encode(const std::string& input, std::string *output);
    } // namespace utils


    typedef struct tagReportMsg
    {
    	void*    msg;
    	unsigned arg1;
    }RptMsg;

    class CMsgQ
    {
    public:
        CMsgQ(int size = 128);
        ~CMsgQ();

    	int  post(void* msg, unsigned arg1);
        int  get(RptMsg& msg);

    private:
        RptMsg*  _ring_buf;
        int      _buf_size;
        int      _head;
        int      _tail;
    };


    class CMcpppReportor
    {
    public:
        static CMcpppReportor *instance();
        static void destroy();
        bool init(const std::string& mcp_conf, const std::string& my_conf);
        bool start();
        void stop();
        void set_sleep_duration(uint32_t sleep_duration);
        int post_report(std::string& data);
        void run();

    private:

        CMcpppReportor(const CMcpppReportor&);
        CMcpppReportor& operator=(const CMcpppReportor&);
        CMcpppReportor();
        ~CMcpppReportor();
        void report_stat();
        void check_report_info();
        bool need_do_report();
        bool report_info();
        bool init_report_request(const std::string& host, std::string *content);
        bool http_visit(const std::string& host, uint16_t port, const std::string& http_req, std::string *rsp);
        bool analyse_report_response(const std::string& rsp);

        bool get_local_ip_address(const std::string& local_ip);

    private:
        struct SHostInfo
        {
            std::string ip;
            uint16_t port;
        }; // record the report server information about ip and port.

    private:
        pthread_t _pid; // the report thread
        pthread_cond_t _cond; // for the report thread's cond_timedwait
        pthread_mutex_t _cond_lock;

        CMsgQ       _msg_queue;
        SHostInfo   _monitor;     // monitor center main server

        time_t _report_timestamp; // the time point (second in a day) to do report.
        uint32_t _sleep_duration; // the sleep duration for the report thread.
        bool _report_done; // whether the report action is done in a day or not.
        volatile bool _stop; // whether the report thread in action or not.
        bool _is_thread_alive; // whether the report thread alive or not.

        std::string _app_so_file;
        std::string _app_so_file_md5;
        std::string _bussiness_name;
        std::string _rtx_linkman;
        std::string _mcppp_ver;

        std::string _report_cgi_name; // the report cgi name on remote http server.
        std::string _cur_ip; // the machine ip where mcd located.
        std::vector<SHostInfo> _hosts; // the candidate report servers.

        static CMcpppReportor *_instance;
    };

} // namespace reportor
#endif  // __MCPPP_REPORTOR_H__
