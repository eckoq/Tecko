// Copyright 2012, Tencent Inc.
// Author: Liu Cheng <codingliu@tencent.com>
//
#ifndef  COMPATIBLE_CCD_NET_HANDLER_H
#define  COMPATIBLE_CCD_NET_HANDLER_H

#include <string>

#include "ccd/app/server_net_handler.h"
#include "ccd/tfc_net_cconn.h"
#include "base/tfc_base_so.h"
#include "base/tfc_base_config_file.h"

namespace app {

class CompatibleCCDNetHandler : public ServerNetHandler
{
public:
    const static int MAX_PORT = 65536;

    CompatibleCCDNetHandler();
    ~CompatibleCCDNetHandler();

    int compatible_init(
        tfc::base::CFileConfig& page,
        tfc::base::CSOFile& so_file,
        unsigned tcp_listen_cnt,
        bool event_notify);

    int init_check_complete_func(tfc::base::CFileConfig &page, tfc::base::CSOFile &so_file);

    virtual int init(std::map<std::string, std::string> config_vars) { return 0; }

    virtual unsigned route_connection(
        unsigned client_ip,
        unsigned short client_port,
        unsigned short listen_port,
        unsigned long long flow,
        unsigned worker_num);

    virtual unsigned route_packet(
         void *msg,
         unsigned msg_len,
         unsigned client_ip,
         unsigned client_port,
         unsigned long long flow,
         unsigned short listen_port,
         unsigned worker_num);

    virtual int check_complete(void* data, unsigned data_len, int client_ip, int client_port, int local_port);

    virtual int sync_request(void* data, unsigned data_len, void **ptr);

    virtual int sync_response(char *outbuff, unsigned buf_size, void *ptr);

private:
    std::string _init_msg;

    tfc::net::mcd_route_init _mcd_route_init_func;
    tfc::net::mcd_pre_route _mcd_pre_route_func;
    tfc::net::mcd_route _mcd_route_func;

    tfc::net::sync_request _sync_req_func;
    tfc::net::sync_response _sync_rsp_func;

    tfc::net::check_complete _check_complete_func;
    tfc::net::check_complete* _check_complete_array;
};

}  // namespace app

#endif  // COMPATIBLE_CCD_NET_HANDLER_H
