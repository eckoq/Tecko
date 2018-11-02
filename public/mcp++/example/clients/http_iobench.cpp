#include <stdio.h>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <sstream>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <assert.h>
#include <vector>
#include <getopt.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <map>

#define MAX_FILE_LEN    256
#define HTTP_FILE_LEN   MAX_FILE_LEN/2
#define SND_BUF_SIZE    512
#define MAX_SAMPLE_TIME 600
#define MAX_THREAD_NUM  64
#define RCV_BUF_SIZE    (3 << 20)
#define US_PER_SEC      1000000
#define US_PER_MSEC     1000.0
#define MS_PER_SEC      1000
#define DEF_URL         "http://10.6.193.185:20001/64B"
#define OP_LEN          9   // "Options: "
#define SEQ_LEN         8
#define OPTION_LEN      17  // "Options: 00000000" that is, OP_LEN + SEQ_LEN
#define EPOLL_WAIT_TIME 1

using namespace std;

typedef unsigned int  UINT;
typedef unsigned long ULONG;
typedef unsigned long long DULONG;

typedef struct tagHttpRequest
{
    struct sockaddr_in addr;
    char host[HTTP_FILE_LEN];
    char file[HTTP_FILE_LEN];
}HttpReq;

typedef struct tagTestStat
{
    DULONG test_cnt;
    DULONG snd_err;
    DULONG rcv_err;

    vector<double> delay;
}TestStat;

typedef struct tagTestConfig
{
    UINT tps_min;
    UINT tps_max;
    UINT tps_step;

    UINT tps_cur;
    
    UINT sample_time;
    bool nodelay;
    bool et;
    UINT thd_num;       // thread number

}TestCfg;

typedef struct tagThreadContext
{
    pthread_t        tid;
    pthread_mutex_t  lock;
    bool             collect;
    char            *rcv_buf;
    int              sd;
    int              epfd;
    TestStat         stat;
    UINT             tps;
}TCntx;

typedef struct tagEpollEventSettings
{
    uint32_t ioEv;
    uint32_t iEv;
}EpEvSets;

/* -------------------------------------------- */

char logfile[MAX_FILE_LEN];
char progname[MAX_FILE_LEN];
bool stop = false;

pthread_mutex_t tmutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t tcond = PTHREAD_COND_INITIALIZER;

HttpReq g_httpReq;
TestCfg g_conf = {50, 1000, 50, 50, 10, false, false, 1};
double g_delayLevels[] = {0.01, 0.05, 0.1, 0.5, 1, 5, 10};

vector<TCntx *> g_thdCntx;

EpEvSets g_epEv;
/* -------------------------------------------- */

void get_basename(char *fullname)
{
    char *p = NULL;
    
    p = rindex(fullname, '/');
    if (NULL != p)
    {
        p++;
        snprintf(progname, (MAX_FILE_LEN - 4), "%s", p);
    }
    else
    {
        snprintf(progname, (MAX_FILE_LEN - 4), "%s", fullname);
    }
    
    snprintf(logfile, MAX_FILE_LEN, "%s.log", progname);
}

void usage()
{    
    printf("Usage: %s [OPTION] -u url\n", progname);
    printf("    -u url    Url to be tested.\n");
    printf("    -t time   Sampling period (in seconds), default value is 10s.\n");
    printf("    -b begin  Minimum of TPS(transation/request per second), default value is 50.\n");
    printf("    -s step   Change step of TPS, default value is 50.\n");
    printf("    -e end    Maximum of TPS, default value is 1000.\n");
    printf("    -z        No delay between requests.\n");
    printf("    -c concurrency\n");
    printf("              Number of threads to generate requests concurrently.\n");
    printf("    -h        Print this message and exit.\n");
    printf("    -g        Use epoll edge trigger mode (level trigger mode is default option).\n");
    printf("    -v        Print version information and exit.\n");
}

