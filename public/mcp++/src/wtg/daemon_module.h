/*
 * daemon_module.h:		Wtg client daemon mudule.
 * Date:					2011-04-08
 */

#ifndef __DAEMON_MODULE_H
#define __DAEMON_MODULE_H

#include <pthread.h>
#include <time.h>
#include <netinet/in.h>

#include "./list.h"
#include "wtg_hash.h"

#define WTG_DEF_STR_LEN					128						// Default string length.

#define WTG_PATH_SIZE					256						// Path string max length.
#define WTG_UFILE_MAX					64						// Max update file count.

/*
 * Network or UNIX domain communicate buffer.
 */
typedef struct {
	int						size;								// Buffer size.
	int						offset;								// First byte in buffer.
	int						length;								// Data length.
	char					*data;								// Buffer data.
	int						need;								// Bytes need to recive (include head). Be set before parse pakcet head. For recive buffer.
} wtg_buff_t;

/*
 * Connection status.
 */
#define WTG_CSTAT_MASTER_NOT_CONNED					1001		// Master connection not connected.
#define WTG_CSTAT_MASTER_RECV_HEAD					1002		// Master connection recive head. 
#define WTG_CSTAT_MASTER_RECV_BODY					1003		// Master connection recive body.
// #define WTG_CSTAT_MASTER_SEND					1004		// Master connection send data.
#define WTG_CSTAT_DOWNLOAD_NOT_CONNED				1101		// File download connection not connected.
#define WTG_CSTAT_DOWNLOAD_RECV_HEAD				1102		// File download connection recive head.
#define WTG_CSTAT_DOWNLOAD_RECV_BODY				1103		// File download connection recive body.
#define WTG_CSTAT_DOWNLOAD_SEND_REQ					1104		// File download connection send HTTP request.
// #define WTG_CSTAT_DOWNLOAD_SEND					1104		// File download connection send data.
#define WTG_CSTAT_DOMAIN_NOT_CONNED					1201		// UNIX domain connection not connected.
#define WTG_CSTAT_DOMAIN_RECV_HEAD					1202		// UNIX domain connection recive head.
#define WTG_CSTAT_DOMAIN_RECV_BODY					1203		// UNIX domain connection recive body.
// #define WTG_CSTAT_DOMAIN_SEND					1204		// UNIX domain connection send data.

/*
 * Update status code.
 */
enum {
	WTG_UPDATE_STAT_IDLE = 0,									// Idle. If not this status, reject update command.
	WTG_UPDATE_STAT_CMD = 1,										// Get command file.
	WTG_UPDATE_STAT_FILE = 2,									// Get update file.
	WTG_UPDATE_STAT_UPDATE = 3,									// Update file.
	WTG_UPDATE_STAT_CTL = 4										// Restart server.
};

/*
 * Update file item.
 */
typedef struct {
	long long				size;								// File size.
	long long				recived;							// File content recived.
	char					uri[WTG_PATH_SIZE];					// Request URI.
	char					dest_path[WTG_PATH_SIZE];			// Dest path.
	char					is_bin;								// 0 not set file x flag, others set file x flag.
	// char					name[WTG_DEF_STR_LEN];				// File name.
} wtg_update_file_t;

/*
 * Update context.
 */
typedef struct {
	unsigned long			id;									// Update task ID.
	
	int						restart;							// 0 not need to restart, others restart.

	char					hostname[WTG_DEF_STR_LEN];			// Download server hostname.
	unsigned short			port;								// Download server PORT.

	int						stat;								// Update status machine.

	char					token[WTG_DEF_STR_LEN];				// Download token.
	int						token_factor;						// Download token factor.
	
	char					cmd_uri[WTG_PATH_SIZE];				// Update command file URI.
	int						cmd_size;							// Update command file size.
	int						cmd_recived;						// Update command file recived bytes.

	int						fd;									// Middle file fd (include command file).

	int						file_cnt;							// Update file count.
	int						cur_idx;							// Current file index.

	wtg_update_file_t		files[WTG_UFILE_MAX];				// Update files.
} wtg_update_context_t;

/*
 * Connection type.
 */
enum {
	WTG_CONN_TYPE_MASTER = 0,									// Connection to wtg master.
	WTG_CONN_TYPE_DOWNLOAD = 1,									// Connection to file server.
	WTG_CONN_TYPE_DOMAIN = 2									// Connection of UNIX domain.
};

/*
 * Network or UNIX domain connection.
 */
typedef struct {
	int						type;								// Connection type.
	int						stat;								// Connection status. No send status.
	time_t					timeout;							// Connection timeout time.

	int						skt;								// Socket fd.

	wtg_buff_t				recv_buf;							// Recive buffer.
	wtg_buff_t				send_buf;							// Send buffer

	list_head_t				list;								// List node.
} wtg_conn_t;

/*
 * Connection pool.
 */
typedef struct {
	int						total_cnt;							// Total connection count.
	wtg_conn_t				*conns;								// Connections.
	wtg_hash_t				used_hash;							// Hash table for connection be used.
	list_head_t				used;								// Connection be used.
	list_head_t				free;								// Free connections.
} wtg_conn_pool_t;

/*
 * Wtg daemon type.
 */
enum {
	WTG_D_MCP =				1,									// Daemon in MCP watchdog.
	WTG_D_NWS =				2,									// Daemon in NWS watchdog.
	WTG_D_WTGD =			3									// Daemon not in watchdog.
};

/*
 * Daemon configuration.
 */
typedef struct {
	int						enabled;							// 0 not enabled, others not enabled.
	int						type;								// Wtg type, WTG_D_MCP, WTG_D_NWS, WTG_D_WTGD
	// in_addr_t				master_ip;							// Wtg master server IP.
	char					master_name[WTG_DEF_STR_LEN];		// Wtg master name.
	unsigned short			master_port;						// Wtg master server PORT.
	// in_addr_t				download_ip;						// Wtg download file server IP.
	// unsigned short			download_port;						// Wtg download file server PORT.
	in_addr_t				client_ip;							// Wtg client main IP.
	unsigned short			client_port;						// Wtg cient main PORT.
	int						report_buf_size;					// Wtg report network buffer size.
	int						mrecv_buf_size;						// Wtg master recive buffer size.
	int						master_timeout;						// Wtg master connection timeout time.
	int						domain_rbuf_size;					// Wtg UNIX domain recive buffer size.
	int						domain_max_conn;					// Wtg UNIX domain connection max count.
	char					domain_address[WTG_DEF_STR_LEN];	// Wtg UNIX domain socket address.
	int						domain_timeout;						// Wtg UNIX domain connection timeout time.
	char					log_name[WTG_DEF_STR_LEN];			// Wtg nws log name. Only NWS use.
	int						download_sbuf_size;					// Wtg update connection send buffer size.
	int						download_rbuf_size;					// Wtg update connection recive buffer size.
	int						download_timeout;					// Wtg update connection timeout time.
	// char					token[WTG_DEF_STR_LEN];				// Wtg download token.
	// int						token_factor;						// Wtg download factor.

	int						epfd;								// EPOLL fd.
	wtg_conn_t				master_conn;						// Network connection to wtg master.
	int						domain_server_skt;					// UNIX domain listen skt.
	wtg_conn_pool_t			domain_conns;						// UNIX domain connection pool.
	wtg_conn_t				download_conn;						// Update connection.

	pthread_t				thd_id;								// Wtg module woker thread ID.
	int						stop;								// 0 not stop wtg, other stop wtg.

	int						is_sub;								// 0 watchdog process, others sub process.

	wtg_update_context_t	update_ctx;							// Update context.
} wtg_daemon_info_t;

#endif

