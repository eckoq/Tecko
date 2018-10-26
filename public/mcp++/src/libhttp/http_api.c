/*
 * http_api.h:			HTTP lib for mcp.
 * Date:				2011-02-28
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <strings.h>
#include <limits.h>
#include <stddef.h>

#include "list.h"
#include "http_api.h"

#define HTTP_VAL_MAX	32		// Max length of HTTP entry-value.
#define HTTP_TMP_LEN	64		// Temp buffer size.

/*
 * http_strtrim():		Erase LWS in a string head and tail.
 * @str:				String will be handle.
 * Returns:			Pointer to string handled.
 * NOTES:			Modified from NWS.
 */
static char*
http_strtrim(char* str)
{
	char		*val = NULL;
	
	if( !str )
		return NULL;

	for ( ; *str != 0 && (*str == ' ' || *str == '\t'); str++);
	val = str + strlen( str ) - 1;	
	while( val > str && *val != 0 && (*val == ' ' || *val == '\t') ) {
		*val-- = 0;
	}
	
	return str;
}

/*
 * http_get_original():	Get HTTP header item from original lines.
 * @name:			HTTP header item name.
 * @len:				String length of name.
 * @items:			HTTP header item table.
 * Returns:			Return const string on found. Caller should not modify the string.
 *					Return NULL on not found.
 */
static char*
http_get_original(const char *name, int len, http_item_table_t *items)
{
	list_head_t			*pos = NULL, *tmp = NULL;
	http_item_elem_t	*elem = NULL;
	
	list_for_each_safe(pos, tmp, &items->left) {
		elem = list_entry(pos, http_item_elem_t, list);
		if ( elem->data && !strncasecmp(name, elem->data, len) ) {
			elem->name = elem->data;
			elem->value = http_strtrim(elem->data + len);
			elem->data = NULL;
			list_del_init(&elem->list);
			list_add_tail(&elem->list, &items->fatched);
			return elem->value;
		}
	}

	return NULL;
}

/*
 * http_get_fetched():	Get HTTP header item from found table.
 * @name:			HTTP header item name.
 * @len:				String length of name.
 * @items:			HTTP header item table.
 * Returns:			Return const string on found. Caller should not modify the string.
 *					Return NULL on not found.
 */
static char*
http_get_fetched(const char *name, int len, http_item_table_t *items)
{
	list_head_t			*pos = NULL, *tmp = NULL;
	http_item_elem_t	*elem = NULL;

	list_for_each_safe(pos, tmp, &items->fatched) {
		elem = list_entry(pos, http_item_elem_t, list);
		if ( elem->name && !strncasecmp(name, elem->name, len) ) {
			return elem->value;
		}
	}

	return NULL;
}

/*
 * http_parse_str():	Parse string by delim.
 * @str:				Source string.
 * @pstr:			Delim string.
 * @end:				Ending string.
 * @items:			HTTP header item table to help parse.
 * @count:			Arrary elements's count of val.
 */
static void
http_parse_str(char* str, char* pstr, char* end, http_item_table_t *items, unsigned short count)
{
	char			*pnext = NULL;
	unsigned short	i = 0;
	
	for( i = 0; i < count && str; i++ )
	{
		items->elems[i].data = http_strtrim(strtok_r(str, pstr, &pnext));
		if( (!items->elems[i].data) || ( end && (!strcmp(items->elems[i].data, end)) ) ) {
			items->elems[i].data = NULL;
			break;
		}
		list_add_tail(&items->elems[i].list, &items->left);
		str = pnext;
	}
}

/*
 * http_type_strict():			Get HTTP packet type. Another version. More strict.
 * @data:					Method string.
 * Returns:					Return HTTP_ERROR, HTTP_HEAD, HTTP_GET, HTTP_POST or HTTP_RSP.
 */
static inline int
http_type_strict(char *data)
{
	if ( strlen(data) < 3 ) {
		return HTTP_ERROR;
	}

	if ( !strcasecmp(data, "GET") ) {
		return HTTP_GET;
	} else if ( !strcasecmp(data, "POST") ) {
		return HTTP_POST;
	} else if ( !strcasecmp(data, "HEAD") ) {
		return HTTP_HEAD;
	} else if ( !strncasecmp(data, "HTTP", 4) ) {
		return HTTP_RSP;
	} else {
		return HTTP_ERROR;
	}
}

