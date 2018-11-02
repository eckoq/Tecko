// Copyright 2013, Tencent Inc.
// Author: Liu Cheng <codingliu@tencent.com>
//
#ifndef  DCC_APP_CLIENT_NET_DEFINE
#define  DCC_APP_CLIENT_NET_DEFINE

#include "dcc/tfc_net_dcc_define.h"

namespace app {

// dcc_req_send, dcc_req_send_shm, dcc_rsp_data, dcc_rsp_data_shm
// dcc_req_send_udp, dcc_req_send_shm_udp, dcc_rsp_data_udp, dcc_rsp_data_shm_udp
// is data, not event or command, they are understand only by mcp frame,
// user do not need to know
//
namespace ClientNetEvent
{
    enum Event {
        INVALID_EVENT,
        CONNECTION_CREATE, // (dcc_rsp_connect_ok)
        CONNECTION_CREATE_FAILED, // (dcc_rsp_connect_failed)

        CONNECTION_CLOSE_FOR_SERVER, // (dcc_rsp_disconnected)
        CONNECTION_CLOSE_FOR_TIMEOUT, // (dcc_rsp_disconnect_timeout)
        CONNECTION_CLOSE_FOR_USER, // (dcc_rsp_disconnect_local)
        CONNECTION_CLOSE_FOR_ERROR, // (dcc_rsp_disconnect_peer_or_error, dcc_rsp_disconnect_error)
        CONNECTION_CLOSE_FOR_PACKET_OVERLOAD, // (dcc_rsp_disconnect_overload, NOT USED)

        ALL_DATA_SEND_OUT, // (dcc_rsp_send_ok)
        NEARLY_ALL_DATA_SEND_OUT, // (dcc_rsp_send_nearly_ok)
        DATA_SEND_FAILED, // (dcc_rsp_send_failed, NOT USED)
        CONNECTION_OVERLOAD, // (dcc_rsp_overload_conn, NOT USED)
        MEMORY_USE_OVERLOAD, // (dcc_rsp_overload_mem

        // add in mcp++ 2.3.1, event_notify and conn_notify_details enabled
        RECEIVING_RESPONSE, // (dcc_rsp_recv_data)
        SENDING_REQUEST, // (dcc_rsp_send_data)
        CHECK_COMPLETE_OK, // (dcc_rsp_check_complete_ok)
        CHECK_COMPLETE_ERROR, // (dcc_rsp_check_complete_error)
    };

    static const char* DESCRIPTION[] =
    {
        "INVALID_EVENT",
        "CONNECTION_CREATE",
        "CONNECTION_CREATE_FAILED",

        "CONNECTION_CLOSE_FOR_SERVER",
        "CONNECTION_CLOSE_FOR_TIMEOUT",
        "CONNECTION_CLOSE_FOR_USER",
        "CONNECTION_CLOSE_FOR_ERROR",
        "CONNECTION_CLOSE_FOR_PACKET_OVERLOAD",

        "ALL_DATA_SEND_OUT",
        "NEARLY_ALL_DATA_SEND_OUT",
        "DATA_SEND_FAILED",
        "CONNECTION_OVERLOAD",
        "MEMORY_USE_OVERLOAD",
        "RECEIVING_RESPONSE",
        "SENDING_REQUEST",
        "CHECK_COMPLETE_OK",
        "CHECK_COMPLETE_ERROR",
    };

    inline const char* GetEventDescription(Event event) {
        return DESCRIPTION[event];
    }
} // namespace ClientNetEvent

namespace NetServerCommand
{
    enum Command {
        CREATE_CONNECTION, // (dcc_req_connect, NOT USED)
        CLOSE_CONNECTION, // (dcc_req_disconnect),
        SET_DOWNLOAD_SPEED, // (dcc_req_set_dspeed),
        SET_UPLOAD_SPEED,  // (dcc_req_set_uspeed),
        SET_DOWNLOAD_UPLOAD_SPEED, // (dcc_req_set_duspeed)
    };

    static const char* DESCRIPTION[] =
    {
        "CREATE_CONNECTION",
        "CLOSE_CONNECTION",
        "SET_DOWNLOAD_SPEED",
        "SET_UPLOAD_SPEED",
        "SET_DOWNLOAD_UPLOAD_SPEED"
    };

    inline const char* GetCommandDescription(Command cmd) {
        return DESCRIPTION[cmd];
    }
}  // namespace NetServerCommand

inline ClientNetEvent::Event convert_client_event(dcc_reqrsp_type type)
{
    switch (type)
    {
    case dcc_rsp_connect_ok:
        return ClientNetEvent::CONNECTION_CREATE;

    case dcc_rsp_connect_failed:
        return ClientNetEvent::CONNECTION_CREATE_FAILED;

    case dcc_rsp_disconnected:
        return ClientNetEvent::CONNECTION_CLOSE_FOR_SERVER;

    case dcc_rsp_disconnect_timeout:
        return ClientNetEvent::CONNECTION_CLOSE_FOR_TIMEOUT;

    case dcc_rsp_disconnect_local:
        return ClientNetEvent::CONNECTION_CLOSE_FOR_USER;

    case dcc_rsp_disconnect_peer_or_error:
    case dcc_rsp_disconnect_error:
        return ClientNetEvent::CONNECTION_CLOSE_FOR_ERROR;

    case dcc_rsp_disconnect_overload:
        return ClientNetEvent::CONNECTION_CLOSE_FOR_PACKET_OVERLOAD;

    case dcc_rsp_send_failed:
        return ClientNetEvent::DATA_SEND_FAILED;

    case dcc_rsp_overload_conn:
        return ClientNetEvent::CONNECTION_OVERLOAD;

    case dcc_rsp_overload_mem:
        return ClientNetEvent::MEMORY_USE_OVERLOAD;

    case dcc_rsp_recv_data:
        return ClientNetEvent::RECEIVING_RESPONSE;

    case dcc_rsp_send_data:
        return ClientNetEvent::SENDING_REQUEST;

    case dcc_rsp_check_complete_ok:
        return ClientNetEvent::CHECK_COMPLETE_OK;

    case dcc_rsp_check_complete_error:
        return ClientNetEvent::CHECK_COMPLETE_ERROR;

    case dcc_rsp_send_ok:
        return ClientNetEvent::ALL_DATA_SEND_OUT;

    case dcc_rsp_send_nearly_ok:
        return ClientNetEvent::NEARLY_ALL_DATA_SEND_OUT;

    default:
        return ClientNetEvent::INVALID_EVENT;
    }
}

inline dcc_reqrsp_type convert_server_command(NetServerCommand::Command cmd)
{
    switch (cmd)
    {
    case NetServerCommand::CREATE_CONNECTION:
        return dcc_req_connect;

    case NetServerCommand::CLOSE_CONNECTION:
        return dcc_req_disconnect;

    case NetServerCommand::SET_DOWNLOAD_SPEED:
        return dcc_req_set_dspeed;

    case NetServerCommand::SET_UPLOAD_SPEED:
        return dcc_req_set_uspeed;

    case NetServerCommand::SET_DOWNLOAD_UPLOAD_SPEED:
        return dcc_req_set_duspeed;
    }
    return dcc_invalid_type;
}

}  // namespace app
#endif  // DCC_APP_CLIENT_NET_DEFINE
