/*
 * wtg_api.c:			Wtg client APIs.
 * Date:				2011-04-12
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stddef.h>
#include <time.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#ifdef __HARDCODE_CONF			// Open this in makefile when need to hard code configuration.
#include "hardcode_conf.h"
#endif
#include "common_proto.h"
#include "sproto.h"
#include "wtg_api.h"

#ifdef __MCP
#include "watchdog_qnf_myconfig.h"
#endif

#ifdef __NWS
#include "service.h"
#endif

#define RETRY_TIME			300				// When reconnection fail during the report(not include intialize), next retry time(sec).
#define RETRY_MAX_CNT		6				// Max retry count.
#define CONN_TIMEOUT		10				// Connection block timeout time(sec).

#define ALARM_MSG_SIZE		256				// Alarm message max length.

#define SEND_BUF_SIZE		65536			// Send buffer size.

#define INT_STR_LEN			32				// Int string length.

#define DEF_USE_FLAG		-1				// Get this means not launch wtg.

#ifdef __DEBUG
#define wa_output(fmt, args...) printf(fmt, ##args)
#else
#define wa_output(fmt, args...)
#endif

static __thread char		send_buf[SEND_BUF_SIZE];	// Send buffer.

/*
 * wtg_api_conn_connect():		Connect to daemon.
 * @entry:					Wtg client API entry.
 * Returns:					0 on success, -1 on error.
 */
static int
wtg_api_conn_connect(wtg_entry_t *entry)
{
	struct sockaddr_un		un;
	socklen_t				len;
	struct timeval			s_timeout;

	if ( entry->skt >= 0 ) {
		close(entry->skt);
		entry->skt = -1;
	}

	entry->skt = socket(PF_UNIX, SOCK_STREAM, 0);
	if ( entry->skt < 0 ) {
		wa_output("[wtg_api]: Create client UNIX domain socket fail!\n");
		goto err_out0;
	}

	s_timeout.tv_sec = CONN_TIMEOUT;
	s_timeout.tv_usec = 0;
	if ( setsockopt(entry->skt, SOL_SOCKET, SO_RCVTIMEO, &s_timeout, sizeof(struct timeval)) ) {
		wa_output("[wtg_api]: Set socket recive timeout time fail! %m\n");
		goto err_out1;
	}
	if ( setsockopt(entry->skt, SOL_SOCKET, SO_SNDTIMEO, &s_timeout, sizeof(struct timeval)) ) {
		wa_output("[wtg_api]: Set socket send timeout time fail! %m\n");
		goto err_out1;
	}

	memset(&un, 0, sizeof(struct sockaddr_un));
	un.sun_family = AF_UNIX;
	strncpy(un.sun_path, entry->addr, UNIX_PATH_MAX - 1);
	len = offsetof(struct sockaddr_un, sun_path) + strlen(un.sun_path);

	if ( connect(entry->skt, (struct sockaddr *)&un, len) < 0 ) {
		wa_output("[wtg_api]: Connect to daemon server UNIX domain socket fail! %m\n");
		goto err_out1;
	}

	entry->retry_cnt = 0;
	entry->retry_time = 0;

	return 0;

err_out1:
	if ( entry->skt >= 0 ) {
		close(entry->skt);
		entry->skt = -1;
	}

err_out0:
	return -1;
}

/*
 * wtg_api_conn_close():	Close a UNIX domain socket connection. Only call when cleanup because of block lock.
 * @entry:				Wtg client API entry.
 */
static inline void
wtg_api_conn_close(wtg_entry_t *entry)
{
	pthread_mutex_lock(&entry->reconn_lock);
	if ( entry->skt >= 0 ) {
		close(entry->skt);
		entry->skt = -1;
	}
	pthread_mutex_unlock(&entry->reconn_lock);	
}

/*
 * wtg_api_conn_reconnect():	Reconnect to daemon.
 * @entry:					Wtg client API entry.
 * Returns:					0 on success, -1 on error.
 */
