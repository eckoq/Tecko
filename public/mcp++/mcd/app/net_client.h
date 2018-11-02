// Copyright 2012, Tencent Inc.
// Author: Liu Cheng <codingliu@tencent.com>
//
#ifndef  MCD_APP_NET_CLIENT_H
#define  MCD_APP_NET_CLIENT_H

#include <stdint.h>
#include "mcd/app/net_event.h"
#include "mcd/app/net_define.h"
#include "dcc/app/client_net_define.h"

namespace tfc {
namespace cache {
class RegMQ;
}
}

namespace app {

class NetClient
{
public:
    NetClient(uint64_t flow, tfc::cache::RegMQ* mq,
              uint32_t ip, uint16_t port,
              uint16_t local_port, NetProtocol::Protocol protocol);

    virtual ~NetClient();

    virtual int send_command(NetClientCommand::Command cmd, uint16_t arg = 0) const;

    virtual int send_response(const char* response, int data_len) const;

    uint64_t _flow;
    mutable tfc::cache::RegMQ* _mq_mcd_2_ccd;
    uint32_t _ip;
    uint16_t _port;
    uint16_t _local_port;
    NetProtocol::Protocol _protocol;
};

}  // namespace app

#endif  // MCD_APP_NET_CLIENT_H
