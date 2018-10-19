/*
 * dev_info.c:			Device information report for wtg..
 * Date:				2011-05-24
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "wtg_client_api.h"
#include "dev_info.h"

using namespace tfc::wtgapi;

extern CWtgClientApi			*wtg;							// Wtg client. For MCP++.

#define SEC_PER_TICK			10								// Seconds per tick.

#define DEV_FILE_RBUF_SIZE		131072							// Device info file read buffer size.

#define DEV_STR_LEN				64								// String default length.

#define ETH_FLOW_FILE			"/proc/net/dev"					// Eth flow file path.
#define ETH_ITEM_CNT			32								// Eth item max count.

static __thread char			dev_rbuf[DEV_FILE_RBUF_SIZE];	// dev file read buffer.

/*
 * Eth flow value. 0 recive, 1 transmit.
 */
typedef struct {
	unsigned long long			last_bytes[2];					// Bytes last time.
	unsigned long long			val_bytes[2];					// Current report bytes value.

	unsigned long long			last_pkts[2];					// Packets last time.
	unsigned long long			val_pkts[2];					// Current report pakcet value.
} eth_value_t;

// Eth key names.
static char						eth_keys[3][4][DEV_STR_LEN] = {
		{"eth_flow#eth0_recive_bytes", "eth_flow#eth0_recive_packets", "eth_flow#eth0_transmit_bytes", "eth_flow#eth0_transmit_packets"},
		{"eth_flow#eth1_recive_bytes", "eth_flow#eth1_recive_packets", "eth_flow#eth1_transmit_bytes", "eth_flow#eth1_transmit_packets"},
		{"eth_flow#lo_recive_bytes", "eth_flow#lo_recive_packets", "eth_flow#lo_transmit_bytes", "eth_flow#lo_transmit_packets"}
	};

#ifdef __DEBUG
#define dbg_output(fmt, args...) printf(fmt, ##args)
#else
#define dbg_output(fmt, args...)
#endif

/*
 * dev_strtrim():		Erase LWS in a string head and tail.
 * @str:				String will be handle.
 * Returns:			Pointer to string handled.
 * NOTES:			Modified from NWS.
 */
static char*
dev_strtrim(char* str)
{
	char		*val = NULL;
	
	if( !str ) {
		return NULL;
	}

	for ( ; *str != 0 && (*str == ' ' || *str == '\t'); str++);
	
	val = str + strlen( str ) - 1;
	
	while( val > str && *val != 0 && (*val == ' ' || *val == '\t') ) {
		*val-- = 0;
	}
	
	return str;
}

/*
 * dev_strparse():		Parse string by delim.
 * @str:				Source string.
 * @pstr:			Delim string.
 * @end:				Ending string.
 * @val:				To store pointer to the sub string.
 * @count:			Arrary elements's count of val.
 * NOTES:			Modified from NWS.
 */
static void
dev_strparse(char *str, char *pstr, char *end, char **val, int count)
{
	char				*pnext = NULL;
	int					i = 0;
	
	for( i = 0; i < count && str != NULL; i++ ) {
		val[i] = dev_strtrim( strtok_r(str, pstr, &pnext) );
		if( val[i] == NULL || ( end != NULL && strcmp(val[i], end) == 0 ) ) {
			val[i] = NULL;
			break;
		}
		str = pnext;
	}
}

/*
 * eth_get_content():	Get eth flow file content.
 * @name:			File name.
 * Returns:			Return file content in bytes on success, -1 on error, 0 on empty file.
 * NOTES:			Content store in dev_rbuf and terminated by '\0'.
 */
static inline int
get_file_content(const char *name)
{
	int							fd = -1, ret = -1;

	fd = open(name, O_RDONLY);
	if ( fd == -1 ) {
		dbg_output("Open dev info file \"%s\" fail! %m\n", name);
		return -1;
	}

	ret = read(fd, dev_rbuf, DEV_FILE_RBUF_SIZE - 1);
	if ( ret <= 0 ) {
		*dev_rbuf = 0;
		dbg_output("Read dev info file \"%s\" fail or empty file! %m\n", name);
	} else {
		dev_rbuf[ret] = 0;
	}
	
	if ( fd >= 0 ) {
		close(fd);
		fd = -1;
	}

	return ret;
}

/*
 * eth_get_val():		Get eth values.
 * @line:				Value line.
 * @val:				To store values.
 * Returns:			0 on success, -1 on error.
 */