static int
wtg_api_conn_reconnect(wtg_entry_t *entry)
{
	if ( pthread_mutex_trylock(&entry->reconn_lock) ) {
		// No block when could not get the lock.
		return -1;
	}

	if ( entry->skt >= 0 ) {
		close(entry->skt);
		entry->skt = -1;
	}

	if ( entry->retry_cnt >= RETRY_MAX_CNT ) {
		goto locked_err_out;
	}

	if ( time(NULL) < entry->retry_time ) {
		goto locked_err_out;
	}

	if ( wtg_api_conn_connect(entry) ) {
		entry->retry_time = time(NULL) + RETRY_TIME;
		entry->retry_cnt++;
		goto locked_err_out;
	}

	pthread_mutex_unlock(&entry->reconn_lock);

	return 0;

locked_err_out:	
	pthread_mutex_unlock(&entry->reconn_lock);

	return -1;
}

/*
 * wtg_api_load_conf():	Load configuration.
 * @entry:				Wtg client API entry.
 * @conf_name:			Watchdog config file name, when type is WTG_API_TYPE_NWS, we ignore this param.
 * Returns:				0 on success, -1 on error.
 */
static inline int
wtg_api_load_conf(wtg_entry_t *entry, const char *conf_name)
{
#ifdef __MCP
	char		**argv = (char **)&conf_name;
#endif
	char		*val = NULL;
	int			ival;

#ifdef __MCP	
	if ( conf_name && myconfig_init(1, argv, 1) ) {
		wa_output("[wtg_api]: Config init fail!\n");
		return -1;
	}
#endif

	ival = myconfig_get_intval("wtg_enable", DEF_USE_FLAG);
	if ( ival == DEF_USE_FLAG ) {
#ifdef __HARDCODE_CONF
		entry->enabled = WTG_ENABLE;
		if ( entry->enabled == 0 ) {
			return 0;
		}
#else
		entry->enabled = 0;
		return 0;
#endif
	} else if ( ival == 0 ) {
		entry->enabled = 0;
		return 0;
	} else {
		entry->enabled = 1;
	}

	val = myconfig_get_value("wtg_domain_address");
	if ( !val ) {
		strncpy(entry->addr, WTG_DEF_DOMAIN_ADDR, WTG_DEF_STR_LEN);
		wa_output("[wtg_api]: No config item \"wtg_domain_address\" found, use default \"%s\"\n",
			entry->addr);
	} else {
		strncpy(entry->addr, val, WTG_DEF_STR_LEN);
	}

	return 0;
}

/*
 * wtg_api_load_conf_v2():	Load configuration V2.
 * @entry:				Wtg client API entry.
 * @wtg_enable:			-1 use default, 0 not enable, others enable.
 * @unix_sock_addr:		NULL use default, others just the address.
 * Returns:				0 on success, -1 on error.
 */
static inline int
wtg_api_load_conf_v2(wtg_entry_t *entry, int wtg_enable, char *unix_sock_addr)
{
	int			ival = wtg_enable;
	char		*val = unix_sock_addr;

	if ( ival == DEF_USE_FLAG ) {
#ifdef __HARDCODE_CONF
		entry->enabled = WTG_ENABLE;
		if ( entry->enabled == 0 ) {
			return 0;
		}
#else
		entry->enabled = 0;
		return 0;
#endif
	} else if ( ival == 0 ) {
		entry->enabled = 0;
		return 0;
	} else {
		entry->enabled = 1;
	}

	if ( !val ) {
		strncpy(entry->addr, WTG_DEF_DOMAIN_ADDR, WTG_DEF_STR_LEN);
		wa_output("[wtg_api]: No config item \"wtg_domain_address\" found, use default \"%s\"\n",
			entry->addr);
	} else {
		strncpy(entry->addr, val, WTG_DEF_STR_LEN);
	}

	return 0;
}

/*
 * wtg_api_entry_reset():	Reset wtg API entry.
 * @entry:				Wtg client API entry.
 * @type:				Program type. WTG_API_TYPE_NWS or WTG_API_TYPE_MCP
 */
static inline void
wtg_api_entry_reset(wtg_entry_t *entry, int type)
{
	memset(entry, 0, sizeof(wtg_entry_t));
	entry->type = type;
	entry->skt = -1;
	entry->retry_time = 0;
	entry->retry_cnt = 0;
	pthread_mutex_init(&entry->reconn_lock, NULL);
}

