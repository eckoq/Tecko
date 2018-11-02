// Copyright 2013, Tencent Inc.
// Author: Liu Cheng <codingliu@tencent.com>
//
#ifndef  CCD_APP_SERVER_NET_DEFINE_H
#define  CCD_APP_SERVER_NET_DEFINE_H

#include "ccd/tfc_net_ccd_define.h"

namespace app {

// ccd_rsp_data, ccd_rsp_data_shm, ccd_rsp_data_udp, ccd_rsp_data_shm_udp
// ccd_req_data, ccd_req_data_shm, ccd_req_data_udp, ccd_req_data_shm_udp
// is data, not event or command, they are understand only by mcp frame,
// user do not need to know
//
namespace ServerNetEvent
{
    enum Event {
        INVALID_EVENT,

        CONNECTION_CREATE,  // (ccd_rsp_connect)

        CONNECTION_CLOSE_FOR_CLIENT,  // (ccd_rsp_disconnect)
        CONNECTION_CLOSE_FOR_TIMEOUT, // (ccd_rsp_disconnect_timeout)
        CONNECTION_CLOSE_FOR_USER, // (ccd_rsp_disconnect_local)
        CONNECTION_CLOSE_FOR_ERROR, // (ccd_rsp_disconnect_peer_or_error, ccd_rsp_disconnect_error)
        CONNECTION_CLOSE_FOR_PACKET_OVERLOAD, // (ccd_rsp_disconnect_overload)

        PACKET_OVERLOAD, // (ccd_rsp_overload)
        CONNECTION_OVERLOAD, // (ccd_rsp_overload_conn),
        MEMORY_USE_OVERLOAD, // (ccd_rsp_overload_mem),

        ALL_DATA_SEND_OUT,  // (ccd_rsp_send_ok)
        NEARLY_ALL_DATA_SEND_OUT, // (ccd_rsp_send_neary_ok)

        // add in mcp++2.3.1, event_notiry and conn_notify_details enabled
        RECEIVING_REQUEST, // (ccd_rsp_recv_data)
        SENDING_RESPONSE,  // (ccd_rsp_send_data)
        CHECK_COMPLETE_OK, // (ccd_rsp_check_complete_ok)
        CHECK_COMPLETE_ERROR, // (ccd_rsp_check_complete_error)
        CONNECTION_IS_EXIST, // (ccd_rsp_cc_ok)
        CONNECTION_NOT_EXIST, // (ccd_rsp_cc_closed)
        RECEIVED_USER_RESPONSE_DATA_AND_SENDING, // (ccd_rsp_reqdata_recved)
    };

    static const char* DESCRIPTION[] =
    {
        "INVALID_EVENT",

        "CONNECTION_CREATE",
        "CONNECTION_CLOSE_FOR_CLIENT",
        "CONNECTION_CLOSE_FOR_TIMEOUT",
        "CONNECTION_CLOSE_FOR_USER",
        "CONNECTION_CLOSE_FOR_ERROR",
        "CONNECTION_CLOSE_FOR_PACKET_OVERLOAD",

        "PACKET_OVERLOAD",
        "CONNECTION_OVERLOAD",
        "MEMORY_USE_OVERLOAD",

        "ALL_DATA_SEND_OUT",
        "NEARLY_ALL_DATA_SEND_OUT",

        "RECEIVING_REQUEST",
        "SENDING_RESPONSE",
        "CHECK_COMPLETE_OK",
        "CHECK_COMPLETE_ERROR",
        "CONNECTION_IS_EXIST",
        "CONNECTION_NOT_EXIST",
        "RECEIVED_USER_RESPONSE_DATA_AND_SENDING"
    };

    inline const char* GetEventDescription(Event event) {
        return DESCRIPTION[event];
    }
}  // namespace ServerNetEvent

namespace NetClientCommand {
    enum Command {
        CLOSE_CONNECTION,  // (ccd_req_disconnect)
        FORCE_CLOSE_CONNECTION,  // (ccd_req_force_disconnect)

