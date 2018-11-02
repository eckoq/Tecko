/*
 * watchdog_worker.c:        Watchdog entry.
 * Date:                    2011-02-21
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
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <asm/byteorder.h>

#include "watchdog/atomic.h"
#include "watchdog/watchdog_qnf_list.h"
#include "watchdog/watchdog_common.h"
#include "watchdog/watchdog_log.h"
#include "watchdog/watchdog_version.h"

#define SC_WARNNING_TICK  600

#ifndef MCP_PLUS_PLUS
#define MCP_PLUS_PLUS
#endif

/* usleep() time. */
#define USLEEP_TIME       1000
/* sleep() time. */
#define SLEEP_TIME        1
/* Wait timeout. */
#define WAIT_TIMEOUT      2
/* Int string length. */
#define INT_STR_LEN       32

/* Wether restart watchdog. 0 not restart, 1 restart. */
static int   watchdog_restart = 0;
/* Wether stop watchdog. */
static int   stop = 0;
/* Wether wait_all call. */
static int   wait_all = 0;

/* Temp path buffer. */
static char  path_buf[PATH_MAX];
/* Temp directory buffer. */
static char  dir_buf[PATH_MAX];
/* Temp command buffer. */
static char  cmd_buf[PATH_MAX];
/* Process stop in stop_all_proc(). */
static int   stop_pids[PROC_MAX_COUNT];

/* Alarm message max size. */
#define ALARM_MSG_SIZE        256
/* Alarm message buffer. */
static char  alarm_buf[ALARM_MSG_SIZE];

#ifndef HUNGUP_TIME
/* Sleep time to release CPU. */
#define HUNGUP_TIME            1
#endif

/*
 * cmd_filter():          Get command line path and command.
 * @src:                  Original command line.
 * Returns:               0 on success, -1 on error.
 *                        Store directory in dir_buf,
 *                        store command in cmd_buf and "./" will be added at command head.
 */
