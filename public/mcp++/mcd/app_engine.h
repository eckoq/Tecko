#ifndef _APP_ENGINE_H_
#define _APP_ENGINE_H_
#include <assert.h>
#include <stdint.h>
#include <limits.h>
#include "tfc_base_so.h"
#include "tfc_base_fast_timer.h"
#include "tfc_base_config_file.h"
#include "tfc_net_ipc_mq.h"
#include "tfc_net_ccd_define.h"
#include "tfc_net_dcc_define.h"
#include "tfc_cache_proc.h"

namespace app
{
    class Application;
}
namespace tfc{namespace cache
{
    const int DEF_BUF_SIZE = 64 * 1024 * 1024; // 64MB

    class CTAppEngine;
    struct RegMQ
    {
        tfc::net::CFifoSyncMQ* rmq;
        tfc::net::CFifoSyncMQ* wmq;
        unsigned rmq_idx;
        unsigned wmq_idx;
        CTAppEngine* proc;
        uint64_t recv_size;
        uint64_t send_size;
    };

    typedef struct tagMQStatInfo
    {
		unsigned _mq_msg_count; 				//取管道消息次数
		unsigned long long _mq_wait_total;		//消息在管道中总时间
		unsigned _mq_wait_max;  				//消息在管道中最大时间
		unsigned _mq_wait_min;					//消息在管道中最小时间
        unsigned _mq_ccd_discard_package_count; // discard timeout pakcage count in ccd mq
        unsigned _mq_dcc_discard_package_count; // discard timeout pakcage count in dcc mq
        unsigned _mq_ccd_package_count;         // pakcage count in ccd mq
        unsigned _mq_dcc_package_count;         // pakcage count in dcc mq
    }MQStatInfo;                                // note: send/recv size stat is in RegMQ object.

    class CTAppEngine : public CacheProc
    {
    public:
        CTAppEngine();
        virtual ~CTAppEngine();

        virtual void  run(const std::string& conf_file);

        void dispatch_ccd(RegMQ* mq);
        void dispatch_dcc(RegMQ* mq);
        void dispatch_timeout(struct timeval& cur_time);

        app::Application* get_application() const { return _application; }

        static RegMQ* get_ccd_mq() { return _ccd_mq; }
        static RegMQ* get_dcc_mq() { return _dcc_mq; }
        static int get_pid() { return _pid; }
        static bool use_monotonic() { return _use_monotonic_time_in_header; }

    protected:
        int init(const std::string& conf_file);
        int start();

        int init_global_config(const tfc::base::CFileConfig& page);
        int init_application();
        int init_log_client(const std::string& conf_file);
        int init_mq(const tfc::base::CFileConfig& config);
        int init_iobuffer();
        int set_app_so_md5_and_inner_ip();

        virtual RegMQ* register_mq(const std::string& req_mq_name, const std::string& rsp_mq_name, bool is_ccd);
        unsigned check_wait_time(time_t& ts_sec, time_t& ts_msec, struct timeval& cur_time);

        inline void ResetMQStatInfo() {
            _mq_stat._mq_msg_count = 0;
            _mq_stat._mq_wait_total = 0;
            _mq_stat._mq_wait_max = 0;
            _mq_stat._mq_wait_min = UINT_MAX;
            _mq_stat._mq_ccd_discard_package_count = 0;
            _mq_stat._mq_dcc_discard_package_count = 0;
            _mq_stat._mq_ccd_package_count = 0;
            _mq_stat._mq_dcc_package_count = 0;
        }

    protected:
        unsigned _stat_time;

        char*    _io_buf;
        unsigned _buf_size;
        struct timeval _last_stat;
        MQStatInfo _mq_stat;
        app::Application* _application;

        // FIXME: in this version, only one ccd and dcc for each mcd
        static RegMQ* _ccd_mq;
        static RegMQ* _dcc_mq;

        bool _dl_global;

        std::string _user_config_file;
        tfc::base::CFileConfig _user_config_page;

        tfc::base::CSOFile _so_file;
        std::string _app_so_file;
        std::string _create_app_intf;
        int _ccd_dequeue_max;

        static int _pid;
        static bool _use_monotonic_time_in_header;
    };
}}

//////////////////////////////////////////////////////////////////////////
#endif//_APP_ENGINE_H_
///:~
