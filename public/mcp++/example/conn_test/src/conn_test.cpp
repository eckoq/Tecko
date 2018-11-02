#include "tfc_cache_proc.h"
#include "tfc_base_config_file.h"
#include "tfc_base_str.h"
#include "tfc_debug_log.h"
#include "tfc_base_fast_timer.h"
#include "tfc_net_dcc_define.h"
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#ifdef _SHMMEM_ALLOC_
#include "myalloc.h"
#endif
#include <sys/time.h>

#define BUF_SIZE (1<<27)

using namespace std;
using namespace tfc::cache;
using namespace tfc::net;
using namespace tfc::base;

#define MSG_TAG 0xA1B2C3D4

typedef struct tagPktHeader
{
    unsigned msg_tag;
    unsigned echo_data;
    unsigned pkt_seq;
    unsigned data_len;
}PktHdr;

void disp_dcc(void* priv);


class CConnTest : public CacheProc
{
public:
    CConnTest(){running_conn = 0;recv_num = 0; timeout_num = 0;test_state = false;test_seq = 1;}
    virtual ~CConnTest(){stop_test();}
    virtual void run(const string& conf_file);

    void dispatch_dcc();
    int enqueue_2_dcc(PktHdr* pkt, unsigned long long flow);

    unsigned inc_seq(){return ++test_seq;}
    unsigned get_seq(){return test_seq;}

    inline void add_redo_seq(unsigned seq){vec_redo_seq.push_back(seq);timeout_num++;}

private:
    void dispatch_timeout();
    void redo_seq();
    int  load_cfg();
    void init_log();
    void check_flags();
    void start_test();
    void stop_test();

private:
    CFifoSyncMQ* mq_mcd_2_dcc;
    CFifoSyncMQ* mq_dcc_2_mcd;

    struct timeval last_stat;

    /*-------- config --------*/
    unsigned conn_cnt;
    unsigned timeout;
    unsigned stattime;
    unsigned dst_ip;
    unsigned dst_port;
    string   cfg_file;
    TLogPara log_para;
    unsigned packet_len;
    bool     low_flow;
    /*------------------------*/

    /*---- running status ----*/
    unsigned running_conn;
    unsigned recv_num;
    unsigned timeout_num;
    bool     test_state;
    unsigned test_seq;
    /*------------------------*/

    CFastTimerQueue timer_queue;
    vector<unsigned> vec_redo_seq;
    vector<unsigned> running_seq;

    char ibuf[BUF_SIZE];
    char obuf[BUF_SIZE];
    TDCCHeader* dccheader;
};

class CConnRecorder : public CFastTimerInfo
{
public:
    CConnRecorder(unsigned msg_seq, unsigned long long flow, CConnTest* proc, unsigned pkt_len)
        :msg_flow(flow),mcdproc(proc)
    {
        pkt_hdr.msg_tag   = MSG_TAG;
        pkt_hdr.echo_data = msg_seq;
        pkt_hdr.data_len  = pkt_len;
        ret_msg_seq       = msg_seq;
    }
    ~CConnRecorder(){}

    inline unsigned msg_seq(){return ret_msg_seq;}
    int  do_next(){pkt_hdr.pkt_seq++;return mcdproc->enqueue_2_dcc(&pkt_hdr, msg_flow);}
    void on_expire(){mcdproc->add_redo_seq(ret_msg_seq);}
    bool on_expire_delete(){return false;}
private:
    PktHdr pkt_hdr;
    unsigned long long msg_flow;
    CConnTest* mcdproc;
};

////////////////////////////////////////
bool g_bQuitTest   = false;
bool g_bChangeState = false;

static void sigusr1_changestate(int sig_val)
{
    g_bChangeState = true;
    signal(SIGUSR1, sigusr1_changestate);
}

static void sigusr2_quittest(int sig_val)
{
    g_bQuitTest = true;
    signal(SIGUSR2, sigusr2_quittest);
}

