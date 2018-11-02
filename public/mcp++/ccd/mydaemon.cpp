#include <unistd.h>
#include <sys/resource.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <sched.h>
#include <sys/file.h>
#include <errno.h>
#include "mydaemon.h"

bool stop = false;
static void sigterm_handler(int signo) {
	stop = true;
}

int mydaemon(const char* name) {
    return mydaemon(name, kDefaultMaxOpenFileNum);
}

int mydaemon(const char* name, int max_open_file_num) {
	daemon(1, 1);
	return initenv(name, max_open_file_num);
}

int initenv(const char* name) {
    return initenv(name, kDefaultMaxOpenFileNum);
}

int initenv(const char* name, int max_open_file_num) {
	rlimit rlim;
	rlim.rlim_cur = max_open_file_num;
	rlim.rlim_max = max_open_file_num;
	int ret = setrlimit(RLIMIT_NOFILE, &rlim);
    if (ret != 0) {
        fprintf(stderr, "setrlimit to:%d error, errno:%d, msg:%m\n",
                max_open_file_num, errno);
    }

	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = sigterm_handler;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGQUIT, &sa, NULL);
	sigaction(SIGUSR2, &sa, NULL);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGCHLD, SIG_DFL);

	char tmp[128] = {0};
	sprintf(tmp, "%s.pid", name);
	FILE* pf = fopen(tmp, "w+");
	fprintf(pf, "%d\n", (int)getpid());
	fclose(pf);
    return ret;
}
void cpubind(const char* name, int cpuid) {

	long cpunr = sysconf(_SC_NPROCESSORS_CONF);
	if ( cpunr == -1 ) {
		fprintf(stderr, "Get count of cpu cores fail! %m\n");
		cpunr = 0;
	}
	else {
		//fprintf(stderr, "CPU cores: %ld.\n", cpunr);
	}

	if ( cpuid >= cpunr ) {
		fprintf(stderr, "%s CPU to bind is not existed! bind_cpu = %d.\n", name, cpuid);
		cpuid = 0;
	}

	cpu_set_t cpu_mask;
	CPU_ZERO(&cpu_mask);
	CPU_SET(cpuid, &cpu_mask);

	if ( sched_setaffinity(0, sizeof(cpu_set_t), &cpu_mask) ) {
		fprintf(stderr, "Bind %s to CPU %d fail! %m\n", name, cpuid);
	} else {
		fprintf(stderr, "Bind %s to CPU %d success!\n", name, cpuid);
	}
}

bool one_instance(const char *lock_file)
{
    int lock_fd = open(lock_file, O_RDWR, 0640);
	if(lock_fd < 0 )
	{
		printf("open lock file %s failed (%m), cannot init server.\n", lock_file);
		return false;
	}

	int ret = flock(lock_fd, LOCK_EX | LOCK_NB);
	if(0 > ret)
	{
		printf("lock file failed (%m), server already running.\n");
		return false;
	}

    return true;
}

/* get memory (Resident set size) usage by current process
 * mem_usage return in bytes
 * read from /proc/self/status VmRSS
 * return false on error
 */
bool get_mem_usage(unsigned long long *nbytes)
{
    static const char* status_path = "/proc/self/status";
    const int buf_size = 256;
    static char buf[buf_size];
    int nkilo;
    *nbytes = 0;
    FILE *f = fopen(status_path, "r");
    if (f == NULL) {
        fprintf(stderr, "open [%s] failed.\n", status_path);
        return false;
    }
    while (fgets(buf, buf_size, f)) {
        if (strcasestr(buf, "VmRSS:")) {
            sscanf(buf, "VmRSS:%dkB", &nkilo);
            /* from kilobyte to byte */
            *nbytes = (unsigned long long)nkilo* 1024;
            fclose(f);
            return true;
        }
    }
    fclose(f);
    return false;
}

/* get total memory in bytes of the server
 * read from /proc/meminfo MemTotal
 * return false on error
 */
bool get_mem_total(unsigned long long *nbytes)
{
    static const char* info_path = "/proc/meminfo";
    const int buf_size = 256;
    static char buf[buf_size];
    int nkilo;
    *nbytes = 0;
    FILE *f = fopen(info_path, "r");
    if (f == NULL) {
        fprintf(stderr, "open [%s] failed.\n", info_path);
        return false;
    }
    while (fgets(buf, buf_size, f)) {
        if (strcasestr(buf, "MemTotal:")) {
            sscanf(buf, "MemTotal:%dkB", &nkilo);
            /* from kilobyte to byte */
            *nbytes = (unsigned long long)nkilo* 1024;
            fclose(f);
            return true;
        }
    }
    fclose(f);
    return false;
}
