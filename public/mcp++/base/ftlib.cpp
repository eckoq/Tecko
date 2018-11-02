#include "ftlib.h"
#include <sys/time.h>
#include <syslog.h>
#include <stdlib.h>
#include <stdio.h>

using namespace tfc::base;

const unsigned FTLIB_MAX_RANGE = 10000;
FTLib* FTLib::ftlib = NULL;

FTLib* FTLib::GetInstance()
{
    if(ftlib == NULL)
    {
        ftlib = new FTLib();
    }
    return ftlib;
}

void FTLib::Release()
{
    if(ftlib != NULL)
    {
        delete ftlib;
        ftlib = NULL;
    }
}

int FTLib::Init(vector<FTModuleRouteTable> module_route_table_list)
{
    for(unsigned i = 0; i < module_route_table_list.size(); ++i)
    {
        FTModuleRouteTable& module = module_route_table_list[i];
        FTModuleStat module_stat;
        string mname = module.module_name;
        module_stat.total_server = module.server_list.size();
        module_stat.valid_server = module_stat.total_server;
        int rrange = FTLIB_MAX_RANGE/ module.server_list.size();
        for(unsigned j = 0; j < module.server_list.size(); ++j)
        {
            FTServerStat* server_stat = new FTServerStat;
            server_stat->ip = module.server_list[j].ip;
            server_stat->port = module.server_list[j].port;
            int rnum = j + 1 == module.server_list.size() ? FTLIB_MAX_RANGE : (j + 1) * rrange;
            InitServerStat(server_stat, rnum);
            module_stat.servers.push_back(server_stat);
        }
        modules[mname] = module_stat;
    }

    tThreshold = FTLIB_DEFAULT_TTHRESHOLD;
    tRetry = FTLIB_DEFAULT_TRETRY;
    cSuccRetry = FTLIB_DEFAULT_CSUCCRETRY;
    calcStamp = FTLIB_DEFAULT_CALCSTAMP;
    expCalcTime = GetCurrTime() + calcStamp;
    kMaxRatio = FTLIB_DEFAULT_KMAXRATIO;

    return 0;
}

int FTLib::Update(enum FT_OPTYPE op, string modulename, FTServerAddr server)
{
    if(modules.find(modulename) == modules.end())
    {
        syslog(LOG_USER | LOG_CRIT | LOG_PID,
               "FTLib Update: Module not find. module name - %s\n",
               modulename.c_str());

        return -1;
    }

    FTServerStat* server_stat = NULL;
    unsigned iServer = -1;
    for(unsigned i = 0; i < modules[modulename].servers.size(); ++i)
    {
        server_stat = modules[modulename].servers[i];
        if(server.ip == server_stat->ip && server.port == server_stat->port)
        {
            iServer = i;
            break;
        }
    }
    if(op == ADD)
    {
        if(iServer != (unsigned)-1)
            return -1;
        server_stat = new FTServerStat;
        server_stat->ip = server.ip;
        server_stat->port = server.port;
        InitServerStat(server_stat, 0);
        modules[modulename].servers.push_back(server_stat);
        modules[modulename].total_server++;
        modules[modulename].valid_server++;

        syslog(LOG_USER | LOG_CRIT | LOG_PID,
               "FTLib Update: New server added. module name - %s, ip - %u, port - %hu\n",
               modulename.c_str(), server.ip, server.port);

        return 0;
    }
    else if(op == DEL)
    {
        if(iServer == (unsigned)-1)
    	    return -1;
    
        if(server_stat->state == NORMAL)
    	    modules[modulename].valid_server--;
        modules[modulename].total_server--;
        delete server_stat;
        modules[modulename].servers.erase(modules[modulename].servers.begin() + iServer);

        syslog(LOG_USER | LOG_CRIT | LOG_PID,
               "FTLib Update: Server deleted. module name - %s, ip - %u, port - %hu\n",
               modulename.c_str(), server.ip, server.port);

        return 0;
    }
    return -1;
}

int FTLib::GetRoute(string modulename, FTServerAddr &server)
{
    if(modules.find(modulename) == modules.end())
    {
        syslog(LOG_USER | LOG_CRIT | LOG_PID,
               "FTLib GetRoute: Module not find. module name - %s\n",
               modulename.c_str());

        return -1;
    }
    
    FTServerStat* server_stat = GetMinDelayServer(modulename);
    if(server_stat == NULL && modules[modulename].valid_server > 0)
    {
    	//return the first valid server when get failed.
        for(unsigned i = 0; i < modules[modulename].servers.size(); ++i)
        {
            if(modules[modulename].servers[i]->state == NORMAL)
            {
                server_stat = modules[modulename].servers[i];
                break;
            }
        }
    }
    if (server_stat == NULL)
    {
    	return -1;
    }
    server.ip = server_stat->ip;
    server.port = server_stat->port;

    return 0;
}

