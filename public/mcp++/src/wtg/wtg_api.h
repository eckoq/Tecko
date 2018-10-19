/*
 * wtg_api.h:			Wtg client APIs.
 * Date:				2011-04-12
 */

#ifndef __WTG_API_H
#define __WTG_API_H

#include <sys/cdefs.h>
#include <time.h>
#include <pthread.h>

__BEGIN_DECLS

#ifdef __FOR_IDE
struct for_ide {};
#endif

#define PORT_MAX_CNT		32					// Max count of program server PORT in base information.

#ifndef VER_STR_LEN
#define VER_STR_LEN			64					// Version string length.
#endif

#ifndef WTG_DEF_STR_LEN
#define WTG_DEF_STR_LEN		128					// Default string length.
#endif

#ifndef WTG_DEF_DOMAIN_ADDR
#define WTG_DEF_DOMAIN_ADDR		"./wtg_domain.skt"		// Wtg default UNIX domain address.
#endif

#ifndef BASE_ITEM_SIZE
#define BASE_ITEM_SIZE		256					// Base information string item max length.
#endif

#ifndef WTG_PROC_ITEM_SIZE
#define WTG_PROC_ITEM_SIZE	128					// Process item string size.
#endif

/*
 * Program type.
 */
enum {
	WTG_API_TYPE_NWS = 1,						// Nws.
	WTG_API_TYPE_MCP = 2						// Mcp++.
};

/*
 * Wtg client entry.
 */
typedef struct {
	int					enabled;				// 0 not enabled wtg, others enabled wtg.
	int					type;					// WTG_API_TYPE_NWS or WTG_API_TYPE_MCP.
	char				addr[WTG_DEF_STR_LEN];	// UNIX Domain IPC server address.
	int					skt;					// UNIX domain IPC socket fd.
	time_t				retry_time;				// If reconnec error, next retry time.
	int					retry_cnt;				// Reconnect error count. Add retry time, never retry when reach max.
	pthread_mutex_t		reconn_lock;			// Lock for reconnect or close. Actually not lock in most case.
} wtg_entry_t;

/*
 * Base information.
 */
typedef struct {
	char			ver[VER_STR_LEN];			// Program version.
	int				port_cnt;					// Count of server PORT.
	unsigned short	ports[PORT_MAX_CNT];		// Server Ports.
	char			plugins[BASE_ITEM_SIZE];	// Plugins. Divid by ';'. Must terminated by '\0.'
	char			prog_path[BASE_ITEM_SIZE];	// Binary program path. Must terminated by '\0.'
	char			conf_path[BASE_ITEM_SIZE];	// Configure file path. Must terminated by '\0.'
} wtg_api_base_t;

/*
 * Value of wtg_api_attr_t::val_type.
 */
enum {
	ATTR_TYPE_STRING =		1,					// Attribute type string.
	ATTR_TYPE_INT =			2					// Attribute type integer.
};

/*
 * Status attribute Report information.
 */
typedef struct {
	int					val_type;				// Attribute value type. ATTR_TYPE_STRING or ATTR_TYPE_INT.
	char				*key;					// Attribute KEY. Must terminated by '\0.'
	union {
		char			*str_val;				// Attribute String Value. Must terminated by '\0.'
		unsigned long	i_val;					// Attribute Integer value.
	};
} wtg_api_attr_t;

/*
 * Process information item.
 */
typedef struct {
	char				key[WTG_PROC_ITEM_SIZE];	// KEY.
	char				value[WTG_PROC_ITEM_SIZE];	// String value.
} wtg_api_proc_t;

/*
 * wtg_api_init():			Initialize a wtg client API set.
 * @entry:				Wtg client API entry.
 * @type:				Program type. WTG_API_TYPE_NWS or WTG_API_TYPE_MCP
 * @send_base:.			0 not send base information, others send base informaion. Send base information means prgram has been restart.
 * @conf_name:			Watchdog config file name, when type is WTG_API_TYPE_NWS, we ignore this param.
 * @base:				Base information. When send_base is zero, we ignore this param.
 * Returns:				0 on success, -1 on error.
 */
extern int
wtg_api_init(wtg_entry_t *entry, int type, int send_base, const char *conf_name, wtg_api_base_t *base);

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
extern int
wtg_api_init_v2(wtg_entry_t *entry, int type, int send_base, wtg_api_base_t *base,
					int wtg_enable, char *unix_sock_addr);

/*
 * wtg_api_destroy():		Wtg client API set cleanup.
 * @entry:				Wtg client API entry.
 */
extern void
wtg_api_destroy(wtg_entry_t *entry);

/*
 * wtg_api_report_attr():	Wtg client attribute status report.
 * @entry:				Wtg client API entry.
 * @attrs:				Report attribute status array.
 * @attr_count:			Effective element count of attrs.
 * Returns:				0 on success, -1 on error.
 */
extern int
wtg_api_report_attr(wtg_entry_t *entry, wtg_api_attr_t *attrs, int attr_count);

/*
 * wtg_api_report_alarm():		Report alarm to wtg master.
 * @entry:					Wtg client API entry.
 * @level:					Alarm level. 1 ~ 5, 1 is most serious.
 * @msg:					Alarm message. Terminate by '\0', and strlen(msg) should <= ALARM_MSG_SIZE - 1.
 * Returns:					0 on success, -1 on error.
 */
extern int
wtg_api_report_alarm(wtg_entry_t *entry, unsigned char level, const char *msg);

/*
 * wtg_api_report_proc:		Report process information.
 * @entry:					Wtg client API entry.
 * @items:					Information items.
 * @item_count:				Effective item count.
 * Returns:					0 on success, -1 on error.
 */
extern int
wtg_api_report_proc(wtg_entry_t *entry, wtg_api_proc_t *items, unsigned item_count);

__END_DECLS

#endif