void version()
{
    printf("Low TPS Http Test, version 1.0.1\n");
    printf("    Release time: %s\n", "2012-10-22");
    printf("    Build time: %s\n\n", __DATE__);
    printf("Written by saintvliu, e-mail: saintvliu@tencent.com\n");
}

int get_addr_port(char *url)
{
    char *fp = NULL;
    char *hp = NULL;

    if ((strlen(url) > 7) && (strncmp(url, "http://", 7) == 0))
    {
        url += 7;
    }
    else
    {
        fprintf(stderr, "no \"http://\".\n");
        return -1;
    }

    if ((fp = strchr(url, '/')) == NULL)
    {
        fprintf(stderr, "no \"/\".\n");
        return -1;
    }
    
    snprintf(g_httpReq.file, HTTP_FILE_LEN, "%s", fp);
    *fp = '\0';
    snprintf(g_httpReq.host, HTTP_FILE_LEN, "%s", url);

    hp = strchr(url, ':');
    if (NULL == hp)
    {
        g_httpReq.addr.sin_port = htons(80);
    }
    else
    {
        int port = atoi(hp + 1);
        if ((1 > port) || (65535 < port))
        {
            fprintf(stderr, "wrong port number.\n");
            return -1;
        }

        g_httpReq.addr.sin_port = htons((unsigned short)port);
        *hp = '\0';
    }

    if (!isdigit(url[0]))
    {
        fprintf(stderr, "the host must in ipv4 format.\n");
        return -1;
    }

    g_httpReq.addr.sin_family = AF_INET;
    
    struct in_addr sin_addr;
    int ret = inet_pton(AF_INET, url, &sin_addr);
    if (ret < 0)
    {
        fprintf(stderr, "call inet_pton failed, ret:%d, errno:%d, host:%s.\n", ret, errno, url);
        return errno ? -errno : ret;
    }
    else if (ret == 0)
    {
        fprintf(stderr, "call inet_pton failed(wrong host format), host:%s.\n", url);
        return -1;
    }
    else
    {
        g_httpReq.addr.sin_addr.s_addr = sin_addr.s_addr;
        return 0;
    }
}

int parse_opt(int argc, char* argv[])
{
    int ret = 0;
    char cmd = '?';
    char url[MAX_FILE_LEN] = {0};
    bool get_url = false;
    
    while((cmd = getopt(argc, argv, "u:t:b:s:e:c:zkvhg")) != -1)
    {
        switch(cmd)
        {
            case 't':
            {
                int stime = atoi(optarg);
                if ((0 >= stime) || (MAX_SAMPLE_TIME < stime))
                {
                    fprintf(stderr, "Invalid argument. Sampling time must be greater than zero and less than %d.\n", MAX_SAMPLE_TIME);
                    return 1;
                }
                g_conf.sample_time = stime;
                break;
            }
            
            case 'c':
            {
                int num = atoi(optarg);
                if ((0 >= num) || (MAX_THREAD_NUM < num))
                {
                    fprintf(stderr, "Invalid argument. Thread number must be greater than zero and less than %d.\n", MAX_THREAD_NUM);
                    return 1;
                }
                g_conf.thd_num = num;
                break;
            }
            
            case 'b':
            {
                g_conf.tps_min = (UINT)atoi(optarg);
                if (g_conf.tps_min < 10)
                {
                    fprintf(stderr, "minimum tps must be not less than 10.\n");
                    ret = -1;
                }
                break;
            }
            
            case 'e':
            {
                g_conf.tps_max = (UINT)atoi(optarg);
                break;
            }
            
            case 's':
            {
                g_conf.tps_step = (UINT)atoi(optarg);
                break;
            }
            
            case 'z':
            {
                g_conf.nodelay = true;
                break;
            }
            
            case 'g':
            {
                g_conf.et = true;
                break;
            }
            
            case 'u':
            {
                get_url = true;
                snprintf(url, MAX_FILE_LEN, optarg);
                if (get_addr_port(url))
                {
                    fprintf(stderr, "cannot parse url.\n");
                    ret = -1;
                }
                break;
            }
            
            case 'v':
            {
                version();
                ret = -1;
                break;
            }
            
            case 'h':
            {
                usage();
                ret = -1;
                break;
            }
            
            default:
            {
                usage();
                ret = 1;
                break;
            }
        }
    }

    if (!get_url)
    {
        fprintf(stderr, "please specify url to be tested.\n");
        return 1;
    }
    
    if (0 == ret)
    {
        if (g_conf.tps_min > g_conf.tps_max)
        {
            fprintf(stderr, "wrong TPS range(%d, %d).\n", g_conf.tps_min, g_conf.tps_max);
            return 1;
        }
        
        if (g_conf.tps_step > (g_conf.tps_max - g_conf.tps_min))
        {
            fprintf(stderr, "wrong TPS adjust step %d.\n", g_conf.tps_step);
            return 1;
        }
    }

    return ret;
}


