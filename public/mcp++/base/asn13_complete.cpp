#include <arpa/inet.h>
#include <string.h>

extern "C"
{

int asn13_net_complete_func(void* pData, unsigned unDataLen)
{
	int iPkgTheoryLen;

         unsigned data_len = unDataLen;
     char *_data = (char *)pData;

     const unsigned MAX_LEN = 1024*1024*128;
     unsigned head_len = 0;
     unsigned cont_len = 0;
     unsigned pkt_len;

     iPkgTheoryLen = 0;
     if (data_len <= 0)
     {
         return 0;
     }

     // first char must be '0'
     //printf("data[0]: %d\n", (int)_data[0]);
     if (_data[0] != '0')
     {
         //printf("_data[0] error\n");
         return -1;
     }

     if (data_len < 4)
     {
         return 0;
     }

     // get head length & content length

     //printf("data[1]: %d\n", (int)_data[1]);

     if ((unsigned)_data[1] > 128)
     {
         unsigned len_len = (unsigned)(_data[1]&127);
         if (len_len > 4)
         {
             return -1;
         }

         char *addr_dest = (char*)(&cont_len)+len_len-1;
         char *addr_src = _data+2;
         head_len = 2+len_len;

         // Only for little-endian CPU

         while (addr_dest >= (char*)(&cont_len))
         {
               memcpy(addr_dest--, addr_src++, 1);
         }

         //printf("BIG cont_len: %u\n", cont_len);
     }
     else
     {
         head_len = 2;
         cont_len = (unsigned)_data[1];
         //printf("nor cont_len: %u\n", cont_len);
     }

     pkt_len = head_len+cont_len;
     iPkgTheoryLen = pkt_len;

     //printf("pkt len: %u\n", pkt_len);

     // pkt too long!

     if (MAX_LEN < pkt_len)
     {
         //printf("pkt_len > MAX_LEN\n");
         return -1;
     }

     // a full pkt?

     if (data_len > pkt_len) // run over
     {
         if (_data[pkt_len] != '0')
         {
               return -1;
         }

         return pkt_len;
     }
     else if (data_len == pkt_len) // full
     {
         return pkt_len;
     }

     else // not full
     {
         return 0;
     }
}

}
