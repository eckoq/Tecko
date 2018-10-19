/*
 * wtg_daemon.h:			Wtg daemon interface. Watchdog must include this file.
 * Date;					2011-04-09
 */

#ifndef __WTG_DAEMON_H
#define __WTG_DAEMON_H

#include <sys/cdefs.h>

__BEGIN_DECLS

/*
 * wtg_start():	Start wtg module.
 * Returns:		0 on success, -1 on error.
 */
extern int
wtg_start();

/*
 * wtg_stop():	Stop wtg module.
 */
extern void
wtg_stop();

/*
 * wtg_release_in_child():		Release resource befor fork() in child process. NWS watchdog must call this befor fork.
 */
extern void
wtg_release_in_child();

__END_DECLS

#endif

