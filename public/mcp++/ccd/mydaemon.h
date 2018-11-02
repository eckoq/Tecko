#ifndef _MYDAEMON_H_
#define _MYDAEMON_H_

/* output log macro, make the code look cleaner */
#define WRITE_LOG(output, fmt, args...)  do\
{\
    if ( output ) \
    {\
        syslog(LOG_USER | LOG_CRIT | LOG_PID, fmt, ##args);\
    }\
}while(0)

#define LOG_5MIN(output, now_time, log_time, fmt, args...)  do\
{\
    if ( output ) \
    {\
        if ( now_time > log_time ) \
        {\
            syslog(LOG_USER | LOG_CRIT | LOG_PID, fmt, ##args);\
            log_time = now_time + 300;\
        }\
    }\
}while(0)

#define LOG_ONCE(fmt, args...)  do\
{\
    static bool log_once_first_print_log = false; \
    if (log_once_first_print_log == false) \
    {\
        syslog(LOG_USER | LOG_CRIT | LOG_PID, fmt, ##args);\
        log_once_first_print_log = true; \
    }\
} while(0)


extern bool stop;				//true-运行，false-退出

const static int kDefaultMaxOpenFileNum = 1000 * 1000;

extern int mydaemon(const char* name);
extern int mydaemon(const char* name, int max_open_file_num);

extern int initenv(const char* name);
extern int initenv(const char* name, int max_open_file_num);

extern void cpubind(const char* name, int cpuid);
extern bool one_instance(const char *lock_file);

/* return memory (Resident set size) usage by current process */
extern bool get_mem_usage(unsigned long long *nbytes);
extern bool get_mem_total(unsigned long long *nbytes);
#endif
