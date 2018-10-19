/*
 * http_complete.cpp:		HTTP complete check for MCP ccd and dcc.
 * Date:					2011-03-07
 */

#include "tfc_base_http.h"

extern "C" {
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
		int retv;

		return HTTP_CHECK_COMPLETE(data, data_len, retv);
	}
}