/*
 * wtg_api_build_ipc_head():	Build IPC protocol header.
 * @head:					Header buffer.
 * @type:					Pakcet type.
 * @len:						Packet length.
 */
static inline void
wtg_api_build_ipc_head(wtg_ipc_head_t *head, char type, int len)
{
	head->magic = WTG_IPC_MAGIC;
	head->type = type;
	head->len = len;
}

/*
 * wtg_api_common_send():		Common UNIX domain send.
 * @entry:					Wtg client API entry.
 * @len:						Send length.
 * Returns:					0 on success, -1 on error.
 */
static inline int
wtg_api_common_send(wtg_entry_t *entry, int len)
{
	if ( send(entry->skt, send_buf, len, MSG_NOSIGNAL) != len ) {
		wa_output("[wtg_api]: Common send network error! %m Reconnecting...\n");
		if ( wtg_api_conn_reconnect(entry) ) {
			// Actually only reconnect reach retry time or not retry too many times. So not output information. Output in connect function.
			return -1;
		}

		if ( send(entry->skt, send_buf, len, MSG_NOSIGNAL) != len ) {
			wa_output("[wtg_api]: Common send network error again! %m Reconnecting next time.\n");
			return -1;
		}
	}

	return 0;
}

/*
 * wtg_api_send_base():	Send base information.
 * @entry:				Wtg client API entry.
 * @base:				Base information. When send_base is zero, we ignore this param.
 * Returns:				0 on success, -1 on error.
 */
static int
wtg_api_send_base(wtg_entry_t *entry, wtg_api_base_t *base)
{
	wtg_ipc_head_t		*head = (wtg_ipc_head_t *)send_buf;
	wtg_base_t			*body = NULL;
	char				*pos = NULL;
	int					i, body_len;

	if ( !entry ) {
		return -1;
	}

	body = (wtg_base_t *)head->data;

	strcpy(body->ver, base->ver);

	if ( base->port_cnt <= 0 || base->port_cnt > PORT_MAX_CNT ) {
		wa_output("[wtg_api]: Invalid PORT count \"%d\"!\n", base->port_cnt);
		return -1;
	}
	pos = body->ports;
	for ( i = 0; i < base->port_cnt; i++ ) {
		pos += sprintf(pos, "%u,", base->ports[i]);
	}
	// Erase last ",".
	*(pos - 1) = 0;

	strcpy(body->plugins, base->plugins);
	strcpy(body->prog_path, base->prog_path);
	strcpy(body->conf_path, base->conf_path);

	body_len = sizeof(wtg_base_t);

	wtg_api_build_ipc_head(head, IPC_TYPE_BASE, body_len);

	if ( send(entry->skt, send_buf, sizeof(wtg_ipc_head_t) + body_len, MSG_NOSIGNAL)
		!= sizeof(wtg_ipc_head_t) + body_len ) {
		wa_output("[wtg_api]: Send base information network error! %m\n");
		return -1;
	}

	return 0;
}

/*
 * wtg_api_destroy():		Wtg client API set cleanup.
 * @entry:				Wtg client API entry.
 */
void
wtg_api_destroy(wtg_entry_t *entry)
{
	if ( !entry ) {
		return;
	}

	wtg_api_conn_close(entry);

	pthread_mutex_destroy(&entry->reconn_lock);
}

/*
 * wtg_api_init():			Initialize a wtg client API set.
 * @entry:				Wtg client API entry.
 * @type:				Program type. WTG_API_TYPE_NWS or WTG_API_TYPE_MCP
 * @send_base:.			0 not send base information, others send base informaion. Send base information means prgram has been restart.
 * @conf_name:			Watchdog config file name, when type is WTG_API_TYPE_NWS, we ignore this param.
 * @base:				Base information. When send_base is zero, we ignore this param.
 * Returns:				0 on success, -1 on error.
 */
