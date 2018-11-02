// Copyright 2013, Tencent Inc.
// Author: Liu Cheng <codingliu@tencent.com>
//
#include "app_engine.h"
#include "log_client.h"
#include "log_type.h"
#include "stat.h"
#include "stat_manager.h"
#include "tfc_base_so.h"
#include "tfc_base_str.h"
#include "tfc_md5.h"
#include "tfc_net_ccd_define.h"
#include "tfc_net_dcc_define.h"

#include "common/clock.h"
#include "mcd/app/application.h"
#include "mcd/app/net_client.h"
#include "mcd/app/net_define.h"
#include "mcd/app/net_event.h"
#include "mcd/app/net_server.h"

using namespace std;
using namespace tfc::net;
using namespace tfc::base;

namespace tfc {
namespace cache {

#define MAX_MQ_WAIT_TIME    10000   //管道最大等待时间(10s)，超过这个时间就需要清管道

tools::CLogClient*      log_client = new (nothrow) tools::CLogClient();
tools::MCDLogInfo       mcd_log_info;
tools::NSLogInfo        ns_log_info;
tools::MCDStatInfo      mcd_stat_info;


void write_mcd_log(unsigned event_type, struct timeval* cur_time, unsigned long long flow,
                   unsigned mq_index = -1, unsigned short err = 0, unsigned wait_time = 0)
{
    if (!flow)
    {
        mcd_log_info.ip         = 0;
        mcd_log_info.port       = 0;
        mcd_log_info.local_port = 0;
    }
    mcd_log_info.flow       = flow;
    mcd_log_info.wait_time  = wait_time;
    mcd_log_info.mq_index   = mq_index;
    mcd_log_info.err        = err;
    log_client->write_log(event_type, cur_time, &mcd_log_info);
}

// use a long name incase function name conflict with application's .so
void mcppp_app_engine_dispatch_ccd(void *priv)
{
    RegMQ* mq = reinterpret_cast<RegMQ*>(priv);
    mq->proc->dispatch_ccd(mq);
}

// use a long name incase function name conflict with application's .so
void mcppp_app_engine_dispatch_dcc(void *priv)
{
    RegMQ* mq = reinterpret_cast<RegMQ*>(priv);
    mq->proc->dispatch_dcc(mq);
}

RegMQ* CTAppEngine::_ccd_mq = NULL;
RegMQ* CTAppEngine::_dcc_mq = NULL;
int CTAppEngine::_pid = -1;
bool CTAppEngine::_use_monotonic_time_in_header = false;

CTAppEngine::CTAppEngine()
    : _stat_time(60),
      _io_buf(NULL),
      _buf_size(DEF_BUF_SIZE),
      _application(NULL),
      _dl_global(false)
{
    _ccd_mq = NULL;
    _dcc_mq = NULL;
    _pid = getpid();
    _use_monotonic_time_in_header = false;
    _last_stat = tools::GET_WALL_CLOCK();
    memset(&_mq_stat, 0, sizeof(_mq_stat));
}

CTAppEngine::~CTAppEngine()
{
    int i;
    RegMQ*   mq;
    MQInfo*  info;
    for(i = 0; i < _infonum; ++i)
    {
        info = &_mq_info[i];
        mq = reinterpret_cast<RegMQ*>(info->_priv);
        if (mq)
        {
            delete mq;
        }
    }

    if (_io_buf)
    {
        free(_io_buf);
    }

    delete _application;
    _application = NULL;
}

void CTAppEngine::run(const std::string& conf_file)
{
    if (init(conf_file) != 0)
    {
        fprintf(stderr, "mcd init fail.\n");
        return;
    }

    start();
}

int CTAppEngine::start() {
    // add_mq_2_epoll
    fprintf(stderr, "app_engine started\n");

    struct timeval cur_time;
    while (!stop)
    {
        run_epoll_4_mq();

        cur_time = tools::GET_WALL_CLOCK();
        dispatch_timeout(cur_time);

        bool application_need_stop = false;
        _application->periodic_call(&application_need_stop);

        if (application_need_stop)
        {
            fprintf(stderr, "exit run loop for periodic_call.\n");
            break;
        }
    }
    _application->finish();
    fprintf(stderr, "app_engine stopped\n");
    return 0;
}


int CTAppEngine::init(const string& conf_file)
{
    CFileConfig page;

    try {
        page.Init(conf_file);
    } catch(...)
    {
        fprintf(stderr, "init app engine, load config from file:%s failed\n", conf_file.c_str());
        return -1;
    }

    if (init_global_config(page) != 0)
    {
        fprintf(stderr, "init global config from file:%s error\n", conf_file.c_str());
        return -1;
    }

    if (init_log_client(conf_file) != 0)
    {
        fprintf(stderr, "init log_client failed\n");
        return -1;
    }

    if (init_mq(page) != 0)
    {
        fprintf(stderr, "init mq failed\n");
        return -1;
    }

    if (init_iobuffer() != 0)
    {
        fprintf(stderr, "init iobuf failed\n");
        return -1;
    }

    if (init_application() != 0)
    {
        fprintf(stderr, "init application failed\n");
        return -1;
    }

    if (set_app_so_md5_and_inner_ip() != 0)
    {
        fprintf(stderr, "set app so md5 failed\n");
        return -1;
    }

    return 0;
}

int CTAppEngine::init_global_config(const tfc::base::CFileConfig& page)
{
    try {
        _use_monotonic_time_in_header = from_str<bool>(page["root\\use_monotonic_time_in_header"]);
    }
    catch(...) {
        _use_monotonic_time_in_header = false;
    }
    try {
        _user_config_file = page["root\\user_conf_file"];
    }
    catch (...)
    {
        fprintf(stderr, "no root\\user_conf_file specified in app_conf_file\n");
        return -1;
    }

    try {
        _user_config_page.Init(_user_config_file);
    } catch (...)
    {
        fprintf(stderr, "load user_config_file:%s failed\n", _user_config_file.c_str());
        return -1;
    }

    try
    {
        _buf_size = from_str<unsigned>(_user_config_page["root\\io_buf_size"]);
    }
    catch(...)
    {
        _buf_size = DEF_BUF_SIZE;
    }

    try
    {
        _ccd_dequeue_max = from_str<int>(_user_config_page["root\\ccd_dequeue_max"]);
    }
    catch(...)
    {
        _ccd_dequeue_max = 1000;
    }
    fprintf(stderr, "ccd dequeue max:%d\n", _ccd_dequeue_max);

    try
    {
        _dl_global = from_str<bool>(_user_config_page["root\\app_so_link_global"]);
    }
    catch (...)
    {
        _dl_global = 0;
    }

    try {
        _app_so_file   = _user_config_page["root\\app_so_file"];
    } catch (...)
    {
        fprintf(stderr, "init AppEngine, but no root\\app_so_file is config in %s\n",
                _user_config_file.c_str());
        return -1;
    }

    try {
        _create_app_intf = _user_config_page["root\\create_app_interface"];
    }
    catch (...)
    {
        fprintf(stderr, "init AppEngine, root\\create_app_interface is not in %s\n",
                _user_config_file.c_str());
        return -1;
    }
    return 0;
}

int CTAppEngine::init_application()
{
    int dl_flag = RTLD_LAZY;
    if (_dl_global)
    {
        dl_flag = RTLD_LAZY | RTLD_GLOBAL;
    }

    int ret = _so_file.open(_app_so_file, dl_flag);
    if (ret != 0)
    {
        fprintf(stderr, "so_file open fail, %s, %m\n", _app_so_file.c_str());
        return -1;
    }

    typedef app::Application* (*app_constructor)();

    app_constructor create_app = (app_constructor) _so_file.get_func(_create_app_intf);
    if (create_app == NULL)
    {
        fprintf(stderr, "so_file open init_app_func fail, %s, %m\n", _app_so_file.c_str());
        return -1;
    }

    std::map<std::string, std::string> user_config_map;
    _user_config_page.GetConfigMap(&user_config_map);

    _application = create_app();
    bool init_succeed = _application->init(user_config_map);

    if (!init_succeed)
    {
        fprintf(stderr, "init application fail.\n");
        return -1;
    }
    return 0;
}

int CTAppEngine::init_log_client(const std::string& conf_file)
{
    memset(&mcd_log_info, 0, sizeof(mcd_log_info));
    memset(&ns_log_info, 0, sizeof(ns_log_info));
    memset(&mcd_stat_info, 0, sizeof(mcd_stat_info));

    if (NULL == log_client)
    {
        fprintf(stderr, "alloc log_client api failed\n");
        return -1;
    }

    if (log_client->init(conf_file) < 0)
    {
        fprintf(stderr, "init log_client from file:%s failed\n", conf_file.c_str());
        return -1;
    }
    return 0;
}

// Only support one ccd to mcd and mcd to one dcc
// One mcd to multiple dcc or multiple ccd is not supported
int CTAppEngine::init_mq(const CFileConfig& page)
{
    const char* MQ_CCD_2_MCD = "root\\mq\\mq_ccd_2_mcd";
    const char* MQ_MCD_2_CCD = "root\\mq\\mq_mcd_2_ccd";
    const char* MQ_MCD_2_DCC = "root\\mq\\mq_mcd_2_dcc";
    const char* MQ_DCC_2_MCD = "root\\mq\\mq_dcc_2_mcd";

    int ROOT_MQ_PATH_LEN = strlen("root\\mq\\");
    bool has_mq = false;

    try {
        std::string ccd_2_mcd_conf = page[MQ_CCD_2_MCD];
        std::string mcd_2_ccd_conf = page[MQ_MCD_2_CCD];

        _ccd_mq = register_mq(MQ_CCD_2_MCD + ROOT_MQ_PATH_LEN,
                              MQ_MCD_2_CCD + ROOT_MQ_PATH_LEN, true);
        if (_ccd_mq == NULL)
        {
            fprintf(stderr, "register ccd mq failed\n");
            return -1;
        }
        has_mq = true;
    } catch (...) {}

    try {
        std::string mcd_2_dcc_conf = page[MQ_MCD_2_DCC];
        std::string dcc_2_mcd_conf = page[MQ_DCC_2_MCD];

        _dcc_mq = register_mq(MQ_DCC_2_MCD + ROOT_MQ_PATH_LEN,
                              MQ_MCD_2_DCC + ROOT_MQ_PATH_LEN, false);
        if (_dcc_mq == NULL)
        {
            fprintf(stderr, "register dcc mq failed\n");
            return -1;
        }
        has_mq = true;
    } catch (...) {}

    if (!has_mq)
    {
        fprintf(stderr, "no mq is init for mcd, error\n");
        return -1;
    }
    return 0;
}

int CTAppEngine::init_iobuffer()
{
    // Init buffer for dequeue from ccd/dcc
    _io_buf = (char*)malloc(_buf_size);
    if (NULL == _io_buf)
    {
        fprintf(stderr, "allocate memory failed.\n");
        return -1;
    }
    return 0;
}

int CTAppEngine::set_app_so_md5_and_inner_ip()
{
    char *md5 = md5_file(const_cast<char*>(_app_so_file.c_str()));
    if (md5 == NULL) {
        fprintf(stderr, "compute the so file md5 info failed.\n");
        return -1;
    }

    if (!log_client->SetSoMd5(md5, strlen(md5)))
    {
        fprintf(stderr, "mcd log client write md5 info failed.\n");
        return -1;
    }

    log_client->SetIpWithInnerIp();

    return 0;
}

RegMQ* CTAppEngine::register_mq(const string& req_mq_name, const string& rsp_mq_name, bool is_ccd)
{
    CFifoSyncMQ* req_mq = NULL;
    CFifoSyncMQ* rsp_mq = NULL;

    if (_mq_list.find(req_mq_name) != _mq_list.end())
    {
        req_mq = _mq_list[req_mq_name]._mq;
    }
    if (_mq_list.find(rsp_mq_name) != _mq_list.end())
    {
        rsp_mq = _mq_list[rsp_mq_name]._mq;
    }
    if ((NULL == req_mq) && (NULL == rsp_mq))
    {
        fprintf(stderr, "no mq specified, req_mq:%s, rsp_mq:%s.\n", req_mq_name.c_str(), rsp_mq_name.c_str());
        return NULL;
    }

    RegMQ* reg_mq = new (nothrow) RegMQ;
    if (NULL == reg_mq)
    {
        fprintf(stderr,"allocate memory failed.\n");
        return NULL;
    }

    reg_mq->rmq  = req_mq;
    reg_mq->wmq  = rsp_mq;
    reg_mq->proc = this;
    if (req_mq) {
        reg_mq->rmq_idx = _mq_list[req_mq_name]._idx;
        reg_mq->recv_size = 0;
    }
    if (rsp_mq) {
        reg_mq->wmq_idx = _mq_list[rsp_mq_name]._idx;
        reg_mq->send_size = 0;
    }

    int ret = 0;
    if (is_ccd)
    {
        ret = add_mq_2_epoll(reg_mq->rmq, mcppp_app_engine_dispatch_ccd, reg_mq);
    }
    else
    {
        ret = add_mq_2_epoll(reg_mq->rmq, mcppp_app_engine_dispatch_dcc, reg_mq);
    }

    if (ret)
    {
        fprintf(stderr,"too many mq (max: %d).\n", MAX_MQ_NUM);
        delete reg_mq;
        return NULL;
    }

    return reg_mq;
}

// check mq wait time
inline unsigned CTAppEngine::check_wait_time(time_t& ts_sec, time_t& ts_msec, struct timeval& cur_time)
{
    unsigned tdiff = 0;
    if (ts_sec)
    {
        long time_diff = (cur_time.tv_sec - ts_sec) * 1000 + (cur_time.tv_usec/1000 - ts_msec);
        if (time_diff < 0)
        {
            time_diff = 0;
        }
        tdiff = time_diff;
    }

    if(tdiff > _mq_stat._mq_wait_max)
    {
        _mq_stat._mq_wait_max = tdiff;
    }
    if(tdiff < _mq_stat._mq_wait_min)
    {
        _mq_stat._mq_wait_min = tdiff;
    }
    _mq_stat._mq_msg_count++;
    _mq_stat._mq_wait_total += tdiff;
    return tdiff;
}

void CTAppEngine::dispatch_ccd(RegMQ* reg_mq)
{
    int      ret;
    unsigned data_len;
    unsigned long long flow;
    TCCDHeader* ccdheader = (TCCDHeader*)_io_buf;
    unsigned mq_wait_time = 0;
    struct timeval cur_time;

    for(int i = 0; i < _ccd_dequeue_max; ++i)
    {
        data_len = 0;

        ret = reg_mq->rmq->try_dequeue(_io_buf, _buf_size, data_len, flow);
        if ((ret < 0) || (data_len <= 0))
        {
            break;
        }

        if (i % 100 == 0)
        {
            cur_time = (_use_monotonic_time_in_header ? tools::GET_MONOTONIC_CLOCK() : tools::GET_WALL_CLOCK());
        }

        reg_mq->recv_size += data_len - CCD_HEADER_LEN;
        _mq_stat._mq_ccd_package_count++;

        mq_wait_time = check_wait_time(ccdheader->_timestamp, ccdheader->_timestamp_msec, cur_time);
        mcd_log_info.ip         = ccdheader->_ip;
        mcd_log_info.port       = ccdheader->_port;
        mcd_log_info.local_port = ccdheader->_listen_port;
        mcd_log_info.seq        = 0;
        write_mcd_log(tools::LOG_MCD_DEQ, &cur_time, flow, reg_mq->rmq_idx, 0, mq_wait_time);

        if (mq_wait_time > MAX_MQ_WAIT_TIME)
        {
            // 发生异常，需要清管道
            _mq_stat._mq_ccd_discard_package_count++;
            break;
        }

        if (ccd_rsp_data != ccdheader->_type && ccd_rsp_data_udp != ccdheader->_type)
        {
            uint32_t extra_info = 0;
            if (ccdheader->_type == ccd_rsp_recv_data ||
                ccdheader->_type == ccd_rsp_send_data ||
                ccdheader->_type == ccd_rsp_check_complete_ok ||
                ccdheader->_type == ccd_rsp_check_complete_error ||
                ccdheader->_type == ccd_rsp_cc_ok ||
                ccdheader->_type == ccd_rsp_cc_closed ||
                ccdheader->_type == ccd_rsp_reqdata_recved) {
                extra_info = *reinterpret_cast<const uint32_t*>(_io_buf + CCD_HEADER_LEN);
            }

            app::NetEvent event(ccdheader, flow, extra_info);
            _application->on_event(&event);
            // fprintf(stderr,"wrong type %d\n",ccdheader->_type); // DEBUG TEST
            continue;
        }

        app::NetProtocol::Protocol protocol = app::NetProtocol::TCP;
        if (ccdheader->_type == ccd_rsp_data_udp)
            protocol = app::NetProtocol::UDP;

        app::NetClient client(
            flow, reg_mq, ccdheader->_ip, ccdheader->_port, ccdheader->_listen_port, protocol);
        _application->on_request(_io_buf + CCD_HEADER_LEN, data_len - CCD_HEADER_LEN, &client);
    }
}

void CTAppEngine::dispatch_dcc(RegMQ* reg_mq)
{
    int      ret;
    unsigned data_len;
    unsigned long long flow;
    TDCCHeader* dccheader = (TDCCHeader*)_io_buf;
    unsigned mq_wait_time = 0;
    struct timeval cur_time;

    for(int i = 0; i < 10000; ++i)
    {
        data_len = 0;

        ret = reg_mq->rmq->try_dequeue(_io_buf, _buf_size, data_len, flow);
        if ((ret < 0) || (data_len <= 0))
        {
            break;
        }

        if (i % 100 == 0)
        {
            cur_time = (_use_monotonic_time_in_header ? tools::GET_MONOTONIC_CLOCK() : tools::GET_WALL_CLOCK());
        }

        _mq_stat._mq_dcc_package_count++;
        reg_mq->recv_size += data_len - DCC_HEADER_LEN;

        mq_wait_time = check_wait_time(dccheader->_timestamp, dccheader->_timestamp_msec, cur_time);
        mcd_log_info.ip         = dccheader->_ip;
        mcd_log_info.port       = dccheader->_port;
        mcd_log_info.local_port = 0;
        mcd_log_info.seq        = 0;
        write_mcd_log(tools::LOG_MCD_DEQ, &cur_time, flow, reg_mq->rmq_idx, 0, mq_wait_time);

        if (mq_wait_time > MAX_MQ_WAIT_TIME)
        {
            // 发生异常，需要清管道
            _mq_stat._mq_dcc_discard_package_count++;
            break;
        }

        if (dcc_rsp_data != dccheader->_type && dcc_rsp_data_udp != dccheader->_type)
        {
            uint32_t extra_info = 0;

            if (dccheader->_type == dcc_rsp_send_data ||
                dccheader->_type == dcc_rsp_recv_data ||
                dccheader->_type == dcc_rsp_check_complete_ok ||
                dccheader->_type == dcc_rsp_check_complete_error) {
                extra_info = *reinterpret_cast<const uint32_t*>(_io_buf + DCC_HEADER_LEN);
            }

            app::NetEvent event(dccheader, flow, extra_info);

            _application->on_event(&event);
            // fprintf(stderr,"wrong type %d\n",dccheader->_type); // DEBUG TEST
            continue;
        }

        app::NetProtocol::Protocol protocol = app::NetProtocol::TCP;
        if (dccheader->_type == dcc_rsp_data_udp)
            protocol = app::NetProtocol::UDP;

        app::NetServer server(dccheader->_ip, dccheader->_port, reg_mq, protocol, flow);
        _application->on_response(_io_buf + DCC_HEADER_LEN,
                                  data_len - DCC_HEADER_LEN,
                                  &server);
    }
}

void CTAppEngine::dispatch_timeout(struct timeval& cur_time)
{
    StatMgr::instance()->check_stat(cur_time);
    if ((cur_time.tv_sec - _last_stat.tv_sec) >= _stat_time)
    {
        StatMgr::instance()->report();
        _last_stat.tv_sec  = cur_time.tv_sec;
        _last_stat.tv_usec = cur_time.tv_usec;
    }

    /* check mcd stat */
    static time_t last_log_time = cur_time.tv_sec;
    unsigned short stat_gap  = log_client->get_stat_gap();
    unsigned short time_diff = (unsigned short)(cur_time.tv_sec - last_log_time);
    if (stat_gap && (time_diff >= stat_gap))
    {
        if(_mq_stat._mq_wait_min == UINT_MAX)
        {
            _mq_stat._mq_wait_min = 0;
        }

        mcd_stat_info.load = _mq_stat._mq_msg_count / time_diff;
        mcd_stat_info.mq_wait_avg = (_mq_stat._mq_msg_count > 0 ? (unsigned)(_mq_stat._mq_wait_total / _mq_stat._mq_msg_count) : 0);
        mcd_stat_info.mq_wait_min = _mq_stat._mq_wait_min;
        mcd_stat_info.mq_wait_max = _mq_stat._mq_wait_max;
        mcd_stat_info.mq_ccd_discard_package_count = _mq_stat._mq_ccd_discard_package_count;
        mcd_stat_info.mq_dcc_discard_package_count = _mq_stat._mq_dcc_discard_package_count;
        mcd_stat_info.mq_ccd_package_count = _mq_stat._mq_ccd_package_count;
        mcd_stat_info.mq_dcc_package_count = _mq_stat._mq_dcc_package_count;
        mcd_stat_info.sample_gap = time_diff;

        ResetMQStatInfo();

        // FIXME: in this version, only one ccd and dcc for each mcd
        if (_ccd_mq != NULL)
        {
            mcd_stat_info.mq_ccd_total_recv_size = _ccd_mq->recv_size;
            mcd_stat_info.mq_ccd_total_send_size = _ccd_mq->send_size;

            _ccd_mq->send_size = 0;
            _ccd_mq->recv_size = 0;
        }

        if (_dcc_mq != NULL)
        {
            mcd_stat_info.mq_dcc_total_recv_size = _dcc_mq->recv_size;
            mcd_stat_info.mq_dcc_total_send_size = _dcc_mq->send_size;

            _dcc_mq->send_size = 0;
            _dcc_mq->recv_size = 0;
        }

        log_client->write_stat(tools::STAT_MCD, &mcd_stat_info);
        last_log_time = cur_time.tv_sec;
    }
}

}
}