/*
 * http_type():				Get HTTP packet type.
 * @data:					HTTP data.
 * @data_len:					Length of data.
 * Returns:					Return HTTP_ERROR, HTTP_HEAD, HTTP_GET, HTTP_POST or HTTP_RSP.
 */
static inline int
http_type(char *data, unsigned data_len)
{
	if ( data_len < 4 ) {
		return HTTP_ERROR;
	}

	if ( !strncasecmp(data, "GET", 3) ) {
		return HTTP_GET;
	} else if ( !strncasecmp(data, "POST", 4) ) {
		return HTTP_POST;
	} else if ( !strncasecmp(data, "HEAD", 4) ) {
		return HTTP_HEAD;
	} else if ( !strncasecmp(data, "HTTP", 4) ) {
		return HTTP_RSP;
	} else {
		return HTTP_ERROR;
	}
}

/*
 * http_head_length():			Get HTTP head length and HTTP type.
 * @data:					HTTP data.
 * @data_len:					Length of data.
 * @type:					To store HTTP method, or set to NULL if not care.
 * Returns:					Return full HTTP head length when head completely,
 *							0 on not complete,
 *							-1 on normal error, such as not a HTTP packet,
 */
static inline int
http_head_length(char *data, unsigned data_len, int *type)
{
	char		*pend;
	char		saved;
	int			tmp_type, at_tail = 0;
	int			head_len;

	if ( !type ) {
		type = &tmp_type;
	}

	*type = HTTP_ERROR;

	if ( data_len < 4 ) {
		return 0;
	}

	*type = http_type(data, data_len);
	if ( *type == HTTP_ERROR ) {
		return -1;
	}

	if ( data[data_len - 1] == '\n' ) {
		if ( !strncmp( (data + data_len - 4), "\r\n\r\n", 4) ) {
			at_tail = 1;
		}
	}

	saved = data[data_len - 1];
	data[data_len - 1] = 0;
	pend = strstr(data, "\r\n\r\n");
	data[data_len - 1] = saved;
	
	if ( pend == NULL ) {
		if ( at_tail ) {
			head_len = data_len;
		} else {
			return data_len > HTTP_HEAD_MAX - 1 ? -1 : 0;
		}
	} else {
		head_len = pend - data + 4;
	}

	if ( head_len > HTTP_HEAD_MAX - 1 ) {
		return -1;
	}

	return head_len;
}

/*
 * http_content_length():		Get HTTP Content-Length value.
 * @data:					HTTP head data. Must full head include "\r\n\r\n"
 * @data_len:					HTTP head length.
 * Returns:					Return Content-Length value on success,
 *							HTTP_CONTENT_L_ERROR on error,
 *							HTTP_CONTENT_L_NFOUND on Content-Length not found.
 */
static int
http_content_length(char *data, unsigned data_len)
{
	const int		name_len = 17;		// strlen("\r\nContent-Length:");
	int				val_len;
	long int		content_len;
	char			saved;
	char			*pos_start = NULL, *pos_end = NULL;
	char			val[HTTP_VAL_MAX];

	saved = data[data_len - 1];
	data[data_len - 1] = 0;
	pos_start = strcasestr(data, "\r\nContent-Length:");
	if ( !pos_start ) {
		data[data_len - 1] = saved;
		return HTTP_CONTENT_L_NFOUND;
	}

	pos_start += name_len;

	pos_end = strstr(pos_start, "\r\n");
	
	data[data_len - 1] = saved;
	
	if ( !pos_end ) {
		return HTTP_CONTENT_L_ERROR;
	}

	if ( pos_end - pos_start > HTTP_VAL_MAX - 1 ) {
		return HTTP_CONTENT_L_ERROR;
	}

	memset(val, 0, sizeof(char) * HTTP_VAL_MAX);
	
	for ( ; (*pos_start == ' ' || *pos_start == '\t') && pos_start < pos_end; pos_start++ );

	for ( val_len = 0; *pos_start != ' ' && *pos_start != '\t' && pos_start < pos_end; pos_start++ ) {
		val[val_len++] = *pos_start;
	}

	if ( !val_len ) {
		return HTTP_CONTENT_L_ERROR;
	}

	content_len = strtol(val, NULL, 10);
	if ( ( content_len == 0 && errno == EINVAL )
		|| ( (content_len == LONG_MIN || content_len == LONG_MAX) && errno == ERANGE )
		|| ( content_len < 0 ) ) {
		return HTTP_CONTENT_L_ERROR;
	}

	return (int)content_len;
}

