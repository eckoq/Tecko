/*
 * watchdog_common.h:	watchdog common head file.
 * Date:					2011-02-17
 */

#ifndef __WATCHDOG_COMMON_H
#define __WATCHDOG_COMMON_H

#include <unistd.h>
#include <sys/types.h>
#include <netinet/in.h>

#include "watchdog_qnf_list.h"
#include "watchdog_myhash.h"
#include "watchdog_ipc.h"

#define CHECK_TICK			10				// Deadlock check tick(sec).

#ifndef CHLD_EXEC_FAIL_EXIT
#define CHLD_EXEC_FAIL_EXIT	21				// Exit code when child process execv fail. Call exit() with this will stop watchdog.
#endif

#define PROC_MAX_COUNT		128				// Max count of processes. Should not too large because of search performance.
#define ARG_MAX_COUNT		128				// Max count of command line args.
#define MAX_PATH_LEN		256				// Max file path length.

#define BASE_SIZE			1024			// Base data buffer size.
#define STAT_SIZE			4096			// Status data buffer size.

/*
 * Stopped process information.
 */
typedef struct stop_proc {
	pid_t			pid;					// Process ID.
	int				cmd_idx;				// Index in command line table and shm table. Also ID.

	list_head_t		list;					// List node.
} stop_proc_t;

/*
 * Process table.
 */
typedef struct proc_tab {
	long			shm_size;				// Share memory size.
	int				proc_cnt;				// Count of processes, equal to watchdog_info.proc_cnt.
	proc_info_t		procs[PROC_MAX_COUNT];	// Process information array.
} proc_tab_t;

/*
 * Command line item status code.
 */
enum {
	CMD_NOT_INITED =		0,
	CMD_INITED =			1,
	CMD_RUNNING =			2,
	CMD_DEAD =				3,
	CMD_STOP =				4,
	CMD_RESTART =			5 };

/*
 * Process command line item's status.
 */
typedef struct cmd_stat {
	int				argc;					// Count of args.
	char			*argv[ARG_MAX_COUNT];	// Arg strings.
	
	pid_t			pid;					// Process ID.
	int				cmd_idx;				// Index in command line table and shm table. Also ID
	int				status;					// Process running status.

	pid_t			stopped_pid;			// Pid saved for process has been stopped.

	list_head_t		list;					// List node.	
} cmd_stat_t;

/*
 * watchdog program information
 */
typedef struct watchdog_info {
	// Configuration.
	key_t			shm_key;							// Share memory KEY.

	unsigned short	local_id;							// Local ID for initialize wtg client. 0 is not set local_id, use default.
	
	int				timeout;							// Process timeout.
	int				deadlock_cnt;						// Process' tick_cnt reach this count means deadlock.
	
	char			log_name[MAX_PATH_LEN];				// Log file name.
	long long		log_rotate_size;					// Log rotate size.

	int				deadlock_kill_enable;				// 0 don't kill when deadlock found, others kill when deadlock.

	int				foreground;							// 0 on run as daemon, others on run foreground.

	unsigned		strict_check;						// Always check wether active process count equal the process count be config with wtg warnning.

	unsigned		force_exit;							// Kill with SIGKILL when process could not kill with SIGTERM on exit. Default 0.
	unsigned		force_exit_wait;					// Sleep seconds between SIGTERM and SIGKILL when force_exit enabled. Default 2.

	char			init_shell[MAX_PATH_LEN];			// Shell script call when start. Don't config this item when no need.
	char			exit_shell[MAX_PATH_LEN];			// Shell script call when stop. Don't config this item when no need.
	
	int				proc_cnt;							// Process count managed by watchdog.
	cmd_stat_t		*proc_cmdlines[PROC_MAX_COUNT];		// Process command line.

	// Runtime information.
	int				shm_id;								// Share memory ID.
	long			shm_size;							// Share memory size.
	int				active_cnt;							// Active process counter.
	proc_tab_t		*p_proc_tab;						// Process table. Also the pointer to the share memory.
	list_head_t		run_list;							// Active process list.
	list_head_t		dead_list;							// Process in deadlock status.
	list_head_t		stopped_list;						// Process has been kill.
	myhash_t		run_hash;							// Running process hash table.
	myhash_t		stopped_hash;						// Stopped process hash table.							// CCD main server IP.
} watchdog_info_t;

extern watchdog_info_t		global_info;				// watchdog global information.

extern int 					w_argc;						// Count of watchdog args.
extern char					**w_argv; 					// Watchdog args.

/*
 * watchdog_calc_shm_size(): Get watchdog process table shm size.
 */
static inline long
watchdog_calc_shm_size()
{
	long	page_size = getpagesize();
	return sizeof(proc_tab_t) % page_size == 0 ? sizeof(proc_tab_t)
		: (sizeof(proc_tab_t) / page_size + 1) * page_size;
}

#endif

