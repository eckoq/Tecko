// Copyright 2012, Tencent Inc.
// Author: Liu Cheng <codingliu@tencent.com>
//
#ifndef  COMPATIBLE_DCC_NET_HANDLER_H
#define  COMPATIBLE_DCC_NET_HANDLER_H

#include "dcc/app/client_net_handler.h"
#include "ccd/tfc_net_cconn.h"
#include "base/tfc_base_so.h"
#include "base/tfc_base_config_file.h"

namespace app {

class CompatibleDCCNetHandler : public ClientNetHandler {
public:
    CompatibleDCCNetHandler() : _check_complete_func(NULL) {}
    ~CompatibleDCCNetHandler() {}

    int compatible_init(
        tfc::base::CFileConfig& page,
        tfc::base::CSOFile& so_file);

    virtual int init(std::map<std::string, std::string> service_config_vars) { return 0; }

    virtual int check_complete(void* data,
                               unsigned data_len,
                               int server_ip,
                               int server_port);


private:
    tfc::net::check_complete _check_complete_func;
};

}  // namespace app

#endif  // COMPATIBLE_DCC_NET_HANDLER_H
