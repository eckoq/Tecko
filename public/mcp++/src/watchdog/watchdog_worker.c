/*
 * watchdog_worker.c:		Watchdog entry.
 * Date:					2011-02-21
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stddef.h>
#include <unistd.h>
#include <signal.h>
#include <limits.h>
#include <fcntl.h>
#include <time.h>
#include <thread>
//#include <asm/atomic.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>

#include "watchdog_qnf_list.h"
#include "watchdog_common.h"
#include "watchdog_log.h"
#include "watchdog_version.h"

#ifdef __ENABLE_REPORT

#include <asm/byteorder.h>
#include "watchdog_qnf_myconfig.h"
#include "daemon_api.h"
#include "watchdog_api.h"
#include "wtg_api.h"
#include "wtg_version.h"

extern char watchdog_config_name[];		//Watchdog config file name.

wtg_entry_t					wtg_entry;	// Wtg client entry.
wtg_entry_t					*wtg = NULL;	// Wtg client.

#ifndef DEF_USE_FLAG
#define DEF_USE_FLAG		-1			// Get this means not launch wtg.
#endif

#define REPORT_TICK			60			// Report process information tick.

#define SC_WARNNING_TICK	600			// Strict check warnning tick.

#define PROC_ITEM_MAX		2048
static wtg_api_proc_t		report_proc_items[PROC_ITEM_MAX];	// Process report buffer.

#endif

#ifndef MCP_PLUS_PLUS
#define MCP_PLUS_PLUS
#endif

#define USLEEP_TIME			1000			// usleep() time.
#define SLEEP_TIME			1				// sleep() time.
#define WAIT_TIMEOUT		2				// Wait timeout.
#define INT_STR_LEN			32				// Int string length.

// 0 not restart, 1 restart.
static int			watchdog_restart = 0;	// Wether restart watchdog.
static int			stop = 0;				// Wether stop watchdog.

static int			wait_all = 0;			// Wether wait_all call.

static char			path_buf[PATH_MAX];		// Temp path buffer.
static char			dir_buf[PATH_MAX];		// Temp directory buffer.
static char			cmd_buf[PATH_MAX];		// Temp command buffer.

static int			stop_pids[PROC_MAX_COUNT];	// Process stop in stop_all_proc(). 

#define ALARM_MSG_SIZE		256				// Alarm message max size.
static char			alarm_buf[ALARM_MSG_SIZE];	// Alarm message buffer.

#ifdef __ENABLE_REPORT
#define CCD_CONF_BUF_SIZE	(1<<20)			// CCD config file read buffer size.

#if 0
static char			ccd_conf_buf[CCD_CONF_BUF_SIZE];	// CCD config file read buffer.
#endif

static char			local_cur_path[PATH_MAX];			// For get wtg local ID. Current path.
#endif

#ifndef HUNGUP_TIME
#define HUNGUP_TIME			1				// Sleep time to release CPU.
#endif

/*
 * cmd_filter():			Get command line path and command.
 * @src:					Original command line.
 * Returns:				0 on success, -1 on error.
 *						Store directory in dir_buf,
 *						store command in cmd_buf and "./" will be added at command head.
 */
inline static int
cmd_filter(const char *src)
{
	int			len;
	char		*pos = NULL;

	strcpy(path_buf, src);

	//if ( !realpath(src, path_buf) ) {
	//	log_sys("Get source command line \"%s\"path fail! %m\n", src);
	//	return -1;
	//}

	len = strlen(path_buf);
	if ( len <= 0 ) {
		log_sys("Empty command line path!\n");
		return -1;
	}

	if ( path_buf[len - 1] == '/' ) {
		log_sys("Command line could not be a directory!\n");
		return -1;
	}

	//if ( path_buf[0] != '/' ) {
	//	log_sys("Command line real path \"%s\" must start with \"/\"!\n", path_buf);
	//	return -1;
	//}

	pos = strrchr(path_buf, '/');
	if ( !pos ) {
		pos = path_buf;
	} else {
		pos++;
	}

	if ( strlen(pos) > PATH_MAX - 3 ) {
		log_sys("Too long command \"%s\"!\n", pos);
		return -1;
	}
	cmd_buf[PATH_MAX - 1] = 0;
	if ( pos != path_buf ) {
		snprintf(cmd_buf, PATH_MAX - 1, "./%s", pos);
	} else {
		snprintf(cmd_buf, PATH_MAX - 1, "%s", pos);
	}

	*pos = 0;
	dir_buf[PATH_MAX - 1] = 0;
	strncpy(dir_buf, path_buf, PATH_MAX - 1);	

	return 0;
}

/*
 * del_from_hash():		Delete item from hash table.
 * @pid:					Pid KEY.
 * NOTES:				Delete from all hash table to ensure correct.
 */
inline static void
del_from_hash(pid_t pid)
{
	myhash_del(&global_info.run_hash, pid, NULL);
	myhash_del(&global_info.stopped_hash, pid, NULL);
}

/*
 * get_cmd_from_list():		Get command item from list.
 * @plist:				List will be searched.
 * @pid:					Pid KEY.
 * @how:				0 compare id with pid, others compare id with stopped_id.
 * Returns:				Return the pointer to the item or NULL on not found.
 */
static cmd_stat_t*
get_cmd_from_list(list_head_t *plist, pid_t pid, int how)
{
	cmd_stat_t		*item = NULL, *pos_item = NULL;
	list_head_t		*list_item = NULL, *tmp_list = NULL;

	list_for_each_safe(list_item, tmp_list, plist) {
		pos_item = list_entry(list_item, cmd_stat_t, list);
		if ( how ) {
			if ( pos_item->stopped_pid == pid ) {
				item = pos_item;
				break;
			}
		} else {
			if ( pos_item->pid == pid ) {
				item = pos_item;
				break;
			}
		}
	}

	return item;
}

/*
 * cmd_item_reset():		Reset item. Not change list.
 * @status:				item->status code will be set.
 */
inline static void
cmd_item_reset(cmd_stat_t *item, int status)
{
	item->pid = -1;
	item->status = status;
	item->stopped_pid = -1;
}

/*
 * cmd_add_to_dead():		Add command item to dead list. Make sure item of index is not NULL before call.
 * @index:				Index also command line ID.
 */
inline static void
cmd_add_to_dead(int index)
{
	cmd_stat_t		*item = global_info.proc_cmdlines[index];
	
	// Delete again anyway.
	list_del_init(&item->list);	
	item->status = CMD_DEAD;	
	list_add_tail(&item->list, &global_info.dead_list);
}

/*
 * set_stop():				Set watchdog to stop.
 */
inline static void
set_stop()
{
	watchdog_restart = 0;
	stop = 1;
}

