#include <string.h>
#include <stdlib.h>
#include <stdio.h>
//#include "tfc_base_http.h"

extern "C"
{
	//simple http parse func
	int net_complete_func(const void* data, unsigned data_len)
	{

//		int retv;
//		return HTTP_CHECK_COMPLETE((char*)data, data_len, retv);
	

		if(data_len < 14)
			return 0;

		char *p, *q;
		if((p = strstr((char*)data, "\r\n\r\n")) != NULL) {
			if((q = strstr((char*)data, "Content-Length:")) == NULL) {	//no http body
				return p - (char*)data + 4;
			}	
			else {														//has http body
				unsigned pkglen = p - (char*)data + 4 + atoi(q + 15);
				if(data_len >= pkglen)
					return pkglen;
				else
					return 0;	
			}
		}	
		else if(data_len > 1024 * 1024 * 50)
			return -1;
		else
			return 0;			

	}
	unsigned mcd_pre_route_func(unsigned ip, unsigned short port, unsigned short listen_port, unsigned long long flow, unsigned req_num)
	{
		// unsigned char* ip_str = (unsigned char*)&ip;
		// printf("%d.%d.%d.%d:%d, listen_port=%d, flow=%llu, req_num=%u\n", ip_str[0], ip_str[1], ip_str[2], ip_str[3], 
		//	port, listen_port, flow, req_num);
		
		return listen_port % req_num;
	}
}
