// Copyright 2012, Tencent Inc.
// Author: Liu Cheng <codingliu@tencent.com>
//
#ifndef  CCD_APP_SERVER_NET_HANDLER_H
#define  CCD_APP_SERVER_NET_HANDLER_H

#include <map>
#include <string>
#include <limits.h>
#include "base/tfc_base_config_file.h"

namespace app {

class ServerNetHandler
{
public:
    ServerNetHandler() {}
    virtual ~ServerNetHandler() {}

    virtual int init(std::map<std::string, std::string> config_vars)
    {
        return 0;
    }

    // default route function
    virtual unsigned route_connection(
        unsigned client_ip,
        unsigned short client_port,
        unsigned short listen_port,
        unsigned long long flow,
        unsigned worker_num)
    {
        return flow % worker_num;
    }

    // no default route func
    virtual unsigned route_packet(
         void* msg,
         unsigned msg_len,
         unsigned client_ip,
         unsigned client_port,
         unsigned long long flow,
         unsigned short listen_port,
         unsigned worker_num)
    {
        return INT_MAX;
    }

    // no default check complete
    virtual int check_complete(void* data, unsigned data_len, int client_ip, int client_port, int listen_port)
    {
        return -1;
    }

    // no default sync request
    virtual int sync_request(void* data, unsigned data_len, void **ptr) {
        return -1;
    }

    // no default sync response
    virtual int sync_response(char *outbuff, unsigned buf_size, void *ptr) {
        return -1;
    }
};

}  // namespace app

#endif  // CCD_APP_SERVER_NET_HANDLER_H