/*
 * scan_shm():			Scan shm table.
 */
static void
scan_shm()
{
	list_head_t			*list_item = NULL, *tmp_list = NULL;
	cmd_stat_t			*item = NULL;
	proc_info_t			*proc_node = NULL;
	int					cnt = 0;
	
	list_for_each_safe(list_item, tmp_list, &global_info.run_list) {
		item = list_entry(list_item, cmd_stat_t, list);
		proc_node = &global_info.p_proc_tab->procs[item->cmd_idx];
		
		if ( !proc_node->used || proc_node->pid == -1 ) {
			// Invalid node. Add to dead list to cleanup.
			log_sys("Invalid shm node found in tick handler. Add to dead list. Command index is %d.\n",
				item->cmd_idx);
			if ( proc_node->pid != -1 ) {
				del_from_hash(proc_node->pid);
			}
			cmd_add_to_dead(item->cmd_idx);
			continue;
		}
		
		cnt = atomic_read((atomic_t *)&proc_node->tick_cnt);
		atomic_sub(cnt, (atomic_t *)&proc_node->tick_cnt);
		if ( cnt ) {
			proc_node->bad_cnt = 0;
		} else if ( proc_node->bad_cnt >= global_info.deadlock_cnt ) {
			log_sys("Deadlock process found, pid = %d, item %d - \"%s\".\n",
				proc_node->pid, item->cmd_idx, item->argv[0]);
#ifdef __ENABLE_REPORT
			if ( wtg ) {
				memset(alarm_buf, 0, ALARM_MSG_SIZE);
				snprintf(alarm_buf, ALARM_MSG_SIZE - 1,
					"Deadlock process found, pid = %d, item %d - \"%s\".",
					proc_node->pid, item->cmd_idx, item->argv[0]);
				wtg_api_report_alarm(wtg, 1, alarm_buf);
			}
#endif
			if ( global_info.deadlock_kill_enable ) {
				proc_node->bad_cnt = 0;
				if ( proc_node->pid != -1 ) {
					del_from_hash(proc_node->pid);
				}
				cmd_add_to_dead(item->cmd_idx);
			} else {
				proc_node->bad_cnt = 0;
			}
		} else {
			proc_node->bad_cnt++;
		}
	}
}

/*
 * tick_handler():			Alarm tick handler.
 * @signo:				Signals.
 */
static void
tick_handler(int signo)
{/*
	list_head_t			*list_item = NULL, *tmp_list = NULL;
	cmd_stat_t			*item = NULL;
	proc_info_t			*proc_node = NULL;
	int					cnt = 0;
*/
	if ( wait_all ) {
		return;
	}
/*
	list_for_each_safe(list_item, tmp_list, &global_info.run_list) {
		item = list_entry(list_item, cmd_stat_t, list);
		proc_node = &global_info.p_proc_tab->procs[item->cmd_idx];
		
		if ( !proc_node->used || proc_node->pid == -1 ) {
			// Invalid node. Add to dead list to cleanup.
			log_sys("Invalid shm node found in tick handler. Add to dead list. Command index is %d.\n",
				item->cmd_idx);
			if ( proc_node->pid != -1 ) {
				del_from_hash(proc_node->pid);
			}
			cmd_add_to_dead(item->cmd_idx);
			continue;
		}
		
		cnt = atomic_read(&proc_node->tick_cnt);
		atomic_sub(cnt, &proc_node->tick_cnt);
		if ( cnt ) {
			proc_node->bad_cnt = 0;
		} else if ( proc_node->bad_cnt >= global_info.deadlock_cnt ) {
			log_sys("Dead process found, pid = %d, item %d - \"%s\".\n",
				proc_node->pid, item->cmd_idx, item->argv[0]);
			proc_node->bad_cnt++;
			if ( proc_node->pid != -1 ) {
				del_from_hash(proc_node->pid);
			}
			cmd_add_to_dead(item->cmd_idx);
		} else {
			proc_node->bad_cnt++;
		}
	}*/
}

/*
 * sigterm_handler():		Signal handler for Signals which will stop watchdog.
 * @signo:				Signals.
 */
static void
stop_handler(int signo) {
	set_stop();
}

/*
 * sigterm_handler():		Signal handler for Signals which will restart watchdog.
 * @signo:				Signals.
 */
static void
restart_handler(int signo) {
	watchdog_restart = 1;
    stop = 1;
}

/*
 * daemon_init():			Initialize daemon.
 * Returns:				0 on success, -1 on error.
 */
static int
daemon_init()
{
	struct sigaction	sa;
	sigset_t			sset;
	pid_t				pid;
	int					pfd;
	char				pid_buf[INT_STR_LEN] = {0};
	char				pid_name[PATH_MAX] = {0};
	int					pid_len;

	if ( global_info.foreground ) {
		log_sys("Run as foreground.\n");
		printf("Run as foreground.\n");
	} else {
		log_sys("Run as daemon.\n");
		printf("Switching to daemon...\n");

		pid = fork();
		if ( pid == -1 ) {
			log_sys("fork() fail when initialize daemon!\n");
			return -1;
		}
		if ( pid != 0 ) {
			// Parent process exit.
			exit(0);
		}
		if ( setsid() == -1 ) {
			log_sys("setsid() fail when initialize daemon!\n");
			return -1;
		}
	}

	// Write PID file.
	snprintf(pid_name, PATH_MAX - 1, "%s.pid", w_argv[0]);
	pid_len = snprintf(pid_buf, INT_STR_LEN - 1, "%ld\n", (long)getpid());
	pfd = open(pid_name, O_CREAT | O_TRUNC | O_WRONLY, 0666);
	if ( pfd == -1 ) {
		log_sys("Create PID file \"%s\" fail! %m\n", pid_name);
		return -1;
	}
	if ( write(pfd, pid_buf, pid_len) != pid_len ) {
		log_sys("Write PID file \"%s\" fail! %m\n", pid_name);
		close(pfd);
		return -1;
	}
	close(pfd);

	// Set signals.
	memset(&sa, 0, sizeof(struct sigaction));
	
	sa.sa_handler = tick_handler;
	sigaction(SIGALRM, &sa, NULL);
	
	sa.sa_handler = stop_handler;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGQUIT, &sa, NULL);
	sigaction(SIGUSR2, &sa, NULL);

	sa.sa_handler = restart_handler;
	sigaction(SIGHUP, &sa, NULL);

	signal(SIGIO, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGXFSZ, SIG_IGN);
	
	signal(SIGCHLD, SIG_DFL);
	signal(SIGSEGV, SIG_DFL);
	signal(SIGBUS, SIG_DFL);
	signal(SIGILL, SIG_DFL);
	signal(SIGFPE, SIG_DFL);
	signal(SIGABRT, SIG_DFL);

	sigemptyset(&sset);
	sigaddset(&sset, SIGSEGV);
	sigaddset(&sset, SIGBUS);
	sigaddset(&sset, SIGABRT);
	sigaddset(&sset, SIGILL);
	sigaddset(&sset, SIGCHLD);
	sigaddset(&sset, SIGFPE);
	sigaddset(&sset, SIGHUP);
	sigaddset(&sset, SIGTERM);
	sigaddset(&sset, SIGINT);
	sigaddset(&sset, SIGQUIT);
	if ( sigprocmask(SIG_UNBLOCK, &sset, NULL) == -1 ) {
		log_sys("Set signal mask fail!\n");
		return -1;
	}

	return 0;
}

