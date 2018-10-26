/*
 * daemon_api.h:			Wtg daemon interface.
 * Date:					2011-04-13
 */

#ifndef __DAEMON_API_H
#define __DAEMON_API_H

#include <sys/cdefs.h>

__BEGIN_DECLS

/*
 * wtg_dump_args():		Dump command args.
 * @pname:				Program name for execv().
 * @argc:				Command line argc.
 * @argv:				Command line argv.
 * Returns:				0 on success, -1 on error.
 */
extern int
wtg_dump_args(char *pname, int argc, char **argv);

/*
 * wtg_start():	Start wtg module.
 * @port:		Client PORT.
 * Returns:		0 on success, -1 on error.
 */
extern int
wtg_start(unsigned short port);

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

