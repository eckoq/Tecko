// Copyright 2012, Tencent Inc.
// Author: Liu Cheng <codingliu@tencent.com>
//
#include "dcc/app/compatible_dcc_net_handler.h"

namespace tfc {
namespace net {
    int dcc_default_check_complete_func(void* data, unsigned data_len)
    {
        return -1;
    }
}  // namespace net
}  // namespace tfc

namespace app {

int CompatibleDCCNetHandler::compatible_init(
        tfc::base::CFileConfig& page,
        tfc::base::CSOFile& so_file)
{
    _check_complete_func = (tfc::net::check_complete) so_file.get_func("net_complete_func");
    if (_check_complete_func == NULL) {
        fprintf(stderr, "check_complete func is NULL, %m\n");
        return -1;
    }
    return 0;
}

int CompatibleDCCNetHandler::check_complete(
    void* data, unsigned data_len, int server_ip, int server_port)
{
    return _check_complete_func(data, data_len);
}

}  // namespace app