/*
 * worker_system():		Rum command line.
 * @cmd:				Command line.
 * Returns:				>= 0 on exit normally and the return value is param transfered to eixt().
 *						-1 on exit unormally or error.
 */
static int
worker_system(const char *cmd)
{
	int			ret = 0;

	ret = system(cmd);
	if ( ret == -1 ) {
		return -1;
	} else {
		if ( WIFEXITED(ret) ) {
			return (WEXITSTATUS(ret));
		} else {
			return -1;
		}
	}
}

/*
 * worker_shell_script():	Run shell script.
 * @shell:				Shell script path.
 * Returns:				0 on success, -1 on error.
 */
static int
worker_shell_script(const char *shell)
{
	char			old_dir[PATH_MAX];

	memset(old_dir, 0, PATH_MAX);
	if ( !getcwd(old_dir, PATH_MAX - 1) ) {
		log_sys("Get local current path fail when prepare run init shell!\n");
		fprintf(stderr, "Get local current path fail when prepare run init shell!\n");
		return -1;
	}
	
	if ( cmd_filter(shell) ) {
		log_sys("Invalid init shell script \"%s\"!\n", shell);
		fprintf(stderr, "Invalid init shell script \"%s\"!\n", shell);
		return -1;
	}

	if ( strlen(dir_buf) > 0 && chdir(dir_buf) == -1 ) {
		log_sys("Change directory to \"%s\" fail when run script \"%s\"! %m\n",
			dir_buf, shell);
		fprintf(stderr, "Change directory to \"%s\" fail when run script \"%s\"! %m\n",
			dir_buf, shell);
		return -1;
	}
	
	if ( worker_system(cmd_buf) == -1 ) {
		log_sys("Run init shell script \"%s\" fail! Continue.\n", shell);
		fprintf(stderr, "Run init shell script \"%s\" fail! Continue.\n", shell);
	}

	if ( chdir(old_dir) ) {
		log_sys("Change to watchdog old directory \"%s\" fail! %m\n", old_dir);
		fprintf(stderr, "Change to watchdog old directory \"%s\" fail! %m\n", old_dir);
		return -1;
	}

	return 0;
}

/*
 * worker_init_proc_tab():	Initialize process table.
 * Returns:				0 on success, -1 on error.
 */
int
worker_init_proc_tab()
{
	int			shm_id;
	long		shm_size;
	void		*shm_addr = NULL;
	
	shm_size = watchdog_calc_shm_size();

	shm_id = shmget(global_info.shm_key,
		(size_t)shm_size, IPC_CREAT | IPC_EXCL | 0666);
	if ( shm_id == -1 ) {
		if ( errno == EEXIST ) {
			/*
			shm_id = shmget(global_info.shm_key, (size_t)shm_size, 0);
			if ( shm_id == -1 ) {
				log_sys("Get shm fail when remove process table shm! %m\n");
				return -1;
			}
			if ( shmctl(shm_id, IPC_RMID, NULL) == -1 ) {
				log_sys("Remove shm fail! %m\n");
				return -1;
			}
			shm_id = shmget(global_info.shm_key,
				(size_t)shm_size, IPC_CREAT | IPC_EXCL | 0666);
			if ( shm_id == -1 ) {
				log_sys("Create shm fail twice! %m\n");
				return -1;
			}*/
			log_sys("Watchdog already existed! Maybe watchdog alread in running! Stop!\n");
			fprintf(stderr, "Watchdog already existed! Maybe watchdog alread in running! Stop!\n");
			return -1;
		} else {
			log_sys("Create shm for process table fail! %m\n");
			fprintf(stderr, "Create shm for process table fail! %m\n");
			return -1;
		}
	}
	global_info.shm_id = shm_id;

	shm_addr = shmat(shm_id, NULL, 0);
	if ( shm_addr == (void *)(-1) ) {
		log_sys("shmat fail! %m\n");
		if ( shmctl(shm_id, IPC_RMID, NULL) == -1 ) {
			log_sys("Remove shm fail when shmat fail! %m\n");
			return -1;
		}
		return -1;
	}
	global_info.p_proc_tab = (proc_tab_t *)shm_addr;

	memset(shm_addr, 0, shm_size);
	
	global_info.shm_size = shm_size;
	
	global_info.p_proc_tab->shm_size = shm_size;
	global_info.p_proc_tab->proc_cnt = global_info.proc_cnt;
	
	return 0;
}

/*
 * worker_destroy_proc_tab():	Cleanup the process table in shm.
 */
void
worker_destroy_proc_tab()
{
	if ( global_info.p_proc_tab != (proc_tab_t *)(-1) ) {
		if ( shmdt((void *)global_info.p_proc_tab) == -1 ) {
			log_sys("Detach shm fail! %m\n");
		}
		global_info.p_proc_tab = (proc_tab_t *)(-1);
	}

	if ( global_info.shm_id != -1 ) {
		if ( shmctl(global_info.shm_id, IPC_RMID, NULL) == -1 ) {
			log_sys("Remove shm fail! %m\n");
		}
		global_info.shm_id = -1;
	}
}

/*
 * set_shm_item():	Set item in shm table.
 * @index:			Command line item index.
 * @pid:				Process ID.
 */
inline static void
set_shm_item(int index, pid_t pid)
{
	global_info.p_proc_tab->procs[index].used = 1;
	global_info.p_proc_tab->procs[index].pid = pid;
	atomic_set((atomic_t *)&global_info.p_proc_tab->procs[index].tick_cnt, 0);
	global_info.p_proc_tab->procs[index].bad_cnt = 0;
}

/*
 * clear_shm_item():	Clear item in shm table.
 * @index:			Command line item index.
 */
inline static void
clear_shm_item(int index)
{
	memset(&global_info.p_proc_tab->procs[index], 0, sizeof(proc_info_t));
	global_info.p_proc_tab->procs[index].pid = -1;
}