        SET_DOWNLOAD_SPEED, // (ccd_req_set_dspeed, KB/s)
        SET_UPLOAD_SPEED, // (ccd_req_set_uspeed, KB/s)
        SET_DOWNLOAD_UPLOAD_SPEED, // (ccd_req_set_duspeed, KB/s)
    };

    static const char* DESCRIPTION[] =
    {
        "CLOSE_CONNECTION",
        "FORCE_CLOSE_CONNECTION",
        "SET_DOWNLOAD_SPEED",
        "SET_UPLOAD_SPEED",
        "SET_DOWNLOAD_UPLOAD_SPEED"
    };

    inline const char* GetCommandDescription(Command cmd) {
        return DESCRIPTION[cmd];
    }
}  // namespace NetClientCommand

inline ServerNetEvent::Event convert_server_event(ccd_reqrsp_type type)
{
    switch (type)
    {
    case ccd_rsp_connect:
        return ServerNetEvent::CONNECTION_CREATE;

    case ccd_rsp_disconnect:
        return ServerNetEvent::CONNECTION_CLOSE_FOR_CLIENT;

    case ccd_rsp_disconnect_timeout:
        return ServerNetEvent::CONNECTION_CLOSE_FOR_TIMEOUT;

    case ccd_rsp_disconnect_local:
        return ServerNetEvent::CONNECTION_CLOSE_FOR_USER;

    case ccd_rsp_disconnect_peer_or_error:
    case ccd_rsp_disconnect_error:
        return ServerNetEvent::CONNECTION_CLOSE_FOR_ERROR;

    case ccd_rsp_disconnect_overload:
        return ServerNetEvent::CONNECTION_CLOSE_FOR_PACKET_OVERLOAD;

    case ccd_rsp_overload:
        return ServerNetEvent::PACKET_OVERLOAD;

    case ccd_rsp_overload_conn:
        return ServerNetEvent::CONNECTION_OVERLOAD;

    case ccd_rsp_overload_mem:
        return ServerNetEvent::MEMORY_USE_OVERLOAD;

    case ccd_rsp_send_ok:
        return ServerNetEvent::ALL_DATA_SEND_OUT;

    case ccd_rsp_send_nearly_ok:
        return ServerNetEvent::NEARLY_ALL_DATA_SEND_OUT;

    case ccd_rsp_recv_data:
        return ServerNetEvent::RECEIVING_REQUEST;

    case ccd_rsp_send_data:
        return ServerNetEvent::SENDING_RESPONSE;

    case ccd_rsp_check_complete_ok:
        return ServerNetEvent::CHECK_COMPLETE_OK;

    case ccd_rsp_check_complete_error:
        return ServerNetEvent::CHECK_COMPLETE_ERROR;

    case ccd_rsp_cc_ok:
        return ServerNetEvent::CONNECTION_IS_EXIST;

    case ccd_rsp_cc_closed:
        return ServerNetEvent::CONNECTION_NOT_EXIST;

    case ccd_rsp_reqdata_recved:
        return ServerNetEvent::RECEIVED_USER_RESPONSE_DATA_AND_SENDING;
    default:
        return ServerNetEvent::INVALID_EVENT;
    }
}

inline ccd_reqrsp_type convert_client_command(NetClientCommand::Command cmd) {
    switch (cmd)
    {
    case NetClientCommand::CLOSE_CONNECTION:
        return ccd_req_disconnect;

    case NetClientCommand::FORCE_CLOSE_CONNECTION:
        return ccd_req_force_disconnect;

    case NetClientCommand::SET_DOWNLOAD_SPEED:
        return ccd_req_set_dspeed;

    case NetClientCommand::SET_UPLOAD_SPEED:
        return ccd_req_set_uspeed;

    case NetClientCommand::SET_DOWNLOAD_UPLOAD_SPEED:
        return ccd_req_set_duspeed;
    }
    return ccd_invalid_type;
}

}  // namespace app
#endif  // CCD_APP_SERVER_NET_DEFINE_H
