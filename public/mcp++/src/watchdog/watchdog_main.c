/*
 * watchdog_main.c:	watch-dog for MCP.
 * Date:				2011-02-17
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "watchdog_qnf_myconfig.h"
#include "watchdog_nws_util_str.h"
#include "watchdog_qnf_list.h"
#include "watchdog_myhash.h"
#include "watchdog_common.h"
#include "watchdog_log.h"
#include "watchdog_version.h"
#include "watchdog_worker.h"

#ifdef __ENABLE_REPORT
#include "daemon_api.h"
#endif

#define DEF_WATCHDOG_KEY			-1			// Default watchdog shm KEY.
#define DEF_LOCAL_ID				0			// Default local id for wtg client.
#define DEF_TIMEOUT					60			// Default timeout.
#define DEF_LOG_SIZE				0			// Default log rotate size(MB). 0 means no limit.
#define DEF_RUN_FLAG				0			// Default run status: daemon.
#define DEF_M_PORT					-1			// Default Managed Platform PORT.
#define DEF_DEADLOCK_KILL_ENABLE	1			// Default deadlock kill flag.
#define DEF_STRICT_CHECK			1			// Default strict_check flag.
#define DEF_FORCE_EXIT				0			// Default force exit flag.
#define DEF_FORCE_EXIT_WAIT			1			// Default force exit wait time.

#define FORCE_EXIT_WAIT_MAX			7200		// Max force exit wait time.

#define MAX_STR_LEN					256			// Max temp string length.

watchdog_info_t		global_info;		// watchdog global information.

int 				w_argc;				// Count of watchdog args.
char				**w_argv; 			// Watchdog args.

/*
 * add_cmd():		Add a process command line to watchdog.
 * @cmd:			Command line string.
 * @index:			Command index, also the ID.
 * Returns:			0 on success, -1 on error.
 */
static int
add_cmd(char *cmd, int index)
{
	char		*args[ARG_MAX_COUNT] = {NULL};
	cmd_stat_t	*cmd_info = NULL;
	int			i = 0;

	cmd_info = (cmd_stat_t *)calloc(1, sizeof(cmd_stat_t));
	if ( !cmd_info ) {
		log_sys("Alloc memory for process command fail!\n");
		goto err_out0;
	}

	if ( strlen(cmd) <= 0 ) {
		log_sys("Empty process command!\n");
		goto err_out1;
	}

	// Only add ARG_MAX_COUNT - 1 because set last char * to NULL for execv().
	cmd_info->argc = str_explode(NULL, cmd, args, ARG_MAX_COUNT - 1);
	if ( cmd_info->argc <= 0 ) {
		// Should not occur.
		log_sys("Process command count could not be zero!\n");
		goto err_out1;
	}

	for ( i = 0; i < cmd_info->argc; i++ ) {
		cmd_info->argv[i] = strdup(args[i]);
		if ( !cmd_info->argv[i] ) {
			log_sys("Alloc memory for command arg \"%s\" fail!\n", args[i]);
			goto err_out2;
		}
	}

	cmd_info->pid = -1;
	cmd_info->cmd_idx = index;
	cmd_info->status = CMD_INITED;
	INIT_LIST_HEAD(&cmd_info->list);

	cmd_info->stopped_pid = -1;

	global_info.proc_cmdlines[global_info.proc_cnt++] = cmd_info;

	return 0;

err_out2:
	if ( cmd_info ) {
		for ( --i; i >= 0; i-- ) {
			if ( cmd_info->argv[i] ) {
				free(cmd_info->argv[i]);
				cmd_info->argv[i] = NULL;
			}
		}
	}

err_out1:
	if ( cmd_info ) {
		free(cmd_info);
		cmd_info = NULL;
	}

err_out0:
	return -1;
}

/*
 * load_conf():		Load watchdog configuration.
 * Returns:			0 on success, -1 on error.
 */
