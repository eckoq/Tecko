/*
 * http_api.h:			HTTP lib for mcp.
 * Date:				2011-02-28
 */

#ifndef __HTTP_API_H
#define __HTTP_API_H

#include <sys/cdefs.h>

__BEGIN_DECLS

#ifdef __FOR_IDES				// Should never define this.
struct for_ide {
	int			ide;
};
#endif

#include "list.h"

#define HTTP_LIMIT_LENGTH							// HTTP packet length control.

#ifdef HTTP_LIMIT_LENGTH
#define HTTP_MAX_LENGTH			(1<<22)				// HTTP packet max length.
#endif

#define HTTP_ERROR				(-1)				// Not a HTTP packet.
#define HTTP_GET				0					// Request method GET.
#define HTTP_POST				1					// Request method POST.
#define HTTP_HEAD				2					// Request method HEAD.
#define HTTP_RSP				3					// HTTP response packet.

#define HTTP_HEAD_MAX			4096				// HTTP head max length.
#define HTTP_HEADER_NAME_LEN	128					// Max length of HTTP header item's name.
#define HTTP_URI_MAX			1024				// Max length of HTTP URI.
#define HTTP_VER_LEN			4					// HTTP version string length. "1.1" or "1.0"
#define HTTP_REASON_LEN			64					// HTTP Reason Phrase string length.
#define HTTP_ITEM_CNT_MAX		64					// HTTP header items' max count.

#define HTTP_CONTENT_L_NFOUND	(-2)				// Return this means HTTP response packet has no Content-Length.
#define HTTP_CONTENT_L_ERROR	(-1)				// Get HTTP Content-Length error.

#define HTTP_NFOUND				((void *)(-1))

#define HTTP_VAL_MAX	32		// Max length of HTTP entry-value.
#define HTTP_TMP_LEN	64		// Temp buffer size.

/*
 * HTTP header item node.
 */
typedef struct {
	char				*data;						// Original data. Set to NULL after be fatched.
	char				*name;						// KEY.
	char				*value;						// VALUE.
	list_head_t			list;						// List node.
} http_item_elem_t;

/*
 * HTTP parsed item table.
 */
typedef struct {
	http_item_elem_t	elems[HTTP_ITEM_CNT_MAX];	// Elements.
	list_head_t			left;						// Headers not be fatched.
	list_head_t			fatched;					// Headers be fatched.
} http_item_table_t;

/*
 * HTTP basic information.
 */
typedef struct {
	int					method;						// HTTP method.
	char				version[HTTP_VER_LEN];		// HTTP version. "1.1" or "1.0".
	char				uri[HTTP_URI_MAX];			// HTTP request URI.
	int					code;						// HTTP code when HTTP response. (method == HTTP_RSP)
	char				reason[HTTP_REASON_LEN];	// HTTP Reason Phrase.
	int					head_len;					// HTTP head length.
	int					body_len;					// HTTP body length.
} http_basic_t;

/*
 * Arg in HTTP message template.
 */
typedef struct {
	int					offset;						// First byte in templete of the arg.
	int					max_length;					// Max arg string length not include the "\r\n" in the line ending.
} http_template_arg_t;

/*
 * HTTP message template.
 */
typedef struct {
	char				*buf;						// Buffer for HTTP message template. Not teminate by '\0'.
	int					len;						// Length of buffer, also the message template length.
	int					arg_cnt;					// Count of args in template.
	http_template_arg_t	*args;						// Args information in the HTTP message template.
} http_template_t;

/*
 * http_head_length():			Get HTTP head length and HTTP type.
 * @data:					HTTP data.
 * @data_len:					Length of data.
 * @type:					To store HTTP method, or set to NULL if not care.
 * Returns:					Return full HTTP head length when head completely,
 *							0 on not complete,
 *							-1 on normal error, such as not a HTTP packet,
 */
extern int
http_head_length(char *data, unsigned data_len, int *type);