/*
 * cmd_stop():			Stop a command line item. Make sure item of index is not NULL before call.
 * @index:				Index also command line ID.
 * @kill_sig:				First choice of kill signal.
 * @normal:				0 not normal, SIGKILL will be send, others not send SIGKILL.
 * Returns:				0 on success, -1 on error. Actually never return -1.
 * NOTES:				Call to stop when get from dead list or stop all finally.
 */
static int
cmd_stop(int index, int kill_sig, int normal)
{
	cmd_stat_t		*item = global_info.proc_cmdlines[index];
	int				tmp_status;

	// Delete from run list or dead list.
	list_del_init(&item->list);
	clear_shm_item(index);
	
	tmp_status = item->status;
	item->status = CMD_STOP;	// Set stop first.

	if ( item->pid == -1 ) {
		log_sys("Pid is -1 when stop£¬%d - \"%s\"!\n",
			index, item->argv[0]);
		goto dont_kill;
	}

	log_sys("Kill process item %d - \"%s\".\n",
		index, item->argv[0]);

	// Kill process.
	if ( kill(item->pid, kill_sig) == -1 ) {
		log_sys("Send SIGSEGV to process %d - \"%s\" shich we want to stop fail! %m\n",
			index, item->argv[0]);
	}

	// Anyway try again when unormal.
#ifdef MCP_PLUS_PLUS
	if ( !normal ) {
		usleep(USLEEP_TIME);
		kill(item->pid, SIGKILL);
	}
#else
	usleep(USLEEP_TIME);
	kill(item->pid, SIGKILL);
#endif


dont_kill:

	item->stopped_pid = item->pid;

	if ( tmp_status != CMD_RUNNING && tmp_status != CMD_DEAD ) {
		log_sys("Wrong process item status %d, %d - \"%s\".\n",
			item->status, index, item->argv[0]);
	}	

	item->pid = -1;

	if ( item->stopped_pid != -1 ) {
		del_from_hash(item->stopped_pid);	// Ensure correct.
		if ( myhash_add(&global_info.stopped_hash, item->stopped_pid, item, NULL) ) {
			// No memory for hash node.
			log_sys("WARNNING!!!! Add command line item to stopped hash table fail!\n");
			// Not return error here because hash table is only used to rase the search performance before wait().
			// If item not in hash table, we also search the stopped_list.
			// Not return error to make the process informance correct.
		}
		list_add_tail(&item->list, &global_info.stopped_list);
	}
	
	return 0;
}

/*
 * cmd_start():			Start a command line item. Item must not be empty.
 * @index:				Index also command line ID.
 * Returns:				0 on success, -1 on error.
 */
static int
cmd_start(int index)
{
	cmd_stat_t		*item = global_info.proc_cmdlines[index];
	pid_t			pid;
	char			*old_cmd = NULL;

	list_del_init(&item->list);		// Remove to ensure correct anyway.

	if ( item->pid != -1 ||
		(item->status != CMD_INITED
		&& item->status != CMD_STOP
		&& item->status != CMD_RESTART) ) {
		log_sys("Command line item %d - \"%s\" status wrong when start! Process ID is %d.\n",
			index, item->argv[0], item->pid);
		return -1;
	}

	log_sys("Start process item %d - \"%s\"...\n",
		index, item->argv[0]);
	
	if ( (pid = fork()) == -1 ) {
		log_sys("Fork process for item item %d - \"%s\" fail!\n",
			index, item->argv[0]);
		return -1;
	}

	// If error occurs between fork end and execv, child must end itself.

	if ( pid == 0 ) {
		// Child process.
#ifdef __ENABLE_REPORT
		wtg_release_in_child();
		if ( wtg ) {
			wtg_api_destroy(wtg);
		}
#endif
		
		if ( cmd_filter(item->argv[0]) ) {
			log_sys("Filter the command fail! Item %d - \"%s\".\n",
				index, item->argv[0]);
			exit(CHLD_EXEC_FAIL_EXIT);	// If found this exit code, means execv fail!
		}
		
		// Change working directory.
		if ( strlen(dir_buf) > 0 && chdir(dir_buf) == -1 ) {
			log_sys("Change directory to \"%s\" fail! Item %d - \"%s\".\n",
				dir_buf, index, item->argv[0]);
			exit(CHLD_EXEC_FAIL_EXIT);	// If found this exit code, means execv fail!
		}

		old_cmd = item->argv[0];
		item->argv[0] = cmd_buf;
		
		usleep(USLEEP_TIME);	// Sleep, so watchdog could reset shm item and child could update item correctly.
		if ( execv(cmd_buf, item->argv) == -1 ) {
			log_sys("Child process execute command line item %d - \"%s\" fail! %m.\n",
				index, old_cmd);
			// Child process exit itself when error.
			exit(CHLD_EXEC_FAIL_EXIT);	// If found this exit code, means execv fail!
		}
	}

	global_info.active_cnt++;		// global_info.active_cnt-- when wait a child stop in any case.

	// If child process execute fai(CHLD_EXEC_FAIL_EXIT).
	// Watchdog will get the case when wait(). In this case must search running table.
	// When wait, must search hash firest, if not found, search restart list and not add the same item to restart list.

	// Must not return error below to make process informance correct.

	// Update shm first to ensure process update the correct information.
	set_shm_item(index, pid);

	item->pid = pid;
	
	del_from_hash(pid);
	if ( myhash_add(&global_info.run_hash, pid, item, NULL) ) {
		// No memory for hash node.
		log_sys("WARNNING!!!! Add command line item to running hash table fail!\n");
		// Not return error here because hash table is only used to rase the search performance before wait().
		// If item not in hash table, we also search the run_list.
		// Not return error to make the process informance correct.
	}

	item->status = CMD_RUNNING;
	
	// Update list after reset shm because of deadlock check depend the list.
	// So we will not check a shm item that is not the correct status.
	// When stop, we must delete list item first.
	list_add_tail(&item->list, &global_info.run_list);
	
	return 0;
}

/*
 * start_all_proc():		Start all processes.
 * Returns:				0 on success, -1 on error.
 */
static int
start_all_proc()
{
	int			i;

	for ( i = 0; i < global_info.proc_cnt; i++ ) {
		if ( cmd_start(i) ) {
			// Not incress retry count when first start. And not restart.
			log_sys("Start command item %d - \"%s\" fail!\n",
				i, global_info.proc_cmdlines[i]->argv[0]);
			return -1;
		}
	}

	return 0;
}

/*
 * stop_all_proc():			Stop all processes.
 * @kill_arg:				First choice kill signal.
 */