int
wtg_api_init(wtg_entry_t *entry, int type, int send_base, const char *conf_name, wtg_api_base_t *base)
{
	if ( !entry ) {
		return -1;
	}
	
	wtg_api_entry_reset(entry, type);

	if ( wtg_api_load_conf(entry, conf_name) ) {
		wa_output("[wtg_api]: Load configuration fail!\n");
		return -1;
	}

	if ( !entry->enabled ) {
		wa_output("[wtg_api]: Not enable wtg!\n");
		return 0;
	}

	if ( wtg_api_conn_connect(entry) ) {
		wa_output("[wtg_api]: Connection to daemon fail!\n");
		return -1;
	}

	if ( send_base ) {
		if ( wtg_api_send_base(entry, base) ) {
			wa_output("[wtg_api]: Send base information fail!\n");
			wtg_api_destroy(entry);
			return -1;
		}
	}
	
	return 0;
}

/*
 * wtg_api_init_v2():		Initialize a wtg client API set V2.
 * @entry:				Wtg client API entry.
 * @type:				Program type. WTG_API_TYPE_NWS or WTG_API_TYPE_MCP
 * @send_base:.			0 not send base information, others send base informaion. Send base information means prgram has been restart.
 * @base:				Base information. When send_base is zero, we ignore this param.
 * @wtg_enable:			-1 use default, 0 not enable, others enable.
 * @unix_sock_addr:		NULL use default, others just the address.
 * Returns:				0 on success, -1 on error.
 */
int
wtg_api_init_v2(wtg_entry_t *entry, int type, int send_base, wtg_api_base_t *base,
					int wtg_enable, char *unix_sock_addr)
{
	if ( !entry ) {
		return -1;
	}
	
	wtg_api_entry_reset(entry, type);

	if ( wtg_api_load_conf_v2(entry, wtg_enable, unix_sock_addr) ) {
		wa_output("[wtg_api]: Load configuration fail!\n");
		return -1;
	}

	if ( !entry->enabled ) {
		wa_output("[wtg_api]: Not enable wtg!\n");
		return 0;
	}

	if ( wtg_api_conn_connect(entry) ) {
		wa_output("[wtg_api]: Connection to daemon fail!\n");
		return -1;
	}

	if ( send_base ) {
		if ( wtg_api_send_base(entry, base) ) {
			wa_output("[wtg_api]: Send base information fail!\n");
			wtg_api_destroy(entry);
			return -1;
		}
	}
	
	return 0;
}

/*
 * wtg_api_report_attr():	Wtg client attribute status report.
 * @entry:				Wtg client API entry.
 * @attrs:				Report attribute status array.
 * @attr_count:			Effective element count of attrs.
 * Returns:				0 on success, -1 on error.
 */
int
wtg_api_report_attr(wtg_entry_t *entry, wtg_api_attr_t *attrs, int attr_count)
{
	wtg_ipc_head_t		*head = (wtg_ipc_head_t *)send_buf;
	wtg_attr_t			*body = NULL;
	int					i, body_len, key_len, val_len;
	char				*cur = NULL, val_str[INT_STR_LEN];

	if ( !entry ) {
		return -1;
	}

	if ( !entry->enabled ) {
		return 0;
	}

	body = (wtg_attr_t *)head->data;

	if ( attr_count <= 0 ) {
		wa_output("[wtg_api]: Invalid attribute count \"%d\" when report!\n", attr_count);
		return -1;
	}

	cur = body->data;

	for ( i = 0; i < attr_count; i++ ) {
		key_len = strlen(attrs[i].key);
		if ( attrs[i].val_type == ATTR_TYPE_INT ) {
			val_len = sprintf(val_str, "%lu", attrs[i].i_val);
		} else if ( attrs[i].val_type == ATTR_TYPE_STRING ) {
			val_len = strlen(attrs[i].str_val);
		} else {
			wa_output("[wtg_api]: Invalid attribute value type \"%d\"!\n", attrs[i].val_type);
			return -1;
		}		

		if ( !key_len || !val_len ) {
			wa_output("[wtg_api]: Reject empty attribute KEY or VALUE!\n");
			return -1;
		}
		if ( ( cur - send_buf + key_len + val_len + (sizeof(int) * 2) ) > SEND_BUF_SIZE ) {
			wa_output("[wtg_api]: Too long status attribute report content!\n");
			return -1;
		}
		
		*((int *)cur) = key_len;
		cur += sizeof(int);
		*((int *)cur) = val_len;
		cur += sizeof(int);

		cur += sprintf(cur, "%s", attrs[i].key);
		if ( attrs[i].val_type == ATTR_TYPE_INT ) {
			cur += sprintf(cur, "%s", val_str);
		} else {
			cur += sprintf(cur, "%s", attrs[i].str_val);
		}
	}
	
	body->length = (unsigned short)(cur - (char *)body);		// Full packet length.
	body_len = body->length;

	wtg_api_build_ipc_head(head, IPC_TYPE_ATTR, body_len);

	if ( wtg_api_common_send(entry, sizeof(wtg_ipc_head_t) + body_len) ) {
		wa_output("[wtg_api]: Send attribute status report network error!\n");
		return -1;
	}

	return 0;
}

