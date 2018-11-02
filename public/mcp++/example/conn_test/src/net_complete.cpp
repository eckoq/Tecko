#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define MSG_TAG 0xA1B2C3D4

typedef struct tagPktHeader
{
    unsigned msg_tag;
    unsigned echo_data;
    unsigned pkt_seq;
    unsigned data_len;
}PktHdr;

extern "C"
{
    //simple http parse func
    int net_complete_func(const void* data, unsigned data_len)
    {
        if (data_len < sizeof(PktHdr))
        {
            return 0;
        }
        PktHdr* hdr = (PktHdr*)data;
        if (hdr->msg_tag != MSG_TAG)
        {
            return -1;
        }

        unsigned pkt_len = sizeof(PktHdr) + hdr->data_len;
        if (data_len < pkt_len)
        {
            return 0;
        }

        return pkt_len;
    }

    unsigned mcd_pre_route_func(unsigned ip, unsigned short port, unsigned short listen_port, unsigned long long flow, unsigned req_num)
    {
        return listen_port % req_num;
    }
}

int main(void)
{
    return 0;
}
