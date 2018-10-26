/*
 * watchdog_log.h:	Declear of log interface.
 * Date:				2011-02-17
 */

#ifndef __WATCHDOG_LOG_H
#define __WATCHDOG_LGO_H

#include <stdarg.h>

/*
 * log_open():	Open output file.
 * @fname:		Output file name. If caller want to output to stdout, please set this to NULL.
 * Returns:		0 on success, -1 on error. If fname != NULL, errno will be set when error occurs.
 */
extern int
log_open(const char *fname);

/*
 * log_sys():		Output, frame should call this to make output.
 * @fmt:			Just as printf().
 * @...			Params.
 */
extern void
log_sys(const char *fmt, ...);

/*
 * log_va():		Output log by va_list args.
 * @fmt:			Just as printf().
 * @ap:			va_list args.
 */
extern void
log_va(const char *fmt, va_list ap);

/*
 * log_close():	Close output file. Stdout will not be close.
 */
extern void
log_close();

/*
 * log_check():	Check log file size. Reset log file when the size over rotate_size.
 * @rotate_size:	Max log file size.
 * Returns:		0 on success, -1 on error.
 */
extern int
log_check(long long rotate_size);

#endif

