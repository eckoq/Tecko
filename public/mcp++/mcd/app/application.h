// Copyright 2012, Tencent Inc.
// Author: Liu Cheng <codingliu@tencent.com>
//
#ifndef  APPLICATION_H
#define  APPLICATION_H

#include <map>
#include <string>
#include <stdint.h>

#include "mcd/app/net_client.h"
#include "mcd/app/net_server.h"
#include "mcd/app/net_event.h"
#include "mcd/app_engine.h"

namespace app {

class Application
{
public:
    Application() {}
    virtual ~Application() {}

    // Return, true: init succeed, false: init failed
    virtual bool init(std::map<std::string, std::string> config_vars) { return true; }


    // TAE will call this function ABOUT every 1-3ms(but not guaranteed)
    virtual void periodic_call(bool* stop) {}


    // Send response: client.send_response(char*, int)
    // Send command: client.send_command(ClientNetCommand::Command) (dcc/app/client_net_define.h)
    virtual void on_request(const char* request, int len, const NetClient* client) { return; }


    // Send request: server->send_response(char*, int)
    // Send event: server.send_send_command(ServerNetCommand::Command) (ccd/app/server_net_define.h)
    virtual void on_response(const char* response, int len, const NetServer* server) { return; }


    // Handle network event(ClientNetEvent and ServerNetEvent)
    virtual void on_event(const NetEvent* event) { return; }


    virtual void finish() { return; }
};

}  // namespace app

#endif  // APPLICATION_H
