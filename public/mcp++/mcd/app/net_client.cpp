// Copyright 2013, Tencent Inc.
// Author: Liu Cheng <codingliu@tencent.com>
//
#include "mcd/app/net_client.h"

#include "common/clock.h"
#include "mcd/app_engine.h"

namespace app {

NetClient::NetClient(uint64_t flow, tfc::cache::RegMQ* mq,
                     uint32_t ip, uint16_t port,
                     uint16_t local_port, NetProtocol::Protocol protocol)
    : _flow(flow),
      _mq_mcd_2_ccd(mq),
      _ip(ip),
      _port(port),
      _local_port(local_port),
      _protocol(protocol) {
}

NetClient::~NetClient() {}

int NetClient::send_command(NetClientCommand::Command cmd, uint16_t arg/* = 0*/) const {
    struct timeval cur_time =
        (tfc::cache::CTAppEngine::use_monotonic() ?  tools::GET_MONOTONIC_CLOCK() : tools::GET_WALL_CLOCK());
    TCCDHeader header;

    header._ip = _ip;
    header._port = _port;
    header._listen_port = _local_port;
    header._type = convert_client_command(cmd);
    header._arg = arg;
    header._timestamp = cur_time.tv_sec;
    header._timestamp_msec = cur_time.tv_usec/1000;

    if (0 > _mq_mcd_2_ccd->wmq->enqueue(reinterpret_cast<char*>(&header),
                                        CCD_HEADER_LEN,
                                        (unsigned long long)_flow))
    {
        fprintf(stderr, "send event to ccd failed\n");
        return -1;
    }
    return 0;
}

int NetClient::send_response(const char* response, int data_len) const {
    struct timeval cur_time =
        (tfc::cache::CTAppEngine::use_monotonic() ?  tools::GET_MONOTONIC_CLOCK() : tools::GET_WALL_CLOCK());
    TCCDHeader header;

    header._ip = _ip;
    header._port = _port;
    header._listen_port = _local_port;
    if (_protocol == NetProtocol::TCP)
        header._type = ccd_req_data;
    else
        header._type = ccd_req_data_udp;
    header._timestamp = cur_time.tv_sec;
    header._timestamp_msec = cur_time.tv_usec / 1000;

    if (0 > _mq_mcd_2_ccd->wmq->enqueue(reinterpret_cast<char*>(&header),
                                        CCD_HEADER_LEN,
                                        response,
                                        data_len,
                                        (unsigned long long)_flow))
    {
        fprintf(stderr, "enqueue 2 ccd failed. msg_flow=%ld, data_len=%d\n", _flow, data_len); // DEBUG TEST
        return -1;
    }
    _mq_mcd_2_ccd->send_size += data_len;
    return 0;
}

}  // namespace app