static inline int
eth_get_val(char *line, eth_value_t *val)
{
	char					*items[16];

	memset(items, 0, sizeof(char *) * 16);

	dev_strparse(line, " ", "", items, 16);

	if ( items[0] == NULL || items[1] == NULL || items[8] == NULL || items[9] == NULL ) {
		dbg_output("Parse value line error!\n");
		return -1;
	}

	val->val_bytes[0] = (unsigned long long)(strtoll(items[0], NULL, 10));
	if ( ( val->val_bytes[0] == 0 && errno == EINVAL )
		|| ( (val->val_bytes[0] == (unsigned long long)LLONG_MIN
		|| val->val_bytes[0] == (unsigned long long)LLONG_MAX)
		&& errno == ERANGE ) ) {
		dbg_output("Get value 0 error (recive bytes)!\n");
		return -1;
	}

	val->val_pkts[0] = (unsigned long long)(strtoll(items[1], NULL, 10));
	if ( ( val->val_pkts[0] == 0 && errno == EINVAL )
		|| ( (val->val_pkts[0] == (unsigned long long)LLONG_MIN
		|| val->val_pkts[0] == (unsigned long long)LLONG_MAX)
		&& errno == ERANGE ) ) {
		dbg_output("Get value 1 error (recive packets)!\n");
		return -1;
	}

	val->val_bytes[1] = (unsigned long long)(strtoll(items[8], NULL, 10));
	if ( ( val->val_bytes[1] == 0 && errno == EINVAL )
		|| ( (val->val_bytes[1] == (unsigned long long)LLONG_MIN
		|| val->val_bytes[1] == (unsigned long long)LLONG_MAX)
		&& errno == ERANGE ) ) {
		dbg_output("Get value 2 error (transmit bytes)!\n");
		return -1;
	}

	val->val_pkts[1] = (unsigned long long)(strtoll(items[9], NULL, 10));
	if ( ( val->val_pkts[1] == 0 && errno == EINVAL )
		|| ( (val->val_pkts[1] == (unsigned long long)LLONG_MIN
		|| val->val_pkts[1] == (unsigned long long)LLONG_MAX)
		&& errno == ERANGE ) ) {
		dbg_output("Get value 3 error (transmit packets)!\n");
		return -1;
	}

	return 0;
}

/*
 * eth_calc():				Just calucate value.
 * @tick:					Report tick.
 * @last;					Last value.
 * @cur:					Current value.
 * Returns:				Return the result.
 */
static inline unsigned int
eth_calc(int tick, unsigned long long last, unsigned long long cur)
{
	if ( cur >= last ) {
		return (unsigned int)( (cur - last) / ((unsigned int)(tick * SEC_PER_TICK)) );
	} else {
		// LONG_MAX?
		return (unsigned int)( cur / ((unsigned int)(tick * SEC_PER_TICK)) );	// Discard last ~ LONG_MAX or LLONG_MAX.
	}
}

/*
 * eth_calc_report_vals():	Calucate eth report values.
 * @tick:					Report tick.
 * @val:					Eth values.
 * @results:				Report values.
 */
static inline void
eth_calc_report_vals(int tick, eth_value_t *val, unsigned int *results)
{
	results[0] = eth_calc(tick, val->last_bytes[0], val->val_bytes[0]);
	results[1] = eth_calc(tick, val->last_pkts[0], val->val_pkts[0]);
	results[2] = eth_calc(tick, val->last_bytes[1], val->val_bytes[1]);
	results[3] = eth_calc(tick, val->last_pkts[1], val->val_pkts[1]);
}

/*
 * eth_flow_report():	Report eth flow.
 * @tick:				Report tick(10 sec).
 * Returns:			0 on success, -1 on error.
 * NOTES:			Not thread safe.
 */
