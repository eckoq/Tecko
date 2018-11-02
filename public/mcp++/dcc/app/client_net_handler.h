// Copyright 2012, Tencent Inc.
// Author: Liu Cheng <codingliu@tencent.com>
//
#ifndef  DCC_APP_CLIENT_NET_HANDLER_H
#define  DCC_APP_CLIENT_NET_HANDLER_H

#include <map>
#include <string>
#include "base/tfc_base_config_file.h"

namespace app {

class ClientNetHandler
{
public:
    ClientNetHandler() {}
    virtual ~ClientNetHandler() {}

    virtual int init(std::map<std::string, std::string> service_config_vars)
    {
        return 0;
    }

    // no default check complete
    virtual int check_complete(void* data,
                               unsigned data_len,
                               int server_ip,
                               int server_port)
    {
        return -1;
    }
};

}  // namespace app

#endif  // DCC_APP_CLIENT_NET_HANDLER_H