////////////////////////////////////////
int CConnTest::load_cfg()
{
    CFileConfig page;
    try {
        page.Init(cfg_file);
    } catch (...) {
        fprintf(stderr, "Load config file \"%s\" fail!\n", cfg_file.c_str());
        return -1;
    }

    try {
        conn_cnt = from_str<unsigned>(page["root\\conn_cnt"]);
    } catch (...) {
        conn_cnt = 3000;
    }

    try {
        timeout = from_str<unsigned>(page["root\\timeout"]);
    } catch (...) {
        timeout = 5000;
    }

    try {
        stattime = from_str<unsigned>(page["root\\stat_time"]);
    } catch (...) {
        stattime = 60;
    }

    try {
        packet_len = from_str<unsigned>(page["root\\packet_len"]);
    } catch (...) {
        packet_len = 64;
    }

    string dip;
    try {
        dip.assign(page["root\\dst_ip"]);
    } catch (...) {
        dip.assign("10.136.153.152");
    }
    struct in_addr addr;
    if (inet_aton(dip.c_str(), &addr))
    {
        dst_ip = (unsigned)addr.s_addr;
        printf("dst_ip: %d\n",dst_ip);
    }
    else
    {
        fprintf(stderr, "convert ip(%s) failed !\n", dip.c_str());
        return -1;
    }

    try {
        dst_port = from_str<unsigned short>(page["root\\dst_port"]);
    } catch (...) {
        dst_port = 32323;
    }

    try {
        low_flow = from_str<bool>(page["root\\low_flow"]);
    } catch (...) {
        low_flow = false;
    }
    if (low_flow)
    {
        // In low flow mode, we do not send another packet until the last one expires.
        try {
            timeout = from_str<unsigned>(page["root\\send_freq"]);
        } catch (...) {
            timeout = 1000;
        }
    }

    log_para.log_level_     = from_str<int>(page["root\\log\\log_level"]);
    log_para.log_type_      = from_str<int>(page["root\\log\\log_type"]);
    log_para.path_          = page["root\\log\\path"];
    log_para.name_prefix_   = page["root\\log\\name_prefix"];
    log_para.max_file_size_ = from_str<int>(page["root\\log\\max_file_size"]);
    log_para.max_file_no_   = from_str<int>(page["root\\log\\max_file_no"]);

    return 0;
}

void CConnTest::init_log()
{
    int ret = DEBUG_OPEN(log_para.log_level_,
                         log_para.log_type_,
                         log_para.path_,
                         log_para.name_prefix_,
                         log_para.max_file_size_,
                         log_para.max_file_no_);

    assert(ret >= 0);
}

void CConnTest::run(const std::string& conf_file)
{
    cfg_file.assign(conf_file);
    if (load_cfg())
    {
        return;
    }
    init_log();

    srand(time(NULL));

    mq_mcd_2_dcc = _mqs["mq_mcd_2_dcc"];
    mq_dcc_2_mcd = _mqs["mq_dcc_2_mcd"];
    add_mq_2_epoll(mq_dcc_2_mcd, disp_dcc, this);
    dccheader = (TDCCHeader*)obuf;

    signal(SIGUSR1, sigusr1_changestate);
    signal(SIGUSR2, sigusr2_quittest);

    fprintf(stderr, "mcd so started\n");
    DEBUG_P(LOG_TRACE, "ConnTest MCD start\n");
    //主循环
    while(!stop && !g_bQuitTest)
    {
        check_flags();
        run_epoll_4_mq();
        dispatch_timeout();
        redo_seq();
    }

    DEBUG_P(LOG_TRACE, "ConnTest MCD stop\n");
    fprintf(stderr, "mcd so stopped\n");
}

void CConnTest::start_test()
{
    if (test_state)
    {
        fprintf(stderr, "already in testing mode\n");
        return;
    }

    stop_test();

    CConnRecorder* rec = NULL;
    unsigned long long flow_base = ((unsigned long long)dst_port << 32) + dst_ip;
    unsigned long long flow;
    unsigned rand_timeout = timeout;
    for (running_conn = 0; running_conn < conn_cnt; running_conn++)
    {
        flow = flow_base + ((unsigned long long)running_conn << 48);
        rec = new (nothrow) CConnRecorder((++test_seq), flow, this, packet_len);
        if (NULL == rec)
        {
            fprintf(stderr, "create CConnRecorder(%d) failed. Running out of memory?\n", running_conn);
            break;
        }

        rec->do_next();
        rand_timeout = ((unsigned)rand() % (timeout / 4)) + (timeout * 7 / 8);
        timer_queue.set(test_seq, rec, rand_timeout);    // never fail
        running_seq.push_back(test_seq);
    }

    if (running_conn)
    {
        test_state = true;
        DEBUG_P(LOG_NORMAL, "Test start! conn_num=%d\n", running_conn);
    }
    else
    {
        fprintf(stderr, "oops! no connection has been created...\n");
        DEBUG_P(LOG_ERROR, "Create connection failed.\n");
    }
}