int
eth_flow_report(int tick)
{
	int						rbytes = 0;
	char					*eth_items[ETH_ITEM_CNT];
	wtg_api_attr_t			eth_attrs[4];
	static eth_value_t		eth_vals[3];					// Eth values. 0 eth0, 1 eth1, 2 lo.
	static int				global_handled[3] = {0, 0, 0};	// Eth global handled. 0 eth0, 1 eth1, 2 lo.
	int						handled[3] = {0, 0, 0};			// Eth handled 0 eth0, 1 eth1, 2 lo.
	int						i;
	unsigned int			results[4];

	if ( (rbytes = get_file_content(ETH_FLOW_FILE)) <= 0 ) {
		dbg_output("Get eth file content fail!\n");
		return -1;
	}

	memset(eth_items, 0, sizeof(char *) * ETH_ITEM_CNT);

	dev_strparse(dev_rbuf, "\n", "", eth_items, ETH_ITEM_CNT);

	for ( i = 0; eth_items[i] != NULL && i < ETH_ITEM_CNT; i++ ) {
		if ( handled[0] && handled[1] && handled[2] ) {
			// Complete.
			break;
		}
		
		if ( handled[0] == 0 ) {
			if ( !strncmp(eth_items[i], "eth0:", 5) ) {
				if ( eth_get_val(eth_items[i] + 5, &(eth_vals[0])) ) {
					dbg_output("Get eth0 value error!\n");
					continue;
				}

				if ( !global_handled[0] ) {
					// Not report first time. Do nothing.
				} else {
					// Report.
					eth_calc_report_vals(tick, &(eth_vals[0]), results);

					eth_attrs[0].val_type = ATTR_TYPE_INT;
					eth_attrs[0].key = eth_keys[0][0];
					eth_attrs[0].i_val = results[0];

					eth_attrs[1].val_type = ATTR_TYPE_INT;
					eth_attrs[1].key = eth_keys[0][1];
					eth_attrs[1].i_val = results[1];

					eth_attrs[2].val_type = ATTR_TYPE_INT;
					eth_attrs[2].key = eth_keys[0][2];
					eth_attrs[2].i_val = results[2];

					eth_attrs[3].val_type = ATTR_TYPE_INT;
					eth_attrs[3].key = eth_keys[0][3];
					eth_attrs[3].i_val = results[3];

					wtg->ReportAttr(eth_attrs, 4);
				}

				// Move corrent to last.
				eth_vals[0].last_bytes[0] = eth_vals[0].val_bytes[0];
				eth_vals[0].last_bytes[1] = eth_vals[0].val_bytes[1];
				eth_vals[0].last_pkts[0] = eth_vals[0].val_pkts[0];
				eth_vals[0].last_pkts[1] = eth_vals[0].val_pkts[1];

				global_handled[0] = 1;
				handled[0] = 1;

				continue;
			}
		}

		if ( handled[1] == 0 ) {
			if ( !strncmp(eth_items[i], "eth1:", 5) ) {
				if ( eth_get_val(eth_items[i] + 5, &(eth_vals[1])) ) {
					dbg_output("Get eth1 value error!\n");
					continue;
				}

				if ( !global_handled[1] ) {
					// Not report first time. Do nothing.
				} else {
					// Report.
					eth_calc_report_vals(tick, &(eth_vals[1]), results);

					eth_attrs[0].val_type = ATTR_TYPE_INT;
					eth_attrs[0].key = eth_keys[1][0];
					eth_attrs[0].i_val = results[0];

					eth_attrs[1].val_type = ATTR_TYPE_INT;
					eth_attrs[1].key = eth_keys[1][1];
					eth_attrs[1].i_val = results[1];

					eth_attrs[2].val_type = ATTR_TYPE_INT;
					eth_attrs[2].key = eth_keys[1][2];
					eth_attrs[2].i_val = results[2];

					eth_attrs[3].val_type = ATTR_TYPE_INT;
					eth_attrs[3].key = eth_keys[1][3];
					eth_attrs[3].i_val = results[3];

					wtg->ReportAttr(eth_attrs, 4);
				}

				// Move corrent to last.
				eth_vals[1].last_bytes[0] = eth_vals[1].val_bytes[0];
				eth_vals[1].last_bytes[1] = eth_vals[1].val_bytes[1];
				eth_vals[1].last_pkts[0] = eth_vals[1].val_pkts[0];
				eth_vals[1].last_pkts[1] = eth_vals[1].val_pkts[1];

				global_handled[1] = 1;
				handled[1] = 1;

				continue;
			}
		}

		if ( handled[2] == 0 ) {
			if ( !strncmp(eth_items[i], "lo:", 3) ) {
				if ( eth_get_val(eth_items[i] + 3, &(eth_vals[2])) ) {
					dbg_output("Get lo value error!\n");
					continue;
				}

				if ( !global_handled[2] ) {
					// Not report first time. Do nothing.
				} else {
					// Report.
					eth_calc_report_vals(tick, &(eth_vals[2]), results);

					eth_attrs[0].val_type = ATTR_TYPE_INT;
					eth_attrs[0].key = eth_keys[2][0];
					eth_attrs[0].i_val = results[0];

					eth_attrs[1].val_type = ATTR_TYPE_INT;
					eth_attrs[1].key = eth_keys[2][1];
					eth_attrs[1].i_val = results[1];

					eth_attrs[2].val_type = ATTR_TYPE_INT;
					eth_attrs[2].key = eth_keys[2][2];
					eth_attrs[2].i_val = results[2];

					eth_attrs[3].val_type = ATTR_TYPE_INT;
					eth_attrs[3].key = eth_keys[2][3];
					eth_attrs[3].i_val = results[3];

					wtg->ReportAttr(eth_attrs, 4);
				}

				// Move corrent to last.
				eth_vals[2].last_bytes[0] = eth_vals[2].val_bytes[0];
				eth_vals[2].last_bytes[1] = eth_vals[2].val_bytes[1];
				eth_vals[2].last_pkts[0] = eth_vals[2].val_pkts[0];
				eth_vals[2].last_pkts[1] = eth_vals[2].val_pkts[1];

				global_handled[2] = 1;
				handled[2] = 1;

				continue;
			}
		}
	}

	return 0;
}
