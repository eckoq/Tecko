/*
 * watchdog_ipc.h:  Watchdog data struct both be used by daemon and client AIPs.
 * Date:            2011-03-17
 */

#ifndef __WATCHDOG_IPC_H
#define __WATCHDOG_IPC_H

/* Addition information max size. */
#define ADD_INFO_SIZE            128

/*
 * Make sure gcc doesn't try to be clever and move things around
 * on us. We need to use _exactly_ the address the user gave us,
 * not some alias that contains the same information.
 * !! Redefined gcc atomic_t for compile error in some case.
 */
typedef struct { volatile int counter; } wd_atomic_t;

enum {
    PROC_TYPE_OTHERS =  0,  // Others.
    PROC_TYPE_CCD    =  1,  // CCD.
    PROC_TYPE_MCD    =  2,  // MCD.
    PROC_TYPE_DCC    =  3   // DCC.
};

/*
 * Process deadlock check information.
 */
typedef struct proc_info {
    int          used;                           // 0 not not be used, 1 on be used.
    pid_t        pid;                            // Process ID.
    wd_atomic_t  tick_cnt;                       // Process touck tick count.
    int          bad_cnt;                        // Process no active checked count.

    int          proc_type;                      // Process type.
    char         frame_version[ADD_INFO_SIZE];   // Frame version.
    char         plugin_version[ADD_INFO_SIZE];  // Plugin version.
    char         server_ports[ADD_INFO_SIZE];    // Server ports. Divided by ','.
    char         add_info_0[ADD_INFO_SIZE];      // Addition information 0.
    char         add_info_1[ADD_INFO_SIZE];      // Addition information 1.
} proc_info_t;

#endif