int get_conn(int &sd)
{
    if (-1 != sd)
    {
        return 0;
    }

    int ret = socket(PF_INET, SOCK_STREAM, 0);
    if (0 > ret)
    {
        fprintf(stderr, "call socket failed.\n");
        return errno ? -errno : ret;
    }
    sd = ret;
    
    /*
    ret = 1;
    if(tcp_nodelay)
    {
        setsockopt(sd, SOL_TCP, TCP_NODELAY, &ret, sizeof(ret));    
    }
    */
    if (connect(sd, (struct sockaddr *)&g_httpReq.addr, (socklen_t)sizeof(struct sockaddr_in)) )
    {
//        ret = errno;
//        fprintf(stderr, "connect failed: %s (%d)\n", strerror(ret), ret);
        fprintf(stderr, "connect failed.\n");
        close(sd);
        sd = -1;
        return -1;
    }
    
    ret = fcntl(sd, F_SETFL, O_RDWR | O_NONBLOCK);
    if (ret < 0)
    {
//        ret = errno;
//        fprintf(stderr, "set nonblock failed: %s (%d)\n", strerror(ret), ret);
        fprintf(stderr, "set nonblock failed.\n");
        close(sd);
        sd = -1;
        return -1;
    }
    ret = 1;
    setsockopt(sd, SOL_TCP, TCP_NODELAY, &ret, sizeof(ret));

    return 0;
}

void reconnect(int &epfd, int &sd)
{
    if (-1 != sd)
    {
        if (epoll_ctl(epfd, EPOLL_CTL_DEL, sd, NULL))
        {
            fprintf(stderr, "EPoll del fail!\n");
            return;
        }
        
        close(sd);
        sd = -1;
    }
    
    if (0 == get_conn(sd))
    {
        struct epoll_event evt;
        memset(&evt, 0, sizeof(struct epoll_event));
        evt.events = g_epEv.ioEv;
        if (epoll_ctl(epfd, EPOLL_CTL_ADD, sd, &evt))
        {
            fprintf(stderr, "EPoll Add fail!\n");
            close(sd);
            sd = -1;
        }
    }

    return;
}