static void
stop_all_proc(int kill_sig)
{
	list_head_t			*list_item = NULL, *tmp_list = NULL;
	cmd_stat_t			*item = NULL;

	memset(stop_pids, -1, sizeof(int) * PROC_MAX_COUNT);

	list_for_each_safe(list_item, tmp_list, &global_info.run_list) {
		item = list_entry(list_item, cmd_stat_t, list);
		stop_pids[item->cmd_idx] = item->pid;
		cmd_stop(item->cmd_idx, kill_sig, 1);
	}

	list_for_each_safe(list_item, tmp_list, &global_info.dead_list) {
		item = list_entry(list_item, cmd_stat_t, list);
		stop_pids[item->cmd_idx] = item->pid;
		cmd_stop(item->cmd_idx, kill_sig, 1);
	}
}

/*
 * wait_all_proc():			Wait all child process.
 */
static void
wait_all_proc()
{
	pid_t		pid;

	wait_all = 1;
	
	while ( global_info.active_cnt > 0 ) {
		alarm(WAIT_TIMEOUT);
		pid = wait(NULL);
		if ( pid == -1 ) {
			// Wait timeout.
			alarm(0);	// Cancle alarm although alarm has been timeout.
			log_sys("Timeout when wait all process. Active count is %d.\n",
				global_info.active_cnt);
			break;
		}
		alarm(0);
		global_info.active_cnt--;
	}

	if ( global_info.active_cnt < 0 ) {
		// Debug information.
		log_sys("WARNNING: active count is < 0 after wait all!\n");
	}
}

/*
 * kill_dead_procs()		Kill processes item in the dead list.
 */
static void
kill_dead_procs()
{
	cmd_stat_t		*item = NULL;
	list_head_t		*list_item = NULL, *tmp_list = NULL;

	list_for_each_safe(list_item, tmp_list, &global_info.dead_list) {
		item = list_entry(list_item, cmd_stat_t, list);
		log_sys("Prepare to kill dead process item %d - \"%s\".\n",
			item->cmd_idx, item->argv[0]);
		cmd_stop(item->cmd_idx, SIGSEGV, 0);	// Item will be remove from the dead list in this function.
	}
}

/*
 * cmd_item_find_exit_unnormal():	Find item when exit unnormally.
 * @cpid:						Pid has been wait.
 * Returns:						Return pointer to the item or NULL on not found.
 */
inline static cmd_stat_t*
cmd_item_find_exit_unnormal(pid_t cpid)
{
	cmd_stat_t		*item = NULL;

	if ( (item = myhash_find(&global_info.stopped_hash, cpid, NULL)) != NULL
		|| (item = myhash_find(&global_info.run_hash, cpid, NULL)) != NULL
		|| (item = get_cmd_from_list(&global_info.dead_list, cpid, 0)) != NULL
		|| (item = get_cmd_from_list(&global_info.stopped_list, cpid, 1)) != NULL
		|| (item = get_cmd_from_list(&global_info.run_list, cpid, 0)) != NULL ) {
		return item;
	}

	return item;	// NULL.
}

/*
 * cmd_item_find_exit_normal():		Find item when exit normally.
 * @cpid:						Pid has been wait.
 * Returns:						Return pointer to the item or NULL on not found.
 */
inline static cmd_stat_t*
cmd_item_find_exit_normal(pid_t cpid)
{
	cmd_stat_t		*item = NULL;

	if ( (item = myhash_find(&global_info.run_hash, cpid, NULL)) != NULL 
		|| (item = get_cmd_from_list(&global_info.dead_list, cpid, 0)) != NULL
		|| (item = myhash_find(&global_info.stopped_hash, cpid, NULL)) != NULL
		|| (item = get_cmd_from_list(&global_info.run_list, cpid, 0)) != NULL
		|| (item = get_cmd_from_list(&global_info.stopped_list, cpid, 1)) != NULL ) {
		return item;
	}

	return item;	// NULL.
}

/*
 * wait_child():		Wait child process exit.
 * returns:			0 on success, -1 on error.
 */
static int
wait_child()
{
	pid_t			cpid;
	int				status, ret = 0;
	cmd_stat_t		*item = NULL;
	
	cpid = waitpid(-1, &status, WNOHANG);
	if ( cpid == -1 && errno == ECHILD ) {
		log_sys("ERROR, no child process, active count is %d.\n",
			global_info.active_cnt);
	}

	if ( cpid > 0 ) {
		// Child process exited.
		if ( WIFEXITED(status) ) {
			if ( WEXITSTATUS(status) == CHLD_EXEC_FAIL_EXIT ) {
				// execv() fail.
				if ( (item = cmd_item_find_exit_normal(cpid)) != NULL ) {
					// Item found.
					list_del_init(&item->list);
					del_from_hash(cpid);
					clear_shm_item(item->cmd_idx);
					cmd_item_reset(item, CMD_INITED);
					global_info.active_cnt--;
					set_stop();
					log_sys("Execute fail be found! Item %d - \"%s\".\n",
						item->cmd_idx, item->argv[0]);
#ifdef __ENABLE_REPORT
					if ( wtg ) {
						memset(alarm_buf, 0, ALARM_MSG_SIZE);
						snprintf(alarm_buf, ALARM_MSG_SIZE - 1,
							"Process start fail! item_idx %d - \"%s\".",
							item->cmd_idx, item->argv[0]);
						wtg_api_report_alarm(wtg, 1, alarm_buf);
					}
#endif
					ret = -1;
					goto err_out;
				} else {
					// Item not found. Ignore it.
					log_sys("Wait uncontrol process exit and ignore it! Pid is %d.\n", cpid);
				}
			} else {
				// Exit normally. Find sequence is the same as above case.
				if ( (item = cmd_item_find_exit_normal(cpid)) != NULL ) {
					// Item found.
					list_del_init(&item->list);
					del_from_hash(cpid);
					clear_shm_item(item->cmd_idx);
					cmd_item_reset(item, CMD_STOP);
					global_info.active_cnt--;
					log_sys("Process item %d - \"%s\" exit normally! Exit code is %d.\n",
						item->cmd_idx, item->argv[0], WEXITSTATUS(status));
				} else {
					// Item not found.
					log_sys("Wait uncontrol process exit and ignore it! Pid is %d. Exit code is %d.\n",
						cpid, WEXITSTATUS(status));
				}
			}
		} else {
			if ( (item = cmd_item_find_exit_unnormal(cpid)) != NULL ) {
				// Item found. Need restart.
				list_del_init(&item->list);
				del_from_hash(cpid);
				clear_shm_item(item->cmd_idx);
				cmd_item_reset(item, CMD_INITED);
				global_info.active_cnt--;
				log_sys("Prepare restart exit unnormally process item %d - \"%s\"...\n",
					item->cmd_idx, item->argv[0]);
#ifdef __ENABLE_REPORT
				if ( wtg ) {
					memset(alarm_buf, 0, ALARM_MSG_SIZE);
					snprintf(alarm_buf, ALARM_MSG_SIZE - 1,
						"Deadlock or coredump process found! Try to restart. item_idx %d - \"%s\".",
						item->cmd_idx, item->argv[0]);
					wtg_api_report_alarm(wtg, 1, alarm_buf);
				}
#endif
				if ( cmd_start(item->cmd_idx) ) {
					log_sys("Restart process item %d - \"%s\" fail!\n",
						item->cmd_idx, item->argv[0]);
					set_stop();
					ret = -1;
					goto err_out;
				}
			} else {
				// Item not found. Ignore it.
				log_sys("Wait uncontrol process exit and ignore it! Pid is %d. Exit unnormally.\n",
					cpid);
			}
		}
	}
	
err_out:

	return ret;
}