/*
 * http_check_complete():		HTTP packet complete check.
 * @data:					Packet data.
 * @data_len:					Length of data.
 * @r_head_len:				To store HTTP head length or set NULL when not care.
 * @r_body_len:				To store HTTP body length or set NULL when not care.
 * Returns:					Return full HTTP packet length when complete,
 *							0 on not complete,
 *							-1 on normal error, such as not a HTTP packet,
 *							-2 on complete but no Content-Length in HTTP POST or response packet.
 */
int
http_check_complete(void *data, unsigned data_len, int *r_head_len, int *r_body_len)
{
	char		*http_data = (char *)data;
	int			type;
	int			head_len, body_len, ret;

	if ( r_head_len ) {
		*r_head_len = 0;
	}

	if ( r_body_len ) {
		*r_body_len = 0;
	}

	head_len = http_head_length(http_data, data_len, &type);
	if ( !head_len ) {
		return 0;
	} else if ( head_len == -1 ) {
		return -1;
	}

	if ( r_head_len ) {
		*r_head_len = head_len;
	}

	if ( type == HTTP_GET || type == HTTP_HEAD ) {
		return head_len;
	}

	body_len = http_content_length(http_data, (unsigned)head_len);
	if ( body_len == HTTP_CONTENT_L_ERROR ) {
		return -1;
	} else if ( body_len == HTTP_CONTENT_L_NFOUND ) {
		if ( r_body_len ) {
			*r_body_len = HTTP_CONTENT_L_NFOUND;
		}
		return -2;
	}

	if ( r_body_len ) {
		*r_body_len = body_len;
	}

	ret = head_len + body_len;

#ifdef HTTP_LIMIT_LENGTH
	if ( ret > HTTP_MAX_LENGTH ) {
		return -1;
	}
#endif

	return data_len >= ret ? ret : 0;
}

/*
 * http_init():			Initialize a HTTP entry. Call befor get HTTP header item.
 * @data:			HTTP data.
 * @data_len:			Length of data.
 * @http_base:		To store HTTP basic information.
 * Returns:			0 on success, -1 on normal error, -2 on HTTP packet not complete.
 */