void *worker_thread(void *para)
{
    TCntx *cntx = (TCntx *)para;
    if (NULL == cntx)
    {
        exit(1);
    }
    
    char    snd_buf[SND_BUF_SIZE] = {0};
    int     ret = 0;
    int     snd_len = 0, rcv_len = 0, left_len, snd_err, rcv_err, events;
    bool    complete = false;
    DULONG  proc_time = 0;
    ULONG   slot = (ULONG)US_PER_MSEC, tps = cntx->tps, per = 0, tdiff = 0, tptr = 0;
    UINT    seq = 0, rcv_seq = 0;
    long    left = 0;
    
    struct epoll_event evt_arr[1];
    int   &epfd = cntx->epfd;
    int   &sd   = cntx->sd;
    char  *rcv_buf = cntx->rcv_buf;
    char  *op = NULL;
    char  *bufptr = NULL;
    char  *rcv_op = NULL;
    char   op_buf[SEQ_LEN + 1];

    UINT   seed = time(NULL) + rand();

    bzero(op_buf, (SEQ_LEN + 1));

    map<UINT, struct timeval> timemap;
    map<UINT, struct timeval>::iterator it;
    
    struct timeval start, end, last, slot_start, slot_end;

    snd_len = snprintf(snd_buf, SND_BUF_SIZE, "GET %s HTTP/1.1\r\nConnection: keep-alive\r\nHost: %s\r\nOptions: 00000000\r\n\r\n", g_httpReq.file, g_httpReq.host);
    op = strstr(snd_buf, "Options: 00000000");
    assert((op > snd_buf) && ((op + OP_LEN) < snd_buf + snd_len));
    op += OP_LEN;

    printf("thread 0x%08x waiting...\n", (UINT)pthread_self());
    
    pthread_mutex_lock(&tmutex);
    pthread_cond_wait(&tcond, &tmutex);
    pthread_mutex_unlock(&tmutex);
    
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
    
    printf("thread 0x%08x start to work.\n", (UINT)pthread_self());
    
    gettimeofday(&slot_start, NULL);
    while(1)
    {
        complete = false;
        snd_err = rcv_err = 0;
        
        pthread_testcancel();
        
        if (!g_conf.nodelay)
        {
            // calculate send frequency
            if (tps <= 100)
            {
                slot = 10;
            }
            else if (tps < 500)
            {
                slot = 50;
            }
            else
            {
                slot = 100;
            }
            per  = tps/slot;
            
            gettimeofday(&slot_end, NULL);
            tdiff = (slot_end.tv_sec - slot_start.tv_sec) * 1000 + (slot_end.tv_usec - slot_start.tv_usec) / 1000;
            if (tdiff >= MS_PER_SEC)
            {
                gettimeofday(&slot_start, NULL);
                tptr = 0;
            }
            
            if (tdiff >= tptr)
            {
                tptr += MS_PER_SEC/slot;
                left  = (rand_r(&seed) % (per << 1)) + 1;
                
                // reenable EPOLLOUT
                epoll_event ev;
                ev.data.fd = sd;
                ev.events = g_epEv.ioEv;
                epoll_ctl(epfd, EPOLL_CTL_MOD, sd, &ev);
//                fprintf(stderr, "tdiff: %ld, tptr: %ld, left: %ld\n", tdiff, tptr, left);
            }
        }
        
        ret = epoll_wait(epfd, evt_arr, 1, EPOLL_WAIT_TIME);
        if (0 == ret)
        {
            continue;
        }
        else if (1 != ret)
        {
            fprintf(stderr, "EPoll wait fail!\n");
            reconnect(epfd, sd);
            continue;
        }
        
        events = evt_arr[0].events;
        if (!(events & (EPOLLIN | EPOLLOUT)))
        {
            reconnect(epfd, sd);
            continue;
        }

        if ( events & EPOLLOUT )
        {
            if (left >= 0)
            {
                gettimeofday(&start, NULL);
                sprintf(op, "%08x", ++seq);
                *(op + SEQ_LEN) = '\r';
//                ret = send(sd, snd_buf, snd_len, 0);
                ret = send(sd, snd_buf, snd_len, MSG_NOSIGNAL | MSG_DONTWAIT);
                if (ret != snd_len)
                {
                    snd_err++;
                }
                else
                {
                    timemap[seq] = start;
                }
            }
            
            if (!g_conf.nodelay)
            {
                if (left <= 0)
                {
                    // if all is sent, then don't wait EPOLLOUT
                    epoll_event ev;
                    ev.data.fd = sd;
                    ev.events = g_epEv.iEv;
                    epoll_ctl(epfd, EPOLL_CTL_MOD, sd, &ev);
//                    fprintf(stderr, "--tdiff: %ld, tptr: %ld, left: %ld\n", tdiff, tptr, left);
                }
                else
                {
                    left--;
                }
            }
            else if ((ret > 0) && (g_conf.et))
            {
                // if all is sent, then don't wait EPOLLOUT
                epoll_event ev;
                ev.data.fd = sd;
                ev.events = g_epEv.ioEv;
                epoll_ctl(epfd, EPOLL_CTL_MOD, sd, &ev);
            }
        }

        if ( events & EPOLLIN )
        {
//            ret = recv(sd, rcv_buf, RCV_BUF_SIZE, 0);
            ret = recv(sd, rcv_buf, RCV_BUF_SIZE, MSG_DONTWAIT);
            if (ret > 0)
            {
                if (0 == rcv_len)
                {
                    rcv_len = ret;
                }

                if (rcv_len <= ret)
                {
                    left_len = ret;
                    bufptr = rcv_buf;
                    while(left_len >= rcv_len)
                    {
                        gettimeofday(&end, NULL);
                        rcv_op = strstr(bufptr, "Options: ");
                        if ((rcv_op > bufptr) && (rcv_op + OPTION_LEN) < (bufptr + rcv_len))
                        {
                            sscanf(rcv_op + OP_LEN, "%08x", &rcv_seq);
                            it = timemap.find(rcv_seq);
                            if (it != timemap.end())
                            {
                                complete = true;
                                last = timemap[rcv_seq];
                                proc_time = (end.tv_sec - last.tv_sec) * US_PER_SEC + (end.tv_usec - last.tv_usec);
                                timemap.erase(it);
                            }
                        }
                        left_len-=rcv_len;
                        bufptr += rcv_len;
                    }
                }
                else
                {
                    rcv_err++;
//                    fprintf(stderr, "recv len: %d\n", ret);
                }
            }
            else
            {
//                ret = errno;
//                fprintf(stderr, "recv failed: %s (%d)\n", strerror(ret), ret);
                rcv_err++;
            }
        }

        pthread_mutex_lock(&cntx->lock);
        if (cntx->collect)
        {
            cntx->stat.test_cnt++;
            if (complete)
            {
                cntx->stat.delay.push_back(proc_time/US_PER_MSEC);
            }
            else
            {
                cntx->stat.snd_err  += snd_err;
                cntx->stat.rcv_err  += rcv_err;
            }
        }
        tps = cntx->tps;
        pthread_mutex_unlock(&cntx->lock);
    }
}

