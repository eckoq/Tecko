// Copyright 2013, Tencent Inc.
// Author: Liu Cheng <codingliu@tencent.com>
//
#include "mcd/app/net_server.h"

#include "common/clock.h"
#include "mcd/app_engine.h"

namespace app {

NetServer::NetServer(const std::string& ip, uint16_t port,
                     NetProtocol::Protocol protocol)
    : _server_ip(string_to_ip(ip)),
      _server_port(port),
      _protocol(protocol) {
    _flow = get_default_flow(_server_ip, _server_port);
    _reg_mq = tfc::cache::CTAppEngine::get_dcc_mq();
}

NetServer::NetServer(uint32_t ip, uint16_t port,
                     NetProtocol::Protocol protocol)
    : _server_ip(ip),
      _server_port(port),
      _protocol(protocol) {
    _flow = get_default_flow(_server_ip, _server_port);
    _reg_mq = tfc::cache::CTAppEngine::get_dcc_mq();
}

NetServer::NetServer(const std::string& ip, uint16_t port,
                     NetProtocol::Protocol protocol,
                     uint64_t flow)
    : _server_ip(string_to_ip(ip)),
      _server_port(port),
      _protocol(protocol),
      _flow(flow) {
    _reg_mq = tfc::cache::CTAppEngine::get_dcc_mq();
}

NetServer::NetServer(uint32_t ip, uint16_t port,
                     NetProtocol::Protocol protocol,
                     uint64_t flow)
    : _server_ip(ip),
      _server_port(port),
      _protocol(protocol),
      _flow(flow) {
    _reg_mq = tfc::cache::CTAppEngine::get_dcc_mq();
}

NetServer::NetServer(uint32_t ip, uint16_t port,
          tfc::cache::RegMQ* reg_mq,
          NetProtocol::Protocol protocol,
          uint64_t flow)
    : _server_ip(ip),
      _server_port(port),
      _reg_mq(reg_mq),
      _protocol(protocol),
      _flow(flow) {
}

NetServer::~NetServer() {}

uint64_t NetServer::get_default_flow(uint64_t ip, uint64_t port) {
    return (tfc::cache::CTAppEngine::get_pid() | (port << 16) | (ip << 32));
}

int NetServer::send_command(NetServerCommand::Command cmd, uint16_t arg/* = 0*/) const {
    unsigned long long flow = _flow;
    struct timeval cur_time =
        (tfc::cache::CTAppEngine::use_monotonic() ?  tools::GET_MONOTONIC_CLOCK() : tools::GET_WALL_CLOCK());
    TDCCHeader header;

    header._ip = _server_ip;
    header._port = _server_port;
    header._type = convert_server_command(cmd);
    header._arg = arg;
    header._timestamp = cur_time.tv_sec;
    header._timestamp_msec = cur_time.tv_usec / 1000;

    if (0 > _reg_mq->wmq->enqueue(reinterpret_cast<char*>(&header),
                                  DCC_HEADER_LEN,
                                  flow))
    {
        fprintf(stderr, "send event to ccd failed\n");
        return -1;
    }
    return 0;
}

int NetServer::send_request(const char* request, int data_len) const {
    unsigned long long flow = _flow;
    struct timeval cur_time =
        (tfc::cache::CTAppEngine::use_monotonic() ?  tools::GET_MONOTONIC_CLOCK() : tools::GET_WALL_CLOCK());
    TDCCHeader header;

    header._ip = _server_ip;
    header._port = _server_port;
    if (_protocol == NetProtocol::TCP)
        header._type = dcc_req_send;
    else
        header._type = dcc_req_send_udp;
    header._timestamp = cur_time.tv_sec;
    header._timestamp_msec = cur_time.tv_usec / 1000;

    if (0 > _reg_mq->wmq->enqueue(
            reinterpret_cast<char*>(&header),
            DCC_HEADER_LEN,
            request,
            data_len,
            flow))
    {
        fprintf(stderr, "enqueue 2 dcc failed. msg_flow=%lld, data_len=%d\n", flow, data_len);
        return -1;
    }
    _reg_mq->send_size += data_len;
    return 0;
}

}  // namespace app