static int
load_conf()
{
	char		*val = NULL, buf[MAX_STR_LEN];
	int			i;
	
	global_info.shm_key =
		(key_t)myconfig_get_intval("watchdog_key", DEF_WATCHDOG_KEY);
	if ( global_info.shm_key == DEF_WATCHDOG_KEY ) {
		log_sys("Get \"watchdog_key\" fail!\n");
		return -1;
	}

	global_info.local_id = myconfig_get_intval("wtg_local_id", DEF_LOCAL_ID);	

	global_info.timeout = myconfig_get_intval("timeout", DEF_TIMEOUT);
	if ( global_info.timeout < CHECK_TICK ) {
		global_info.timeout = CHECK_TICK;
	}
	global_info.deadlock_cnt =
		((global_info.timeout + CHECK_TICK - 1) / CHECK_TICK) - 1;

	val = myconfig_get_value("log_file");
	if ( !val ) {
		log_sys("Get log file name fail!\n");
		return -1;
	}
	strncpy(global_info.log_name, val, MAX_PATH_LEN - 1);
	if ( log_open(global_info.log_name) ) {
		log_sys("Open log file \"%s\" fail!\n", global_info.log_name);
		fprintf(stderr, "Watchdog open log file \"%s\" fail!\n", global_info.log_name);
		return -1;
	}
	global_info.log_rotate_size =
		myconfig_get_intval("log_rotate_size", DEF_LOG_SIZE);
	if ( global_info.log_rotate_size > 0 ) {
		global_info.log_rotate_size = (global_info.log_rotate_size << 20);
	} else {
		global_info.log_rotate_size = 0;
	}

	global_info.deadlock_kill_enable = myconfig_get_intval("deadlock_kill_enable", DEF_DEADLOCK_KILL_ENABLE);

	global_info.foreground = myconfig_get_intval("foreground", DEF_RUN_FLAG);

	global_info.strict_check = (unsigned)myconfig_get_intval("strict_check", DEF_STRICT_CHECK);
	if ( global_info.strict_check ) {
		log_sys("Watchdog strict check enabled!\n");
		fprintf(stderr, "Watchdog strict check enabled!\n");
	} else {
		log_sys("Watchdog strict check not enabled!\n");
		fprintf(stderr, "Watchdog strict check not enabled!\n");
	}

	global_info.force_exit = (unsigned)myconfig_get_intval("force_exit", DEF_FORCE_EXIT);
	if ( global_info.force_exit ) {
		global_info.force_exit_wait = (unsigned)myconfig_get_intval("force_exit_wait", DEF_FORCE_EXIT_WAIT);
		if ( global_info.force_exit_wait > FORCE_EXIT_WAIT_MAX ) {
			log_sys("Too large force_exit_wait - %u, set to %u.\n",
				global_info.force_exit_wait, FORCE_EXIT_WAIT_MAX);
			global_info.force_exit_wait = FORCE_EXIT_WAIT_MAX;
		}
	}

	val = myconfig_get_value("init_shell");
	if ( !val ) {
		memset(global_info.init_shell, 0, sizeof(char) * MAX_PATH_LEN);
	} else {
		strncpy(global_info.init_shell, val, MAX_PATH_LEN - 1);
	}

	val = myconfig_get_value("exit_shell");
	if ( !val ) {
		memset(global_info.exit_shell, 0, sizeof(char) * MAX_PATH_LEN);
	} else {
		strncpy(global_info.exit_shell, val, MAX_PATH_LEN - 1);
	}

	// Mark first message in log file.
	log_sys("Launching watchdog...\n");
	if ( global_info.deadlock_kill_enable ) {
		log_sys("Deadlock kill enabled!\n");
		fprintf(stderr, "Deadlock kill enabled!\n");
	} else {
		log_sys("Deadlock kill not enabled!\n");
		fprintf(stderr, "Deadlock kill not enabled!\n");		
	}
	DUMP_VERSION(1);

	// Process command lines.
	for ( i = 0; (val = myconfig_get_multivalue("commandline", i))
		&& i < PROC_MAX_COUNT; i++ ) {
		memset(buf, 0, sizeof(char) * MAX_STR_LEN);
		strncpy(buf, val, MAX_STR_LEN - 1);
		if ( add_cmd(buf, i) ) {
			log_sys("Add command line \"%s\" fail!\n", val);
			fprintf(stderr, "Add command line \"%s\" fail!\n", val);
			return -1;
		}
		log_sys("Process command line \"%s\" loaded!\n", val);
	}
	if ( global_info.proc_cnt <= 0 ) {
		log_sys("No process command line found in watchdog configuration!\n");
		fprintf(stderr, "No process command line found in watchdog configuration!\n");
		return -1;
	}

	return 0;
}

/*
 * reset_global_info():	Reset global information.
 */
inline static void
reset_global_info()
{
	memset(&global_info, 0, sizeof(watchdog_info_t));
	global_info.shm_id = -1;
	global_info.p_proc_tab = (void *)(-1);
}

/*
 * init_global_info():	Initizlize global information.
 * Returns:			0 on success, -1 on error.
 */