inline static int cmd_filter(const char *src)
{
    int    len;
    char  *pos = NULL;

    strcpy(path_buf, src);

    len = strlen(path_buf);
    if ( len <= 0 ) {
        log_sys("Empty command line path!\n");
        return -1;
    }

    if ( path_buf[len - 1] == '/' ) {
        log_sys("Command line could not be a directory!\n");
        return -1;
    }

    pos = strrchr(path_buf, '/');
    if (!pos) {
        pos = path_buf;
    } else {
        pos++;
    }

    if (strlen(pos) > PATH_MAX - 3) {
        log_sys("Too long command \"%s\"!\n", pos);
        return -1;
    }
    cmd_buf[PATH_MAX - 1] = 0;
    if (pos != path_buf) {
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
 * del_from_hash():        Delete item from hash table.
 * @pid:                    Pid KEY.
 * NOTES:                Delete from all hash table to ensure correct.
 */
inline static void del_from_hash(pid_t pid)
{
    myhash_del(&global_info.run_hash, pid, NULL);
    myhash_del(&global_info.stopped_hash, pid, NULL);
}

/*
 * get_cmd_from_list():  Get command item from list.
 * @plist:               List will be searched.
 * @pid:                 Pid KEY.
 * @how:                 0 compare id with pid, others compare id with stopped_id.
 * Returns:              Return the pointer to the item or NULL on not found.
 */
static cmd_stat_t* get_cmd_from_list(list_head_t *plist, pid_t pid, int how)
{
    cmd_stat_t   *item = NULL, *pos_item = NULL;
    list_head_t  *list_item = NULL, *tmp_list = NULL;

    list_for_each_safe(list_item, tmp_list, plist) {
        pos_item = list_entry(list_item, cmd_stat_t, list);
        if (how) {
            if (pos_item->stopped_pid == pid) {
                item = pos_item;
                break;
            }
        } else {
            if (pos_item->pid == pid) {
                item = pos_item;
                break;
            }
        }
    }

    return item;
}

/*
 * cmd_item_reset():  Reset item. Not change list.
 * @status:           item->status code will be set.
 */
inline static void cmd_item_reset(cmd_stat_t *item, int status)
{
    item->pid = -1;
    item->status = status;
    item->stopped_pid = -1;
}

/*
 * cmd_add_to_dead():  Add command item to dead list. Make sure item of index is not NULL before call.
 * @index:             Index also command line ID.
 */
inline static void cmd_add_to_dead(int index)
{
    cmd_stat_t  *item = global_info.proc_cmdlines[index];

    /* Delete again anyway. */
    list_del_init(&item->list);
    item->status = CMD_DEAD;
    list_add_tail(&item->list, &global_info.dead_list);
}

/*
 * set_stop():  Set watchdog to stop.
 */
inline static void set_stop()
{
    watchdog_restart = 0;
    stop = 1;
}

/*
 * scan_shm():  Scan shm table.
 */
static void scan_shm()
{
    list_head_t  *list_item = NULL, *tmp_list = NULL;
    cmd_stat_t   *item = NULL;
    proc_info_t  *proc_node = NULL;
    int          cnt = 0;

    list_for_each_safe(list_item, tmp_list, &global_info.run_list) {
        item = list_entry(list_item, cmd_stat_t, list);
        proc_node = &global_info.p_proc_tab->procs[item->cmd_idx];

        if (!proc_node->used || proc_node->pid == -1) {
            /* Invalid node. Add to dead list to cleanup. */
            log_sys("Invalid shm node found in tick handler. Add to dead list. "
                    "Command index is %d.\n", item->cmd_idx);
            if (proc_node->pid != -1) {
                del_from_hash(proc_node->pid);
            }
            cmd_add_to_dead(item->cmd_idx);
            continue;
        }

        cnt = atomic_read((atomic_t *)&proc_node->tick_cnt);
        atomic_sub(cnt, (atomic_t *)&proc_node->tick_cnt);
        if (cnt) {
            proc_node->bad_cnt = 0;
        } else if (proc_node->bad_cnt >= global_info.deadlock_cnt) {
            log_sys("Deadlock process found, pid = %d, item %d - \"%s\".\n",
                    proc_node->pid, item->cmd_idx, item->argv[0]);
            if (global_info.deadlock_kill_enable) {
                proc_node->bad_cnt = 0;
                if (proc_node->pid != -1) {
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

enum RestartLastNormallyQuitCMD {
    DoRestart = 0,
    DoneRestart,
} restart_the_last_normally_quit_cmd;

static void restart_cmd_handler(int signo)
{
   fprintf(stderr, "recv sig usr1, restart the last normally quit process.\n");
   restart_the_last_normally_quit_cmd = DoRestart;
}

static int cmd_start(int index);

static void do_restart_the_last_normally_quit_cmd() {
    if (global_info.normally_stopped_cnt < 1) {
        return;
    }
  
    cmd_stat_t *item = NULL;
    list_head_t *list_item = NULL, *tmp_list = NULL;
    list_for_each_safe(list_item, tmp_list, &global_info.normally_stopped_list) {
        item = list_entry(list_item, cmd_stat_t, list);
        break;
    }
  
    global_info.normally_stopped_cnt--;
    list_del_init(&item->list);
  
    if (cmd_start(item->cmd_idx)) {
        log_sys("Restart process item %d - \"%s\" fail!\n",
                item->cmd_idx, item->argv[0]);
        set_stop();
        fprintf(stderr, "restart error, watchdog quit.\n");
        return;
    }
    fprintf(stderr, "restart the last normally quit process successful.\n");
}

const int DISABLE_LINUX_OOM_SCORE_ADJ = -1000;
const int DISABLE_LINUX_OOM_ADJ = -17;
const int ENABLE_LINUX_OOM_SCORE_ADJ = 0;
const int ENABLE_LINUX_OOM_ADJ = 0;

void change_oomkiller_for_process(int pid, int oom_score_adj, int oom_adj) {
    /*
     * By default, Linux tends to kill the watchdog in out-of-memory
     * situations, because it blames the watchdog for the sum of child
     * process sizes *including shared memory*.  (This is unbelievably
     * stupid, but the kernel hackers seem uninterested in improving it.)
     * Therefore it's often a good idea to protect the watchdog by
     * setting its oom_score_adj value negative (which has to be done in a
     * root-owned startup script). If you just do that much, all child
     * processes will also be protected against OOM kill, which might not
     * be desirable.  You can then choose to build with
     * LINUX_OOM_SCORE_ADJ #defined to 0, or to some other value that you
     * want child processes to adopt here.
     */

    char proc_file[1024];
    {
        /*
         * Use open() not stdio, to ensure we control the open flags. Some
         * Linux security environments reject anything but O_WRONLY.
         */
        sprintf(proc_file, "/proc/%d/oom_score_adj", pid);
        int fd = open(proc_file, O_WRONLY, 0);
        /* We ignore all errors */
        if (fd >= 0) {
            char  buf[16];
            int   rc;

            snprintf(buf, sizeof(buf), "%d\n", oom_score_adj);
            rc = write(fd, buf, strlen(buf));
            (void) rc;
            close(fd);
        }
    }

    /*
     * Older Linux kernels have oom_adj(before 2.6.36) not oom_score_adj.
     * This works similarly except with a different scale of adjustment values.
     */
    {
        sprintf(proc_file, "/proc/%d/oom_adj", pid);
        int fd = open(proc_file, O_WRONLY, 0);
        /* We ignore all errors */
        if (fd >= 0) {
            char  buf[16];
            int   rc;

            snprintf(buf, sizeof(buf), "%d\n", oom_adj);
            rc = write(fd, buf, strlen(buf));
            (void) rc;
            close(fd);
        }
    }
}

/*
 * tick_handler(): Alarm tick handler.
 * @signo:         Signals.
 */
static void tick_handler(int signo)
{
    /*
    list_head_t            *list_item = NULL, *tmp_list = NULL;
    cmd_stat_t            *item = NULL;
    proc_info_t            *proc_node = NULL;
    int                    cnt = 0;
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
 * sigterm_handler():  Signal handler for Signals which will stop watchdog.
 * @signo:             Signals.
 */
static void stop_handler(int signo)
{
    set_stop();
}

/*
 * sigterm_handler():  Signal handler for Signals which will restart watchdog.
 * @signo:             Signals.
 */
static void restart_handler(int signo)
{
    watchdog_restart = 1;
    stop = 1;
}

/*
 * daemon_init():  Initialize daemon.
 * Returns:        0 on success, -1 on error.
 */
static int daemon_init()
{
    struct sigaction  sa;
    sigset_t          sset;
    pid_t             pid;
    int               pfd;
    char              pid_buf[INT_STR_LEN] = {0};
    char              pid_name[PATH_MAX] = {0};
    int               pid_len;

    if (global_info.foreground) {
        log_sys("Run as foreground.\n");
        printf("Run as foreground.\n");
    } else {
        log_sys("Run as daemon.\n");
        printf("Switching to daemon...\n");

        pid = fork();
        if (pid == -1) {
            log_sys("fork() fail when initialize daemon!\n");
            return -1;
        }
        if (pid != 0) {
            /* Parent process exit. */
            exit(0);
        }
        if (setsid() == -1) {
            log_sys("setsid() fail when initialize daemon!\n");
            return -1;
        }
    }

    /* Write PID file. */
    snprintf(pid_name, PATH_MAX - 1, "%s.pid", w_argv[0]);
    pid_len = snprintf(pid_buf, INT_STR_LEN - 1, "%ld\n", (long)getpid());
    pfd = open(pid_name, O_CREAT | O_TRUNC | O_WRONLY, 0666);
    if (pfd == -1) {
        log_sys("Create PID file \"%s\" fail! %m\n", pid_name);
        return -1;
    }
    if (write(pfd, pid_buf, pid_len) != pid_len) {
        log_sys("Write PID file \"%s\" fail! %m\n", pid_name);
        close(pfd);
        return -1;
    }
    close(pfd);

    /* Set signals. */
    memset(&sa, 0, sizeof(struct sigaction));

    restart_the_last_normally_quit_cmd = DoneRestart;
    sa.sa_handler = restart_cmd_handler;
    sigaction(SIGUSR1, &sa, NULL);

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
    if (sigprocmask(SIG_UNBLOCK, &sset, NULL) == -1) {
        log_sys("Set signal mask fail!\n");
        return -1;
    }

    return 0;
}

/*
 * worker_system():  Rum command line.
 * @cmd:             Command line.
 * Returns:          >= 0 on exit normally and the return value is param transfered to eixt().
 *                   -1 on exit unormally or error.
 */
static int worker_system(const char *cmd)
{
    int  ret = 0;

    ret = system(cmd);
    if (ret == -1) {
        return -1;
    } else {
        if (WIFEXITED(ret)) {
            return (WEXITSTATUS(ret));
        } else {
            return -1;
        }
    }
}

/*
 * worker_shell_script():  Run shell script.
 * @shell:                 Shell script path.
 * Returns:                0 on success, -1 on error.
 */
static int worker_shell_script(const char *shell)
{
    char  old_dir[PATH_MAX];

    memset(old_dir, 0, PATH_MAX);
    if (!getcwd(old_dir, PATH_MAX - 1) ) {
        log_sys("Get local current path fail when prepare run init shell!\n");
        fprintf(stderr,
                "Get local current path fail when prepare run init shell!\n");
        return -1;
    }

    if (cmd_filter(shell)) {
        log_sys("Invalid init shell script \"%s\"!\n", shell);
        fprintf(stderr, "Invalid init shell script \"%s\"!\n", shell);
        return -1;
    }

    if (strlen(dir_buf) > 0 && chdir(dir_buf) == -1) {
        log_sys("Change directory to \"%s\" fail when run script \"%s\"! %m\n",
                dir_buf, shell);
        fprintf(stderr,
                "Change directory to \"%s\" fail when run script \"%s\"! %m\n",
                dir_buf, shell);
        return -1;
    }

    if (worker_system(cmd_buf) == -1) {
        log_sys("Run init shell script \"%s\" fail! Continue.\n", shell);
        fprintf(stderr, "Run init shell script \"%s\" fail! Continue.\n", shell);
    }

    if (chdir(old_dir)) {
        log_sys("Change to watchdog old directory \"%s\" fail! %m\n", old_dir);
        fprintf(stderr,
                "Change to watchdog old directory \"%s\" fail! %m\n", old_dir);
        return -1;
    }

    return 0;
}

/*
 * worker_init_proc_tab():  Initialize process table.
 * Returns:                 0 on success, -1 on error.
 */
int worker_init_proc_tab()
{
    int   shm_id;
    long  shm_size;
    void  *shm_addr = NULL;

    shm_size = watchdog_calc_shm_size();

    shm_id = shmget(global_info.shm_key, (size_t)shm_size,
                    IPC_CREAT | IPC_EXCL | 0666);
    if (shm_id == -1) {
        if (errno == EEXIST) {
            log_sys("Watchdog already existed! Maybe watchdog alread in "
                    "running! Stop!\n");
            fprintf(stderr, "Watchdog already existed! Maybe watchdog alread "
                    "in running! Stop!\n");
            return -1;
        } else {
            log_sys("Create shm for process table fail! %m\n");
            fprintf(stderr, "Create shm for process table fail! %m\n");
            return -1;
        }
    }
    global_info.shm_id = shm_id;

    shm_addr = shmat(shm_id, NULL, 0);
    if (shm_addr == (void *)(-1)) {
        log_sys("shmat fail! %m\n");
        if (shmctl(shm_id, IPC_RMID, NULL) == -1) {
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
 * worker_destroy_proc_tab():  Cleanup the process table in shm.
 */
void worker_destroy_proc_tab()
{
    if (global_info.p_proc_tab != (proc_tab_t *)(-1)) {
        if (shmdt((void *)global_info.p_proc_tab) == -1) {
            log_sys("Detach shm fail! %m\n");
        }
        global_info.p_proc_tab = (proc_tab_t *)(-1);
    }

    if (global_info.shm_id != -1) {
        if (shmctl(global_info.shm_id, IPC_RMID, NULL) == -1) {
            log_sys("Remove shm fail! %m\n");
        }
        global_info.shm_id = -1;
    }
}

/*
 * set_shm_item():  Set item in shm table.
 * @index:          Command line item index.
 * @pid:            Process ID.
 */
inline static void set_shm_item(int index, pid_t pid)
{
    global_info.p_proc_tab->procs[index].used = 1;
    global_info.p_proc_tab->procs[index].pid = pid;
    atomic_set((atomic_t *)&global_info.p_proc_tab->procs[index].tick_cnt, 0);
    global_info.p_proc_tab->procs[index].bad_cnt = 0;
}

/*
 * clear_shm_item():    Clear item in shm table.
 * @index:            Command line item index.
 */
inline static void clear_shm_item(int index)
{
    memset(&global_info.p_proc_tab->procs[index], 0, sizeof(proc_info_t));
    global_info.p_proc_tab->procs[index].pid = -1;
}

/*
 * cmd_stop():  Stop a command line item. Make sure item of index is not NULL before call.
 * @index:      Index also command line ID.
 * @kill_sig:   First choice of kill signal.
 * @normal:     0 not normal, SIGKILL will be send, others not send SIGKILL.
 * Returns:     0 on success, -1 on error. Actually never return -1.
 * NOTES:       Call to stop when get from dead list or stop all finally.
 */
static int cmd_stop(int index, int kill_sig, int normal)
{
    cmd_stat_t  *item = global_info.proc_cmdlines[index];
    int         tmp_status;

    /* Delete from run list or dead list. */
    list_del_init(&item->list);
    clear_shm_item(index);

    tmp_status = item->status;
    item->status = CMD_STOP;    /* Set stop first. */

    if (item->pid == -1) {
        log_sys("Pid is -1 when stop, %d - \"%s\"!\n",
            index, item->argv[0]);
        goto dont_kill;
    }

    log_sys("Kill process item %d - \"%s\".\n",
        index, item->argv[0]);

    /* Kill process. */
    if (kill(item->pid, kill_sig) == -1) {
        log_sys("Send SIGSEGV to process %d - \"%s\" shich we want to stop "
                "fail! %m\n", index, item->argv[0]);
    }

    /* Anyway try again when unormal. */
#ifdef MCP_PLUS_PLUS
    if (!normal) {
        usleep(USLEEP_TIME);
        kill(item->pid, SIGKILL);
    }
#else
    usleep(USLEEP_TIME);
    kill(item->pid, SIGKILL);
#endif


dont_kill:

    item->stopped_pid = item->pid;

    if (tmp_status != CMD_RUNNING && tmp_status != CMD_DEAD) {
        log_sys("Wrong process item status %d, %d - \"%s\".\n",
            item->status, index, item->argv[0]);
    }

    item->pid = -1;

    if (item->stopped_pid != -1) {
        del_from_hash(item->stopped_pid);    /* Ensure correct. */
        if (myhash_add(&global_info.stopped_hash, item->stopped_pid,
                       item, NULL))
        {
            /* No memory for hash node. */
            log_sys("WARNNING!!!! Add command line item to stopped hash table "
                    "fail!\n");
            /*
             * Not return error here because hash table is only used to rase 
             * the search performance before wait().
             * If item not in hash table, we also search the stopped_list.
             * Not return error to make the process informance correct.
             */
        }
        list_add_tail(&item->list, &global_info.stopped_list);
    }

    return 0;
}

/*
 * cmd_start():  Start a command line item. Item must not be empty.
 * @index:       Index also command line ID.
 * Returns:      0 on success, -1 on error.
 */
static int cmd_start(int index)
{
    cmd_stat_t  *item = global_info.proc_cmdlines[index];
    pid_t       pid;
    char        *old_cmd = NULL;

    /* Remove to ensure correct anyway. */
    list_del_init(&item->list);

    if (item->pid != -1
        || (item->status != CMD_INITED
            && item->status != CMD_STOP
            && item->status != CMD_RESTART))
    {
        log_sys("Command line item %d - \"%s\" status wrong when start! "
                "Process ID is %d.\n", index, item->argv[0], item->pid);
        return -1;
    }

    log_sys("Start process item %d - \"%s\"...\n", index, item->argv[0]);

    if ((pid = fork()) == -1) {
        log_sys("Fork process for item item %d - \"%s\" fail!\n",
                index, item->argv[0]);
        return -1;
    }

    /* If error occurs between fork end and execv, child must end itself. */

    if (pid == 0) {
        /* Child process. */
        if (global_info.disable_oomkiller_kill_watchdog == 1) {
            int childpid = getpid();
            change_oomkiller_for_process(childpid, ENABLE_LINUX_OOM_SCORE_ADJ,
                                         ENABLE_LINUX_OOM_ADJ);
        }

        if (cmd_filter(item->argv[0])) {
            log_sys("Filter the command fail! Item %d - \"%s\".\n",
                index, item->argv[0]);
            /* If found this exit code, means execv fail! */
            exit(CHLD_EXEC_FAIL_EXIT);
        }

        /* Change working directory. */
        if (strlen(dir_buf) > 0 && chdir(dir_buf) == -1) {
            log_sys("Change directory to \"%s\" fail! Item %d - \"%s\".\n",
                    dir_buf, index, item->argv[0]);
            /* If found this exit code, means execv fail! */
            exit(CHLD_EXEC_FAIL_EXIT);
        }

        old_cmd = item->argv[0];
        item->argv[0] = cmd_buf;

        /*
         * Sleep, so watchdog could reset shm item and child could update item
         * correctly.
         */
        usleep(USLEEP_TIME);
        if (execv(cmd_buf, item->argv) == -1) {
            log_sys("Child process execute command line item %d - \"%s\" fail! "
                    "%m.\n", index, old_cmd);
            /* Child process exit itself when error. */
            /* If found this exit code, means execv fail! */
            exit(CHLD_EXEC_FAIL_EXIT);
        }
    }

    /* global_info.active_cnt-- when wait a child stop in any case. */
    global_info.active_cnt++;

    /*
     * If child process execute fail (CHLD_EXEC_FAIL_EXIT).
     * Watchdog will get the case when wait(). In this case must search running
     * table. When wait, must search hash first, if not found, search restart 
     * list and not add the same item to restart list.
     * Must not return error below to make process informance correct.
     */

    /* Update shm first to ensure process update the correct information. */
    set_shm_item(index, pid);

    item->pid = pid;

    del_from_hash(pid);
    if (myhash_add(&global_info.run_hash, pid, item, NULL)) {
        /* No memory for hash node. */
        log_sys("WARNNING!!!! Add command line item to running hash table "
                "fail!\n");
        /*
         * Not return error here because hash table is only used to rase the
         * search performance before wait().
         * If item not in hash table, we also search the run_list.
         * Not return error to make the process informance correct.
         */
    }

    item->status = CMD_RUNNING;

    /*
     * Update list after reset shm because of deadlock check depend the list.
     * So we will not check a shm item that is not the correct status.
     * When stop, we must delete list item first.
     */
    list_add_tail(&item->list, &global_info.run_list);

    return 0;
}

/*
 * start_all_proc():  Start all processes.
 * Returns:           0 on success, -1 on error.
 */
static int start_all_proc()
{
    int  i;

    for (i = 0; i < global_info.proc_cnt; i++) {
        if (cmd_start(i)) {
            /* Not incress retry count when first start. And not restart. */
            log_sys("Start command item %d - \"%s\" fail!\n",
                    i, global_info.proc_cmdlines[i]->argv[0]);
            return -1;
        }
    }

    return 0;
}

/*
 * stop_all_proc():  Stop all processes.
 * @kill_arg:        First choice kill signal.
 */
static void stop_all_proc(int kill_sig)
{
    list_head_t  *list_item = NULL, *tmp_list = NULL;
    cmd_stat_t   *item = NULL;

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
 * wait_all_proc():  Wait all child process.
 */
static void wait_all_proc()
{
    pid_t  pid;

    wait_all = 1;

    while (global_info.active_cnt > 0) {
        alarm(WAIT_TIMEOUT);
        pid = wait(NULL);
        if (pid == -1) {
            /* Wait timeout. */
            alarm(0);/* Cancle alarm although alarm has been timeout. */
            log_sys("Timeout when wait all process. Active count is %d.\n",
                    global_info.active_cnt);
            break;
        }
        alarm(0);
        global_info.active_cnt--;
    }

    if (global_info.active_cnt < 0) {
        /* Debug information. */
        log_sys("WARNNING: active count is < 0 after wait all!\n");
    }
}

/*
 * kill_dead_procs()  Kill processes item in the dead list.
 */
static void kill_dead_procs()
{
    cmd_stat_t   *item = NULL;
    list_head_t  *list_item = NULL, *tmp_list = NULL;

    list_for_each_safe(list_item, tmp_list, &global_info.dead_list) {
        item = list_entry(list_item, cmd_stat_t, list);
        log_sys("Prepare to kill dead process item %d - \"%s\".\n",
            item->cmd_idx, item->argv[0]);
        /* Item will be remove from the dead list in this function. */
        cmd_stop(item->cmd_idx, SIGSEGV, 0);
    }
}

/*
 * cmd_item_find_exit_unnormal():  Find item when exit unnormally.
 * @cpid:                          Pid has been wait.
 * Returns:                        Return pointer to the item or NULL on not found.
 */
inline static cmd_stat_t* cmd_item_find_exit_unnormal(pid_t cpid)
{
    cmd_stat_t  *item = NULL;

    if ((item = (cmd_stat_t*)myhash_find(&global_info.stopped_hash, cpid, NULL)) != NULL
        || (item = (cmd_stat_t*)myhash_find(&global_info.run_hash, cpid, NULL)) != NULL
        || (item = get_cmd_from_list(&global_info.dead_list, cpid, 0)) != NULL
        || (item = get_cmd_from_list(&global_info.stopped_list, cpid, 1)) != NULL
        || (item = get_cmd_from_list(&global_info.run_list, cpid, 0)) != NULL)
    {
        return item;
    }

    return item;    /* NULL. */
}

/*
 * cmd_item_find_exit_normal():  Find item when exit normally.
 * @cpid:                        Pid has been wait.
 * Returns:                      Return pointer to the item or NULL on not found.
 */
inline static cmd_stat_t* cmd_item_find_exit_normal(pid_t cpid)
{
    cmd_stat_t  *item = NULL;

    if ((item = (cmd_stat_t*)myhash_find(&global_info.run_hash, cpid, NULL)) != NULL
        || (item = get_cmd_from_list(&global_info.dead_list, cpid, 0)) != NULL
        || (item = (cmd_stat_t*)myhash_find(&global_info.stopped_hash, cpid, NULL)) != NULL
        || (item = get_cmd_from_list(&global_info.run_list, cpid, 0)) != NULL
        || (item = get_cmd_from_list(&global_info.stopped_list, cpid, 1)) != NULL)
    {
        return item;
    }

    return item;    /* NULL. */
}

/*
 * wait_child():  Wait child process exit.
 * returns:       0 on success, -1 on error.
 */
static int wait_child()
{
    pid_t       cpid;
    int         status, ret = 0;
    cmd_stat_t  *item = NULL;
    unsigned    i = 0;

again:
    if (++i > 10) {
        log_sys("Too many wait process in a row!\n");
        goto err_out;
    }

    cpid = waitpid(-1, &status, WNOHANG);
    if (cpid == -1 && errno == ECHILD) {
        log_sys("ERROR, no child process, active count is %d.\n",
                global_info.active_cnt);
    }

    if (cpid > 0) {
        /* Child process exited. */
        if (WIFEXITED(status)) {
            if (WEXITSTATUS(status) == CHLD_EXEC_FAIL_EXIT) {
                /* execv() fail. */
                if ((item = cmd_item_find_exit_normal(cpid)) != NULL) {
                    /* Item found. */
                    list_del_init(&item->list);
                    del_from_hash(cpid);
                    clear_shm_item(item->cmd_idx);
                    cmd_item_reset(item, CMD_INITED);
                    global_info.active_cnt--;
                    set_stop();
                    log_sys("Execute fail be found! Item %d - \"%s\".\n",
                            item->cmd_idx, item->argv[0]);
                    ret = -1;
                    goto err_out;
                } else {
                    /* Item not found. Ignore it. */
                    log_sys("Wait uncontrol process exit and ignore it! "
                            "Pid is %d.\n", cpid);
                }
            } else {
                /* Exit normally. Find sequence is the same as above case. */
                if ((item = cmd_item_find_exit_normal(cpid)) != NULL) {
                    /* Item found. */
                    list_del_init(&item->list);
                    del_from_hash(cpid);
                    clear_shm_item(item->cmd_idx);
                    cmd_item_reset(item, CMD_STOP);
                    global_info.active_cnt--;
                    log_sys("Process item %d - \"%s\" exit normally! Exit code "
                            "is %d.\n", item->cmd_idx, item->argv[0],
                            WEXITSTATUS(status));
                    list_add_tail(&item->list,
                                  &global_info.normally_stopped_list);
                    global_info.normally_stopped_cnt++;
                } else {
                    /* Item not found. */
                    log_sys("Wait uncontrol process exit and ignore it! "
                            "Pid is %d. Exit code is %d.\n", cpid,
                            WEXITSTATUS(status));
                }
            }
        } else {
            if ((item = cmd_item_find_exit_unnormal(cpid)) != NULL) {
                /* Item found. Need restart. */
                list_del_init(&item->list);
                del_from_hash(cpid);
                clear_shm_item(item->cmd_idx);
                cmd_item_reset(item, CMD_INITED);
                global_info.active_cnt--;
                log_sys("Prepare restart exit unnormally process "
                        "item %d - \"%s\"...\n", item->cmd_idx, item->argv[0]);

                ++global_info.cmd_unnormally_exit_count[item->cmd_idx];
                if (global_info.max_coredump_one_day != 0
                    && global_info.cmd_unnormally_exit_count[item->cmd_idx]
                       >= global_info.max_coredump_one_day)
                {
                    log_sys("Too may coredumps:%d one day for item:%d process:%s\n",
                            global_info.cmd_unnormally_exit_count[item->cmd_idx],
                            item->cmd_idx, item->argv[0]);
                } else {
                    if (cmd_start(item->cmd_idx)) {
                        log_sys("Restart process item %d - \"%s\" fail!\n",
                                item->cmd_idx, item->argv[0]);
                        set_stop();
                        ret = -1;
                        goto err_out;
                    }
                }
            } else {
                /* Item not found. Ignore it. */
                log_sys("Wait uncontrol process exit and ignore it! Pid is %d. "
                        "Exit unnormally.\n", cpid);
            }
        }
        goto again;
    }

err_out:

    return ret;
}

static void reset_unnormally_exit_count_if_pass_one_day() {
    static time_t  last_reset = 0;
    time_t         now = time(0);

    if (now - last_reset < 24 * 60 * 60) {
        return;
    }

    /*
     * Reset unnormally_exit_count,
     * do not reset those already get to max_coredump_one_day
     */
    int i = 0;
    for (i = 0; i < PROC_MAX_COUNT; ++i) {
        if (global_info.cmd_unnormally_exit_count[i]
            < global_info.max_coredump_one_day)
            global_info.cmd_unnormally_exit_count[i] = 0;
    }
    last_reset = now;
}

/*
 * watchdog_entry():  Watchdog worker entry.
 * Returns:           0 on success, -1 on error.
 */
static int watchdog_entry()
{
    /* Strict check warnning timeout; */
    static time_t  sc_warnning_timeout = 0;
    int            ret = 0;
    time_t         tick_time_out, cur_time;

    if (start_all_proc()) {
        log_sys("Start all processes fail!\n");
        ret = -1;
        goto err_out;
    }

    fprintf(stderr, "Watchdog start!\n");

    tick_time_out = time(NULL) + CHECK_TICK;
    while (!stop && global_info.active_cnt) {
        if (time(NULL) > tick_time_out) {
            if (log_check(global_info.log_rotate_size)) {
                log_sys("Log check fail!\n");
            }

            /* Dead lock check and get report information. */
            scan_shm();
            tick_time_out = time(NULL) + CHECK_TICK;
        }

        if (wait_child()) {
            log_sys("Wait child process fail!\n");
            ret = -1;
            goto err_out;
        }

        if (global_info.strict_check
            && global_info.active_cnt < global_info.proc_cnt)
        {
            cur_time = time(NULL);
            if (sc_warnning_timeout + SC_WARNNING_TICK < cur_time) {
                sc_warnning_timeout = cur_time;
                log_sys("Watchdog active process count not equal config "
                        "process count!\n");
            }
        }

        kill_dead_procs();

        if (global_info.reboot_normally_exited_proc == 1) {
            do_restart_the_last_normally_quit_cmd();
        }

        if (restart_the_last_normally_quit_cmd == DoRestart) {
            restart_the_last_normally_quit_cmd = DoneRestart;
            do_restart_the_last_normally_quit_cmd();
        }

        reset_unnormally_exit_count_if_pass_one_day();
        sleep(HUNGUP_TIME);        /* Release CPU. */
    }

    if (global_info.active_cnt == 0 && ret == 0) {
        log_sys("Processes all exited and watchdog status is correct!\n");
    }

err_out:
    set_stop();

    return ret;
}

/*
 * watchdog_stop():  Watchdog stop.
 * Returns:          0 on success, -1 on error. Actually, -1 will never return.
 */
static void watchdog_stop()
{
    pid_t  pid;
    int    exit_code = 0, i;

    /* Cancel SIGALRM first. */
    alarm(0);
    stop_all_proc(SIGTERM);
    wait_all_proc();

    /*
     * Stop all processes in the process group of watchdog.
     * Avoid any uncontrol process exist.
     */
    kill(0, SIGTERM);

    if (global_info.force_exit) {
        if ( global_info.force_exit_wait > 0 ) {
            sleep(global_info.force_exit_wait);
        }

        for (i = 0; i < PROC_MAX_COUNT; i++) {
            if (stop_pids[i] > 0) {
                kill(stop_pids[i], SIGKILL);
            }
        }
    }

    worker_destroy_proc_tab();

    if (strlen(global_info.exit_shell) > 0) {
        log_sys("Run exit shell script \"%s\".\n", global_info.exit_shell);
        fprintf(stderr, "Run exit shell script \"%s\".\n",
                global_info.exit_shell);
        if (worker_shell_script(global_info.exit_shell)) {
            log_sys("Run exit shell script \"%s\" fail!\n",
                    global_info.exit_shell);
            fprintf(stderr, "Run exit shell script \"%s\" fail!\n",
                    global_info.exit_shell);
        }
    }

    if (watchdog_restart) {
        log_sys("Prepare restart watchdog...\n");
    } else {
        log_sys("Prepare stop watchdog...\n");
    }

    log_close();        /* Close log. */

    if (watchdog_restart) {
        pid = fork();
        if (pid == -1) {
            log_sys("fork fail when restart watchdog!\n");
            exit_code = 18;
            goto out;
        }

        if (pid > 0) {
            /* Parent process exit. */
            goto out;
        }

        if (pid == 0) {
            /* Restart. */
            log_sys("Restart watchdog...\n");
            if (execv(w_argv[0], w_argv) == -1) {
                log_sys("Restart watchdog fail!\n");
                exit_code = 19;
                goto out;
            }
        }
    }

out:

    if (exit_code == 0) {
        printf("Watchdog stop successfully!\n");
    } else {
        printf("Watchdog stop fail!\n");
    }

    exit(exit_code);
}


/*
 * worker_watchdog_start:  Watchdog entry entrance.
 */
void worker_watchdog_start()
{
    int  exit_status = 0;

    memset(stop_pids, -1, sizeof(int) * PROC_MAX_COUNT);

    if (strlen(global_info.init_shell) > 0) {
        log_sys("Run init shell script \"%s\".\n", global_info.init_shell);
        fprintf(stderr, "Run init shell script \"%s\".\n",
                global_info.init_shell);

        if (worker_shell_script(global_info.init_shell)) {
            log_sys("Run init shell script \"%s\" fail!\n",
                    global_info.init_shell);
            fprintf(stderr, "Run init shell script \"%s\" fail!\n",
                    global_info.init_shell);
            goto err_out0;
        }
    }

    if (daemon_init()) {
        log_sys("Start daemon fail!\n");
        printf("Start daemon fail!\n");
        exit_status = 11;
        goto err_out0;
    }

    if (global_info.disable_oomkiller_kill_watchdog == 1) {
        log_sys("Watchdog require to disable oomkiller kill watchdog\n");
        int pid = getpid();
        change_oomkiller_for_process(pid, DISABLE_LINUX_OOM_SCORE_ADJ,
                                     DISABLE_LINUX_OOM_ADJ);
    }

    if (watchdog_entry()) {
        log_sys("Watchdog running error!\n");
        printf("\nWatchdog running error!\n");
        exit_status = 12;
        goto err_out1;
    }

err_out1:
    watchdog_stop();

err_out0:
    worker_destroy_proc_tab();

    exit(exit_status);
}