/*
 * wtg_api_report_alarm():		Report alarm to wtg master.
 * @entry:					Wtg client API entry.
 * @level:					Alarm level. 1 ~ 5, 1 is most serious.
 * @msg:					Alarm message. Terminate by '\0', and strlen(msg) should <= ALARM_MSG_SIZE - 1.
 * Returns:					0 on success, -1 on error.
 */
int
wtg_api_report_alarm(wtg_entry_t *entry, unsigned char level, const char *msg)
{
	wtg_ipc_head_t		*head = (wtg_ipc_head_t *)send_buf;
	alarm_report_req_v2	*body = NULL;
	int					body_len = sizeof(alarm_report_req_v2);

	if ( !entry ) {
		return -1;
	}

	if ( !entry->enabled ) {
		return 0;
	}

	body = (alarm_report_req_v2 *)head->data;

	memset(body, 0, sizeof(alarm_report_req_v2));
	
	body->time = (unsigned long)time(NULL);
	body->level = level;
	body->type = ALARM_TYPE_CLIENT;

	strncpy(body->msg, msg, ALARM_MSG_SIZE - 1);

	wtg_api_build_ipc_head(head, IPC_TYPE_ALARM, body_len);

	if ( wtg_api_common_send(entry, sizeof(wtg_ipc_head_t) + body_len) ) {
		wa_output("[wtg_api]: Send alarm report network error!\n");
		return -1;
	}

	return 0;
}

/*
 * wtg_api_report_proc:		Report process information.
 * @entry:					Wtg client API entry.
 * @items:					Information items.
 * @item_count:				Effective item count.
 * Returns:					0 on success, -1 on error.
 */
int
wtg_api_report_proc(wtg_entry_t *entry, wtg_api_proc_t *items, unsigned item_count)
{
	wtg_ipc_head_t		*head = (wtg_ipc_head_t *)send_buf;
	report_process_info	*body = NULL;
	int					body_len = sizeof(report_process_info);
	int					i;
	char				*pos = NULL;
	unsigned			content_len = 0;
	int					line_len;

	if ( !entry ) {
		return -1;
	}

	if ( !entry->enabled ) {
		return 0;
	}

	if ( item_count == 0 ) {
		return 0;
	}

	body = (report_process_info *)head->data;

	memset(body, 0, sizeof(report_process_info));

	// Construct message.
	pos = body->content;
	for ( i = 0; i < item_count; i++ ) {
		if ( content_len + strlen(items[i].key) + strlen(items[i].value) + 5 > WTG_PROC_INFO_SIZE ) {
			wa_output("[wtg_api]: Too long proc information content, not complete!\n");
			break;
		}

		line_len = sprintf(pos, "%s##%s\n", items[i].key, items[i].value);
		pos += line_len;
		content_len += line_len;
	}

	if ( content_len + 2 > WTG_PROC_INFO_SIZE ) {
		wa_output("[wtg_api]: Not enough space for proc information content!\n");
		return -1;
	}

	sprintf(pos, "\n");

	wtg_api_build_ipc_head(head, IPC_TYPE_PROC, body_len);

	if ( wtg_api_common_send(entry, sizeof(wtg_ipc_head_t) + body_len) ) {
		wa_output("[wtg_api]: Send process information report network error!\n");
		return -1;
	}

	return 0;
}

