#include "stat_manager.h"
#include <sstream>
#include <stdio.h>
#include "mcppp_reportor.h"

using namespace std;
using namespace tfc::base;
using namespace tfc::cache;

/////////////////////////////////////////////

StatMgr* StatMgr::_stat_mgr = NULL;

StatMgr* StatMgr::instance()
{
    if(NULL == _stat_mgr)
    {
        _stat_mgr = new StatMgr();
    }
    return _stat_mgr;
}

void StatMgr::release()
{
    if(NULL != _stat_mgr)
    {
        delete _stat_mgr;
        _stat_mgr = NULL;
    }
}

void StatMgr::update_stat(string& server_name, unsigned ip, unsigned short port, bool is_timeout/* = false*/)
{
    if (server_name.empty())
    {
        return;
    }

    vector<ServerStat>& svr_list = _servers[server_name];
    for(unsigned i = 0; i < svr_list.size(); ++i)
    {
        ServerStat& server = svr_list[i];
        if ((server.ip == ip) && (server.port == port))
        {
            server.msg_cnt++;
            if (is_timeout)
            {
                server.to_cnt++;
            }
            return;
        }
    }

    ServerStat svr_stat;
    svr_stat.ip   = ip;
    svr_stat.port = port;
    svr_stat.msg_cnt = 1;
    if (is_timeout)
    {
        svr_stat.to_cnt = 1;
    }
    else
    {
        svr_stat.to_cnt = 0;
    }
    svr_list.push_back(svr_stat);
}

void StatMgr::report()
{
    if (_servers.empty())
    {
        return;
    }

    stringstream ss;

    ss << "[";
    map<string, vector<ServerStat> >::iterator mit;
    for (mit = _servers.begin(); mit != _servers.end(); )
    {
        vector<ServerStat>& srv_list = mit->second;
        ss << "{\"service_name\":\"" << mit->first << "\",\"machine\":[";
        for (unsigned i = 0; i < srv_list.size(); i++)
        {
            if ((0 == srv_list[i].msg_cnt) || (0 == srv_list[i].to_cnt))
            {
                continue;
            }
            ss << "{\"ip\":" << srv_list[i].ip
               << ",\"port\":" << srv_list[i].port
               << ",\"cnt\":" << srv_list[i].msg_cnt
               << ",\"ratio\":" << ((double)srv_list[i].to_cnt / srv_list[i].msg_cnt) << "}";
            if ((i + 1) < srv_list.size())
            {
                ss << ",";
            }
        }
        ss << "]}";
        srv_list.clear();

        if ((++mit) != _servers.end())
        {
            ss << ",";
        }
    }
    ss << "]";
    _servers.clear();

    string tmpstr = ss.str();
    printf("stat: %s\n",ss.str().c_str());
}

int StatMgr::stat_init(unsigned msg_seq, std::string& server_name, unsigned ip, unsigned short port)
{
    MsgTracker* msg = NULL;
    int ret = _timer_queue.get(msg_seq, (CFastTimerInfo**)&msg);
    if ((0 == ret) && (NULL != msg))
    {
        if ((msg->_server_name != server_name) || (msg->_ip != ip) || (msg->_port != port))
        {
            fprintf(stderr, "StatMgr::init_stat msg_seq(%u) conflict.\n",msg_seq);
            return -1;
        }

        _timer_queue.set(msg_seq, msg);
        return 0;
    }

    msg = new MsgTracker(server_name, ip, port, msg_seq);
    return _timer_queue.set(msg_seq, msg);
}

void StatMgr::stat_fini(unsigned msg_seq)
{
    MsgTracker* msg = NULL;
    int ret = _timer_queue.get(msg_seq, (CFastTimerInfo**)&msg);
    if ((0 == ret) && (NULL != msg))
    {
        update_stat(msg->_server_name,msg->_ip,msg->_port);
        delete msg;
    }
}