void CConnTest::stop_test()
{
    CConnRecorder* rec = NULL;
    int ret = 0;
    unsigned i = 0;
    for (i = 0; i < running_seq.size(); i++)
    {
        ret = timer_queue.get(running_seq[i], (CFastTimerInfo**)&rec);
        if (!ret && rec)
        {
            delete rec;
        }
    }

    if (i != 0)
    {
        DEBUG_P(LOG_NORMAL, "Test stop! conn_num=%d, recv_num=%d, timeout_num=%d\n", running_conn, recv_num, timeout_num);
    }

    running_conn = 0;
    running_seq.clear();

    test_state = false;
}

void CConnTest::dispatch_dcc()
{
    unsigned data_len;
    unsigned flow;
    int ret, qret;
    CConnRecorder* rec = NULL;
    TDCCHeader* rsp_hdr = (TDCCHeader*)ibuf;
    unsigned rand_timeout = timeout;

    PktHdr* hdr = (PktHdr*)(ibuf + DCC_HEADER_LEN);
    for (unsigned i = 0; (i < 1000) && !g_bQuitTest; i++)
    {
        data_len = 0;
        //这里使用try_dequeue，永远不会阻塞
        ret = mq_dcc_2_mcd->try_dequeue(ibuf, BUF_SIZE, data_len, flow);
        if (ret || (data_len <= DCC_HEADER_LEN))
        {
            break;
        }

        if (!low_flow && (rsp_hdr->_type == dcc_rsp_data))
        {
            qret = timer_queue.get(hdr->echo_data, (CFastTimerInfo**)&rec);
            if (!qret && rec)
            {
                recv_num++;
                rec->do_next();
                rand_timeout = ((unsigned)rand() % (timeout / 4)) + (timeout * 7 / 8);
                timer_queue.set(hdr->echo_data, rec, rand_timeout);
            }
            else
            {
                DEBUG_P(LOG_ERROR, "no record found. seq=%d, flow=%lld\n", hdr->echo_data, flow);
            }
        }
    }
}

void CConnTest::dispatch_timeout()
{
    struct timeval cur_time;
    gettimeofday(&cur_time,NULL);
    timer_queue.check_expire(cur_time);
    if ((cur_time.tv_sec - last_stat.tv_sec) >= stattime)
    {
        last_stat.tv_sec  = cur_time.tv_sec;
        last_stat.tv_usec = cur_time.tv_usec;
        if (test_state)
        {
            if (low_flow)
            {
                DEBUG_P(LOG_NORMAL, "low flow stat: conn_num=%d, send_num=%d\n", running_conn, timeout_num);
            }
            else
            {
                DEBUG_P(LOG_NORMAL, "normal stat: conn_num=%d, recv_num=%d, timeout_num=%d\n", running_conn, recv_num, timeout_num);
            }
            recv_num = 0;
            timeout_num = 0;
        }
    }
}

int CConnTest::enqueue_2_dcc(PktHdr* pkt, unsigned long long flow)
{
    memcpy(obuf + DCC_HEADER_LEN, (char*)pkt, sizeof(PktHdr));

    dccheader->_ip   = dst_ip;
    dccheader->_port = dst_port;
    dccheader->_type = dcc_req_send;

    int ret = mq_mcd_2_dcc->enqueue(obuf, sizeof(TDCCHeader) + sizeof(PktHdr) + pkt->data_len, flow);
    if(ret != 0)
    {
        DEBUG_P(LOG_ERROR, "mcd2dcc mq full\n");
        return -1;
    }

    return 0;
}

void CConnTest::redo_seq()
{
    int ret;
    unsigned seq;
    unsigned rand_timeout = timeout;

    if (vec_redo_seq.size() == 0)
    {
        return ;
    }

    for (unsigned idx = 0; idx < vec_redo_seq.size(); idx++)
    {
        CConnRecorder* rec;
        seq = vec_redo_seq[idx];

        ret = timer_queue.get(seq, (CFastTimerInfo**)&rec);
        if(0 != ret)
        {
            continue;
        }

        ret = rec->do_next();
        rand_timeout = ((unsigned)rand() % (timeout / 4)) + (timeout * 7 / 8);
        timer_queue.set(seq, rec, rand_timeout);
    }

    vec_redo_seq.clear();

}

void CConnTest::check_flags()
{
    if (g_bChangeState)
    {
        if (test_state)
        {
            stop_test();
        }
        else
        {
            load_cfg();
            start_test();
        }
        g_bChangeState = false;
    }
}

void disp_dcc(void* priv)
{
    CConnTest* app = (CConnTest*)priv;
    app->dispatch_dcc();
}

extern "C"
{
    CacheProc* create_app()
    {
        return new CConnTest();
    }

    void get_so_information(ReportInfoMap& info)
    {
        info[BUSSINESS_NAME] = "ConnTest, ver0.9.0";
        info[RTX] = "saintvliu";
    }
}