TCntx* create_thread_context()
{
    TCntx *cntx = NULL;
    char  *buf  = NULL;
    struct epoll_event evt;
    int    epfd, sd   = -1;
    

    cntx = (TCntx *)malloc(sizeof(TCntx));
    if (NULL == cntx)
    {
        fprintf(stderr, "allocate thread context failed.\n");
        return NULL;
    }

    buf = (char *)malloc(RCV_BUF_SIZE * sizeof(char));
    if (NULL == buf)
    {
        fprintf(stderr, "allocate recv buffer failed.\n");
        free(cntx);
        return NULL;
    }

    if ((epfd = epoll_create(1)) == -1)
    {
        fprintf(stderr, "create EPoll fd fail!\n");
        free(cntx);
        free(buf);
        return NULL;
    }

    if (get_conn(sd))
    {
        free(cntx);
        free(buf);
        close(epfd);
        return NULL;
    }

    memset(&evt, 0, sizeof(struct epoll_event));
    evt.events = g_epEv.ioEv;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, sd, &evt))
    {
        fprintf(stderr, "EPoll Add fail!\n");
        free(cntx);
        free(buf);
        close(epfd);
        close(sd);
        return NULL;
    }

    pthread_mutex_init(&cntx->lock, NULL);

    bzero(&cntx->stat, sizeof(TestStat));
    cntx->stat.delay.clear();
    cntx->rcv_buf = buf;
    cntx->sd      = sd;
    cntx->tid     = (pthread_t)-1;
    cntx->collect = true;
    cntx->tps     = g_conf.tps_cur;
    cntx->epfd    = epfd;

    return cntx;
}

