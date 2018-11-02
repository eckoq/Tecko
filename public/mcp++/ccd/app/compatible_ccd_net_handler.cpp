// Copyright 2012, Tencent Inc.
// Author: Liu Cheng <codingliu@tencent.com>
//
#include "ccd/app/compatible_ccd_net_handler.h"
#include "base/tfc_base_str.h"
#include <errno.h>

using namespace tfc::net;
using namespace tfc::base;

namespace tfc { namespace net {
int default_mcd_route_init(const char* msg)
{
    return 0;
}

unsigned default_mcd_pre_route(unsigned ip, unsigned short port, unsigned short listen_port, unsigned long long flow, unsigned worker_num)
{
    return flow % worker_num;
}

// In old mcp++, no default mcd_route_func
unsigned default_mcd_route(void *msg, unsigned msg_len, unsigned long long flow, unsigned short listen_port, unsigned worker_num)
{
    return INT_MAX;
}

// In old mcp++, no default check_complete
int default_check_complete(void* data, unsigned data_len)
{
    return -1;
}

}  // namespace net
}  // namespace tfc

namespace app {

CompatibleCCDNetHandler::CompatibleCCDNetHandler()
{
    _mcd_route_init_func = NULL;
    _mcd_pre_route_func = NULL;
    _mcd_route_func = NULL;

    _sync_req_func = NULL;
    _sync_rsp_func = NULL;

    _check_complete_func = NULL;
    _check_complete_array = new tfc::net::check_complete[MAX_PORT];
    memset(_check_complete_array, 0, sizeof(tfc::net::check_complete) * MAX_PORT);
}

CompatibleCCDNetHandler::~CompatibleCCDNetHandler()
{
    delete[] _check_complete_array;
}

int CompatibleCCDNetHandler::compatible_init(
    tfc::base::CFileConfig& page, tfc::base::CSOFile& so_file, unsigned tcp_listen_cnt, bool event_notify)
{
    if (tcp_listen_cnt != 0 && init_check_complete_func(page, so_file) != 0)
    {
        fprintf(stderr, "Load complete function fail!\n");
        return -1;
    }

    const char* msg = NULL;
    try
    {
        _init_msg = page["root\\mcd_route_init_msg"];
        msg = _init_msg.c_str();
    } catch (...)
    {
    }

    _mcd_route_init_func = (mcd_route_init)so_file.get_func("mcd_route_init_func");
    if (_mcd_route_init_func == NULL)
        _mcd_route_init_func = tfc::net::default_mcd_route_init;

    if (_mcd_route_init_func(msg) != 0)
    {
        fprintf(stderr, "mcd_route_init_func() run fail! Prepare stop CCD!\n");
        return -1;
    }

    _mcd_route_func = (mcd_route)so_file.get_func("mcd_route_func");
    if (_mcd_route_func == NULL)
    {
        _mcd_route_func = tfc::net::default_mcd_route;
        fprintf(stderr, "No \"mcd_route_func\", use default mcd route tactic.\n");
    }
    else
    {
        fprintf(stderr, "Use custom mcd route tactic.\n");
    }

    _mcd_pre_route_func = (mcd_pre_route)so_file.get_func("mcd_pre_route_func");
    if (_mcd_pre_route_func == NULL && event_notify && _mcd_route_func != tfc::net::default_mcd_route)
    {
        fprintf(stderr, "event notify enable & mcd route define, mcd pre route must required!!!\n");
        return -1;
    }

    if (_mcd_pre_route_func == NULL)
    {
        fprintf(stderr, "No \"mcd_pre_route_func\", use default mcd pre route tactic.\n");
        _mcd_pre_route_func = tfc::net::default_mcd_pre_route;
    }
    else
    {
        fprintf(stderr, "Use custom mcd pre route tactic.\n");
    }

    bool sync_enabled = false;
    try {
        sync_enabled = from_str<bool>(page["root\\sync_enable"]);
    } catch (...) {
    }

    _sync_req_func = (tfc::net::sync_request)so_file.get_func("sync_request_func");
    _sync_rsp_func = (tfc::net::sync_response)so_file.get_func("sync_response_func");
    if (sync_enabled)
    {
        if (_sync_req_func == NULL)
        {
            fprintf(stderr, "Get sync_request_func fail!\n");
            return -1;
        }

        if (_sync_rsp_func == NULL)
        {
            fprintf(stderr, "Get sync_rsp_func fail!\n");
            return -1;
        }
    }

    return 0;
}

int CompatibleCCDNetHandler::init_check_complete_func(
    tfc::base::CFileConfig &page, tfc::base::CSOFile &so_file)
{
    const int MAX_NAME_LEN = 256;
    const int MAX_COMPLETE_FUNC_NUM = 1024;

    char                cc_name[MAX_NAME_LEN], c_buf[MAX_NAME_LEN];
    char                *pos = NULL;
    string              str_name;
    long                tmp_port;
    unsigned short      port;
    int                 i;

    try {
        str_name = page["root\\default_complete_func"];
    } catch (...) {
        str_name = "net_complete_func";
    }
    _check_complete_func = (tfc::net::check_complete)so_file.get_func(str_name.c_str());
    if (_check_complete_func == NULL) {
        fprintf(stderr, "Get default net complete func \"%s\" fail! %m\n", str_name.c_str());
        return -1;
    }

    for ( i = 0; i < MAX_COMPLETE_FUNC_NUM; i++ ) {
        memset(cc_name, 0, MAX_NAME_LEN);
        memset(c_buf, 0, MAX_NAME_LEN);
        pos = NULL;

        snprintf(cc_name, MAX_NAME_LEN - 1, "root\\spec_complete_func_%d", i);
        try {
            str_name = page[cc_name];
        } catch (...) {
            break;
        }

        strncpy(c_buf, str_name.c_str(), MAX_NAME_LEN - 1);

        pos = strchr(c_buf, ':');
        if ( pos == NULL ) {
            fprintf(stderr, "Invalid \"%s\" param - \"%s\"!\n", cc_name, c_buf);
            return -1;
        }

        *pos = 0;
        pos++;

        if ( strlen(c_buf) == 0 || strlen(pos) == 0 ) {
            fprintf(stderr, "Empty port string or complete function name in \"%s\"!\n", cc_name);
            return -1;
        }

        tmp_port = strtol(c_buf, NULL, 10);
        if ( ((tmp_port == LONG_MIN || tmp_port == LONG_MAX) && errno == ERANGE)
            || (tmp_port == 0 && errno == EINVAL)
            || (tmp_port <= 0 || tmp_port >= MAX_PORT) ) {
            fprintf(stderr, "Invalid port string \"%ld\" in \"%s\".\n", tmp_port, cc_name);
            return -1;
        }
        port = (unsigned short)tmp_port;

        _check_complete_array[port] = (tfc::net::check_complete)so_file.get_func(pos);
        if ( _check_complete_array[port] == NULL ) {
            fprintf(stderr, "Load complete func \"%s\" fail! %m\n", pos);
            return -1;
        }

        fprintf(stderr, "Specific complete function \"PORT - %hu, FUNC - %s\" has been load!\n",
                port, pos);
    }
    return 0;
}

unsigned CompatibleCCDNetHandler::route_connection(
    unsigned ip, unsigned short port, unsigned short listen_port, unsigned long long flow, unsigned worker_num)
{
    return _mcd_pre_route_func(ip, port, listen_port, flow, worker_num);
}

unsigned CompatibleCCDNetHandler::route_packet(
    void* msg,
    unsigned msg_len,
    unsigned client_ip,
    unsigned client_port,
    unsigned long long flow,
    unsigned short listen_port,
    unsigned worker_num)
{
    return _mcd_route_func(msg, msg_len, flow, listen_port, worker_num);
}

int CompatibleCCDNetHandler::check_complete(void* data, unsigned data_len, int client_ip, int client_port, int listen_port)
{
    if (_check_complete_array[listen_port] != NULL) {
        return _check_complete_array[listen_port](data, data_len);
    }

    if (_check_complete_func == NULL)
        return -1;
    return _check_complete_func(data, data_len);
}

int CompatibleCCDNetHandler::sync_request(void* data, unsigned data_len, void** ptr)
{
    return _sync_req_func(data, data_len, ptr);
}

int CompatibleCCDNetHandler::sync_response(char* outbuff, unsigned buf_size, void* ptr)
{
    return _sync_rsp_func(outbuff, buf_size, ptr);
}

}  // namespace app
