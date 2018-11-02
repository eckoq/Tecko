/*
 * poppy_complete.cpp:	Poopy complete check.
 * Date:					2012-06-28
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

#include "http_api.h"

extern "C" {
	int
	sync_request_func(void *data, unsigned data_len, void **user)
	{
		return http_check_complete(data, data_len, NULL, NULL);
	}

	int
	sync_response_func(char *outbuff, unsigned buff_max, void *user)
	{
		static const unsigned RSPBUF_LEN = 8192;
		static char rspbuf[RSPBUF_LEN];
		char *pos = NULL;
		int len = 0;

		pos = rspbuf;

		len += snprintf(pos, RSPBUF_LEN - len, "HTTP/1.1 200 OK\r\n");
		pos = rspbuf + len;

		len += snprintf(pos, RSPBUF_LEN - len, "\r\n");

		if ( (unsigned)len > buff_max ) {
			return -1;
		}

		memcpy(outbuff, rspbuf, len);

		return len;
	}
	
	/*
	 * net_complete_func():		Packet completation check for ccd and dcc.
	 * @data:					Packet data.
	 * @data_len:					Length of data.
	 * Returns:					Return first full packet length in bytes.
	 *							0 on not complete.
	 *							-1 on error.
	 */
	int
	net_complete_func(void *data, unsigned data_len)
	{
		static unsigned pkt_max = (1<<30);
		unsigned pkt_len, meta_len, sub_data_len;
		
		if ( data_len < (sizeof(unsigned) + sizeof(unsigned)) ) {
			return 0;
		}

		meta_len = ntohl(*((unsigned*)data));
		sub_data_len = ntohl(*(((unsigned*)data) + 1));

		pkt_len = meta_len + sub_data_len + sizeof(unsigned) + sizeof(unsigned);

		if ( pkt_len > pkt_max ) {
			return -1;
		}

		if ( data_len < pkt_len ) {
			return 0;
		}

		return pkt_len;
	}
}