#ifdef __ENABLE_REPORT

#if 0
/*
 * get_ccd_port():			Get CCD server PORT for wtg.
 * Returns:				Return CCD server PORT. 0 maybe means get PORT fail.
 */
unsigned short
get_ccd_port()
{
	int				i;
	char			*tmp_str = NULL;
	cmd_stat_t		*cmd = global_info.proc_cmdlines[0];
	unsigned short	port = 0;
	long			tmp_port = 0;
	int				len;
	char			*pos = NULL;
	int				fd = -1, ccd_file_len = 0;

	for ( i = 0; i < global_info.proc_cnt; i++ ) {
		if ( (tmp_str = strcasestr(global_info.proc_cmdlines[i]->argv[0], "ccd")) ) {
			cmd = global_info.proc_cmdlines[i];
			break;
		}
	}

	if ( cmd->argc < 2 ) {
		log_sys("Too little argc when get CCD port!\n");
		goto out;
	}

	strcpy(path_buf, cmd->argv[0]);

	len = strlen(path_buf);
	if ( len <= 0 ) {
		log_sys("Empty command line path when get CCD port!\n");
		goto out;
	}

	if ( path_buf[len - 1] == '/' ) {
		log_sys("Command line could not be a directory when get CCD port!\n");
		goto out;
	}

	pos = strrchr(path_buf, '/');
	if ( !pos ) {
		pos = path_buf;
	} else {
		pos++;
	}

	*pos = 0;

	strcpy(dir_buf, cmd->argv[1]);
	if ( dir_buf[0] == '/' ) {
		strcpy(path_buf, dir_buf);
	} else {
		strcat(path_buf, dir_buf);
	}

	// path_buf is the CCD config file path.
	fd = open(path_buf, O_RDONLY);
	if ( fd == -1 ) {
		log_sys("Open CCD config file fail when get CCD port! %m\n");
		goto out;
	}

	memset(ccd_conf_buf, 0, sizeof(char) * CCD_CONF_BUF_SIZE);
	ccd_file_len = read(fd, ccd_conf_buf, CCD_CONF_BUF_SIZE - 1);
	if ( ccd_file_len <= 0 ) {
		log_sys("Read CCD config file fail! %d, %m\n", ccd_file_len);
		goto out;
	}
	if ( ccd_file_len == CCD_CONF_BUF_SIZE - 1 ) {
		log_sys("Too long ccd config file length %d, maybe error!\n", ccd_file_len);
	}

	pos = strstr(ccd_conf_buf, "bind_port");
	if ( !pos ) {
		log_sys("bind port not found in CCD config file!\n");
		goto out;
	}

	for ( pos += strlen("bind_port"); *pos == ' ' || *pos == '\t'; pos++ );

	if ( *pos != '=' ) {
		log_sys("Invalid CCD config format!\n");
		goto out;
	} else {
		pos++;
	}

	for ( ; *pos == ' ' || *pos == '\t'; pos++ );

	tmp_port = strtol(pos, NULL, 10);
	if ( ( tmp_port == 0 && errno == EINVAL )
		|| ( (tmp_port == LONG_MIN || tmp_port == LONG_MAX) && errno == ERANGE )
		|| ( tmp_port < 0 || tmp_port >= 65536 ) ) {
		log_sys("Invalid CCD port!\n");
		goto out;
	}
	port = (unsigned short)tmp_port;

out:
	if ( fd >= 0 ) {
		close(fd);
		fd = -1;
	}
	
	return port;
}
#endif

/*
 * wtg_report_proc():		Report process information to wtg master.
 */
