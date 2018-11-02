#include <arpa/inet.h>
#include <string.h>

extern "C"
{

int asn20_net_complete_func(void* pData, unsigned unDataLen);

int net_complete_func(void* pData, unsigned unDataLen)
{
	return asn20_net_complete_func(pData, unDataLen);
}

}