int
http_init(char *data, unsigned data_len, http_basic_t *http_basic)
{
	int				ret, i;
	long			tmp_l;
	char			*pos = NULL, *tail = NULL;
	char			saved, saved_tail;
	char			tmp_buf[HTTP_TMP_LEN];

	memset(http_basic, 0, sizeof(http_basic_t));
	http_basic->code = INT_MIN;

	// Check complete.
	ret = http_check_complete(data, data_len, &http_basic->head_len, &http_basic->body_len);
	if ( ret == -1 ) {
		return -1;
	} else if ( ret == 0 ) {
		return -2;
	}

	// Parse start line.
	saved = data[http_basic->head_len - 1];
	data[http_basic->head_len - 1] = 0;
	tail = strstr(data, "\r\n");
	data[http_basic->head_len - 1] = saved;
	if ( !tail ) {
		return -1;
	}
	saved_tail = *tail;
	*tail = 0;

	for ( pos = data; *pos != ' ' && *pos != '\t' && pos < tail; pos++ );
	// Modified to '\t' ' ' and != '\0'
	if ( pos == tail ) {
		*tail = saved_tail;
		return -1;
	}

	saved = *pos;
	*pos = 0;	
	http_basic->method = http_type_strict(data);
	*pos = saved;

	if ( http_basic->method == HTTP_ERROR ) {
		*tail = saved_tail;
		return -1;
	} else if ( http_basic->method == HTTP_RSP ) {
		// Response packet.
		// Version.
		// First 5 bytes is "HTTP/".
		for ( i = 5 ; data[i] != ' ' && data[i] != '\t' && data + i < pos && i - 5 < HTTP_VER_LEN - 1; i++ ) {
			http_basic->version[i - 5] = data[i];
		}
		if ( i - 5 == 0 ) {
			*tail = saved_tail;
			return -1;
		}
		http_basic->version[i] = 0;
		if ( data + i != pos ) {
			*tail = saved_tail;
			return -1;
		}
		// Code.
		for ( ; (*pos == ' ' || *pos == '\t') && pos < tail; pos++ );
		for ( i = 0 ; *pos != ' ' && *pos != '\t' && pos < tail && i < HTTP_TMP_LEN - 1; pos++, i++ ) {
			tmp_buf[i] = *pos;
		}
		if ( i == 0 ) {
			*tail = saved_tail;
			return -1;
		}
		tmp_buf[i] = 0;
		tmp_l = strtol(tmp_buf, NULL, 10);
		if ( ( tmp_l == 0 && errno == EINVAL )
			|| ( (tmp_l == LONG_MIN || tmp_l == LONG_MAX) && errno == ERANGE )
			|| ( tmp_l < 0 ) ) {
			*tail = saved_tail;
			return -1;
		}
		http_basic->code = (int)tmp_l;
		
		// Reason Phrase.
		for ( ; (*pos == ' ' || *pos == '\t') && pos < tail; pos++ );
		if ( pos == tail ) {
			// No reason string.
			*tail = saved_tail;
			return -1;
		}
		memset(http_basic->reason, 0, sizeof(char) * HTTP_REASON_LEN);
		strncpy(http_basic->reason, pos, HTTP_REASON_LEN - 1);
	} else {
		// Request packet.
		for ( ; (*pos == ' ' || *pos == '\t') && pos < tail; pos++ );
		// URI.
		for ( i = 0; *pos != ' ' && *pos != '\t' && pos < tail && i < HTTP_URI_MAX - 1; pos++, i++ ) {
			http_basic->uri[i] = *pos;
		}
		if ( i == 0 ) {
			// No URI.
			*tail = saved_tail;
			return -1;
		}
		// If too long, only store HTTP_URI_MAX - 1 in head.
		http_basic->uri[i] = 0;

		// Version.
		for ( ; (*pos == ' ' || *pos == '\t') && pos < tail; pos++ );
		for ( i = 0; i < 5 && pos < tail; pos++, i++ );
		// First 5 bytes is "HTTP/".
		if ( i != 5 ) {
			*tail = saved_tail;
			return -1;
		}
		for ( i = 0 ; pos < tail && i < HTTP_VER_LEN - 1; pos++, i++ ) {
			http_basic->version[i] = *pos;
		}
		if (  i == 0 ) {
			*tail = saved_tail;
			return -1;
		}
		http_basic->version[i] = 0;
	}

	*tail = saved_tail;

	return 0;
}

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
const char*
http_get_value(const char* name, char *http_head, int head_len, http_item_table_t *parse_tab, int *err)
{
	char			*start = NULL;
	char			buf_name[HTTP_HEADER_NAME_LEN];
	int				name_len;
	char			*val = NULL;
	int				tmp_err = 0;

	if ( !err ) {
		err = &tmp_err;
	}

	*err = 0;

	if ( !parse_tab ) {
		*err = -1;
		return NULL;
	}

	if ( http_head ) {
		// First call.
		// "\r\n\r\n" to "\r\n\0\n"
		http_head[head_len - 2] = 0;

		// Skip start line.
		start = strstr(http_head, "\r\n");
		if ( !start ) {
			*err = -1;
			return NULL;
		}
		start += 2;

		memset(parse_tab, 0, sizeof(http_item_table_t));
		INIT_LIST_HEAD(&parse_tab->left);
		INIT_LIST_HEAD(&parse_tab->fatched);

		http_parse_str(start, "\r\n", "", parse_tab, HTTP_ITEM_CNT_MAX);
	}

	name_len = strlen(name);
	if ( name_len > HTTP_HEADER_NAME_LEN - 2 ) {
		*err = -2;
		return NULL;
	}
	memset(buf_name, 0, sizeof(char) * HTTP_HEADER_NAME_LEN);
	memcpy(buf_name, name, name_len);
	if ( buf_name[name_len - 1] != ':' ) {
		buf_name[name_len++] = ':';
	}

	if ( !http_head ) {
		// Only sub call run here.
		val = http_get_fetched(buf_name, name_len, parse_tab);
	}

	if ( !val ) {
		val = http_get_original(buf_name, name_len, parse_tab);
	}
	
	return (const char *)val;
}

