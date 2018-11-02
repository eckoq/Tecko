/*
 * echo server
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#include "tfc_cache_proc.h"
#include "tfc_net_ccd_define.h"
#include "myalloc.h"

#define BUF_SIZE    (1<<27)

using namespace tfc::cache;
using namespace tfc::net;


class CEchoServer : public CacheProc
{
public:
    CEchoServer(){}
    virtual ~CEchoServer(){}
    virtual void run(const std::string& conf_file);

    void dispatch_ccd();

private:

    CFifoSyncMQ* mq_ccd_2_mcd;
    CFifoSyncMQ* mq_mcd_2_ccd;
    char         rw_buf[BUF_SIZE];

    TCCDHeader* ccdheader;
};

bool g_bStopServer = false;

static void disp_ccd(void *priv)
{
    CEchoServer* app = (CEchoServer*)priv;
    app->dispatch_ccd();
}

static void sigusr2_handle(int sig_val)
{
    g_bStopServer = true;
    signal(SIGUSR2, sigusr2_handle);
}

void CEchoServer::run(const std::string& conf_file)
{
    mq_ccd_2_mcd = _mqs["mq_ccd_2_mcd"];
    mq_mcd_2_ccd = _mqs["mq_mcd_2_ccd"];
    add_mq_2_epoll(mq_ccd_2_mcd, disp_ccd, this);

    signal(SIGUSR2, sigusr2_handle);
    ccdheader = (TCCDHeader*)rw_buf;

    fprintf(stderr, "mcd so started\n");

    while(!stop && !g_bStopServer)
    {
        run_epoll_4_mq();
    }

    fprintf(stderr, "mcd so stopped\n");
}

void CEchoServer::dispatch_ccd()
{
    unsigned data_len;
    unsigned long long flow;
    int      ret;

    for (unsigned i = 0; (i < 500) && (!g_bStopServer); i++)
    {
        data_len = 0;
        ret = mq_ccd_2_mcd->try_dequeue(rw_buf, BUF_SIZE, data_len, flow);
        if (ret || (data_len <= CCD_HEADER_LEN))
        {
            break;
        }

        if (ccd_rsp_data == ccdheader->_type)
        {
            ccdheader->_type = ccd_req_data;
            mq_mcd_2_ccd->enqueue(rw_buf, data_len, flow);
        }
    }
}

extern "C"
{
    CacheProc* create_app()
    {
        return new CEchoServer();
    }

    void get_so_information(ReportInfoMap& info)
    {
        info[BUSSINESS_NAME] = "EchoServer, ver0.9.0";
        info[RTX] = "saintvliu";
    }
}


