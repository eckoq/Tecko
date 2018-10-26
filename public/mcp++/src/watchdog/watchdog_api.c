/*
 * watchdog_api.c:	Watchdog client APIs.
 * Date:				2011-02-23
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdarg.h>
#include <asm/atomic.h>
#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "watchdog_common.h"
#include "watchdog_api.h"

__BEGIN_DECLS
#include "watchdog_qnf_myconfig.h"
__END_DECLS

/*
 * watchdog_client_output():		Print information to stdout.
 * Notes:							The same as printf but the compile switch and return nothing.
 */
static void
watchdog_client_output(const char *fmt, ...)
{
	va_list		ap;
	va_start(ap, fmt);

#ifdef __OUTPUT_INFO
	vprintf(fmt, ap);
#endif

	va_end(ap);
}

/*
 * watchdog_client_reset_entry():	Watchdog client entry reset.
 * @client:						Point to the client entry.
 */
inline static void
watchdog_client_reset_entry(watchdog_client_t *client)
{
	memset(client, 0, sizeof(watchdog_client_t));
	client->shm_key = -1;
	client->shm_fd = -1;
	client->shm_addr = (void *)-1;
	client->pid = -1;
}

/*
 * watchdog_client_load_conf():	Load watchdog configuration.
 * @client:					Point to the client entry.
 * @conf_name:				Watchdog configure file name.
 * Returns:					0 on success, -1 on error.
 */
inline static int
watchdog_client_load_conf(watchdog_client_t *client, const char *conf_name)
{
	char		**argv = NULL;
	int			val;

	argv = (char **)&conf_name;

	if ( strlen(conf_name) >= NAME_STR_MAX ) {
		watchdog_client_output("[watchdog_clent] Too long file name \"%s\"!\n", conf_name);
		return -1;
	}
	
	if ( myconfig_init(1, argv, 1) ) {
		watchdog_client_output("[watchdog_clent] Config init fail!\n");
		return -1;
	}

	strncpy(client->conf_name, conf_name, NAME_STR_MAX - 1);

	val = myconfig_get_intval("watchdog_key", -1);
	if ( val == -1 ) {
		watchdog_client_output("[watchdog_clent] \"watchdog_key\" error or not found in configure file.\n");
		return -1;
	}
	client->shm_key = (key_t)val;	
	
	return 0;
}

/*
 * watchdog_client_load_shm():	Load watchdog shm.
 * @client:					Point to the client entry.
 * Returns:					0 on success, -1 on error.
 */
inline static int
watchdog_client_load_shm(watchdog_client_t *client)
{
	client->shm_fd = shmget(client->shm_key, watchdog_calc_shm_size(), 0);
	if ( client->shm_fd == -1 ) {
		watchdog_client_output("[watchdog_clent] Get shm fail! KEY %d. %m\n", client->shm_key);
		return -1;
	}

	client->shm_addr = shmat(client->shm_fd, NULL, 0);
	if ( client->shm_addr == (void *)(-1) ) {
		watchdog_client_output("[watchdog_clent] Shm attach fail! KEY %ld, ID %ld.\n",
			client->shm_key, client->shm_fd);
		return -1;
	}

	return 0;
}

/*
 * watchdog_client_load_proc():	Get process information.
 * @client:					Point to the client entry.
 * @proc_type:				Process type.
 * @frame_version:			Frame version.
 * @plugin_version:			Plugin version.
 * @server_ports:				Server PORTs.
 * @add_info_0:				Addition information 0.
 * @add_info_1:				Addition information 1.
 * Returns:					0 on success, -1 on process information not found.
 */