/*
 * http_template_init():		Build a HTTP message template.
 * @entry:				HTTP message template descript entry.
 * @msg_template:		Message template, need not ending by '\0'.
 * @msg_len:				Message template length also the HTTP head length. Not include '\0'.
 * @args:				HTTP args.
 * @arg_cnt:				Count of args.
 * Returns:				0 on success, -1 on error.
 */
int
http_template_init(http_template_t *entry,
	const char *msg_template, int msg_len,
	http_template_arg_t *args, int arg_cnt)
{
	int			i;
	
	memset(entry, 0, sizeof(http_template_t));
	
	if ( arg_cnt < 0 || msg_len <= 0 || msg_len > HTTP_HEAD_MAX || !entry ) {
		return -1;
	}

	entry->buf = (char *)malloc(sizeof(char) * msg_len);
	if ( !entry->buf ) {
		goto err_out0;
	}
	entry->len = msg_len;
	memcpy(entry->buf, msg_template, sizeof(char) * msg_len);

	if ( arg_cnt > 0 ) {
		entry->args = (http_template_arg_t *)calloc(arg_cnt, sizeof(http_template_arg_t));
		if ( !entry->args ) {
			goto err_out1;
		}
	}
	entry->arg_cnt = arg_cnt;

	for ( i = 0; i < arg_cnt; i++ ) {
		entry->args[i].offset = args[i].offset;
		entry->args[i].max_length = args[i].max_length;
	}
	
	return 0;

err_out1:
	if ( entry->buf ) {
		free(entry->buf);
		entry->buf = NULL;
	}

err_out0:	
	return -1;
}

/*
 * http_template_destroy():		Clean up the HTTP message template.
 * @entry:					HTTP message template descript entry.
 */
void
http_template_destroy(http_template_t *entry)
{
	if ( entry->args ) {
		free(entry->args);
		entry->args = NULL;
	}
	entry->arg_cnt = 0;

	if ( entry->buf ) {
		free(entry->buf);
		entry->buf = NULL;
	}
	entry->len = 0;
}

/*
 * http_template_fill_args():		Fill args to template text.
 * @entry:					HTTP message template descript entry.
 * @args:					HTTP args. Each arg must endding by '\0'.
 * @template_text:			Data stored the template text.
 * Returns:					Return the HTTP message size in bytes on success. -1 on error.
 */
static inline int
http_template_fill_args(http_template_t *entry, char **args, char *template_text)
{
	int		i, tmp_len;
	
	for ( i = 0; i < entry->arg_cnt; i++ ) {
		if ( !args[i] ) {
			return -1;
		}
		tmp_len = strlen(args[i]);
		if ( tmp_len > entry->args[i].max_length ) {
			return -1;
		}
		memset(template_text + entry->args[i].offset, ' ', sizeof(char) * entry->args[i].max_length);
		memcpy(template_text + entry->args[i].offset, args[i], sizeof(char) * tmp_len);
	}

	return sizeof(char) * entry->len;
}

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
int
http_template_produce(http_template_t *entry,
	char *buf, int buf_len,
	char **args, int arg_cnt)
{	
	if ( !entry || !buf || buf_len < entry->len || !args || arg_cnt != entry->arg_cnt ) {
		return -1;
	}

	memcpy(buf, entry->buf, sizeof(char) * entry->len);

	return http_template_fill_args(entry, args, buf);
}

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
int
http_template_produce_ref(http_template_t *entry,
	char **r_data, int *r_data_len,
	char **args, int arg_cnt)
{
	if ( r_data ) {
		*r_data = NULL;
	}

	if ( r_data_len ) {
		*r_data_len = -1;
	} else {
		return -1;
	}

	if ( !entry || !r_data || !args || arg_cnt != entry->arg_cnt ) {
		return -1;
	}

	*r_data_len = http_template_fill_args(entry, args, entry->buf);

	if ( *r_data_len == -1 ) {
		return -1;
	}

	*r_data = entry->buf;

	return (*r_data_len);
}

