#include <stdio.h>
#include <unistd.h>
#include <string>
#include "tfc_net_open_mq.h"

using namespace std;
using namespace tfc::net;
int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("usage: get_mq_stat conf_file\n");
        return 0;
    }

    int ret = access(argv[1], R_OK);
    if (ret != 0)
    {
        fprintf(stderr, "access file %s error: %m\n", argv[1]);
        return 1;
    }

    string file_name = argv[1];
    CFifoSyncMQ* mq = NULL;
    try
    {
        mq = GetMQ(file_name);
    }
    catch(...)
    {
        fprintf(stderr, "open mq failed %m\n");
        return 1;
    }

    CFifoSyncMQ::TFifoSyncMQStat mq_stat;
    mq->get_stat(mq_stat);
    printf("fd:     %d\n",mq_stat._fd);
    printf("wlock:  %d\n",mq_stat._semlockmq_stat._wlock);
    printf("rlock:  %d\n",mq_stat._semlockmq_stat._rlock);
    printf("key:    %d\n",mq_stat._semlockmq_stat._mq_stat._shm_key);
    printf("shm id: %d\n",mq_stat._semlockmq_stat._mq_stat._shm_id);
    printf("size:   %d\n",mq_stat._semlockmq_stat._mq_stat._shm_size);
    printf("total len:   %d\n",mq_stat._semlockmq_stat._mq_stat._total_len);
    printf("free len:    %d\n",mq_stat._semlockmq_stat._mq_stat._free_len);
    printf("used len:    %d\n",mq_stat._semlockmq_stat._mq_stat._used_len);
    return 0;
}