static void
wtg_report_proc()
{
	static time_t		report_timeout = 0;
	static const char	*ccd = "ccd", *mcd = "mcd", *dcc = "dcc", *other = "other";
	const char			*type_str = NULL;
	list_head_t			*list_item = NULL, *tmp_list = NULL;
	cmd_stat_t			*item = NULL;
	proc_info_t			*proc_node = NULL;
	unsigned			item_cnt = 0;
	unsigned			tmp_max = PROC_ITEM_MAX;

	if ( time(NULL) > report_timeout ) {
		if ( tmp_max == 0 ) {
			return;
		}
		
		memset(report_proc_items, 0, sizeof(wtg_api_proc_t) * PROC_ITEM_MAX);

		if ( strlen(watchdog_config_name) > 0 ) {
			snprintf(report_proc_items[item_cnt].key, ADD_INFO_SIZE - 1,
				"watchdog_conf_name");
			strncpy(report_proc_items[item_cnt].value, watchdog_config_name, ADD_INFO_SIZE - 1);
			item_cnt++;
			if ( item_cnt >= PROC_ITEM_MAX ) {
				// Full.
				goto out;
			}
		}

		if ( strlen(local_cur_path) > 0 ) {
			snprintf(report_proc_items[item_cnt].key, ADD_INFO_SIZE - 1,
				"watchdog_bin_path");
			strncpy(report_proc_items[item_cnt].value, local_cur_path, ADD_INFO_SIZE - 1);
			item_cnt++;
			if ( item_cnt >= PROC_ITEM_MAX ) {
				// Full.
				goto out;
			}
		}

		if ( strlen(version_str) > 0 ) {
			snprintf(report_proc_items[item_cnt].key, ADD_INFO_SIZE - 1,
				"mcp++_watchdog_version");
			strncpy(report_proc_items[item_cnt].value, version_str, ADD_INFO_SIZE - 1);
			item_cnt++;
			if ( item_cnt >= PROC_ITEM_MAX ) {
				// Full.
				goto out;
			}
		}

		if ( strlen(wtg_version_string) > 0 ) {
			snprintf(report_proc_items[item_cnt].key, ADD_INFO_SIZE - 1,
				"wtg_client_version");
			strncpy(report_proc_items[item_cnt].value, wtg_version_string, ADD_INFO_SIZE - 1);
			item_cnt++;
			if ( item_cnt >= PROC_ITEM_MAX ) {
				// Full.
				goto out;
			}
		}
		
		list_for_each_safe(list_item, tmp_list, &global_info.run_list) {
			item = list_entry(list_item, cmd_stat_t, list);
			proc_node = &global_info.p_proc_tab->procs[item->cmd_idx];
		
			if ( !proc_node->used || proc_node->pid == -1 ) {
				// Invalid node. Del in shm_scan.
				log_sys("Invalid shm node found in wtg report process information. Command index is %d.\n",
					item->cmd_idx);
				continue;
			}

			if ( proc_node->proc_type == PROC_TYPE_CCD ) {
				type_str = ccd;
			} else if ( proc_node->proc_type == PROC_TYPE_MCD ) {
				type_str = mcd;
			} else if ( proc_node->proc_type == PROC_TYPE_DCC ) {
				type_str = dcc;
			} else {
				type_str = other;
			}
		
			if ( strlen(proc_node->frame_version) > 0 ) {
				snprintf(report_proc_items[item_cnt].key, ADD_INFO_SIZE - 1,
					"Proc_%d_%s::frame_version", item->cmd_idx, type_str);
				strncpy(report_proc_items[item_cnt].value, proc_node->frame_version, ADD_INFO_SIZE - 1);
				item_cnt++;
				if ( item_cnt >= PROC_ITEM_MAX ) {
					// Full.
					break;
				}
			}

			if ( strlen(proc_node->plugin_version) > 0 ) {
				snprintf(report_proc_items[item_cnt].key, ADD_INFO_SIZE - 1,
					"Proc_%d_%s::plugin_version", item->cmd_idx, type_str);
				strncpy(report_proc_items[item_cnt].value, proc_node->plugin_version, ADD_INFO_SIZE - 1);
				item_cnt++;
				if ( item_cnt >= PROC_ITEM_MAX ) {
					// Full.
					break;
				}
			}

			if ( strlen(proc_node->server_ports) > 0 ) {
				snprintf(report_proc_items[item_cnt].key, ADD_INFO_SIZE - 1,
					"Proc_%d_%s::server_ports", item->cmd_idx, type_str);
				strncpy(report_proc_items[item_cnt].value, proc_node->server_ports, ADD_INFO_SIZE - 1);
				item_cnt++;
				if ( item_cnt >= PROC_ITEM_MAX ) {
					// Full.
					break;
				}
			}

			if ( strlen(proc_node->add_info_0) > 0 ) {
				snprintf(report_proc_items[item_cnt].key, ADD_INFO_SIZE - 1,
					"Proc_%d_%s::add_info_0", item->cmd_idx, type_str);
				strncpy(report_proc_items[item_cnt].value, proc_node->add_info_0, ADD_INFO_SIZE - 1);
				item_cnt++;
				if ( item_cnt >= PROC_ITEM_MAX ) {
					// Full.
					break;
				}
			}

			if ( strlen(proc_node->add_info_1) > 0 ) {
				snprintf(report_proc_items[item_cnt].key, ADD_INFO_SIZE - 1,
					"Proc_%d_%s::add_info_1", item->cmd_idx, type_str);
				strncpy(report_proc_items[item_cnt].value, proc_node->add_info_1, ADD_INFO_SIZE - 1);
				item_cnt++;
				if ( item_cnt >= PROC_ITEM_MAX ) {
					// Full.
					break;
				}
			}
		}

out:
		if ( wtg && item_cnt > 0 ) {
			wtg_api_report_proc(wtg, report_proc_items, item_cnt);
		}

		report_timeout = time(NULL) + REPORT_TICK;
	}
}

/*
 * from32to16():	TCP checksum internal, copied from linux source.
 */
static inline unsigned short
from32to16(unsigned int x)
{
	/* add up 16-bit and 16-bit for 16+c bit */
	x = (x & 0xffff) + (x >> 16);
	/* add up carry.. */
	x = (x & 0xffff) + (x >> 16);
	return x;
}

/*
 * do_csum():		TCP checksum internal, copied from linux source.
 */
static unsigned int
do_csum(const unsigned char *buff, int len)
{
	int odd, count;
	unsigned int result = 0;

	if (len <= 0)
		goto out;
	odd = 1 & (unsigned long) buff;
	if (odd) {
#ifdef __LITTLE_ENDIAN
		result += (*buff << 8);
#else
		result = *buff;
#endif
		len--;
		buff++;
	}
	count = len >> 1;		/* nr of 16-bit words.. */
	if (count) {
		if (2 & (unsigned long) buff) {
			result += *(unsigned short *) buff;
			count--;
			len -= 2;
			buff += 2;
		}
		count >>= 1;		/* nr of 32-bit words.. */
		if (count) {
			unsigned int carry = 0;
			do {
				unsigned int w = *(unsigned int *) buff;
				count--;
				buff += 4;
				result += carry;
				result += w;
				carry = (w > result);
			} while (count);
			result += carry;
			result = (result & 0xffff) + (result >> 16);
		}
		if (len & 2) {
			result += *(unsigned short *) buff;
			buff += 2;
		}
	}
	if (len & 1)
#ifdef __LITTLE_ENDIAN
		result += *buff;
#else
		result += (*buff << 8);
#endif
	result = from32to16(result);
	if (odd)
		result = ((result >> 8) & 0xff) | ((result & 0xff) << 8);
out:
	return result;
}

/*
 * get_tcp_checksum():	Get TCP checksum.
 * @buff:				Data.
 * @len:					Length of data.
 * Returns:				Return the checksum or 0 on invalid.
 */
unsigned short
get_tcp_checksum(const char *buff, unsigned len)
{
	return (unsigned short)do_csum((const unsigned char *)buff, (int)len);
}

/*
 * get_wtg_local_id():	Get wtg local ID.
 * @path_str:			Local path string.
 * Returns:			Just the local ID.
 */
static unsigned short
get_wtg_local_id(char *path_str)
{
	return get_tcp_checksum(path_str, strlen(path_str));
}

/*
 * init_wtg_client():	Initialize wtg client.
 * Returns:			0 on success, -1 on error.
 */
inline static int
init_wtg_client()
{
	int			ival = DEF_USE_FLAG;
	char		*val = NULL;
	
	ival = myconfig_get_intval("wtg_enable", DEF_USE_FLAG);
	val = myconfig_get_value("wtg_domain_address");

	if ( wtg_api_init_v2(&wtg_entry, WTG_API_TYPE_MCP, 0, NULL, ival, val) ) {
		log_sys("Initialize wtg client fail!\n");
		return -1;
	}

	wtg = &wtg_entry;

	return 0;
}
#endif