inline void delete_thread_context(TCntx *cntx)
{
    if ((pthread_t)-1 != cntx->tid)
    {
        pthread_cancel(cntx->tid);
        
        pthread_join(cntx->tid, NULL);
    }

    pthread_mutex_destroy(&cntx->lock);

    cntx->stat.delay.clear();
    
    if (-1 != cntx->sd)
    {
        if (epoll_ctl(cntx->epfd, EPOLL_CTL_DEL, cntx->sd, NULL) )
        {
            fprintf(stderr, "EPoll del fail!\n");
        }
        close(cntx->sd);
    }
    
    close(cntx->epfd);
    
    if (NULL != cntx->rcv_buf)
    {
        free(cntx->rcv_buf);
    }
}

int init_thread()
{
    UINT      i    = 1;
    TCntx    *cntx = NULL;
    cpu_set_t cpu_mask;
    
    pthread_mutex_init(&tmutex, NULL);
    pthread_cond_init(&tcond, NULL);
        
    long cpu_nr = sysconf(_SC_NPROCESSORS_CONF);
    if (-1 == cpu_nr)
    {
        fprintf(stderr, "Get count of cpu cores fail!\n");
    }
    
    for (; i <= g_conf.thd_num; i++)
    {
        cntx = create_thread_context();
        if (NULL == cntx)
        {
            return 1;
        }
        g_thdCntx.push_back(cntx);
        
        if (pthread_create(&cntx->tid, NULL, worker_thread, (void *)cntx))
        {
            fprintf(stderr, "cannot create working thread.\n");
            return 1;
        }

        if (1 < cpu_nr)
        {
            CPU_ZERO(&cpu_mask);
            CPU_SET((i % cpu_nr), &cpu_mask);
            if (pthread_setaffinity_np(cntx->tid, sizeof(cpu_set_t), &cpu_mask))
            {
                fprintf(stderr, "bind cpu failed.\n");
            }
        }
    }

    // wait for all threads
    sleep(1);
    pthread_mutex_lock(&tmutex);
    pthread_cond_broadcast(&tcond);
    pthread_mutex_unlock(&tmutex);

    return 0;
}

void clean_up()
{
    int thread_num = g_thdCntx.size(), i = 0;
    for (; i < thread_num; i++)
    {
        delete_thread_context(g_thdCntx[i]);
    }

    pthread_cond_destroy(&tcond);
    pthread_mutex_destroy(&tmutex);
}

void sig_handler(int signo)
{
    int i = 0, thread_num = g_thdCntx.size();
    TCntx *pcntx = NULL;

    printf("get signal\n");

    for (;i < thread_num; i++)
    {
        pcntx = g_thdCntx[i];
        if ((pthread_t)-1 != pcntx->tid)
        {
            pthread_cancel(pcntx->tid);
        }
    }
    stop = true;
}

template<typename T>
static T get_max(const vector<T>& data)
{
    typename vector<T>::const_iterator it, end;
    it  = data.begin();
    end = data.end();

    T max = *it;
    for ( ; it != end; it++ )
    {
        if (max < *it)
        {
            max = *it;
        }
    }

    return max;
}

