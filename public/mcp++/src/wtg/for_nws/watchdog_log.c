/*
 * watchdog_log.h:	Declear of log interface.
 * Date:				2011-02-17
 */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

#define STDOUT_FD		1			// stdout fd.
#define LOGBUF_LEN		2048		// Log tmp buf length.
#define TIME_STR_LEN	128			// Max time string length.
#define NAME_LEN		256			// Log file name max length.

static char		log_name[NAME_LEN];	// Log file name.
static char		tmp_name[NAME_LEN];	// Temp log name buffer.
static int		logfd = STDOUT_FD;	// Output file fd.
static char		std_flag = 1;		// If output to stdout, this flag will be set.

/*
 * log_open():	Open output file.
 * @fname:		Output file name. If caller want to output to stdout, please set this to NULL or "".
 * Returns:		0 on success, -1 on error. If fname != NULL, errno will be set when error occurs.
 */
int
log_open(const char *fname)
{
	memset(log_name, 0, sizeof(char) * NAME_LEN);
	if ( fname != NULL && strlen(fname) > 0 ) {
		// Use tmp_name to handle case "fname == log_name".
		strncpy(tmp_name, fname, NAME_LEN - 1);
		strncpy(log_name, tmp_name, NAME_LEN - 1);
	}
	
	std_flag = 0;
	return ( fname == NULL || strlen(fname) == 0 ? std_flag = 1, logfd = STDOUT_FD, 0 :
		((logfd = open(fname, O_WRONLY | O_CREAT | O_APPEND, 0666)) == -1 ? -1 : 0) );
}

/*
 * Output log internal.
 */
#define LOG(fd, fmt, ap) do {	\
	struct tm		tmm; 	\
	time_t 			now = time(NULL);	\
	long long		time_len, content_len;	\
	char			buf[LOGBUF_LEN];	\
	localtime_r(&now, &tmm);  	\
	time_len = snprintf(buf, TIME_STR_LEN - 1, "[%04d-%02d-%02d %02d:%02d:%02d] ",	\
		tmm.tm_year + 1900, tmm.tm_mon + 1, tmm.tm_mday,	\
		tmm.tm_hour, tmm.tm_min, tmm.tm_sec);	\
	content_len = vsnprintf(buf + time_len, LOGBUF_LEN - time_len - 1, fmt, ap);	\
	write(fd, buf, time_len + content_len);	\
} while(0)

/*
 * log_sys():		Output log.
 * @fmt:			Just as printf().
 * @...			Params.
 */
void
log_sys(const char *fmt, ...)
{
	va_list		ap;
	va_start(ap, fmt);
	LOG(logfd, fmt, ap);
	va_end(ap);
}

/*
 * log_va():		Output log by va_list args.
 * @fmt:			Just as printf().
 * @ap:			va_list args.
 */
void
log_va(const char *fmt, va_list ap)
{
	LOG(logfd, fmt, ap);
}

/*
 * log_close():	Close output file. Stdout will not be close.
 */
void
log_close()
{
	if ( !std_flag && logfd != STDOUT_FD ) {
		close(logfd);
		logfd = STDOUT_FD;
		std_flag = 1;
	}
}

/*
 * log_check():	Check log file size. Reset log file when the size over rotate_size.
 * @rotate_size:	Max log file size.
 * Returns:		0 on success, -1 on error.
 */
int
log_check(long long rotate_size)
{
	struct stat		st;
	
	if ( !std_flag && logfd != STDOUT_FD ) {
		if ( rotate_size > 0 ) {
			if ( !fstat(logfd, &st) ) {
				if ( st.st_size > rotate_size ) {
					// Reset log file.
					log_close();
					if ( strlen(log_name) > 0 ) {
						if ( unlink(log_name) ) {
							return -1;
						}
					}
					if ( log_open(log_name) ) {
						return -1;
					}
				}
			} else {
				return -1;
			}
		}
	}

	return 0;
}