/*
 * watchdog_entry():		Watchdog worker entry.
 * Returns:				0 on success, -1 on error.
 */
static int
watchdog_entry()
{
	static time_t	sc_warnning_timeout = 0;	// Strict check warnning timeout;
	int				ret = 0;
	time_t			tick_time_out, cur_time;	

#ifdef __ENABLE_REPORT
	// Initialize wtg network.
	unsigned short	local_id;

	memset(local_cur_path, 0, PATH_MAX);

	if ( global_info.local_id ) {
		local_id = global_info.local_id;
	} else {
		if ( !getcwd(local_cur_path, PATH_MAX - 1) ) {
			log_sys("Get local current path fail!\n");
			ret = -1;
			goto err_out;
		}
		local_id = get_wtg_local_id(local_cur_path);
	}
	
	if ( wtg_start(local_id) ) {
		log_sys("Start wtg daemon fail!\n");
		ret = -1;
		goto err_out;
	}
	log_sys("Wtg start successfully!\n");

	if ( init_wtg_client() ) {
		log_sys("Init wtg client fail!\n");
		ret = -1;
		goto err_out;
	}

	if ( wtg ) {
		wtg_api_report_alarm(wtg, 1, "Server restart!");
	}
#endif
	
	if ( start_all_proc() ) {
		log_sys("Start all processes fail!\n");
		ret = -1;
		goto err_out;
	}

	log_sys("Watchdog start!\n");
	fprintf(stderr, "Watchdog start!\n");

	tick_time_out = time(NULL) + CHECK_TICK;
	while ( !stop && global_info.active_cnt ) {
		if ( time(NULL) > tick_time_out ) {
			if ( log_check(global_info.log_rotate_size) ) {
				log_sys("Log check fail!\n");
			}

			// Dead lock check and get report information.
			scan_shm();

#ifdef __ENABLE_REPORT
			wtg_report_proc();
#endif

			tick_time_out = time(NULL) + CHECK_TICK;
		}

		if ( wait_child() ) {
			log_sys("Wait child process fail!\n");
			ret = -1;
			goto err_out;
		}

		if ( global_info.strict_check && global_info.active_cnt < global_info.proc_cnt ) {
			cur_time = time(NULL);
			if ( sc_warnning_timeout + SC_WARNNING_TICK < cur_time ) {
				sc_warnning_timeout = cur_time;
				log_sys("Watchdog active process count not equal config process count!\n");
				if ( wtg ) {
					wtg_api_report_alarm(wtg, 1,
						"Watchdog active process count not equal config process count!\n");
				}
			}
		}

		kill_dead_procs();

		sleep(HUNGUP_TIME);		// Release CPU.
	}

	if ( global_info.active_cnt == 0 && ret == 0 ) {
		log_sys("Processes all exited and watchdog status is correct!\n");
	}

err_out:
	set_stop();

	return ret;
}

/*
 * watchdog_stop():			Watchdog stop.
 * Returns:					0 on success, -1 on error. Actually, -1 will never return.
 */
static void
watchdog_stop()
{
	pid_t			pid;
	int				exit_code = 0, i;
	
	// Cancel SIGALRM first.
	alarm(0);

	stop_all_proc(SIGTERM);

	wait_all_proc();

	// Stop all processes in the process group of watchdog. Avoid any uncontrol process exist.
	kill(0, SIGTERM);

	if ( global_info.force_exit ) {
		if ( global_info.force_exit_wait > 0 ) {
			sleep(global_info.force_exit_wait);
		}
		
		for ( i = 0; i < PROC_MAX_COUNT; i++ ) {
			if ( stop_pids[i] > 0 ) {
				kill(stop_pids[i], SIGKILL);
			}
		}
	}

	worker_destroy_proc_tab();

#ifdef __ENABLE_REPORT
	wtg_stop();
#endif

	if ( strlen(global_info.exit_shell) > 0 ) {
		log_sys("Run exit shell script \"%s\".\n", global_info.exit_shell);
		fprintf(stderr, "Run exit shell script \"%s\".\n", global_info.exit_shell);
		if ( worker_shell_script(global_info.exit_shell) ) {
			log_sys("Run exit shell script \"%s\" fail!\n", global_info.exit_shell);
			fprintf(stderr, "Run exit shell script \"%s\" fail!\n", global_info.exit_shell);
		}
	}

	if ( watchdog_restart ) {
		log_sys("Prepare restart watchdog...\n");
	} else {
		log_sys("Prepare stop watchdog...\n");
	}

	log_close();		// Close log.

	if ( watchdog_restart ) {
		pid = fork();
		if ( pid == -1 ) {
			log_sys("fork fail when restart watchdog!\n");
			exit_code = 18;
			goto out;
		}

		if ( pid > 0 ) {
			// Parent process exit.
			goto out;
		}

		if ( pid == 0 ) {
			// Restart.
			log_sys("Restart watchdog...\n");
			if ( execv(w_argv[0], w_argv) == -1 ) {
				log_sys("Restart watchdog fail!\n");
				exit_code = 19;
				goto out;
			}
		}
	}

out:

	if ( exit_code == 0 ) {
		printf("Watchdog stop successfully!\n");
	} else {
		printf("Watchdog stop fail!\n");
	}

	exit(exit_code);
}

/*
 * worker_watchdog_start:		Watchdog entry entrance.
 */
void
worker_watchdog_start()
{
	int				exit_status = 0;

	memset(stop_pids, -1, sizeof(int) * PROC_MAX_COUNT);

	if ( strlen(global_info.init_shell) > 0 ) {
		log_sys("Run init shell script \"%s\".\n", global_info.init_shell);
		fprintf(stderr, "Run init shell script \"%s\".\n", global_info.init_shell);

		if ( worker_shell_script(global_info.init_shell) ) {
			log_sys("Run init shell script \"%s\" fail!\n", global_info.init_shell);
			fprintf(stderr, "Run init shell script \"%s\" fail!\n", global_info.init_shell);
			goto err_out0;
		}
	}
	
	if ( daemon_init() ) {
		log_sys("Start daemon fail!\n");
		printf("Start daemon fail!\n");
		exit_status = 11;
		goto err_out0;
	}

	if ( watchdog_entry() ) {
		log_sys("Watchdog running error!\n");
		printf("\nWatchdog running error!\n");
		exit_status = 12;
		goto err_out1;
	}

err_out1:
	watchdog_stop();

err_out0:	
	worker_destroy_proc_tab();

#ifdef __ENABLE_REPORT
	wtg_stop();
#endif
	
	exit(exit_status);
}

