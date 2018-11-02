// Copyright 2012, Tencent Inc.
// Author: Liu Cheng <codingliu@tencent.com>
//
#ifndef  MCD_APP_NET_SERVER_H
#define  MCD_APP_NET_SERVER_H

#include <stdint.h>

#include "mcd/app/utils.h"
#include "mcd/app/net_event.h"
#include "mcd/app/net_define.h"

namespace tfc {
namespace cache {
class RegMQ;
}
}

namespace app {

class NetServer
{
public:
    // NetServer Constructors
    // use default flow
    NetServer(const std::string& ip, uint16_t port,
              NetProtocol::Protocol protocol);

    NetServer(uint32_t ip, uint16_t port,
              NetProtocol::Protocol protocol);

    NetServer(const std::string& ip, uint16_t port,
              NetProtocol::Protocol protocol, uint64_t flow);

    NetServer(uint32_t ip, uint16_t port,
              NetProtocol::Protocol protocol, uint64_t flow);

    NetServer(uint32_t ip, uint16_t port,
              tfc::cache::RegMQ* reg_mq,
              NetProtocol::Protocol protocol,
              uint64_t flow);

    virtual ~NetServer();

    // NetServer methods
    virtual int send_command(NetServerCommand::Command cmd, uint16_t arg = 0) const;
    virtual int send_request(const char* request, int data_len) const;

    static uint64_t get_default_flow(uint64_t ip, uint64_t port);

    uint32_t _server_ip;
    uint32_t _server_port;
    mutable tfc::cache::RegMQ* _reg_mq;
    NetProtocol::Protocol _protocol;
    uint64_t _flow;
};

}  // namespace app
#endif  // MCD_APP_NET_SERVER_H