int FTLib::GetRoute(unsigned key, string modulename, FTServerAddr &server)
{
    if(modules.find(modulename) == modules.end())
    {
        syslog(LOG_USER | LOG_CRIT | LOG_PID,
               "FTLib GetRoute by key: Module not find. module name - %s\n",
               modulename.c_str());

        return -1;
    }

    FTModuleStat &module = modules[modulename];
    if(module.total_server == 0 || module.valid_server == 0)
    {
        syslog(LOG_USER | LOG_CRIT | LOG_PID,
               "FTLib GetRoute by key: No server find. module name - %s\n",
               modulename.c_str());

        return -1;
    }
    int iServer = key % module.total_server;
    if(module.servers[iServer]->state == NORMAL)
    {
        server.ip = module.servers[iServer]->ip;
        server.port = module.servers[iServer]->port;
        return 0;
    }
    if(module.servers[iServer]->state == TIMEOUT_RECOVERING && 
       module.servers[iServer]->expect_retry_time <= GetCurrTime())
    {
        server.ip = module.servers[iServer]->ip;
        server.port = module.servers[iServer]->port;
        module.servers[iServer]->state = TIMEOUT_RECOVERING_WAITING;
        return 0;
    }
    
    iServer = key % module.valid_server;
    unsigned vCount = -1;
    for(unsigned i = 0; i < module.servers.size(); ++i)
    {
        if(module.servers[i]->state == NORMAL)
            ++vCount;
        if(vCount == (unsigned)iServer)
        {
            server.ip = module.servers[i]->ip;
            server.port = module.servers[i]->port;
            return 0;
        }
    }

    return -1;
}

int FTLib::Report(unsigned ip, unsigned short port, int delay, bool is_timeout)
{
    string module_name;
    FTServerStat* server = SearchServer(ip, port, module_name);
    if(server == NULL)
    {
        syslog(LOG_USER | LOG_CRIT | LOG_PID,
               "FTLib Report : No server find. module name - %s, ip - %u, port - %hu\n",
               module_name.c_str(), ip, port);

        return -1;
    }

    if(server->state == NORMAL)
    {
        server->conn_count++;
        server->total_delay += delay;
        if(is_timeout)
        {
            server->timeout_count++;
            unsigned valid_server = modules[module_name].valid_server;
            if(server->timeout_count >= server->conn_count * tThreshold
                && valid_server > 1 && (valid_server - 1) * 100 / modules[module_name].total_server >= kMaxRatio)
            {
                server->state = TIMEOUT;
                server->succ_retry_count = 0;
                server->total_delay = 0;
                server->conn_count = 0;
                server->rnum = 0;
                modules[module_name].valid_server--;
                server->expect_retry_time = GetCurrTime() + tRetry;

                syslog(LOG_USER | LOG_CRIT | LOG_PID,
                       "FTLib Report : Server timeout in Normal state. module name - %s, ip - %u, port - %hu\n",
                       module_name.c_str(), ip, port);
            }
        }
    }
    //The server is in TIMEOUT_RECOVERING state
    else
    {
        if(is_timeout)
        {
            server->state = TIMEOUT;
            server->succ_retry_count = 0;
            server->total_delay = 0;
            server->conn_count = 0;
            server->rnum = 0;
            server->expect_retry_time = GetCurrTime() + tRetry;

            syslog(LOG_USER | LOG_CRIT | LOG_PID,
                   "FTLib Report : Server timeout in timeout_recovering state. module name - %s, ip - %u, port - %hu\n",
                   module_name.c_str(), ip, port);
        }
        else
        {
            server->succ_retry_count++;
            server->state = TIMEOUT_RECOVERING;
            server->total_delay += delay;
            if(server->succ_retry_count >= cSuccRetry)
            {
                unsigned avgDelay = server->total_delay / server->succ_retry_count;
                InitServerStat(server, 0);
                server->avg_delay = avgDelay;
                modules[module_name].valid_server++;

                syslog(LOG_USER | LOG_CRIT | LOG_PID,
                       "FTLib Report : Timeout server reset. module name - %s, ip - %u, port - %hu\n",
                       module_name.c_str(), ip, port);
            }
        }
    }

    if(expCalcTime <= GetCurrTime())
        RefreshStat();

    return 0;
}

