#include <arpa/inet.h>
#include <string.h>

extern "C"
{

int asn20_net_complete_func(void* pData, unsigned unDataLen)
{
        if (unDataLen < sizeof(int)*2)
                return 0;

        int iMsgTag = ntohl(*(int*)pData);
        if (iMsgTag != 0x4E534153)      //SASN ,equ(=) fast than memcmp
        {
                return -1;
        }

        int iMsgLen = ntohl(*((int*)pData+1));

        if (iMsgLen <= (int)unDataLen)
        {
                return iMsgLen;
        }

        return 0;
}

}
