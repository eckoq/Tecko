#include <arpa/inet.h>
#include <string.h>

extern "C"
{
int asn13_net_complete_func(void* pData, unsigned unDataLen);

int net_complete_func(void* pData, unsigned unDataLen)
{
    return asn13_net_complete_func(pData, unDataLen);
}

}