/*
 * http_content_length():		Get HTTP Content-Length value.
 * @data:					HTTP head data. Must full head include "\r\n\r\n"
 * @data_len:					HTTP head length.
 * Returns:					Return Content-Length value on success,
 *							HTTP_CONTENT_L_ERROR on error,
 *							HTTP_CONTENT_L_NFOUND on Content-Length not found.
 */
extern int
http_content_length(char *data, unsigned data_len);

/*
 * http_check_complete():		HTTP packet complete check.
 * @data:					Packet data.
 * @data_len:					Length of data.
 * @r_head_len:				To store HTTP head length or set NULL when not care.
 * @r_body_len:				To store HTTP body length or set NULL when not care.
 * Returns:					Return full HTTP packet length when complete,
 *							0 on not complete,
 *							-1 on normal error, such as not a HTTP packet,
 */
extern int
http_check_complete(void *data, unsigned data_len, int *r_head_len, int *r_body_len);


/*
 * http_init():					Initialize a HTTP entry. Call befor get HTTP header item.
 * @data:					HTTP data.
 * @data_len:					Length of data.
 * @http_base:				To store HTTP basic information.
 * Returns:					0 on success, -1 on normal error, -2 on HTTP packet not complete.
 */
extern int
http_init(char *data, unsigned data_len, http_basic_t *http_basic);

/*
 * http_get_value():		Get HTTP header item.
 * @name:				HTTP header item name.
 * @http_head:			Set pointer to HTTP head data when first call. Set NULL shen sub call.
 * @head_len:			Set to HTTP head length when first call. This value will be ignored in sub calls.
 * @parse_tab:			Table to help parse. Caller should not modify it.
 * @err:					Set to -1 if error occurs and http_head has not been parsed,
 *						-2 when other errors. Set to 0 when success. The param could be NULL when caller ignore the error.
 * Returns:				Return a const char* when item be found. Caller should not modify the string. Use it as read only.
 *						NULL on not found or error.
 */
extern const char*
http_get_value(const char* name, char *http_head, int head_len, http_item_table_t *parse_tab, int *err);

/*
 * http_template_init():		Build a HTTP message template.
 * @entry:				HTTP message template descript entry.
 * @msg_template:		Message template, need not ending by '\0'.
 * @msg_len:				Message template length also the HTTP head length. Not include '\0'.
 * @args:				HTTP args.
 * @arg_cnt:				Count of args.
 * Returns:				0 on success, -1 on error.
 */
extern int
http_template_init(http_template_t *entry,
	const char *msg_template, int msg_len,
	http_template_arg_t *args, int arg_cnt);

/*
 * http_template_destroy():		Clean up the HTTP message template.
 * @entry:					HTTP message template descript entry.
 */
extern void
http_template_destroy(http_template_t *entry);

/*
 * http_template_produce():	Produce a HTTP head message.
 * @entry:					HTTP message template descript entry.
 * @buf:						Buffer to store the output message.
 * @buf_len:					Length of buf.
 * @args:					HTTP args. Each arg must endding by '\0'.
 * @arg_cnt:					Count of args.
 * Returns:					Return the HTTP message size in bytes on success. -1 on error.
 * NOTE:						Output message will not endding by '\0', so caller could not use it as a string.
 */
extern int
http_template_produce(http_template_t *entry,
	char *buf, int buf_len,
	char **args, int arg_cnt);

/*
 * http_template_produce_ref():	Produce a HTTP head message and return template text refer.
 * @entry:					HTTP message template descript entry.
 * @r_data:					To store the refer.
 * @r_data_len:				To store data length.
 * @args:					HTTP args. Each arg must endding by '\0'.
 * @arg_cnt:					Count of args.
 * Returns:					Return the HTTP message size in bytes on success. -1 on error.
 * NOTE:						Output message will not endding by '\0', so caller could not use it as a string.
 */
extern int
http_template_produce_ref(http_template_t *entry,
	char **r_data, int *r_data_len,
	char **args, int arg_cnt);

__END_DECLS

#endif

