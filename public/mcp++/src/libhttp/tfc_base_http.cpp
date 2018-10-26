/*
 * tfc_base_http.cpp:		HTTP lib for MCP.
 * Date:					2011-03-04
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include "http_api.h"
#include "tfc_base_http.h"

using namespace tfc::http;

////////////////////////////////////////////////////////////////
// Utilities.
////////////////////////////////////////////////////////////////

/*
 * HttpCheckComplete():		Packet completation check for MCP.
 * @data:					Packet data.
 * @data_len:					Length of data.
 * Returns:					Return first full packet length in bytes.
 *							0 on not complete.
 *							-1 on error.
 */
int
tfc::http::HttpCheckComplete(void *data, unsigned data_len)
{
	int		retv;
	
	return HTTP_CHECK_COMPLETE(data, data_len, retv);
}

////////////////////////////////////////////////////////////////
// Class CHttpHot
////////////////////////////////////////////////////////////////
/*
 * CHttpHot():		Constructor
 */
CHttpHot::CHttpHot()
{
	content_length = HTTP_CONTENT_L_ERROR;
	connection = -1;

	host = NULL;
	referer = NULL;
	user_agent = NULL;
	cookie = NULL;
	accept_encoding = NULL;

	server = NULL;
	content_encoding = NULL;
	transfer_encoding = NULL;
	location = NULL;
}

////////////////////////////////////////////////////////////////
// Class CHttpParse
////////////////////////////////////////////////////////////////

/*
 * CHttpParse():		Constructor.
 */
CHttpParse::CHttpParse()
{
	__inited = false;
	__parsed = false;
	__pkg_len = 0;
	__buf = NULL;
	__len = 0;
}

/*
 * ~CHttpParse():		Destructor. Clean up resource.
 */
CHttpParse::~CHttpParse()
{
	__inited = false;
	__parsed = false;

	if ( __buf ) {
		free(__buf);
		__buf = NULL;
	}
}

/*
 * IsInited():			Wether object has been initialized.
 * Returns:			true on initialized or false on not initialized.
 */
bool
CHttpParse::IsInited()
{
	return __inited;
}

/*
 * Init():				Initialize class CHttpParse. Call this first befor get Header value.
 * @data:			HTTP packet data.
 * @data_len:			Length of data.
 * Returns:			0 on success, -1 on error.
 */
int
CHttpParse::Init(void *data, unsigned data_len)
{
	int			ret;

	if ( __inited ) {
		return -1;
	}

	ret = http_init((char *)data, data_len, &__basic);
	if ( ret == -1 ) {
		return -1;
	} else if ( ret == -2 ) {
		__pkg_len = data_len;
	} else {
		__pkg_len = __basic.head_len + __basic.body_len;
	}

	__buf = (char *)calloc(__basic.head_len + 1, sizeof(char));
	if ( !__buf ) {
		return -1;
	}
	__len = __basic.head_len + 1;

	memcpy(__buf, data, sizeof(char) * __basic.head_len);

	__inited = true;

	return 0;
}

/*
 * GetValue():		Get HTTP header item value.
 * @name:			HTTP header item name.
 * Returns:			Return value string on found, caller should not modify the string.
 *					NULL on not found or error.
 */
const char*
CHttpParse::GetValue(const char *name)
{
	int			err = 0;
	const char	*val = NULL;

	if ( !__inited ) {
		return NULL;
	}
	
	if ( !__parsed ) {
		val = http_get_value(name, __buf, __basic.head_len, &__headers, &err);
		if ( err != -1 ) {
			__parsed = true;
		}
	} else {
		val = http_get_value(name, NULL, 0, &__headers, &err);
	}

	return val;
}

/*
 * HeadLength():		Get HTTP packet head part length.
 * Returns:			Head length or -1 on error (usually on object not initialized).
 */
int
CHttpParse::HeadLength()
{
	return __inited ? __basic.head_len : -1;
}

/*
 * BodyLength():		Get HTTP packet entry body length.
 * Returns:			HTTP body length. May equal but not absolute equal Content-Length.
 *					Return HTTP_CONTENT_L_NFOUND means no Content-Length header item in POST or RESPONSE packet.
 *					Return HTTP_CONTENT_L_ERROR on error (usually on object not initialized).
 */
int
CHttpParse::BodyLength()
{
	return __inited ? __basic.body_len : HTTP_CONTENT_L_ERROR;
}

/*
 * PkgLength():		Get first HTTP packet length in data buffer given by caller when call init().
 * Returns:			 HTTP packet length above or -1 on error (usually on object not initialized).
 */
