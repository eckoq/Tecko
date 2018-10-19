/*
 * watchdog_worker.h:		Watchdog entry.
 * Date:					2011-02-21
 */

#ifndef __WATCHDOG_WORKER_H
#define __WATCHDOG_WORKER_H

/*
 * worker_watchdog_start:		Watchdog entry entrance.
 */
void
worker_watchdog_start();

/*
 * worker_init_proc_tab():	Initialize process table.
 * Returns:				0 on success, -1 on error.
 */
extern int
worker_init_proc_tab();

/*
 * worker_destroy_proc_tab():	Cleanup the process table in shm.
 */
extern void
worker_destroy_proc_tab();


#endif