static int
watchdog_client_load_proc(watchdog_client_t *client,
						int proc_type, const char *frame_version, const char *plugin_version,
						const char *server_ports,
						const char *add_info_0, const char *add_info_1)
{
	int				i;
	proc_tab_t		*p_proc_tab = NULL;
	
	client->pid = getpid();

	p_proc_tab = (proc_tab_t *)(client->shm_addr);
	
	for ( i = 0; i < PROC_MAX_COUNT; i++ ) {
		if ( p_proc_tab->procs[i].used && p_proc_tab->procs[i].pid == client->pid ) {
			client->p_proc_info = &p_proc_tab->procs[i];
			atomic_inc((atomic_t *)&client->p_proc_info->tick_cnt);
			client->cmd_idx = i;

			client->p_proc_info->proc_type = proc_type;
			
			memset(client->p_proc_info->frame_version, 0, ADD_INFO_SIZE);
			memset(client->p_proc_info->plugin_version, 0, ADD_INFO_SIZE);
			memset(client->p_proc_info->server_ports, 0, ADD_INFO_SIZE);
			memset(client->p_proc_info->add_info_0, 0, ADD_INFO_SIZE);
			memset(client->p_proc_info->add_info_1, 0, ADD_INFO_SIZE);

			if ( frame_version ) {
				strncpy(client->p_proc_info->frame_version, frame_version, ADD_INFO_SIZE - 1);
			}

			if ( plugin_version ) {
				strncpy(client->p_proc_info->plugin_version, plugin_version, ADD_INFO_SIZE - 1);
			}

			if ( server_ports ) {
				strncpy(client->p_proc_info->server_ports, server_ports, ADD_INFO_SIZE - 1);
			}

			if ( add_info_0 ) {
				strncpy(client->p_proc_info->add_info_0, add_info_0, ADD_INFO_SIZE - 1);
			}

			if ( add_info_1 ) {
				strncpy(client->p_proc_info->add_info_1, add_info_1, ADD_INFO_SIZE - 1);
			}
			
			return 0;
		}
	}
	
	return -1;
}

/*
 * watchdog_client_shm_clean():	Clean up shm.
 * @client:					Point to the client entry.
 */
inline static void
watchdog_client_shm_clean(watchdog_client_t *client)
{
	if ( client->shm_addr != (void *)(-1) ) {
		if ( shmdt(client->shm_addr) == -1 ) {
			watchdog_client_output("[watchdog_clent] Shm detach fail!\n");
			return;
		}
	}

	client->shm_addr = (void *)(-1);
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
int
watchdog_client_init(watchdog_client_t *client, const char *conf_name,
						int proc_type, const char *frame_version, const char *plugin_version,
						const char *server_ports,
						const char *add_info_0, const char *add_info_1)
{
	if ( !client ) {
		watchdog_client_output("[watchdog_clent] Empty client!\n");
		return -1;
	}

	watchdog_client_reset_entry(client);

	if ( watchdog_client_load_conf(client, conf_name) ) {
		watchdog_client_output("[watchdog_clent] Load watchdog configuration fail! Configure file name is \"%s\"\n",
			conf_name);
		return -1;
	}

	if ( watchdog_client_load_shm(client) ) {
		watchdog_client_output("[watchdog_clent] Load shm fail!\n");
		return -1;
	}

	if ( watchdog_client_load_proc(client, proc_type,
			frame_version, plugin_version, server_ports,
			add_info_0, add_info_1) ) {
		watchdog_client_output("[watchdog_clent] Get process information fail! Process not found!\n");
		watchdog_client_shm_clean(client);
		return -1;
	}
	
	return 0;
}

/*
 * watchdog_client_touch():	Touch process table in shm. Declear not dead.
 * @client:					Point to the client entry.
 * Returns:					0 on success, -1 on error.
 */
int
watchdog_client_touch(watchdog_client_t *client)
{
	if ( !client ) {
		watchdog_client_output("[watchdog_clent] Enpty watchdog entry when touch!\n");
		return -1;
	}

	if ( !client->p_proc_info ) {
		watchdog_client_output("[watchdog_clent] Enpty process node pointer when touch!\n");
		return -1;
	}

	if ( !client->p_proc_info->used ) {
		watchdog_client_output("[watchdog_clent] Invalid process node in shm when touch!\n");
		return -1;
	}

	if ( client->p_proc_info->pid != client->pid ) {
		watchdog_client_output("[watchdog_clent] Process ID is not the same as node in shm!\n");
		return -1;
	}
	
	atomic_inc((atomic_t *)&client->p_proc_info->tick_cnt);
	
	return 0;
}

/*
 * watchdog_clinet_index():		Get process index. Help wtg.
 * @client:					Point to the client entry.
 * Returns:					The process index or -1 on error.
 */
int
watchdog_client_index(watchdog_client_t *client)
{
	if ( !client ) {
		watchdog_client_output("[watchdog_clent] Enpty watchdog entry when get index!\n");
		return -1;
	}

	return client->cmd_idx;
}

/*
 * watchdog_clinet_destroy():	Watchdog client cleanup.
 * @client:					Point to the client entry.
 */
void
watchdog_clinet_destroy(watchdog_client_t *client)
{
	watchdog_client_shm_clean(client);
	watchdog_client_reset_entry(client);
}