int
CHttpParse::PkgLength()
{
	return __inited ? __pkg_len : -1;
}

/*
 * HttpMethod:		Get HTTP method in request packet start line.
 * Returns:			HTTP_ERROR: Not a HTTP packet or error.
 *					HTTP_GET: Request method GET.
 *					HTTP_POST: Request method POST.
 *					HTTP_HEAD: Request method HEAD.
 *					HTTP_RSP: HTTP response packet.
 */
int
CHttpParse::HttpMethod()
{
	return __inited ? __basic.method : HTTP_ERROR;
}

/*
 * HttpVersion():		Get HTTP protol version in start line.
 * Returns:			Version string or NULL on error.
 */
const char*
CHttpParse::HttpVersion()
{
	return ( __inited && strlen(__basic.version) > 0 ) ? (const char*)__basic.version : NULL;
}

/*
 * HttpUri():			Get HTTP request URI in start line.
 * Returns:			HTTP request URI or NULL on response pakcet or no URI string or error.
 */
const char*
CHttpParse::HttpUri()
{
	return ( __inited && strlen(__basic.uri) > 0 ) ? (const char*)__basic.uri : NULL;
}

/*
 * HttpCode():		Get HTTP return code in response pakcet start line.
 * Returns:			HTTP code or INT_MIN on error (usually on object not initialized).
 */
int
CHttpParse::HttpCode()
{
	return __inited ? __basic.code : INT_MIN;
}

/*
 * HttpReason():		Get HTTP Reason Phrase in response packet start line.
 * Returns:			Reason Phrase string or NULL on request packet or no Reason Phrase string or error.
 */
const char*
CHttpParse::HttpReason()
{
	return ( __inited && strlen(__basic.reason) > 0 ) ? (const char*)__basic.reason : NULL;
}

/*
 * GET_STR_VAL():	Internal macro for get string value.
 * @member:			Member's name of __hot.
 * @name:			HTTP header item's name.
 * Returns:			HTTP header item value string or NULL on not found or error.
 */
#define GET_STR_VAL(member, name) ( ( __hot.member == (char*)(-1) ) ? NULL :	\
	( __hot.member ? __hot.member : ( (__hot.member = (char *)GetValue(name)) ? __hot.member :	\
	( (__hot.member = (char *)(-1)), ((char *)(0 + 0)) ) ) ) )

/*
 * HttpContentLength():	Get HTTP Content-Length.
 * Returns:				Content-Length value on found.
 *						-1 on not found or error.
 */
long
CHttpParse::HttpContentLength()
{
	const char*		val = NULL;

	if ( __hot.content_length == -2 ) {
		return -1;
	}
	
	if ( __hot.content_length == -1 ) {
		if ( !(val = GetValue("Content-Length:")) ) {
			__hot.content_length = -2;
			return -1;
		}
		
		__hot.content_length = strtol(val, NULL, 10);
		if ( ( __hot.content_length == 0 && errno == EINVAL )
			|| ( (__hot.content_length == LONG_MIN || __hot.content_length == LONG_MAX) && errno == ERANGE )
			|| ( __hot.content_length < 0 ) ) {
			__hot.content_length = -2;
			return -1;
		}

		return __hot.content_length;
	} else {
		return __hot.content_length;
	}
}

/*
 * HttpConnection():		Get HTTP Connection.
 * Returns:				1 on Keep-Alive, 0 on Close, -1 on error or not found.
 */
int
CHttpParse::HttpConnection()
{
	const char*		val = NULL;

	if ( __hot.connection == -2 ) {
		return -1;
	}
	
	if ( __hot.connection == -1 ) {
		if ( !(val = GetValue("Connection:")) ) {
			__hot.connection = -2;
			return -1;
		}

		if ( !strncasecmp("Keep-Alive", val, 10) ) {
			__hot.connection = 1;
			return __hot.connection;
		} else if ( !strncasecmp("Close", val, 5) ) {
			__hot.connection = 0;
			return __hot.connection;
		} else {
			__hot.connection = -2;
			return -1;
		}
	} else {
		return (__hot.connection != 0 && __hot.connection != 1) ? -1 : __hot.connection;
	}
}

/*
 * HttpHost():			Get HTTP Host.
 * Returns:				HTTP Host string or NULL on not found or error.
 */
const char*
CHttpParse::HttpHost()
{
	return (const char*)GET_STR_VAL(host, "Host:");
}

/*
 * HttpReferer():			Get HTTP Referer.
 * Returns:				HTTP Referer string or NULL on not found or error.
 */
const char*
CHttpParse::HttpReferer()
{
	return (const char*)GET_STR_VAL(referer, "Referer:");
}