int FTLib::RefreshStat()
{
    expCalcTime = GetCurrTime() + calcStamp;
    map<string, FTModuleStat>::iterator iter = modules.begin();
    for(; iter != modules.end(); ++iter)
    {
        string module_name = iter->first;
        unsigned tweight = 0;
        for(unsigned i = 0; i < iter->second.servers.size(); ++i)
        {
            FTServerStat* server = iter->second.servers[i];
            if(server->state == TIMEOUT && server->expect_retry_time <= GetCurrTime())
            {
                server->state = TIMEOUT_RECOVERING;
                server->succ_retry_count = 0;
            }
            else if(server->state == NORMAL)
            {
                server->avg_delay = server->conn_count == 0 ? server->avg_delay : server->total_delay / server->conn_count;
                tweight += FTLIB_MAX_TIMEOUT / server->avg_delay;
            }
        }
        unsigned cValid = 1;
        int pre_rnum = 0;
        for(unsigned i = 0; i < iter->second.servers.size(); ++i)
        {
            FTServerStat* server = iter->second.servers[i];
            if(server->state == NORMAL)
            {
                if(cValid == (unsigned)iter->second.valid_server)
                {
                    server->rnum = FTLIB_MAX_RANGE;
                }
                else
                {
                    int rnum = FTLIB_MAX_TIMEOUT * FTLIB_MAX_RANGE / tweight / server->avg_delay;
                    rnum += pre_rnum;
                    pre_rnum = rnum;
                    server->rnum = rnum;
                }
                server->conn_count = 0;
                server->timeout_count = 0;
                server->total_delay = 0;
                server->succ_retry_count = 0;
                server->expect_retry_time = 0;
                cValid++;
            }
        }
    }
    return 0;
}

FTServerStat* FTLib::GetMinDelayServer(string module_name)
{
    if(modules.find(module_name) == modules.end())
        return NULL;

    int rnum = rand() % FTLIB_MAX_RANGE;
    FTServerStat* minServer = NULL;
    bool hasMinServer = false;
    for(unsigned i = 0; i < modules[module_name].servers.size(); ++i)
    {
        FTServerStat* server = modules[module_name].servers[i];

        if(server->state == TIMEOUT
            || server->state == TIMEOUT_RECOVERING_WAITING)
            continue;

        if(server->state == TIMEOUT_RECOVERING)
        {
            server->state = TIMEOUT_RECOVERING_WAITING;
            return server;
        }

        if(!hasMinServer && server->rnum >= rnum)
        {
            minServer = server;
            hasMinServer = true;
        }
    }

    return minServer;
}

int FTLib::InitServerStat(FTServerStat* server, int rnum)
{
    if(server == NULL)
        return -1;
    server->state = NORMAL;
    server->conn_count = 0;
    server->timeout_count = 0;
    server->total_delay = 0;
    server->avg_delay = 20;
    server->succ_retry_count = 0;
    server->expect_retry_time = 0;
    server->rnum = rnum;
    return 0;
}

//implement simple, could be more efficiency
FTServerStat* FTLib::SearchServer(unsigned ip, unsigned short port, string &module_name)
{
    map<string, FTModuleStat>::iterator iter = modules.begin();
    for(; iter != modules.end(); ++iter)
    {
        for(unsigned i = 0; i < iter->second.servers.size(); ++i)
        {
            FTServerStat* server = iter->second.servers[i];
            if(server->ip == ip && server->port == port)
            {
                module_name = iter->first;
                return server;
            }
        }
    }
    return NULL;
}

unsigned long long FTLib::GetCurrTime()
{
    struct timeval nowtime;
    gettimeofday(&nowtime, NULL);
    return nowtime.tv_sec * 1000 + nowtime.tv_usec / 1000;
}

void FTLib::PrintAllStat()
{
    map<string, FTModuleStat>::iterator iter = modules.begin();
    for(; iter != modules.end(); ++iter)
    {
        printf("-------------------------module:%s-----------------------------\n", iter->first.c_str());
        printf("total server: %d\n", iter->second.total_server);
        printf("valid server: %d\n", iter->second.valid_server);
        for(unsigned i = 0; i < iter->second.servers.size(); ++i)
        {
            FTServerStat* server = iter->second.servers[i];
            printf("*** ip - %u, port - %hu\n", server->ip, server->port);
            printf("    conn_count - %u, timeout_count - %u, total_delay - %u, avg_delay - %u\n", server->conn_count,
                   server->timeout_count, server->total_delay, server->avg_delay);
            printf("    state - %d, expect_retry_time - %llu, succ_retry_count - %u, rnum - %d\n", server->state,
                   server->expect_retry_time, server->succ_retry_count, server->rnum);
        }
    }
}