void stat_and_writelog(fstream &fd, double test_time)
{
    static int level_size = sizeof(g_delayLevels)/sizeof(double);
    int i = 0, j, thread_num = g_thdCntx.size();
    double      totalTime = 0, t = 0;
    vector<UINT> levelNum(level_size + 1, 0);
    TestStat   *pstat   = NULL;
    UINT        dataLen = 0;
    vector<double>::iterator it, end;
    stringstream ss(stringstream::out);
    double      total_tps = 0;
    double      tpr       = 0;

    for (i = 0; i < thread_num; i++)
    {
        pstat = &g_thdCntx[i]->stat;
        dataLen = pstat->delay.size();
        
        ss << setw(10) << pstat->test_cnt
           << setw(10) << pstat->snd_err
           << setw(10) << pstat->rcv_err 
           << " | " << setw(10) << dataLen;
        
        if (dataLen <= 0)
        {
            ss << " ( Warning: thread 0x"
               << hex << setfill('0') << setw(8) << g_thdCntx[i]->tid
               << dec << setfill(' ') << " has no test record at this round. ) " << endl;
            continue;
        }

        totalTime = 0;
        levelNum.assign(level_size + 1, 0);
        it = pstat->delay.begin(), end = pstat->delay.end();
        for ( ; it != end; it++)
        {
            t = *it;
            totalTime += t;

            for (j = 0; j < level_size; j++)
            {
                if (t < g_delayLevels[j])
                {
                    levelNum[j]++;
                    break;
                }
            }
            
            if (t > g_delayLevels[level_size - 1])
            {
                levelNum[level_size]++;
            }
        }

        total_tps += (double)dataLen/totalTime;
        tpr       += (double)totalTime/dataLen;

        ss << fixed << setprecision(3) << setw(10) << (double)totalTime/dataLen;
        ss << fixed << setprecision(3) << setw(10) << get_max(pstat->delay) << " | ";
        for (j = 0; j <= level_size; j++)
        {
            ss << setw(10) << levelNum[j];
        }
        ss << setw(10) << totalTime/1000 << endl;
    }
    
    // print header
    string ft(180, '-');
    if (g_conf.nodelay)
    {
        fd << "+ In zero-delay mode, test time: " << setprecision(3) << test_time << endl;
    }
    else
    {
        fd << "+ current TPS per thread: " << g_conf.tps_cur
           << ", test time: " << setprecision(3) << test_time << endl;
    }
    fd << "+ request per second: " << fixed << setprecision(3) << total_tps * 1000 << endl;
    fd << "+ time per request:   " << setprecision(3) << tpr/thread_num << " ms, (across all concurrent requests: " << tpr/thread_num/thread_num << " ms)" << endl;
    
    fd << "  test_num  send_err  recv_err |    req_num   avg(ms)   max(ms) | ";
    for (i = 0; i < level_size; i++)
    {
        stringstream stmp;
        stmp << "<" << g_delayLevels[i] << "ms";
        fd << setw(10) << stmp.str();
    }

    stringstream stmp;
    stmp << ">" << g_delayLevels[i-1] << "ms";
    fd << setw(10) << stmp.str() << "  total_time(s)" << endl << ft << endl;
    
    fd << ss.str() << endl;

    fd.flush();
}

void print_conf()
{
    printf("test url:    %s%s\n", g_httpReq.host, g_httpReq.file);
    printf("thread num:  %d\n",   g_conf.thd_num);
    if (g_conf.nodelay)
    {
        printf("zero-delay:  yes\n");
        if (0 == g_conf.tps_step)
        {
            printf("sample num:  1\n");
        }
        else
        {
            printf("sample num:  %d\n", ((g_conf.tps_max-g_conf.tps_min + g_conf.tps_step)/g_conf.tps_step));
        }
    }
    else
    {
        printf("zero-delay:  no\n");
        printf("tps min:     %d\n",   g_conf.tps_min);
        printf("tps max:     %d\n",   g_conf.tps_max);
        printf("tps step:    %d\n",   g_conf.tps_step);
    }
    printf("sample time: %d\n",   g_conf.sample_time);
    printf("epoll mode:  %d\n",   g_conf.et ? "ET":"LT");
    printf("\n");
}

void init_env()
{
    struct sigaction sa; 
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sig_handler;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);
    sigaction(SIGUSR2, &sa, NULL);
    signal(SIGPIPE, SIG_IGN);

    g_conf.tps_cur = g_conf.tps_min;
}