/*
 * HttpUserAgent():		Get HTTP User-Agent.
 * Returns:				HTTP User-Agent string or NULL on not found or error.
 */
const char*
CHttpParse::HttpUserAgent()
{
	return (const char*)GET_STR_VAL(user_agent, "User-Agent:");
}

/*
 * HttpCookie():			Get HTTP Cookie.
 * Returns:				HTTP Cookie string or NULL on not found or error.
 */
const char*
CHttpParse::HttpCookie()
{
	return (const char*)GET_STR_VAL(cookie, "Cookie:");
}

/*
 * HttpAcceptEncoding():	Get HTTP Accept-Encoding.
 * Returns:				HTTP Accept-Encoding string or NULL on not found or error.
 */
const char*
CHttpParse::HttpAcceptEncoding()
{
	return (const char*)GET_STR_VAL(accept_encoding, "Accept-Encoding:");
}

/*
 * HttpServer():			Get HTTP Server.
 * Returns:				HTTP Server string or NULL on not found or error.
 */
const char*
CHttpParse::HttpServer()
{
	return (const char*)GET_STR_VAL(server, "Server:");
}

/*
 * HttpContentEncoding():	Get HTTP Content-Encoding.
 * Returns:				HTTP Content-Encoding string or NULL on not found or error.
 */
const char*
CHttpParse::HttpContentEncoding()
{
	return (const char*)GET_STR_VAL(content_encoding, "Content-Encoding:");
}

/*
 * HttpTransferEncoding():	Get HTTP Transfer-Encoding.
 * Returns:				HTTP Transfer-Encoding string or NULL on not found or error.
 */
const char*
CHttpParse::HttpTransferEncoding()
{
	return (const char*)GET_STR_VAL(transfer_encoding, "Transfer-Encoding:");
}

/*
 * HttpLocation():			Get HTTP Location.
 * Returns:				HTTP Location string or NULL on not found or error.
 */
const char*
CHttpParse::HttpLocation()
{
	return (const char*)GET_STR_VAL(location, "Location:");
}

////////////////////////////////////////////////////////////////
// Class CHttpTemplate
////////////////////////////////////////////////////////////////

/*
 * CHttpTemplate():	Constructor.
 */
CHttpTemplate::CHttpTemplate()
{
	__inited = false;
}

/*
 * CHttpTemplate():	Destructor. Clean up the resource.
 */
CHttpTemplate::~CHttpTemplate()
{
	if ( __inited ) {
		http_template_destroy(&__entry);
	}

	__inited = false;
}

/*
 * IsInited():			Wether the object has been initialized.
 * Returns:			true on initialized or false on not initialized.
 */
bool
CHttpTemplate::IsInited()
{
	return __inited;
}

/*
 * Init():				Initialize the object.
 * @msg_template:	Message template, need not ending by '\0'.
 * @msg_len:			Message template length also the HTTP head length. Not include '\0'.
 * @args:			HTTP args.
 * @arg_cnt:			Count of args.
 * Returns:			0 on success, -1 on error.
 */
int
CHttpTemplate::Init(const char *msg_template, int msg_len,
	http_template_arg_t *args, int arg_cnt)
{
	if ( __inited || http_template_init(&__entry, msg_template, msg_len, args, arg_cnt) ) {
		return -1;
	}

	__inited = true;
	
	return 0;
}

/*
 * Produce:			Produce a HTTP head message.
 * @buf:				Buffer to store the output message.
 * @buf_len:			Length of buf.
 * @args:			HTTP args. Each arg must endding by '\0'.
 * @arg_cnt:			Count of args.
 * Returns:			Return the HTTP message size in bytes on success. -1 on error.
 * NOTE:				Output message will not endding by '\0', so caller could not use it as a string.
 */
int
CHttpTemplate::Produce(char *buf, int buf_len, char **args, int arg_cnt)
{
	return __inited ? http_template_produce(&__entry, buf, buf_len, args, arg_cnt) : -1;
}

/*
 * ProduceRef():				Produce a HTTP message by the template but only return refer to data.
 * @r_data:					To store the refer.
 * @r_data_len:				To store data length.
 * @args:					HTTP args. Each arg must endding by '\0'.
 * @arg_cnt:					Count of args.
 * Returns:					Return the HTTP message size in bytes on success. -1 on error.
 * NOTE:						Output message will not endding by '\0', so caller could not use it as a string.
 */
int
CHttpTemplate::ProduceRef(char **r_data, int *r_data_len, char **args, int arg_cnt)
{
	return __inited ? http_template_produce_ref(&__entry, r_data, r_data_len, args, arg_cnt) : -1;
}


