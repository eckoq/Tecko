// Copyright 2013, Tencent Inc.
// Author: Liu Cheng <codingliu@tencent.com>
//
#ifndef  MCD_APP_NET_DEFINE_H
#define  MCD_APP_NET_DEFINE_H

#include "ccd/app/server_net_define.h"
#include "dcc/app/client_net_define.h"

namespace app {

struct NetProtocol
{
    enum Protocol {
        TCP,
        UDP
    };
};

}  // namespace app

#endif  // MCD_APP_NET_DEFINE_H
