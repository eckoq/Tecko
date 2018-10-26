/*
 * watchdog_nws_util_str.h:		Copied from nws.
 * Date:						2011-02-18
 */

#ifndef __WATCHDOG_NWS_UTIL_STR_H
#define __WATCHDOG_NWS_UTIL_STR_H

/*
 * NULL IFS: default blanks
 * first byte is NULL, IFS table
 * first byte is NOT NULL, IFS string
 */
int
str_explode(const char *ifs, char *line0, char *field[], int n);

#endif

