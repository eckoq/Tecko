/*
 * watchdog_version.h:		watchdog version.
 * Date:					2011-02-17
 */

#ifndef __WATCHDOG_VERSION_H
#define __WATCHDOG_VERSION_H

#include "watchdog_log.h"

extern const char	version_str[];		// The version string. Modifiy this string before release.

/*
 * Print version.
 * @show_timeval:		0 not show timeval, others show timeval.
 */
#define DUMP_VERSION(show_timeval) do {	\
	show_timeval ? log_sys("%s\n", version_str)	\
		: printf("%s\n", version_str);	\
} while(0)

#endif