inline void disable_collect()
{
    int i = 0, thread_num = g_thdCntx.size();
    TCntx *pcntx = NULL;

    for (;i < thread_num; i++)
    {
        pcntx = g_thdCntx[i];
        pthread_mutex_lock(&pcntx->lock);
        pcntx->collect = false;
        pthread_mutex_unlock(&pcntx->lock);
    }
}

inline void reinit_collect()
{
    int i = 0, thread_num = g_thdCntx.size();
    TCntx    *pcntx = NULL;
    TestStat *pstat = NULL;

    for (;i < thread_num; i++)
    {
        pcntx = g_thdCntx[i];
        pthread_mutex_lock(&pcntx->lock);
        pcntx->collect = true;
        pstat = &pcntx->stat;
        pstat->delay.clear();
        pstat->test_cnt = pstat->snd_err = pstat->rcv_err = 0;
        pcntx->tps = g_conf.tps_cur;
        pthread_mutex_unlock(&pcntx->lock);
    }
}

int main(int argc, char* argv[])
{
    int ret = 0;
    
    bzero(&g_httpReq, sizeof(HttpReq));
    get_basename(argv[0]);
    if (argc < 2)
    {
        usage();
        return 1;
    }

    ret = parse_opt(argc, argv);
    if (ret < 0)
    {
        return 0;
    }
    else if (ret > 0)
    {
        return ret;
    }
    else
    {
        // do nothing
    }

    if (g_conf.et)
    {
        g_epEv.iEv  = EPOLLIN | EPOLLERR | EPOLLET;
        g_epEv.ioEv = EPOLLOUT | EPOLLIN | EPOLLERR | EPOLLET;
    }
    else
    {
        g_epEv.iEv  = EPOLLIN | EPOLLERR;
        g_epEv.ioEv = EPOLLOUT | EPOLLIN | EPOLLERR;
    }
    
    if (g_httpReq.host[0] == 0)
    {
        char url[256] = DEF_URL;
        if (get_addr_port(url))
        {
            fprintf(stderr, "cannot parse url.\n");
            return 1;
        }
    }

    print_conf();

    init_env();

    fstream fd;
    fd.open(logfile, fstream::out | fstream::app);
    if (!fd.is_open())
    {
        fprintf(stderr, "cannot open log file.\n");
        return 1;
    }

    if (init_thread())
    {
        fprintf(stderr, "init thread failed.");
        clean_up();
        fd.close();
        return 1;
    }

    printf("start testing...\n");
    time_t t;
    char tnow[30] = {0};
    t = time(NULL);
    strftime(tnow, 30, "%H:%M:%S, %a %b %d %Y", localtime(&t));
    fd << endl << endl << "*------ Test start at " << tnow << " ------*" << endl;

    struct timeval end, now;
    while(!stop)
    {
        // delay for a sample period
        gettimeofday(&end, NULL);
        end.tv_sec += g_conf.sample_time;
        do
        {
            usleep(200000);
            gettimeofday(&now, NULL);
        }
        while ((!stop) && (now.tv_sec < end.tv_sec));

        disable_collect();
        
        stat_and_writelog(fd, ((now.tv_sec - end.tv_sec + g_conf.sample_time) + (double)(now.tv_usec - end.tv_usec)/US_PER_SEC));

        g_conf.tps_cur += g_conf.tps_step;
        if ((0 == g_conf.tps_step) || (g_conf.tps_cur > g_conf.tps_max))
        {
            stop = true;
        }
        else
        {
            reinit_collect();
        }
    }
    printf("stop testing...\n");
    
    clean_up();
    t = time(NULL);
    strftime(tnow, 30, "%H:%M:%S, %a %b %d %Y", localtime(&t));
    fd << "*------ Test end at " << tnow << " ------*" << endl;
    fd.close();

    return 0;
}

