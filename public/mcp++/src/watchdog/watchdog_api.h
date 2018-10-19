/*
 * watchdog_api.h:	Watchdog client APIs.
 * Date:				2011-02-23
 */

#ifndef __WATCHDOG_API_H
#define __WATCHDOG_API_H

#include <stdlib.h>
#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <netinet/in.h>

#include "watchdog_ipc.h"

__BEGIN_DECLS

#ifdef FOR_IDE				// Never define.
struct for_ide {};
#endif

#ifndef CHLD_EXEC_FAIL_EXIT
#define CHLD_EXEC_FAIL_EXIT	21					// Exit code could stop watchdog daemon.
#endif

#define NAME_STR_MAX		256					// Max file path length.

/*
 * Watchdog client information.
 */
typedef struct watchdog_client {
	// Configuration.
	char			conf_name[NAME_STR_MAX];	// Configure file name.
	key_t			shm_key;					// Shm KEY.

	// Runtime information.
	pid_t			pid;						// Process ID.
	int				shm_fd;						// Shm fd.
	void			*shm_addr;					// Shm attached address.
	proc_info_t		*p_proc_info;				// Process information. Pointer to the process's information in shm.
	int				cmd_idx;					// Program index.
} watchdog_client_t;

/*
 * err_exit():			Call this when frame initialize fail. Watchdog will stop.
 */
static inline void
err_exit()
{
	exit(CHLD_EXEC_FAIL_EXIT);
}

/*
 * watchdog_client_init():		Initialize the watchdog client.
 * @client:					Point to the client entry.
 * @conf_name:				Watchdog configure file name.
 * @proc_type:				Process type.
 * @frame_version:			Frame version.
 * @plugin_version:			Plugin version.
 * @server_ports:				Server PORTs. Divided by ','.
 * @add_info_0:				Addition information 0.
 * @add_info_1:				Addition information 1.
 * Returns:					0 on success, -1 on error.
 */
extern int
watchdog_client_init(watchdog_client_t *client, const char *conf_name,
						int proc_type, const char *frame_version, const char *plugin_version,
						const char *server_ports,
						const char *add_info_0, const char *add_info_1);

/*
 * watchdog_client_touch():	Touch process table in shm. Declear not dead.
 * @client:					Point to the client entry.
 * Returns:					0 on success, -1 on error.
 */
extern int
watchdog_client_touch(watchdog_client_t *client);

/*
 * watchdog_clinet_index():		Get process index. Help wtg.
 * @client:					Point to the client entry.
 * Returns:					The process index or -1 on error.
 */
extern int
watchdog_client_index(watchdog_client_t *client);

/*
 * watchdog_clinet_destroy():	Watchdog client cleanup.
 * @client:					Point to the client entry.
 */
extern void
watchdog_clinet_destroy(watchdog_client_t *client);

__END_DECLS

#endif

