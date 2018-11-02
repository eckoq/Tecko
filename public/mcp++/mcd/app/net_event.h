// Copyright 2012, Tencent Inc.
// Author: Liu Cheng <codingliu@tencent.com>
//
#ifndef  MCD_APP_NET_EVENT_H
#define  MCD_APP_NET_EVENT_H

#include <stdint.h>
#include "ccd/tfc_net_ccd_define.h"
#include "dcc/tfc_net_dcc_define.h"

#include "ccd/app/server_net_define.h"
#include "dcc/app/client_net_define.h"

namespace app {

struct NetEvent
{
    NetEvent(TDCCHeader* header, unsigned long long flow, uint32_t extra_info)
        : _ip(header->_ip),
          _port(header->_port),
          _arg(header->_arg),
          _timestamp(header->_timestamp),
          _timestamp_msec(header->_timestamp_msec),
          _client_event(convert_client_event(dcc_reqrsp_type(header->_type))),
          _server_event(ServerNetEvent::INVALID_EVENT),
          _flow(flow),
          _event_extra_info(extra_info)
    {}

    NetEvent(TCCDHeader* header, unsigned long long flow, uint32_t extra_info)
        : _ip(header->_ip),
          _port(header->_port),
          _arg(header->_arg),
          _timestamp(header->_timestamp),
          _timestamp_msec(header->_timestamp_msec),
          _client_event(ClientNetEvent::INVALID_EVENT),
          _server_event(convert_server_event(ccd_reqrsp_type(header->_type))),
          _flow(flow),
          _event_extra_info(extra_info)
    {}

    uint32_t _ip;
    uint16_t _port;
    uint16_t _arg;
    uint32_t _timestamp; // second
    uint32_t _timestamp_msec;  // millsecond

    // client or server event, only one is valid
    ClientNetEvent::Event _client_event;
    ServerNetEvent::Event _server_event;

    unsigned long long _flow;
    uint32_t _event_extra_info;
};
}  // namespace app

#endif  // MCD_APP_NET_EVENT_H
