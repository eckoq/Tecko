#include <unistd.h>
#include <sys/resource.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <sched.h>
#include "mydaemon.h"

bool stop = false;
static void sigterm_handler(int signo) {
	stop = true;
}
void mydaemon(const char* name) {
	daemon(1, 1);
	initenv(name);
}
void initenv(const char* name) {
	
	rlimit rlim;
	rlim.rlim_cur = 1000000;
	rlim.rlim_max = 1000000;
	setrlimit(RLIMIT_NOFILE, &rlim);

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
