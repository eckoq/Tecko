#include <iostream>
#include <assert.h>
using namespace std;

extern "C"
{
int net_complete_func(const void* data, unsigned data_len)
{
	char *_data = (char *)data;
	const unsigned MAX_LEN = 1024*1024*128;

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
	unsigned head_len = 0;
	unsigned cont_len = 0;
	//printf("data[1]: %d\n", (int)_data[1]);
	if ((unsigned)_data[1] > 128)
	{
	    unsigned len_len = (unsigned)(_data[1]&127);
	    head_len = 2+len_len;
	    char *addr_dest = (char*)(&cont_len)+len_len-1;
	    char *addr_src = _data+2;
	    // Only for little-endian CPU 
//		unsigned i = 0;
	    while (addr_dest >= (char*)(&cont_len))
	    {
			memcpy(addr_dest--, addr_src++, 1);
/*			i++; //delexxie
			if(i>len_len) return -1;
*/	    }
	    //printf("BIG cont_len: %u\n", cont_len);
	}
	else
	{
	    head_len = 2;
	    cont_len = (unsigned)_data[1];
	    //printf("nor cont_len: %u\n", cont_len);
	}

	unsigned pkt_len = head_len+cont_len;
	//printf("pkt len: %u\n", pkt_len);
	// pkt too long!
	if (MAX_LEN < pkt_len)
	{
	    printf("pkt_len > MAX_LEN\n");
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
