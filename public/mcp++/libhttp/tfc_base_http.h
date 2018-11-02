/*
 * tfc_base_http.h:		HTTP lib for MCP.
 * Date:					2011-03-04
 */

#ifndef __TFC_BASE_HTTP_H
#define __TFC_BASE_HTTP_H

#include <stdlib.h>
#include <limits.h>

#include "http_api.h"

extern "C" {

/*
 * HTTP_CHECK_COMPLETE():	HTTP pakcet check.
 * @data:					Packet data.
 * @data_len:					Length of data.
 * Returns:					Return first full packet length in bytes.
 *							0 on not complete.
 *							-1 on error.
 * NOTES:					Unified macro to ensure a same HTTP complete check in MCP both in cpp and .so extern C.
 */
#ifdef HTTP_LIMIT_LENGTH
#define HTTP_CHECK_COMPLETE(data, data_len, retv) ( (retv = http_check_complete(data, data_len, NULL, NULL)) == -2 ?	\
		( retv = ( data_len > HTTP_MAX_LENGTH ? -1 : data_len ) ) : retv )
#else
#define HTTP_CHECK_COMPLETE(data, data_len, retv) ( (retv = http_check_complete(data, data_len, NULL, NULL)) == -2 ?	\
		(retv = data_len) : retv )
#endif

namespace tfc {
	namespace http {
		// Utilities.
		int HttpCheckComplete(void *data, unsigned data_len);		// Packet completation check for MCP.

		// Hot HTTP headers.
		class CHttpHot {
		public:
			CHttpHot();		// Construction.
			~CHttpHot() {}	// Destruction.

			// Hot headers.
			long				content_length;				// HTTP Content-Length.
			int					connection;					// HTTP Connection. 0 Close, 1 Keep-Alive.

			char				*host;						// HTTP Host.
			char				*referer;					// HTTP Referer.
			char				*user_agent;				// HTTP User-Agent.
			char				*cookie;					// HTTP Cookie.
			char				*accept_encoding;			// HTTP Accept-Encoding.

			char				*server;					// HTTP Server.
			char				*content_encoding;			// HTTP Content-Encoding.
			char				*transfer_encoding;			// HTTP Transfer-Encoding.
			char				*location;					// HTTP Location.
		};
		
		// class for HTTP parse.
		class CHttpParse {
		public:
			CHttpParse();						// Construction.
			~CHttpParse();						// Destruction.

			bool		IsInited();					// Wether object has been initialized.

			int		 	Init(void *data, unsigned data_len);		// Initialize. Call this first befor call function belows.
			const char	*GetValue(const char *name);			// Get HTTP header value.

			int			HeadLength();			// HTTP head length.
			int			BodyLength();			// HTTP body length. May equal but not absolute equal Content-Length.
			int			PkgLength();				// First HTTP packet length in data buffer given by caller when call init().

			// Get start line information.
			int			HttpMethod();			// HTTP method in request packet start line.
			const char	*HttpVersion();			// HTTP protol version in start line.
			const char	*HttpUri();				// HTTP request URI in start line.
			int			HttpCode();				// HTTP return code in response pakcet start line.
			const char	*HttpReason();			// HTTP Reason Phrase in response packet start line.

			// Get hot header items.
			long		HttpContentLength();		// HTTP Content-Length.
			int			HttpConnection();			// HTTP Connection.
			const char	*HttpHost();					// HTTP Host.
			const char	*HttpReferer();				// HTTP Referer.
			const char	*HttpUserAgent();			// HTTP User-Agent.
			const char	*HttpCookie();				// HTTP Cookie.
			const char	*HttpAcceptEncoding();	// HTTP Accept-Encoding.
			const char	*HttpServer();				// HTTP Server.
			const char	*HttpContentEncoding();	// HTTP Content-Encoding.
			const char	*HttpTransferEncoding();	// HTTP Transfer-Encoding.
			const char	*HttpLocation();				// HTTP Location.
			
		private:
			bool					__inited;			// Wether been initialized.
			bool					__parsed;			// GetValue has been call once at least.
			long					__pkg_len;			// First HTTP packet length in data buffer given pass to Init().
			char					*__buf;				// Buffer for HTTP head.
			int						__len;				// Buffer length.
			http_basic_t			__basic;			// HTTP basic information.
			http_item_table_t		__headers;			// HTTP header items.
			CHttpHot				__hot;				// HTTP host items.
		};

		// HTTP message template.
		class CHttpTemplate {
		public:
			CHttpTemplate();				// Constructor.
			~CHttpTemplate();				// Destructor.

			bool		IsInited();				// Wether been initialized.

			int			Init(const char *msg_template, int msg_len,
							http_template_arg_t *args, int arg_cnt);	// Initialize the Object.
			int			Produce(char *buf, int buf_len,
						char **args, int arg_cnt);						// Produce a HTTP message by the template.
			int			ProduceRef(char **r_data, int *r_data_len,
						char **args, int arg_cnt);						// Produce a HTTP message by the template but only return refer to data.
			
		private:
			bool					__inited;			// Wether been initialized.
			http_template_t			__entry;			// HTTP message template entry.
		};
	}
}

}

#endif