inline static int
init_global_info()
{
	reset_global_info();

	INIT_LIST_HEAD(&global_info.run_list);
	INIT_LIST_HEAD(&global_info.dead_list);
	INIT_LIST_HEAD(&global_info.stopped_list);
	
	if ( myhash_init(&global_info.run_hash, PROC_MAX_COUNT) ) {
		log_sys("Initialize running process hash table fail!\n");
		return -1;
	}

	if ( myhash_init(&global_info.stopped_hash, PROC_MAX_COUNT) ) {
		log_sys("Initialize stopped process hash table fail!\n");
		return -1;
	}

	return 0;
}

/*
 * watchdog_init():	Initialize the watchdog.
 * Returns:			0 on success, -1 on error.
 */
inline static int
watchdog_init()
{
	if ( init_global_info() ) {
		log_sys("Initialize global information fail!\n");
		fprintf(stderr, "Initialize global information fail!\n");
		return -1;
	}
	
	if ( load_conf() ) {
		log_sys("Watchdog load configuration fail!\n");
		fprintf(stderr, "Watchdog load configuration fail!\n");
		return -1;
	}

	if ( worker_init_proc_tab() ) {
		log_sys("Watchdog initialize process table fail!\n");
		fprintf(stderr, "Watchdog initialize process table fail!\n");
		return -1;
	}

	return 0;
}

/*
 * dump_args():		Save watchdog launch args.
 * @argc:			Count of watchdog commandline args.
 * @argv:			Watchdog commandline args.
 * Returns:			0 on success, -1 on error.
 */
static int
dump_args(int argc, char **argv)
{
	int			i;

	if ( argc == 0 ) {
		log_sys("Watchdog argc could not be zero!\n");
		return -1;
	}
	
	w_argv = (char **)calloc(argc, sizeof(char *));
	if ( !w_argv ) {
		log_sys("Alloc memory for argv array fail!\n");
		return -1;
	}

	for ( i = 0; i < argc; i++ ) {
		w_argv[i] = strdup(argv[i]);
		if ( !w_argv[i] ) {
			log_sys("Alloc memory for argv %d fail!\n", i);
			return -1;
		}
	}

	w_argc = argc;

	if ( wtg_dump_args(argv[0], argc, argv) ) {
		log_sys("Wtg dump command line args fail!\n");
		return -1;
	}

	return 0;
}

/*
 * usage():			Print the watchdog usage.
 */
inline static void
usage()
{
	printf("Usage: watchdog [OPTION]...\n");
	printf("Options:\n");
	printf("     -v     Show watchdog's version.\n");
	printf("     -h     Print this usage.\n");
}

/*
 * parse_cmd():		Parse the command line.
 * @argc:			Count of command line args.
 * @argv:			Command args.
 * Returns:			0 on success. -1 on error.
 */
static int
parse_cmd (int argc, char **argv)
{
	int		c = 0;

	if (argc > 1) {
		if (argc > 2) {
			printf("Too many command args.\n");
			usage();
			exit(8);
		}

		c = getopt(argc, argv, "vh");	
		switch (c) {
			case 'v':
				DUMP_VERSION(0);
				exit(0);
			case 'h':
				usage();
				exit(0);
			default:
				printf("Invalid args found!\n");
				usage();
				exit(9);
		}
	}
	
	return 0;
}

/*
 * main():		Program entrance.
 * @argc:		Count of commandline args.
 * @argv:		Commandline args.
 * Returns code:	Status code 0 on eixt normally, others on error exit.
 */
int
main(int argc, char **argv)
{
	// Parse command line params.
	if ( parse_cmd(argc, argv) ) {
		printf("Parse command line fail!\n");
		exit(1);
	}

	if ( dump_args(argc, argv) ) {
		log_sys("Save watchdog command line args fail!\n");
		fprintf(stderr, "Save watchdog command line args fail!\n");
		exit(2);
	}

	// Initialize watchdog configure file.
	if ( myconfig_init(argc, argv, 0) ) {
		log_sys("Config init fail!\n");
		fprintf(stderr, "Config init fail!\n");
		exit(3);
	}

	// Initialize watchdog.
	if ( watchdog_init() ) {
		log_sys("Watchdog initializa fail!\n");
		fprintf(stderr, "Watchdog initializa fail!\n");
		exit(4);
	}

	// Start watchdog.
	worker_watchdog_start();

	// Program will exit in watchdog when stop normally. Run here means error.
	worker_destroy_proc_tab();
	log_sys("Unexpect program exit!\n");
	fprintf(stderr, "Unexpect program exit!\n");
	
	exit(5);
}

