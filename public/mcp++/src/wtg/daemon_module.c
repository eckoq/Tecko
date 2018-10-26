/*
 * daemon_module.c:		Wtg client daemon mudule.
 * Date:					2011-04-08
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdarg.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>
#include <sys/un.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netdb.h>

#include "sproto.h"
#include "./list.h"
#include "wtg_version.h"
#include "wtg_hash.h"
#include "common_proto.h"
#ifdef __HARDCODE_CONF			// Open this in makefile when need to hard code configuration.
#include "hardcode_conf.h"
#endif
#include "daemon_module.h"
#include "daemon_api.h"

#ifdef __MCP

#include "watchdog_log.h"
#include "watchdog_qnf_myconfig.h"

/*
 * wtg_log():		Output log.
 * @fmt:			Just like printf().
 * @...:			Args.
 */
#define wtg_log(fmt, args...) log_sys(fmt, ##args)

#else
#ifdef __NWS
/*
#include "service.h"

#define WTG_DEF_LOG_NAME		"../log/wtg.log"	// Default wtg log file name.
#define WTG_LOG_LEVEL			LOG_FAULT			// Log level.
#define WTG_LOG_ROTATE_SIZE		64					// Log roteate size.

DECLARE_LOG(wtg_log_fd, update_wtg_log)

#define wtg_log(fmt, args...) LOG(wtg_log_fd, WTG_LOG_LEVEL, fmt, ##args)
*/
#include "service.h"
#include "./for_nws/watchdog_log.h"

#define WTG_LOG_ROTATE_SIZE		(64<<20)				// Log roteate size.

/*
 * wtg_log():		Output log.
 * @fmt:			Just like printf().
 * @...:			Args.
 */
#define wtg_log(fmt, args...) log_sys(fmt, ##args)

#else

/*
 * wtg_log():		Output log.
 * @fmt:			Just like printf().
 * @...:			Args.
 */
#define wtg_log(fmt, args...) printf(fmt, ##args)

#endif
#endif

#ifdef __FOR_IDE	// Never define.
struct ide {};
#endif

#define WTG_DEF_MASTER_PORT			(-1)					// Wtg default master PORT.
#define WTG_DEF_DOWNLOAD_PORT		WTG_DEF_MASTER_PORT		// Wtg default master PORT.
#define WTG_DEF_REPORT_BUF_SIZE		4194304					// Wtg default report buffer size.
#define WTG_DEF_MRECV_SIZE			65536					// Wtg default master recive buffer size.
#define WTG_DEF_DOMAIN_RBUF_SIZE	65536					// Wtg default UNIX domain connection recive buffer size.
#define WTG_TMP_BUF_SIZE			65536					// Wtg temp buffer size.

#define WTG_DEF_MASTER_TIMEOUT		300						// Master connection default timeout time.
#define WTG_MASTER_TIMEOUT_MIN		120						// Master connection min timeout.

#ifdef __NWS
#define WTG_DEF_LOG_NAME			"../log/wtg_daemon.log"	// Default log name, only for NWS.
#endif

#define WTG_DEF_DOMAIN_CONN_CNT		128						// Wtg default UNIX domain connection count.
#define WTG_DEF_DOMAIN_ADDR			"./wtg_domain.skt"		// Wtg default UNIX domain address.
#define WTG_DEF_DOMAIN_TIMEOUT		1800					// Wtg default UNIX domain connection timeout time.

#define WTG_UPDATE_FLAG_FILE		"./wtg_update.flag"		// Wtg update flag file.

#ifndef DEF_USE_FLAG
#define DEF_USE_FLAG				-1						// Get this means not launch wtg.
#endif

#define WTG_DOMAIN_TIMEOUT_MIN		600						// Wtg min UNIX domain connection timeout time.
#define WTG_DOMAIN_TIMEOUT_MAX		3600					// Wtg max UNIX domain connection timeout time.

#define WTG_EPOLL_WAIT_TIME			1						// epoll_wait() timeout.
#define WTG_EPOLL_EXT_CNT			2						// EPOLL set extend count.
#define WTG_EPOLL_EVENT_CNT			(1<<10)					// EPOLL wait max events' count.

#define WTG_CONN_CHECK_TIME			300						// Unix domain connection timeout check time(sec).

#define WTG_RECONN_LOG_TIME			300						// When reconnection fail, log time tick.

#define WTG_UNLINK_WAIT_TIME		1						// unlink() wait time.
#define WTG_DOMAIN_ADDR_WAIT_TIME	WTG_UNLINK_WAIT_TIME	// Sleep time for unlink UNIX domain socket address file.

#define WTG_REPROT_PIDBIN_ERR		0						// stat_report_req::pid not defined value.
#define WTG_REPORT_PIDBIN_NWS		1						// stat_report_req::pid nws.
#define WTG_REPORT_PIDBIN_MCP		2						// stat_report_req::pid mcp.

#define WTG_REPORT_TYPE_BASE		1						// stat_report_req::type base infomation.
#define WTG_REPORT_TYPE_ATTR		2						// stat_report_req::type attribute.

#define WTG_DEF_DOWNLOAD_SBUF_SIZE	8192					// Update connection default send buffer size.
#define WTG_DEF_DOWNLOAD_RBUF_SIZE	65536					// Update connection default recive buffer size.
#define WTG_DEF_DOWNLOAD_TIMEOUT	60						// Update connection default timeout time(sec).
#define WTG_DOWNLOAD_TIMEOUT_MIN	15						// Update connection min timeout.
#define WTG_DOWNLOAD_SBUF_RESERVE	4096					// Update connection send buffer reserve size.
#define WTG_DEF_UCMD_BUF_MAX		(1<<20)					// Update command file max size.

#define WTG_DEF_FILE_SERVER_PORT	80						// Default file server PORT.

// #define WTG_DEF_DOWNLOAD_TOKEN			"wtgdownload"		// Download token.
// #define WTG_DEF_DOWNLOAD_TOKEN_FACTOR	9801				// Download token time cookie factor.

#define WTG_INT_STR_MAX				64						// Integer string max length.

#define WTG_TMP_EXT					".tmp"					// Middle file extend name.
#define WTG_BAK_EXT					".bak"					// Backup file extend name.

#define WTG_ARG_CNT					16						// Command line args max count.

#define WTG_WAIT_EXIT_TIME			5						// Wait main process exit time.

static char							pro_name[WTG_PATH_SIZE];					// Program naem.
static int							pro_argc;									// Program argc.
static char							*pro_argv[WTG_ARG_CNT];						// Program argv.
static char							pro_argv_str[WTG_ARG_CNT][WTG_PATH_SIZE];	// Program argv buffer.

static wtg_daemon_info_t			wtg_info;				// Wtg daemon information.

static __thread proto_header			wtg_proto_header;					// proto_header tmp buffer.
static __thread stat_report_req_v2		wtg_stat_report_req;				// stat_report_req tmp buffer.
static __thread task_status_v2			wtg_task_status;					// task_status tmp buffer.
static __thread report_file_content_v2	wtg_report_file_content;			// report_file_content tmp buffer.

static __thread char				http_head[WTG_DOWNLOAD_SBUF_RESERVE];	// HTTP response head.
static __thread char				cmd_content[WTG_DEF_UCMD_BUF_MAX];		// Update command file buffer.
static __thread char				fname[WTG_PATH_SIZE + 4];				// File name tmp buffer.
static __thread char				fname2[WTG_PATH_SIZE + 4];				// File name tmp buffer 2.
static __thread char				url_buf[WTG_PATH_SIZE];					// Command URL parse buffer.

#define WTG_RSP_BUF_SIZE			4096									// Response buffer size.
static __thread char				rsp_buf[WTG_RSP_BUF_SIZE];				// Response buffer.

#define WTG_LOG_REPORT_SIZE			65536					// Log file report max size.
#define WTG_DEF_FILE_CONTENT_SIZE	(1<<20)					// Report file content temp buffer size.
static __thread char				rep_file_content[WTG_DEF_FILE_CONTENT_SIZE];	// Report file content tmp buffer.

static pid_t						daemon_pid = -1;		// Pid of daemon.

/*
 * wtg_set_nonblock():		Set to nonblock. Also set FD_CLOEXEC.
 * @fd:					Fd want to be set.
 * Returns:				0 on success, -1 on error.
 */
static inline int
wtg_set_nonblock(int fd)
{
	int		flag;
	
	if ( (flag = fcntl(fd, F_GETFL)) == -1 ) {
		wtg_log("Wtg get file flag fail! %m\n");
		return -1;
	}

	if ( fcntl(fd, F_SETFL, flag | O_NONBLOCK | FD_CLOEXEC) == -1 ) {
		wtg_log("Wtg set file flag fail! %m\n");
		return -1;
	}

	return 0;
}

/*
 * wtg_epoll_add():		Add a socket fd to EPOLL set.
 * @fd:					Socket fd.
 * @evt_flag:				EPOLL event expect EPOLLIN, EPOLLET and EPOLLERR. This three event always be set.
 * Returns:				0 on success, -1 on error.
 */
static inline int
wtg_epoll_add(int fd, int evt_flag)
{
	struct epoll_event		evt;

	evt.events = (evt_flag | EPOLLIN | EPOLLET | EPOLLERR);
	evt.data.fd = fd;
	if ( epoll_ctl(wtg_info.epfd, EPOLL_CTL_ADD, fd, &evt) == -1 ) {
		wtg_log("Wtg add socket fd \"%d\" to EPOLL set fail! %m\n", fd);
		return -1;
	}

	return 0;
}

/*
 * wtg_epoll_mod():		Modified a socket's fd EPOLL event.
 * @fd:					Socket fd.
 * @evt_flag:				EPOLL event expect EPOLLIN, EPOLLET and EPOLLERR. This three event always be set.
 * Returns:				0 on success, -1 on error.
 */
static inline int
wtg_epoll_mod(int fd, int evt_flag)
{
	struct epoll_event		evt;

	evt.events = (evt_flag | EPOLLIN | EPOLLET | EPOLLERR);
	evt.data.fd = fd;
	if ( epoll_ctl(wtg_info.epfd, EPOLL_CTL_MOD, fd, &evt) == -1 ) {
		wtg_log("Wtg modified a socket's fd \"%d\" EPOLL event fail! Epfd %d. errno %d. %m\n",
			fd, wtg_info.epfd, errno);
		return -1;
	}

	return 0;
}

/*
 * wtg_epoll_del():			Delete a fd from EPOLL set.
 * @fd:					Fd to delete.
 * Returns:				0 on success, -1 on error.
 */
static inline int
wtg_epoll_del(int fd)
{
	return epoll_ctl(wtg_info.epfd, EPOLL_CTL_DEL, fd, NULL);
}

/*
 * wtg_conn_touch():			Touch a connection, update the timeout time.
 * @conn:					Connection to touch.
 * @timeout:					Timeout time.
 */
static inline void
wtg_conn_touch(wtg_conn_t *conn, int timeout)
{
	conn->timeout = time(NULL) + timeout;
}

/*
 * wtg_domain_conn_touch():	Touch a UNIX domain connection, update the timeout time.
 * @conn:					Connection to touch.
 */
static inline void
wtg_domain_conn_touch(wtg_conn_t *conn)
{
	wtg_conn_touch(conn, wtg_info.domain_timeout);
}

/*
 * wtg_conn_buff_free():	Free connection buffer.
 * @conn:				Pointer to the connection.
 */
static inline void
wtg_conn_buff_free(wtg_conn_t *conn)
{
	if ( conn->send_buf.data ) {
		free(conn->send_buf.data);
		memset(&conn->send_buf, 0, sizeof(wtg_buff_t));
	}
	
	if ( conn->recv_buf.data ) {
		free(conn->recv_buf.data);
		memset(&conn->recv_buf, 0, sizeof(wtg_buff_t));
	}
}

/*
 * wtg_conn_buff_reset():	Reset connection buffer.
 * @conn:				Pointer to the connection.
 */
static inline void
wtg_conn_buff_reset(wtg_conn_t *conn)
{
	conn->send_buf.offset = 0;
	conn->send_buf.length = 0;
	conn->recv_buf.offset = 0;
	conn->recv_buf.length = 0;
	conn->recv_buf.need = 0;
}

/*
 * wtg_conn_buff_append():	Append data into connection buffer.
 * @buf:						Buffer to append.
 * @data:					Data to fill.
 * @len:						Length of data.
 * Returns:					0 on success, -1 on error.
 */
static inline int
wtg_conn_buff_append(wtg_buff_t *buf, char *data, int len)
{
	if ( len > buf->size - buf->offset - buf->length ) {
#ifdef __DEBUG
		wtg_log("Wtg buffer has not enough space for append data! Buf: size %d offset %d length %d.\n",
			buf->size, buf->offset, buf->length);
#endif
		return -1;
	}

	memcpy(buf->data + buf->offset + buf->length, data, len);
	buf->length += len;

	return 0;
}

/*
 * wtg_conn_buff_use():		Use buffer data.
 * @buf:						Buffer to use.
 * @len:						Length to use.
 * NOTES:					If len is greater than data in buffer, use all buffer data.
 */
static inline void
wtg_conn_buff_use(wtg_buff_t *buf, int len)
{
	if ( len <= 0 ) {
		return;
	}

	if ( buf->length <= len ) {
		buf->offset = 0;
		buf->length = 0;
		return;
	}

	memmove(buf->data, buf->data + buf->offset + len, buf->length - len);
	buf->offset = 0;
	buf->length -= len;
}

/*
 * wtg_conn_init():	Initialize connection.
 * @conn:			Pointer to the connection.
 * @conn_type:		Connection type. Defined in daemon_module.h.
 * Returns:			0 on success, -1 on error.
 */
static int
wtg_conn_init(wtg_conn_t *conn, int conn_type)
{
	if ( conn_type == WTG_CONN_TYPE_DOMAIN ) {
		conn->recv_buf.data = (char *)malloc(sizeof(char) * wtg_info.domain_rbuf_size);
		if ( !conn->recv_buf.data ) {
			wtg_log("Wtg alloc memory for domain connection recive buffer fail!\n");
			goto err_out;
		}
		conn->recv_buf.size = wtg_info.domain_rbuf_size;
	} else if ( conn_type == WTG_CONN_TYPE_MASTER ) {
		conn->send_buf.data = (char *)malloc(sizeof(char) * wtg_info.report_buf_size);
		if ( !conn->send_buf.data ) {
			wtg_log("Wtg alloc memory for master connection send buffer fail!\n");
			goto err_out;
		}
		conn->send_buf.size = wtg_info.report_buf_size;

		conn->recv_buf.data = (char *)malloc(sizeof(char) * wtg_info.mrecv_buf_size);
		if ( !conn->recv_buf.data ) {
			wtg_log("Wtg alloc memory for master connection recive buffer fail!\n");
			goto err_out;
		}
		conn->recv_buf.size = wtg_info.mrecv_buf_size;
	} else if ( conn_type == WTG_CONN_TYPE_DOWNLOAD ) {
		conn->send_buf.data = (char *)malloc(sizeof(char) * wtg_info.download_sbuf_size);
		if ( !conn->send_buf.data ) {
			wtg_log("Wtg alloc memory for update connection send buffer fail!\n");
			goto err_out;
		}
		conn->send_buf.size = wtg_info.download_sbuf_size;

		conn->recv_buf.data = (char *)malloc(sizeof(char) * wtg_info.download_rbuf_size);
		if ( !conn->recv_buf.data ) {
			wtg_log("Wtg alloc memory for update connection recive buffer fail!\n");
			goto err_out;
		}
		conn->recv_buf.size = wtg_info.download_rbuf_size;
	} else {
		wtg_log("Wtg invalid connection type \"%d\" when connection initialize!\n", conn_type);
	}

	wtg_conn_buff_reset(conn);

	if ( conn_type == WTG_CONN_TYPE_DOMAIN ) {
		conn->stat = WTG_CSTAT_DOMAIN_NOT_CONNED;
	} else if ( conn_type == WTG_CONN_TYPE_MASTER ) {
		conn->stat = WTG_CSTAT_MASTER_NOT_CONNED;
	} else if ( conn_type == WTG_CONN_TYPE_DOWNLOAD ) {
		conn->stat = WTG_CSTAT_DOWNLOAD_NOT_CONNED;
	}

	conn->skt = -1;
	conn->type = conn_type;
	INIT_LIST_HEAD(&conn->list);

	if ( conn_type == WTG_CONN_TYPE_DOMAIN ) {
		list_add_tail(&conn->list, &wtg_info.domain_conns.free);
	}

	return 0;

err_out:
	wtg_conn_buff_free(conn);
	
	return -1;
}

/*
 * wtg_conn_close():		Close a connection.
 * @conn:				Pointer to the connection.
 */
static inline void
wtg_conn_close(wtg_conn_t *conn)
{
	if ( conn->skt >= 0 ) {
		if ( wtg_info.epfd >= 0 ) {
			wtg_epoll_del(conn->skt);
		}
		list_del_init(&conn->list);
		if ( conn->type == WTG_CONN_TYPE_DOMAIN ) {
			wtg_hash_del(&wtg_info.domain_conns.used_hash, conn->skt, NULL);			
			list_add_tail(&conn->list, &wtg_info.domain_conns.free);
			conn->stat = WTG_CSTAT_DOMAIN_NOT_CONNED;
		} else if ( conn->type == WTG_CONN_TYPE_MASTER ) {
			conn->stat = WTG_CSTAT_MASTER_NOT_CONNED;
		} else if ( conn->type == WTG_CONN_TYPE_DOWNLOAD ) {
			conn->stat = WTG_CSTAT_DOWNLOAD_NOT_CONNED;
		} else {
			wtg_log("Wtg invalid connection type \"%d\".\n", conn->type);
		}
		close(conn->skt);
		conn->skt = -1;
	} else {
		list_del_init(&conn->list);
		if ( conn->type == WTG_CONN_TYPE_DOMAIN ) {
			list_add_tail(&conn->list, &wtg_info.domain_conns.free);
			conn->stat = WTG_CSTAT_DOMAIN_NOT_CONNED;
		} else if ( conn->type == WTG_CONN_TYPE_MASTER ) {
			conn->stat = WTG_CSTAT_MASTER_NOT_CONNED;
		} else if ( conn->type == WTG_CONN_TYPE_DOWNLOAD ) {
			conn->stat = WTG_CSTAT_DOWNLOAD_NOT_CONNED;
		} else {
			wtg_log("Wtg invalid connection type \"%d\".\n", conn->type);
		}
	}

	wtg_conn_buff_reset(conn);
}

/*
 * wtg_conn_cleanup():	Cleanup a connection.
 * @conn:				Pointer to the connection.
 */
static void
wtg_conn_cleanup(wtg_conn_t *conn)
{
	wtg_conn_close(conn);

	list_del_init(&conn->list);

	wtg_conn_buff_free(conn);
}

/*
 * wtg_get_socket():		Get a socket to connect.
 * Returns:				Return the socket or -1 on error.
 */
static int
wtg_get_socket()
{
	int						fd = -1;
	struct timeval			time_out = {4, 0};

	fd = socket(PF_INET, SOCK_STREAM, 0);
	if ( fd == -1 ) {
		perror("Wtg create socket fail!\n");
		goto err_out;
	}

	if ( setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &time_out, sizeof(struct timeval)) ) {
		perror("Wtg set socket connect send timeout fail!\n");
		goto err_out;
	}

	if ( setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &time_out, sizeof(struct timeval)) ) {
		perror("Wtg set socket connect recive timeout fail!\n");
		goto err_out;
	}

	return fd;

err_out:
	if ( fd >= 0 ) {
		close(fd);
		fd = -1;
	}

	return -1;
}

/*
 * wtg_domain_get_used_by_fd():	Get connection from used list by socket fd.
 * @fd:							Socket fd.
 * Returns:						Pointer to the connection or NULL on not found.
 */
static wtg_conn_t*
wtg_domain_get_used_by_fd(int fd)
{
	wtg_conn_t			*conn = NULL;
	list_head_t			*list_item = NULL, *tmp_list = NULL;
	
	list_for_each_safe(list_item, tmp_list, &wtg_info.domain_conns.used) {
		conn = list_entry(list_item, wtg_conn_t, list);
		if ( conn->skt == fd ) {
			return conn;
		}
	}

	return NULL;
}

/*
 * wtg_close_domain_by_skt():		Close UNIX domain socket by socket fd.
 * @fd:							Socket fd.
 */
static inline void
wtg_close_domain_by_fd(int fd)
{
	wtg_conn_t			*conn = NULL;
	
	if ( (conn = (wtg_conn_t *)wtg_hash_find(&wtg_info.domain_conns.used_hash, fd, NULL))
		== NULL ) {
		wtg_log("Wtg get active from hash fail when error handle! Try get from list.\n");
		if ( (conn = wtg_domain_get_used_by_fd(fd)) == NULL ) {
			wtg_log("Wtg get active from list fail when error handle! Close socket.\n");
			wtg_epoll_del(fd);
			close(fd);
			return;
		}
	}

	wtg_conn_close(conn);
}

/*
 * wtg_connect_master():				Get a master connect socket fd.
 * @name:							Server hostname.
 * @port:							Server port.
 * Returns:							Socket fd on success, -1 on error.
 */
static int
wtg_connect_master(const char *name, unsigned short port)
{
	int						fd = -1;
	struct hostent			*ent = NULL;
	struct in_addr			*ip_addr = NULL;
	struct sockaddr_in		addr;
	// struct timeval			time_out = {4, 0};
#ifdef __HARDCODE_CONF
	struct in_addr			ip_addr_ent;
#endif

	fd = wtg_get_socket();
	if ( fd == -1 ) {
		perror("Wtg get socket fail!\n");
		goto err_out0;
	}
/*
	fd = socket(PF_INET, SOCK_STREAM, 0);
	if ( fd == -1 ) {
		perror("Wtg create socket fail when connect!\n");
		goto err_out0;
	}

	if ( setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &time_out, sizeof(struct timeval)) ) {
		perror("Wtg set socket connect send timeout fail! 1\n");
		goto err_out1;
	}

	if ( setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &time_out, sizeof(struct timeval)) ) {
		perror("Wtg set socket connect recive timeout fail! 1\n");
		goto err_out1;
	}*/

	// Get IP address by hostname.
	memset(&addr, 0, sizeof(struct sockaddr_in));
	
	///////
	ent = gethostbyname(name);
#ifdef __HARDCODE_CONF
	if ( !ent ) {
		goto ip_directly;
	}
#else
	if ( !ent ) {
		perror("Wtg get host by name fail! 1\n");
		goto err_out1;
	}
#endif

	if ( ent->h_addrtype != AF_INET ) {
		perror("Wtg address of name is not IPV4! 1\n");
		goto err_out1;
	}

#ifdef __HARDCODE_CONF
ip_directly:
	if ( ent ) {
#endif
		ip_addr = (struct in_addr *)(ent->h_addr);
#ifdef __HARDCODE_CONF
	} else {
		ip_addr = &ip_addr_ent;
		// Assert conn->type == WTG_CONN_TYPE_MASTER
		ip_addr->s_addr = inet_addr(WTG_MASTER_IP);
		if ( ip_addr->s_addr == INADDR_NONE ) {
			perror("Wtg translate master IP fail!\n");
			goto err_out1;
		}
	}
#endif
	/////////

/*
	ent = gethostbyname(name);
	if ( !ent ) {
		perror("Wtg get host by name fail when connect!\n");
		goto err_out1;
	}

	if ( ent->h_addrtype != AF_INET ) {
		perror("Wtg address of name is not IPV4 when connect!\n");
		goto err_out1;
	}

	ip_addr = (struct in_addr *)(ent->h_addr);
*/

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = ip_addr->s_addr;
	addr.sin_port = htons(port);

	if ( connect(fd, (struct sockaddr *)&addr, (socklen_t)sizeof(struct sockaddr_in)) ) {
#ifdef __HARDCODE_CONF
		if ( fd >= 0 ) {
			close(fd);
			fd = -1;
		}
		fd = wtg_get_socket();
		if ( fd == -1 ) {
			perror("Wtg get socket fail!\n");
			goto err_out1;
		}
		addr.sin_addr.s_addr = inet_addr(WTG_MASTER_IP_EXTERN);
		if ( connect(fd, (struct sockaddr *)&addr, (socklen_t)sizeof(struct sockaddr_in)) ) {
			if ( fd >= 0 ) {
				close(fd);
				fd = -1;
			}
			fd = wtg_get_socket();
			if ( fd == -1 ) {
				perror("Wtg get socket fail!\n");
				goto err_out1;
			}
			addr.sin_addr.s_addr = inet_addr(WTG_MASTER_IP);
			if ( connect(fd, (struct sockaddr *)&addr, (socklen_t)sizeof(struct sockaddr_in)) ) {
				// Retry fail!
				perror("Wtg connect fail!\n");
				goto err_out1;				
			}
		}
#else
		// wtg_log("Wtg connect fail when reconnect! %m\n");
		perror("Wtg connect fail!\n");
		goto err_out1;
#endif
	}

	return fd;

err_out1:
	if ( fd >= 0 ) {
		close(fd);
		fd = -1;
	}
err_out0:

	return -1;
}

/*
 * wtg_connect():						Get a connect socket fd.
 * @ip:								Server IP.
 * @port:							Server port.
 * Returns:							Socket fd on success, -1 on error.
 *//*
static int
wtg_connect(in_addr_t ip, unsigned short port)
{
	int						fd = -1;
	struct sockaddr_in		addr;

	fd = socket(PF_INET, SOCK_STREAM, 0);
	if ( fd == -1 ) {
		perror("Wtg create socket fail when connect!\n");
		goto err_out0;
	}

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = ip;
	addr.sin_port = htons(port);

	if ( connect(fd, (struct sockaddr *)&addr, (socklen_t)sizeof(struct sockaddr_in)) ) {
		perror("Wtg connect fail!\n");
		goto err_out1;
	}

	return fd;

err_out1:
	if ( fd >= 0 ) {
		close(fd);
		fd = -1;
	}
err_out0:

	return -1;
}
*/

/*
 * wtg_network_conn_reconnect():		Network connection reconnect.
 * @conn:							Pointer to the connection.
 * @name:							Server name.
 * @port:							Server PORT connect to.
 * @evt_flag:							EPOLL event expect EPOLLIN, EPOLLET and EPOLLERR. This three event always be set.
 * Returns:							0 on success, -1 on error.
 */
static int
wtg_network_conn_reconnect(wtg_conn_t *conn,
									const char *name,
									unsigned short port,
									int evt_flag)
{
	struct hostent			*ent = NULL;
	struct sockaddr_in		addr;
	struct in_addr			*ip_addr = NULL;
	// struct timeval			time_out = {4, 0};
#ifdef __HARDCODE_CONF
	struct in_addr			ip_addr_ent;
#endif

	wtg_conn_close(conn);

	if ( !name || !strlen(name) ) {
		wtg_log("Wtg empty server name when reconnect!\n");
		return -1;
	}

	if ( conn->type != WTG_CONN_TYPE_MASTER && conn->type != WTG_CONN_TYPE_DOWNLOAD ) {
		wtg_log("Wtg invaild connection type \"%d\" when reconnect!\n", conn->type);
		return -1;
	}

	conn->skt = wtg_get_socket();
	if ( conn->skt == -1 ) {
		wtg_log("Wtg get socket fail! %m\n");
		goto err_out;
	}

/*
	conn->skt = socket(PF_INET, SOCK_STREAM, 0);
	if ( conn->skt == -1 ) {
		wtg_log("Wtg create socket fail when network reconnect! %m\n");
		goto err_out;
	}

	if ( setsockopt(conn->skt, SOL_SOCKET, SO_SNDTIMEO, &time_out, sizeof(struct timeval)) ) {
		wtg_log("Wtg set socket connect send timeout fail! %m\n");
		goto err_out;
	}

	if ( setsockopt(conn->skt, SOL_SOCKET, SO_RCVTIMEO, &time_out, sizeof(struct timeval)) ) {
		wtg_log("Wtg set socket connect recive timeout fail! %m\n");
		goto err_out;
	}*/

	// Get IP address by hostname.
	memset(&addr, 0, sizeof(struct sockaddr_in));

	ent = gethostbyname(name);
#ifdef __HARDCODE_CONF
	if ( !ent ) {
		if ( conn->type == WTG_CONN_TYPE_MASTER ) {
			// IP directly.
			goto ip_directly;
		} else {
			wtg_log("Wtg get host by name \"%s\" fail! %s\n", name, hstrerror(h_errno));
			goto err_out;
		}
	}
#else
	if ( !ent ) {
		wtg_log("Wtg get host by name \"%s\" fail! %s\n", name, hstrerror(h_errno));
		goto err_out;
	}
#endif

	if ( ent->h_addrtype != AF_INET ) {
		wtg_log("Wtg address of name \"%s\" is not IPV4!\n", name);
		goto err_out;
	}

#ifdef __HARDCODE_CONF
ip_directly:
	if ( ent ) {
#endif
		ip_addr = (struct in_addr *)(ent->h_addr);
#ifdef __HARDCODE_CONF
	} else {
		ip_addr = &ip_addr_ent;
		// Assert conn->type == WTG_CONN_TYPE_MASTER
		ip_addr->s_addr = inet_addr(WTG_MASTER_IP);
		if ( ip_addr->s_addr == INADDR_NONE ) {
			wtg_log("Wtg translate master IP \"%s\" fail!\n", WTG_MASTER_IP);
			goto err_out;
		}
	}
#endif

	// wtg_log("Wtg reconnect to \"%s\" - \"%s\".\n",
	//	name, inet_ntoa(*ip_addr));

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = ip_addr->s_addr;
	addr.sin_port = htons(port);
	if ( connect(conn->skt, (struct sockaddr *)&addr, (socklen_t)sizeof(struct sockaddr_in)) ) {
#ifdef __HARDCODE_CONF
		if ( conn->type == WTG_CONN_TYPE_MASTER ) {
			if ( conn->skt >= 0 ) {
				close(conn->skt);
				conn->skt = -1;
			}
			conn->skt = wtg_get_socket();
			if ( conn->skt == -1 ) {
				wtg_log("Wtg get socket fail! %m\n");
				goto err_out;
			}
			addr.sin_addr.s_addr = inet_addr(WTG_MASTER_IP_EXTERN);
			if ( connect(conn->skt, (struct sockaddr *)&addr, (socklen_t)sizeof(struct sockaddr_in)) ) {
				if ( conn->skt >= 0 ) {
					close(conn->skt);
					conn->skt = -1;
				}
				conn->skt = wtg_get_socket();
				if ( conn->skt == -1 ) {
					wtg_log("Wtg get socket fail! %m\n");
					goto err_out;
				}
				addr.sin_addr.s_addr = inet_addr(WTG_MASTER_IP);
				if ( connect(conn->skt, (struct sockaddr *)&addr, (socklen_t)sizeof(struct sockaddr_in)) ) {
					// Retry fail!
					goto err_out;
				}
			}
		} else {
#endif
			// wtg_log("Wtg connect fail when reconnect! %m\n");
			goto err_out;
#ifdef __HARDCODE_CONF
		}
#endif
	}

	if ( wtg_set_nonblock(conn->skt) ) {
 		wtg_log("Wtg set socket to nonblock fail when reconnect!\n");
		goto err_out;
	}

	if ( wtg_epoll_add(conn->skt, evt_flag) ) {
		wtg_log("Wtg add connection to EPOLL set fail!\n");
		goto err_out;
	}

	if ( conn->type == WTG_CONN_TYPE_MASTER ) {
		conn->stat = WTG_CSTAT_MASTER_RECV_HEAD;
		wtg_conn_touch(conn, wtg_info.master_timeout);
	} else if ( conn->type == WTG_CONN_TYPE_DOWNLOAD ) {
		conn->stat = WTG_CSTAT_DOWNLOAD_SEND_REQ;
		wtg_conn_touch(conn, wtg_info.download_timeout);
	}

	return 0;

err_out:
	wtg_conn_close(conn);

	return -1;
}

/*
 * wtg_network_conn_reconnect():		Network connection reconnect.
 * @conn:							Pointer to the connection.
 * @ip:								Server IP connect to.
 * @port:							Server PORT connect to.
 * @evt_flag:							EPOLL event expect EPOLLIN, EPOLLET and EPOLLERR. This three event always be set.
 * Returns:							0 on success, -1 on error.
 */ /*
static int
wtg_network_conn_reconnect(wtg_conn_t *conn, in_addr_t ip, unsigned short port, int evt_flag)
{
	struct sockaddr_in		addr;

	if ( conn->type != WTG_CONN_TYPE_MASTER && conn->type != WTG_CONN_TYPE_DOWNLOAD ) {
		wtg_log("Wtg invaild connection type \"%d\" when reconnect!\n", conn->type);
		return -1;
	}
	
	wtg_conn_close(conn);

	conn->skt = socket(PF_INET, SOCK_STREAM, 0);
	if ( conn->skt == -1 ) {
		wtg_log("Wtg create socket fail when network reconnect! %m\n");
		goto err_out;
	}

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = ip;
	addr.sin_port = htons(port);
	if ( connect(conn->skt, (struct sockaddr *)&addr, (socklen_t)sizeof(struct sockaddr_in)) ) {
		// wtg_log("Wtg connect fail when reconnect! %m\n");
		goto err_out;
	}

	if ( wtg_set_nonblock(conn->skt) ) {
 		wtg_log("Wtg set socket to nonblock fail when reconnect!\n");
		goto err_out;
	}

	if ( wtg_epoll_add(conn->skt, evt_flag) ) {
		wtg_log("Wtg add connection to EPOLL set fail!\n");
		goto err_out;
	}

	if ( conn->type == WTG_CONN_TYPE_MASTER ) {
		conn->stat = WTG_CSTAT_MASTER_RECV_HEAD;
	} else if ( conn->type == WTG_CONN_TYPE_DOWNLOAD ) {
		conn->stat = WTG_CSTAT_DOWNLOAD_SEND_REQ;
	}

	return 0;

err_out:
	wtg_conn_close(conn);

	return -1;
}
*/

/*
 * wtg_domain_listen_close():	Close UNIX domain listen socket.
 */
static void
wtg_domain_listen_close()
{
	if ( wtg_info.domain_server_skt >= 0 ) {
		if ( wtg_info.epfd >= 0 ) {
			wtg_epoll_del(wtg_info.domain_server_skt);
		}
		close(wtg_info.domain_server_skt);
		wtg_info.domain_server_skt = -1;
	}

	if ( !wtg_info.is_sub ) {
		// Only daemon remove the UNIX domain server address file.
		unlink(wtg_info.domain_address);
	}
}

/*
 * wtg_domain_listen_reset():	Reset UNIX domain listen socket.
 * Returns:					0 on success, -1 on error.
 */
static int
wtg_domain_listen_reset()
{
	struct sockaddr_un	un;
	
	wtg_domain_listen_close();
	sleep(WTG_DOMAIN_ADDR_WAIT_TIME);

	wtg_info.domain_server_skt = socket(PF_UNIX, SOCK_STREAM, 0);
	if ( wtg_info.domain_server_skt == -1 ) {
		wtg_log("Wtg create UNIX domain socket fail! %m\n");
		goto err_out0;
	}

	if ( wtg_set_nonblock(wtg_info.domain_server_skt) ) {
		wtg_log("Wtg set UNIX domain socket to nonblock fail!\n");
		goto err_out1;
	}

	memset(&un, 0, sizeof(struct sockaddr_un));
	un.sun_family = AF_UNIX;
	strncpy(un.sun_path, wtg_info.domain_address, UNIX_PATH_MAX - 1);

	if ( bind(wtg_info.domain_server_skt, (struct sockaddr *)&un,
		(socklen_t)(offsetof(struct sockaddr_un, sun_path) + strlen(un.sun_path))) == -1 ) {
		wtg_log("Wtg bind UNIX domain socket fail! %m\n");
		goto err_out1;
	}

	if ( listen(wtg_info.domain_server_skt, wtg_info.domain_max_conn) == -1 ) {
		wtg_log("Wtg UNIX domain socket listen fail! %m");
		goto err_out2;
	}

	if ( wtg_epoll_add(wtg_info.domain_server_skt, 0) ) {
		wtg_log("Add UNIX domain listen socket to EPOLL set fail! %m\n");
		goto err_out2;
	}

	return 0;

err_out2:
	unlink(wtg_info.domain_address);

err_out1:
	if ( wtg_info.domain_server_skt >= 0 ) {
		close(wtg_info.domain_server_skt);
		wtg_info.domain_server_skt = -1;
	}

err_out0:
	return -1;
}

/*
 * wtg_resource_destroy():		Wtg resource cleanup.
 */
static void
wtg_resource_destroy()
{
	int			i;

	wtg_conn_cleanup(&wtg_info.download_conn);

	wtg_domain_listen_close();

	wtg_conn_cleanup(&wtg_info.master_conn);
	
	if ( wtg_info.domain_conns.conns ) {
		for ( i = 0; i < wtg_info.domain_max_conn; i++ ) {
			wtg_conn_cleanup(&wtg_info.domain_conns.conns[i]);
		}

		free(wtg_info.domain_conns.conns);
		wtg_info.domain_conns.conns = NULL;
	}

	if ( wtg_info.epfd >= 0 ) {
		close(wtg_info.epfd);
		wtg_info.epfd = -1;
	}

	wtg_hash_destroy(&wtg_info.domain_conns.used_hash);

#ifdef __NWS
	log_close();
#endif
}

/*
 * wtg_scan_domain_active_conns():		Scan UNIX domain active connections. Kick out timeout connections.
 */
static void
wtg_scan_domain_active_conns()
{
	list_head_t		*list_item = NULL, *tmp_list = NULL;
	wtg_conn_t		*conn = NULL;
	time_t			cur_time;

	cur_time = time(NULL);
	list_for_each_safe(list_item, tmp_list, &wtg_info.domain_conns.used) {
		conn = list_entry(list_item, wtg_conn_t, list);
		if ( cur_time > conn->timeout ) {
// #ifdef __DEBUG
			wtg_log("Wtg found a timeout UNIX domain connection \"%d\".\n", conn->skt);
// #endif
			wtg_conn_close(conn);
		}
	}
}

/*
 * wtg_build_proto_header():		Build protocol header.
 * @cmd:						Protocol content command.
 * @len:							packet content length.
 */
static inline void
wtg_build_proto_header(unsigned int cmd, int len)
{
	wtg_proto_header.length = (unsigned int)(sizeof(proto_header) + len);
	wtg_proto_header.cmd = cmd;
}

/*
 * wtg_build_report_req_header():	Build report_req packet header.
 * @type:						Report type.
 * @content_len:					Content data length.
 */
static inline void
wtg_build_report_req_header(int type, int content_len)
{	
	wtg_build_proto_header(CMD_STAT_REPORT_V2, sizeof(stat_report_req_v2) + content_len);
	
	wtg_stat_report_req.ip = (unsigned long)wtg_info.client_ip;
	wtg_stat_report_req.port = (unsigned short)wtg_info.client_port;
#ifdef __MCP
	wtg_stat_report_req.pid = WTG_REPORT_PIDBIN_MCP;
#else
#ifdef __NWS
	wtg_stat_report_req.pid = WTG_REPORT_PIDBIN_NWS;
#else
	wtg_stat_report_req.pid = WTG_REPROT_PIDBIN_ERR;
#endif
#endif

	if ( wtg_info.type == WTG_D_WTGD ) {
		wtg_stat_report_req.pid = WTG_REPORT_PIDBIN_MCP;
	}

	wtg_stat_report_req.type = type;
	wtg_stat_report_req.length = (unsigned short)content_len;
}

/*
 * wtg_update_send_response():			Send update response status.
 * @res_code:						Response status code.
 * @mode:							0 use common master connection, others use a new connection.
 * Returns:							0 on success, -1 on error.
 */
static int
wtg_update_send_response(int res_code, int mode)
{
	wtg_update_context_t	*ctx = &wtg_info.update_ctx;
	int						fd = -1, send_len;

	wtg_build_proto_header(CMD_UPGRADE_RESLUT_REPORT_V2, sizeof(task_status_v2));

	wtg_task_status.ip = (unsigned long)wtg_info.client_ip;
	wtg_task_status.port = (unsigned short)wtg_info.client_port;
	wtg_task_status.tid = ctx->id;
	wtg_task_status.status = (unsigned long)res_code;
	wtg_task_status.time = (unsigned long)time(NULL);

	if ( mode ) {
		memcpy(rsp_buf, &wtg_proto_header, sizeof(proto_header));
		memcpy(rsp_buf + sizeof(proto_header), &wtg_task_status, sizeof(task_status_v2));

		send_len = sizeof(proto_header) + sizeof(task_status_v2);

		fd = wtg_connect_master(wtg_info.master_name, wtg_info.master_port);
		if ( fd < 0 ) {
			perror("Wtg connect to master fail when send response!\n");
			return -1;
		}

		if ( send(fd, rsp_buf, (size_t)send_len, MSG_NOSIGNAL) != send_len ) {
			perror("Wtg send update response status fail!\n");
			if ( fd >= 0 ) {
				close(fd);
				fd = -1;
			}
			return -1;
		}

		if ( fd >= 0 ) {
			close(fd);
			fd = -1;
		}
	} else {
		if ( wtg_conn_buff_append(&wtg_info.master_conn.send_buf,
			(char *)(&wtg_proto_header), sizeof(proto_header)) ) {
			wtg_log("Wtg write proto header to send buffer fail when send response! Buffer maybe full!\n");
			return -1;
		}

		if ( wtg_conn_buff_append(&wtg_info.master_conn.send_buf,
			(char *)(&wtg_task_status), sizeof(task_status_v2)) ) {
			wtg_log("Wtg write task status to send buffer fail when send response! Buffer maybe full!\n");
			return -1;
		}
	}

	return 0;
}

/*
 * wtg_update_close_download_file():		Close download file.
 */
static inline void
wtg_update_close_download_file()
{
	wtg_update_context_t	*ctx = &wtg_info.update_ctx;
	
	if ( ctx->fd >= 0 ) {
		close(ctx->fd);
		ctx->fd = -1;
	}
}

/*
 * wtg_update_restart_server():		Restart server.
 * Returns:						0 on success, -1 on error.
 */
static int
wtg_update_restart_server()
{
	if ( strlen(pro_name) == 0 ) {
		perror("Wtg empty program name when restart! Restart not support!\n");
		return -1;
	}

	execv(pro_name, pro_argv);
	
	perror("Wtg restart server fail!\n");
	return -1;
}

/*
 * wtg_update_roll_back():		Update roll back.
 */
static void
wtg_update_roll_back()
{
	wtg_update_context_t	*ctx = &wtg_info.update_ctx;
	unsigned long			res_code;
	int						i;

	unlink(WTG_UPDATE_FLAG_FILE);

	wtg_conn_close(&wtg_info.download_conn);

	if ( ctx->stat == WTG_UPDATE_STAT_CMD ) {
		res_code = UPDATE_RSTAT_CMD_ERR;
		wtg_update_send_response(res_code, 0);

		goto cmd;
	} else if ( ctx->stat == WTG_UPDATE_STAT_FILE ) {
		res_code = UPDATE_RSTAT_FILE_ERR;
		wtg_update_send_response(res_code, 0);

		goto file;
	} else if ( ctx->stat == WTG_UPDATE_STAT_UPDATE ) {
		res_code = UPDATE_RSTAT_UPDATE_ERR;
		wtg_update_send_response(res_code, 1);

		goto update;
	} else if ( ctx->stat == WTG_UPDATE_STAT_CTL ) {
		res_code = UPDATE_RSTAT_CTL_ERR;
		wtg_update_send_response(res_code, 1);

		goto ctl;
	} else {
		res_code = UPDATE_RSTAT_OTHER_ERR;
		wtg_update_send_response(res_code, 0);

		goto other;
	}

ctl:
	for ( i = 0; i < ctx->file_cnt; i++ ) {
		sprintf(fname2, "%s%s", ctx->files[i].dest_path, WTG_BAK_EXT);
		unlink(ctx->files[i].dest_path);
		sleep(WTG_UNLINK_WAIT_TIME);
		if ( rename(fname2, ctx->files[i].dest_path) ) {
			perror("Wtg rename file fail when roll back!\n");
		}
	}
	
update:
	if ( wtg_update_restart_server() ) {
		perror("Wtg restart server fail when update roll back!\n");
	}
	
file:
cmd:
other:
	ctx->stat = WTG_UPDATE_STAT_IDLE;
	ctx->id = 0;
}

/*
 * wtg_master_conn_check():	Master connection timeout check.
 * @conn:					Master connection.
 */
static inline void
wtg_master_conn_check(wtg_conn_t *conn)
{
	if ( conn->skt >= 0 ) {
		if ( time(NULL) > conn->timeout ) {
			wtg_log("Wtg master connection timeout, reconnect...\n");
			
			if ( wtg_network_conn_reconnect(conn,
				wtg_info.master_name, wtg_info.master_port, EPOLLOUT) ) {
				wtg_log("Wtg master connection reconnect error when timeout, retry later!\n");
				wtg_info.master_conn.skt = -1;
			} else {
				wtg_log("Wtg master connection reconnect successfully when timeout.\n");
			}
		}
	}

	// Reconnect in next main loop when skt < 0.
}

/*
 * wtg_download_conn_check():	Update connection timeout check.
 * @conn:					Update connection.
 */
static inline void
wtg_download_conn_check(wtg_conn_t *conn)
{
	if ( conn->skt >= 0 ) {
		if ( time(NULL) > conn->timeout ) {
			wtg_log("Wtg update connection timeout, roll back...\n");
			
			// Close connection in roll back.
			wtg_update_roll_back();
		}
	} else if ( wtg_info.update_ctx.stat != WTG_UPDATE_STAT_IDLE ) {
		// When update by fork, must pthread_exit immediately to avoid this. No fork will not run here because update in worker thread.
		wtg_log("Wtg invalid updata status \"%d\" when connection not connect!\n", conn->stat);
		// Only change status but roll back. Roll back only in a update circle to avoid risk. Avoid redundance roll back.
		wtg_info.update_ctx.stat = WTG_UPDATE_STAT_IDLE;
	}
}

/*
 * wtg_update_http_req_header():	Build update HTTP request header.
 * @token:						Download token.
 * @factor:						Download token factor.
 * Returns:						0 on success, -1 on error.
 */
static inline int
wtg_update_http_req_header(const char *uri, char *token, int factor)
{
	char				*data = NULL;
	char				*start = NULL;
	unsigned long long	cur_time = (unsigned long long)time(NULL);

	if ( wtg_info.download_conn.send_buf.size
		- wtg_info.download_conn.send_buf.offset
		- wtg_info.download_conn.send_buf.length
		< WTG_DOWNLOAD_SBUF_RESERVE ) {
		wtg_log("Wtg update connection send buffer not enough space, maybe error!\n");
		return -1;
	}

	if ( strlen(token) == 0 || factor == 0 ) {
		wtg_log("Wtg invalid token \"%s\" or factor \"%d\"!\n", token, factor);
		return -1;
	}

	data = wtg_info.download_conn.send_buf.data
		+ wtg_info.download_conn.send_buf.offset + wtg_info.download_conn.send_buf.length;
	start = data;

	data += sprintf(data, "GET %s?%s%llu HTTP/1.1\r\n", uri,
		token, cur_time % factor);
	data += sprintf(data, "Content-Length: 0\r\n");
	data += sprintf(data, "Connection: keep-alive\r\n");
	data += sprintf(data, "Cookie: %llu\r\n\r\n", cur_time);

	wtg_info.download_conn.send_buf.length += (int)(data - start);

	return 0;
}

/*
 * wtg_update_http_check_complete():	Update HTTP response head complete check.
 * @conn:							Network connection.
 * @head_len:						HTTP head length.
 * Returns:							>0 complete and means Content-Length.
 *									0 not complete,
 *									-1 on normal error.
 *									-2 on HTTP return code not 200.
 */
static int
wtg_update_http_check_complete(wtg_conn_t *conn, int *head_len)
{
	wtg_buff_t					*buf = &conn->recv_buf;
	char						*data = NULL, *pend = NULL, *pend_end = NULL, saved;
	char						val[WTG_INT_STR_MAX];
	int							len, at_tail = 0, i;
	long						body_len;

	data = buf->data + buf->offset;
	len = buf->length;

	if ( len < 4 ) {
		return 0;
	}

	if ( strncasecmp(data, "HTTP", 4) ) {
		wtg_log("Wtg invalid update file server response \"%c%c%c%c\"!\n",
			*data, *(data + 1), *(data + 2), *(data +3));
		return -1;
	}

	if ( data[len - 1] == '\n' ) {
		if ( !strncmp( (data + len - 4), "\r\n\r\n", 4) ) {
			at_tail = 1;
		}
	}

	saved = data[len - 1];
	data[len - 1] = 0;
	pend = strstr(data, "\r\n\r\n");
	data[len - 1] = saved;

	if ( pend == NULL ) {
		if ( at_tail ) {
			*head_len = len;
			goto parse_head;
		} else {
			if ( len > WTG_DOWNLOAD_SBUF_RESERVE - 1 ) {
				wtg_log("Too long HTTP response head! 1\n");
				return -1;
			} else {
				return 0;
			}
		}
	} else {
		*head_len = pend - data + 4;
		goto parse_head;
	}

parse_head:
	if ( *head_len > WTG_DOWNLOAD_SBUF_RESERVE - 1 ) {
		wtg_log("Too long HTTP response head! 2\n");
		return -1;
	}

	memset(http_head, 0, sizeof(char) * WTG_DOWNLOAD_SBUF_RESERVE);
	memcpy(http_head, data, sizeof(char) * (*head_len));

	for ( i = 8; (http_head[i] == ' ' || http_head[i] == '\t') && i < *head_len; i++ );

	// Here include the tail LWS. So add 3 but 2.
	if ( i + 3 >= *head_len ) {
		wtg_log("Wtg invalid HTTP response head!\n");
		return -1;
	}

	if ( http_head[i] != '2' || http_head[i + 1] != '0' || http_head[i + 2] != '0' ) {
		wtg_log("Wtg HTTP response code is not 200. \"%c%c%c\"\n",
			http_head[i], http_head[i + 1], http_head[i + 2]);
		return -2;
	}

	i += 3;
	
	if ( http_head[i] != ' ' && http_head[i] != '\t' ) {
		wtg_log("Wtg invalid HTTP response start line!\n");
		return -1;
	}

	pend = strcasestr(http_head, "\r\nContent-Length:");
	if ( !pend ) {
		wtg_log("Wtg HTTP response no \"Content-Length\" item!\n");
		return -1;
	}

	pend += 17;

	for ( ; (*pend == ' ' || *pend == '\t') && pend - http_head < (*head_len); pend++ );

	pend_end = strstr(pend, "\r\n");
	if ( !pend_end ) {
		wtg_log("Wtg no HTTP response \"Content-Length\" tail!\n");
		return -1;
	}

	if ( pend_end - pend > WTG_INT_STR_MAX - 1 ) {
		wtg_log("Wtg too long HTTP \"Content-Length\" item!\n");
		return -1;
	}
	
	memset(val, 0, sizeof(char) * WTG_INT_STR_MAX);
	for ( i = 0; pend[i] != ' ' && pend[i] != '\t' && pend[i] != '\r' && pend + i < pend_end;
		i++ ) {
		val[i] = pend[i];
	}

	if ( strlen(val) == 0 ) {
		wtg_log("Wtg empty \"Content-Length\" item!\n");
		return -1;
	}

	body_len = strtol(val, NULL, 10);
	if ( ( body_len == 0 && errno == EINVAL )
		|| ( (body_len == LONG_MIN || body_len == LONG_MAX) && errno == ERANGE )
		|| ( body_len <= 0 ) ) {
		wtg_log("Wtg invalid HTTP \"Content-Length\" value \"%s\"!\n", val);
		return -1;
	}

	return body_len;
}

/*
 * wtg_update_context_reset():		Reset update context.
 */
static inline void
wtg_update_context_reset()
{
	wtg_update_context_t		*ctx = &wtg_info.update_ctx;

	ctx->cmd_size = 0;
	ctx->cmd_recived = 0;
	ctx->fd = -1;
	ctx->file_cnt = 0;
	ctx->cur_idx = 0;
	memset(ctx->files, 0, sizeof(wtg_update_file_t) * WTG_UFILE_MAX);
}

/*
 * wtg_update_ctx_refresh_clen():	Update context refresh content_length.
 */
static inline void
wtg_update_ctx_refresh_clen(long long content_length)
{
	wtg_update_context_t	*ctx = &wtg_info.update_ctx;
	
	if ( ctx->stat == WTG_UPDATE_STAT_CMD ) {
		ctx->cmd_size = (int)content_length;
	} else if ( ctx->stat == WTG_UPDATE_STAT_FILE ) {
		ctx->files[ctx->cur_idx].size = content_length;
	}
	// Other case catch in other functions.
}

/*
 * wtg_update_cmd_parse():		Parse update command file content.
 * Returns:						0 on success, -1 on error.
 */
static int
wtg_update_cmd_parse()
{
	wtg_update_context_t	*ctx = &wtg_info.update_ctx;
	char					*start = NULL, *end = NULL;
	long					file_cnt;
	int						i;

	cmd_content[ctx->cmd_size] = 0;

	start = cmd_content;
	end = strchr(start, '\n');
	if ( !end ) {
		wtg_log("Wtg cmd parse get start line fail!\n");
		return -1;
	}
	*end = 0;
	if ( strlen(start) > WTG_INT_STR_MAX - 1 ) {
		wtg_log("Wtg too long cmd start line \"%s\"!\n");
		return -1;
	}
	file_cnt = strtol(start, NULL, 10);
	if ( ( file_cnt == 0 && errno == EINVAL )
		|| ( (file_cnt == LONG_MIN || file_cnt == LONG_MAX) && errno == ERANGE )
		|| ( file_cnt <= 0 ) ) {
		wtg_log("Wtg cmd invalid start line file count value \"%s\"!\n", start);
		return -1;
	}	
	ctx->file_cnt = file_cnt;
	
	start = end + 1;
	for ( i = 0; i < ctx->file_cnt; i++ ) {
		end = strchr(start, ';');
		if ( !end ) {
			wtg_log("Wtg get dest path fail! idx - %d.\n", i);
			return -1;
		}
		*end = 0;
		if ( strlen(start) > WTG_PATH_SIZE - 1 ) {
			wtg_log("Wtg too long dest path \"%s\"! idx - %d.\n", start, i);
			return -1;
		}
		strcpy(ctx->files[i].dest_path, start);
		
		start = end + 1;
		end = strchr(start, ';');
		if ( !end ) {
			wtg_log("Wtg get URI fail! idx - %d.\n", i);
			return -1;
		}
		*end = 0;
		if ( strlen(start) > WTG_PATH_SIZE - 1 ) {
			wtg_log("Wtg too long URI \"%s\"! idx - %d.\n", start, i);
			return -1;
		}
		strcpy(ctx->files[i].uri, start);

		start = end + 1;
		if ( *start == '0' ) {
			ctx->files[i].is_bin = 0;
		} else if ( *start == '1' ) {
			ctx->files[i].is_bin = 1;
		} else {
			wtg_log("Wtg invalid update file binary flag \"%c\" in command parse! idx - %d.\n",
				*start, i);
			return -1;
		}

		start++;
		if ( *start != '\n' ) {
			wtg_log("Wtg invalid update command line terminate charecter \"%c\"! idx - %d.\n",
				*start ,i);
			return -1;
		}

		start++;
	}

	if ( *start != 0 ) {
		wtg_log("Wtg unexcepted command content tail charecter \"%c\"!\n", *start);
		return -1;
	}

	wtg_log("Wtg update command parse successfully! Start update!\n");
	
	return 0;
}

/*
 * wtg_update_cmd_handle():		Update connection command handle.
 * Returns:						1 on success and complete,
 *								0 on success but complete, should continue,
 *								-1 on error.
 */
static int
wtg_update_cmd_handle()
{
	wtg_conn_t					*conn = &wtg_info.download_conn;
	wtg_buff_t					*buf = &wtg_info.download_conn.recv_buf;
	wtg_update_context_t		*ctx = &wtg_info.update_ctx;
	unsigned int				copy_len = 0;

	if ( ctx->cmd_size > WTG_DEF_UCMD_BUF_MAX - 1 ) {
		wtg_log("Wtg too long command content data length \"%d\"!\n", ctx->cmd_size);
		return -1;
	}

	if ( ctx->cmd_recived + buf->length > ctx->cmd_size ) {
		wtg_log("Wtg warnning: redundance command content data! Discard it!\n");
		copy_len = (unsigned int)(ctx->cmd_size - ctx->cmd_recived);
	} else {
		copy_len = (unsigned int)(buf->length);
	}

	memcpy(cmd_content + ctx->cmd_recived, buf->data + buf->offset, copy_len);

	ctx->cmd_recived += copy_len;

	if ( ctx->cmd_recived < ctx->cmd_size ) {
		return 0;
	}

	if ( wtg_update_cmd_parse() ) {
		wtg_log("Wtg parse update command content fail!\n");
		return -1;
	}

	if ( ctx->file_cnt <= 0 ) {
		wtg_log("Wtg empty update task!\n");
		return -1;
	}

	if ( wtg_update_http_req_header(ctx->files[ctx->cur_idx].uri, ctx->token, ctx->token_factor) ) {
		wtg_log("Wtg build update file HTTP request header fail! Idx - \"%d\", URI - \"%s\".\n",
			ctx->cur_idx, ctx->files[ctx->cur_idx].uri);
		return -1;
	}

	if ( wtg_epoll_mod(conn->skt, EPOLLOUT) ) {
		wtg_log("Wtg update file connection EPOLLOUT modify fail!\n");
		return -1;
	}
	
	ctx->stat = WTG_UPDATE_STAT_FILE;

	return 1;
}

/*
 * wtg_update_files():				Replace files.
 * Returns:						0 on success, -1 on error.
 */
static int
wtg_update_files()
{
	wtg_update_context_t	*ctx = &wtg_info.update_ctx;

	for ( ctx->cur_idx = 0 ; ctx->cur_idx < ctx->file_cnt; ctx->cur_idx++ ) {
		sprintf(fname, "%s%s", ctx->files[ctx->cur_idx].dest_path, WTG_TMP_EXT);
		sprintf(fname2, "%s%s", ctx->files[ctx->cur_idx].dest_path, WTG_BAK_EXT);
		unlink(fname2);
		sleep(WTG_UNLINK_WAIT_TIME);
		if ( rename(ctx->files[ctx->cur_idx].dest_path, fname2) ) {
			perror("Wtg rename file fail! 1\n");
			goto err_out;
		}
		sleep(WTG_UNLINK_WAIT_TIME);
		if ( rename(fname, ctx->files[ctx->cur_idx].dest_path) ) {
			perror("Wtg rename file fail! 2\n");
			if ( rename(fname2, ctx->files[ctx->cur_idx].dest_path) ) {
				perror("Wtg rename file fail! 3\n");
			}
			goto err_out;
		}
	}

	ctx->stat = WTG_UPDATE_STAT_CTL;

	return 0;

err_out:
	while ( --ctx->cur_idx >= 0 ) {
		sprintf(fname2, "%s%s", ctx->files[ctx->cur_idx].dest_path, WTG_BAK_EXT);
		unlink(ctx->files[ctx->cur_idx].dest_path);
		sleep(WTG_UNLINK_WAIT_TIME);
		if ( rename(fname2, ctx->files[ctx->cur_idx].dest_path) ) {
			perror("Wtg rename file fail! 4\n");
		}
	}
	
	return -1;
}

/*
 * wtg_update_op():				Replace files and restart the server.
 */
static void
wtg_update_op()
{
	if ( wtg_update_files() ) {
		perror("Wtg replace files fail!\n");
		wtg_update_roll_back();
		// Exit in roll back.
	}

	if ( wtg_update_restart_server() ) {
		perror("Wtg restart server fail!\n");
		wtg_update_roll_back();
		// Exit in roll back.
	}
}

/*
 * wtg_update_proc():				Launch update process.
 * Returns:						0 on success, -1 on error.
 */
static int
wtg_update_proc()
{
	pid_t					ctl_pid;
	wtg_update_context_t	*ctx = &wtg_info.update_ctx;

	if ( wtg_info.type != WTG_D_MCP && wtg_info.type != WTG_D_NWS ) {
		wtg_log("Wtg daemon not in watchdog, reject update operation!\n");
		return -1;
	}

	daemon_pid = getpid();

	wtg_log("Wtg update prepare fork control process!\n");

	ctl_pid = fork();
	if ( ctl_pid < 0 ) {
		wtg_log("Wtg fork control fail! %m\n");
		return -1;
	} else {
		ctx->stat = WTG_UPDATE_STAT_UPDATE;
		ctx->cur_idx = 0;	// Set zeor for replace files.
		wtg_resource_destroy();
		
		if ( ctl_pid > 0 ) {
			pthread_exit(NULL);
		} else {
			if ( kill(daemon_pid, SIGTERM) ) {
				perror("Kill main process fail! Maybe process already exited! Ignore it!");
			}

			sleep(WTG_WAIT_EXIT_TIME);

			wtg_update_op();
		}
	}

	return 0;
}

/*
 * wtg_update_file_handle():		Updae connection file handle.
 * Returns:						1 on success and complete,
 *								0 on success but complete, should continue,
 *								-1 on error.
 */
static int
wtg_update_file_handle()
{
	wtg_conn_t					*conn = &wtg_info.download_conn;
	wtg_buff_t					*buf = &wtg_info.download_conn.recv_buf;
	wtg_update_context_t		*ctx = &wtg_info.update_ctx;
	unsigned int				copy_len = 0;

	if ( ctx->files[ctx->cur_idx].recived == 0 ) {
		sprintf(fname, "%s%s", ctx->files[ctx->cur_idx].dest_path, WTG_TMP_EXT);
		if ( ctx->files[ctx->cur_idx].is_bin ) {
			ctx->fd = open(fname, O_CREAT | O_TRUNC | O_RDWR, 0755);
		} else {
			ctx->fd = open(fname, O_CREAT | O_TRUNC | O_RDWR, 0644);
		}		
		if ( ctx->fd == -1 ) {
			wtg_log("Create tmp file \"%s\" fail! %m\n", fname);
			return -1;
		}
	}

	if ( ctx->files[ctx->cur_idx].recived + buf->length > ctx->files[ctx->cur_idx].size ) {
		wtg_log("Wtg warnning: redundance update file content data! Discard it!\n");
		copy_len = (unsigned int)(ctx->files[ctx->cur_idx].size
			- ctx->files[ctx->cur_idx].recived);
	} else {
		copy_len = (unsigned int)(buf->length);
	}

	if ( write(ctx->fd, buf->data + buf->offset, copy_len) != copy_len ) {
		wtg_log("Wtg write update tmp file \"%s\" fail! %m\n", fname);
		return -1;
	}

	ctx->files[ctx->cur_idx].recived += copy_len;

	if ( ctx->files[ctx->cur_idx].recived < ctx->files[ctx->cur_idx].size ) {
		return 0;
	}

	wtg_update_close_download_file();

	ctx->cur_idx++;
	if ( ctx->cur_idx >= ctx->file_cnt ) {
		wtg_conn_close(conn);
		if ( wtg_update_proc() ) {
			wtg_log("Wtg update process fail!\n");
			return -1;
		}

		return 1;
	}

	if ( wtg_update_http_req_header(ctx->files[ctx->cur_idx].uri, ctx->token, ctx->token_factor) ) {
		wtg_log("Wtg build update file HTTP request header fail 2! Idx - \"%d\", URI - \"%s\".\n",
			ctx->cur_idx, ctx->files[ctx->cur_idx].uri);
		return -1;
	}

	if ( wtg_epoll_mod(conn->skt, EPOLLOUT) ) {
		wtg_log("Wtg update file connection EPOLLOUT modify fail 2!\n");
		return -1;
	}

	return 1;
}

/*
 * wtg_update_handle_body():		Handle update content.
 * Returns:						1 on success and complete,
 *								0 on success but complete, should continue,
 *								-1 on error.
 */
static inline int
wtg_update_handle_body()
{
	wtg_update_context_t	*ctx = &wtg_info.update_ctx;
	
	if ( ctx->stat == WTG_UPDATE_STAT_CMD ) {
		return wtg_update_cmd_handle();
	} else if ( ctx->stat == WTG_UPDATE_STAT_FILE ) {
		return wtg_update_file_handle();
	} else {
		wtg_log("Wtg invalid update status \"%d\" when handle recive data body!\n",
			ctx->stat);
		return -1;
	}
}

/*
 * wtg_download_conn_in():			Update connection recive data.
 */
static void
wtg_download_conn_in()
{
	int						rbytes;
	wtg_conn_t				*conn = &wtg_info.download_conn;
	wtg_buff_t				*buf = &wtg_info.download_conn.recv_buf;
	int						ret, head_len = 0;
	long long				body_len = 0;

again:	
	if ( buf->size - buf->offset - buf->length <= 0 ) {
		wtg_log("Wtg update download connection recive buffer full!\n");
		wtg_update_roll_back();
		return;
	}
	
	rbytes = recv(conn->skt, (buf->data + buf->offset + buf->length),
		(size_t)(buf->size - buf->offset - buf->length), MSG_DONTWAIT);
	
	if ( rbytes == 0 ) {
		// Should not occur in normal because connection will be closed by client before replace file. And they are in serial.
		wtg_log("Wtg update download connection be closed! Should not occur!\n");
		wtg_update_roll_back();
		return;
	} else if ( rbytes < 0 ) {
		if ( errno == EWOULDBLOCK || errno == EAGAIN ) {
			// Only here return normaly.
			return;
		} else if ( errno == EINTR ) {
			goto again;
		} else {
			wtg_log("Wtg update download connection network recive error! %m\n");
			wtg_update_roll_back();
			return;
		}
	} else {
		wtg_conn_touch(conn, wtg_info.download_timeout);
		
		// Handle data later.
		buf->length += rbytes;

handle:
		if ( conn->stat == WTG_CSTAT_DOWNLOAD_RECV_HEAD ) {
			ret = wtg_update_http_check_complete(conn, &head_len);
			if ( ret > 0 ) {
				body_len = ret;
				conn->stat = WTG_CSTAT_DOWNLOAD_RECV_BODY;
				wtg_update_ctx_refresh_clen(body_len);
				wtg_conn_buff_use(buf, head_len);
				goto handle;
			} else if ( ret == 0) {
				goto again;
			} else {
				wtg_log("Wtg update download connection check HTTP response header error!\n");
				wtg_update_roll_back();
				return;
			}
		} else if ( conn->stat == WTG_CSTAT_DOWNLOAD_RECV_BODY ) {
			if ( buf->length <= 0 ) {
				goto again;
			}
			// Only enter handle when data length > 0.
			ret = wtg_update_handle_body();
			if ( ret > 0 ) {
				// Also goto again for FIN.
				// Goto again and jump to EAGAIN or FIN or other CSTAT-errout(recv() > 0).
				wtg_conn_buff_use(buf, buf->length);
				conn->stat = WTG_CSTAT_DOWNLOAD_SEND_REQ;
				goto again;	// Because use all data, so goto again for FIN or EAGAIN or errout when recv() > 0.
			} else if ( ret == 0 ) {
				// Goto recive left data.
				wtg_conn_buff_use(buf, buf->length);
				goto again;
			} else {
				wtg_log("Wtg update download connection handle body error!\n");
				wtg_update_roll_back();
				return;
			}
			// Shoud goto again. If data left update buffer data and offset length need goto complete check.
		} else {
			wtg_log("Wtg invalid update download connection status \"%d\" when recive data!\n",
				conn->stat);
			wtg_update_roll_back();
			return;
		}
	}
}

/*
 * wtg_download_conn_out():		Update connection send data.
 */
static void
wtg_download_conn_out()
{
	int					sbytes;
	wtg_buff_t			*buf = &wtg_info.download_conn.send_buf;
	wtg_conn_t			*conn = &wtg_info.download_conn;

	if ( conn->stat != WTG_CSTAT_DOWNLOAD_SEND_REQ ) {
		wtg_log("Wtg invalid update download connection status \"%d\", close the connection",
			conn->stat);
		wtg_update_roll_back();
		return;
	}

again:
	if ( buf->length <= 0 ) {
		wtg_epoll_mod(conn->skt, 0);
		conn->stat = WTG_CSTAT_DOWNLOAD_RECV_HEAD;
		return;
	}

	sbytes = send(conn->skt, buf->data + buf->offset, buf->length,
		MSG_DONTWAIT | MSG_NOSIGNAL);

	if ( sbytes < 0 ) {
		if ( errno == EWOULDBLOCK || errno == EAGAIN ) {
			return;
		} else if ( errno == EINTR ) {
			goto again;
		} else {
			wtg_log("Wtg update download connection network error! %m\n");
			wtg_update_roll_back();
			return;
		}
	} else {
		wtg_conn_touch(conn, wtg_info.download_timeout);
		
		wtg_conn_buff_use(buf, sbytes);
		goto again;
	}
}

/*
 * wtg_update_init():				Update initialize.
 * Returns:						0 on success, -1 on error.
 */
static int
wtg_update_init()
{
	wtg_update_context_t		*ctx = &wtg_info.update_ctx;
	wtg_conn_t					*conn = &wtg_info.download_conn;
	int							fd;
	char						tmp_buf[WTG_INT_STR_MAX] = {0};

	// Create update flag file.
	fd = open(WTG_UPDATE_FLAG_FILE, O_CREAT | O_TRUNC | O_RDWR, 0644);
	if ( fd == -1 ) {
		wtg_log("Wtg create update flag file fail! %m\n");
		goto err_out0;
	}
	sprintf(tmp_buf, "%lu", ctx->id);
	if ( write(fd, tmp_buf, strlen(tmp_buf)) != strlen(tmp_buf) ) {
		wtg_log("Wtg write update flag file fail! %m\n");
		close(fd);
		goto err_out0;
	}
	close(fd);
	
	wtg_update_context_reset();
	memset(cmd_content, 0, sizeof(char) * WTG_DEF_UCMD_BUF_MAX);

	if ( wtg_network_conn_reconnect(conn, ctx->hostname, ctx->port, EPOLLOUT) ) {
		wtg_log("Wtg connect to update file server fail!\n");
		goto err_out0;
	}

	if ( wtg_update_http_req_header(ctx->cmd_uri, ctx->token, ctx->token_factor) ) {
		wtg_log("Wtg build command file HTTP request head fail!\n");
		goto err_out1;
	}

	ctx->stat = WTG_UPDATE_STAT_CMD;

	wtg_log("Wtg launch update! File server name \"%s\", File server PORT \"%d\", Task ID - %lu, Command URI \"%s\", Token \"%s\", Token factor - %d.\n",
		ctx->hostname, ctx->port, ctx->id, ctx->cmd_uri, ctx->token, ctx->token_factor);

	return 0;

err_out1:
	wtg_conn_close(conn);

err_out0:
	ctx->stat = WTG_UPDATE_STAT_IDLE;
	
	return -1;
}

/*
 * wtg_domain_check_complete():	Domain connection head complete check.
 * conn:							Dommain connection.
 * Returns:						>0, complete and return full packet length need to recive include the head.
 *								0, not complete.
 *								-1, error.
 */
static inline int
wtg_domain_check_complete(wtg_conn_t *conn)
{
	wtg_buff_t			*buf = &conn->recv_buf;
	wtg_ipc_head_t		*header = NULL;

	if ( buf->length < sizeof(wtg_ipc_head_t) ) {
		return 0;
	}

	header = (wtg_ipc_head_t *)(buf->data + buf->offset);
	if ( header->magic != WTG_IPC_MAGIC ) {
		wtg_log("Wtg UNIX domain connection magic number error!\n");
		return -1;
	} else {
		conn->stat = WTG_CSTAT_DOMAIN_RECV_BODY;
		buf->need = header->len + sizeof(wtg_ipc_head_t);
		return buf->need;
	}
}

/*
 * wtg_domain_handle_body():	Handle UNIX domain connection body.
 * @conn:					Domain connection.
 * Returns:					>0 pakcet recive compelte and handle complete. Return bytes of the packet.
 *							0 packet not recive complete.
 *							-1 on error.
 */
static int
wtg_domain_handle_body(wtg_conn_t *conn)
{
	wtg_buff_t			*buf = &conn->recv_buf;
	wtg_ipc_head_t		*header = NULL;
	int					len;
	alarm_report_req_v2	*alarm = NULL;
	report_process_info	*proc = NULL;

	header = (wtg_ipc_head_t *)buf->data;

	if ( buf->length < buf->need ) {
		return 0;
	}

	// Build proto header.
	if ( header->type == IPC_TYPE_BASE ) {
		wtg_build_report_req_header(WTG_REPORT_TYPE_BASE, header->len);
	} else if ( header->type == IPC_TYPE_ATTR ) {
		wtg_build_report_req_header(WTG_REPORT_TYPE_ATTR, header->len);
	} else if ( header->type == IPC_TYPE_ALARM ) {
		wtg_build_proto_header(CMD_ALARM_REPORT_V2, sizeof(alarm_report_req_v2));
		alarm = (alarm_report_req_v2 *)header->data;
		alarm->ip = wtg_info.client_ip;
		alarm->port = wtg_info.client_port;
	} else if ( header->type == IPC_TYPE_PROC ) {
		wtg_build_proto_header(CMD_REPORT_PROCESS_INFO, sizeof(report_process_info));
		proc = (report_process_info *)header->data;
		proc->ip = wtg_info.client_ip;
		proc->port = wtg_info.client_port;
	} else {
		wtg_log("Wtg invalid UNIX domain connection packet type \"%d\" recived!\n", header->type);
		goto update;
	}

	if ( header->type == IPC_TYPE_BASE || header->type == IPC_TYPE_ATTR ) {
		// Write master send buffer.
		if ( wtg_conn_buff_append(&wtg_info.master_conn.send_buf,
			(char *)(&wtg_proto_header), sizeof(proto_header)) ) {
			wtg_log("Wtg write proto header to send buffer fail! Buffer maybe full!\n");
			return -1;
		}
		if ( wtg_conn_buff_append(&wtg_info.master_conn.send_buf,
			(char *)(&wtg_stat_report_req), sizeof(stat_report_req_v2)) ) {
			wtg_log("Wtg write stat report request header to send buffer fail! Buffer Maybe full!\n");
			return -1;
		}
		if ( wtg_conn_buff_append(&wtg_info.master_conn.send_buf,
			header->data, header->len) ) {
			wtg_log("Wtg write stat report request content to send buffer fail! Buffer Maybe full!\n");
			return -1;
		}
	} else if ( header->type == IPC_TYPE_ALARM ) {
		if ( wtg_conn_buff_append(&wtg_info.master_conn.send_buf,
			(char *)(&wtg_proto_header), sizeof(proto_header)) ) {
			wtg_log("Wtg write proto header to send buffer fail when report alarm! Buffer maybe full!\n");
			return -1;
		}
		if ( wtg_conn_buff_append(&wtg_info.master_conn.send_buf,
			header->data, header->len) ) {
			wtg_log("Wtg write alarm content to send buffer fail when report alarm! Buffer maybe full!\n");
			return -1;
		}
	} else if ( header->type == IPC_TYPE_PROC ) {
		if ( wtg_conn_buff_append(&wtg_info.master_conn.send_buf,
			(char *)(&wtg_proto_header), sizeof(proto_header)) ) {
			wtg_log("Wtg write proto header to send buffer fail when report process! Buffer maybe full!\n");
			return -1;
		}
		if ( wtg_conn_buff_append(&wtg_info.master_conn.send_buf,
			header->data, header->len) ) {
			wtg_log("Wtg write alarm content to send buffer fail when report process! Buffer maybe full!\n");
			return -1;
		}
	} else {
		// Implement later.
	}

update:
	len = buf->need;
	buf->need = 0;
	conn->stat = WTG_CSTAT_DOMAIN_RECV_HEAD;
	
	wtg_conn_buff_use(buf, len);

	return len;
}

/*
 * wtg_domain_accept():	UNIX domain server socket accept connections.
 */
static void
wtg_domain_accept()
{
	int				client_skt;
	wtg_conn_t		*conn = NULL;

again:
	client_skt = accept(wtg_info.domain_server_skt, NULL, NULL);

	if ( client_skt < 0 ) {
		if ( errno == EWOULDBLOCK || errno == EAGAIN ) {
			return;
		} else if ( errno == EINTR ) {
			goto again;
		} else {
			wtg_log("Wtg accept fail! Try to reset listen socket...\n");
			if ( wtg_domain_listen_reset() ) {
				wtg_log("Wtg UNIX domain server socket reset fail in accept handler! Try later!\n");
				wtg_info.domain_server_skt = -1;
			}
			return;
		}
	} else {
		if ( list_empty(&wtg_info.domain_conns.free) ) {
			close(client_skt);
			goto again;
		}
		conn = list_entry(wtg_info.domain_conns.free.next, wtg_conn_t, list);
		list_del_init(&conn->list);
		
		conn->skt = client_skt;
		wtg_conn_buff_reset(conn);
		conn->stat = WTG_CSTAT_DOMAIN_RECV_HEAD;
		wtg_domain_conn_touch(conn);
		
		list_add_tail(&conn->list, &wtg_info.domain_conns.used);
		if ( wtg_epoll_add(conn->skt, 0) ) {
			wtg_log("Wtg add new UNIX domain connection to EPOLL set fail!\n");
			wtg_conn_close(conn);
			goto again;
		}
		if ( wtg_set_nonblock(conn->skt) ) {
			wtg_log("Wtg new UNIX domain connection set nonblock fail!\n");
			wtg_conn_close(conn);
			goto again;
		}
		wtg_hash_del(&wtg_info.domain_conns.used_hash, conn->skt, NULL);
		if ( wtg_hash_add(&wtg_info.domain_conns.used_hash, conn->skt, conn, NULL) ) {
			wtg_log("Wtg add new UNIX domain connection to active hash table fail!\n");
			wtg_conn_close(conn);
			goto again;
		}

		goto again;
	}
}

/*
 * wtg_domain_conn_in():	Handle UNIX domain IPC connection EPOLLIN event.
 * @fd:					Socket fd.
 */
static void
wtg_domain_conn_in(int fd)
{
	int				rbytes, ret;
	wtg_conn_t		*conn = NULL;

	conn = wtg_hash_find(&wtg_info.domain_conns.used_hash, fd, NULL);
	if ( !conn ) {
		wtg_log("Wtg connection not found in hash when handle UNIX doamin EPOLLIN!\n");
		wtg_close_domain_by_fd(fd);
		return;
	}
	if ( conn->skt != fd ) {
		// Actually should never occur.
		wtg_log("Wtg error, fd \"%d\" in connection is not equal event fd \"%d\"!\n", conn->skt, fd);
		wtg_close_domain_by_fd(fd);
		wtg_conn_close(conn);
		return;
	}

	wtg_domain_conn_touch(conn);

again:
	if ( conn->recv_buf.size - conn->recv_buf.offset - conn->recv_buf.length <= 0 ) {
		// Too large one API request.
		wtg_log("Wtg UNIX domain connection recive buffer full! Buffer size %d.\n", conn->recv_buf.size);
		wtg_conn_close(conn);
		return;
	}
	
	rbytes = recv(fd, (conn->recv_buf.data + conn->recv_buf.offset + conn->recv_buf.length),
		(conn->recv_buf.size - conn->recv_buf.offset - conn->recv_buf.length), MSG_DONTWAIT);

	if ( rbytes == 0 ) {
		if ( conn->stat != WTG_CSTAT_DOMAIN_RECV_HEAD ) {
			wtg_log("wtg invalid connection status \"%d\" when close, maybe lost any data!\n", conn->stat);
		}
		wtg_conn_close(conn);
		return;		
	} else if ( rbytes < 0 ) {
		if ( errno == EWOULDBLOCK || errno == EAGAIN ) {
			return;			// Most case return here.
		} else if ( errno == EINTR ) {
			goto again;
		} else {
			wtg_log("Wtg UNIX domain IPC connection recive error! %m\n");
			wtg_conn_close(conn);
			return;
		}
	} else {
		conn->recv_buf.length += rbytes;

handle:
		if ( conn->stat == WTG_CSTAT_DOMAIN_RECV_HEAD ) {
			ret = wtg_domain_check_complete(conn);
			if ( ret > 0 ) {
				goto handle;
			} else if ( ret == 0) {
				goto again;
			} else {
				wtg_log("Wtg UNIX domain IPC connection head check error!\n");
				wtg_conn_close(conn);
				return;
			}
		} else if ( conn->stat == WTG_CSTAT_DOMAIN_RECV_BODY ) {
			ret = wtg_domain_handle_body(conn);
			if ( ret > 0 ) {
				goto handle;
			} else if ( ret == 0 ) {
				goto again;
			} else {
				wtg_log("Wtg UNIX domain IPC connection handle body error!\n");
				wtg_conn_close(conn);
				return;
			}
			// Shoud goto again. If data left update buffer data and offset length need goto complete check.
		} else {
			wtg_log("Wtg invalid UNIX domain IPC connection status \"%d\" when recive data!\n", conn->stat);
			wtg_conn_close(conn);
			return;
		}
	}
}

/*
 * wtg_master_check_complete():	Master connection packet complete check.
 * conn:							Master connection.
 * Returns:						>0, complete and return full packet length.
 *								0, not complete.
 *								-1, error.
 */
static inline int
wtg_master_check_complete(wtg_conn_t *conn)
{
	wtg_buff_t			*buf = &conn->recv_buf;
	proto_header		*header = NULL;

	if ( buf->length < sizeof(proto_header) ) {
		return 0;
	}

	header = (proto_header *)(buf->data + buf->offset);

	if ( header->length > (unsigned int)(buf->size - buf->offset) ) {
		wtg_log("Wtg too long master packet length. Maybe error!\n");
		return -1;
	}

	if ( (unsigned int)buf->length < header->length ) {
		return 0;
	} else {
		conn->stat = WTG_CSTAT_MASTER_RECV_BODY;
		buf->need = header->length;
		return header->length;
	}
}

/*
 * wtg_update_parse_cmd_url():		Parse update command URL.
 * @url:							Update command URL.
 * Returns:						0 on success, -1 on error.
 */
static int
wtg_update_parse_cmd_url(const char *url)
{
	wtg_update_context_t	*ctx = &wtg_info.update_ctx;
	char					*start = NULL, *end = NULL, *pend = NULL;

	strcpy(url_buf, url);

	if ( strncasecmp(url_buf, "http://", strlen("http://")) ) {
		wtg_log("Wtg command URL not start with \"http://\"! URL is \"%s\".\n", url);
		return -1;
	}

	start = url_buf + strlen("http://");
	end = strchr(start, '/');
	if ( !end ) {
		wtg_log("Wtg invalid update command URL \"%s\"!\n", url);
		return -1;
	}
	strcpy(ctx->cmd_uri, end);
	*end = 0;

	pend = strchr(start, ':');
	if ( !pend ) {
		wtg_log("Wtg no PORT in URL, use default \"%d\"!\n", WTG_DEF_FILE_SERVER_PORT);
		ctx->port = WTG_DEF_FILE_SERVER_PORT;

		if ( strlen(start) > WTG_DEF_STR_LEN - 1 ) {
			wtg_log("Wtg too long file server name \"%s\" 1!\n", start);
			return -1;
		}
		strcpy(ctx->hostname, start);
			
		return 0;
	}	
	*pend = 0;
	pend++;
	
	ctx->port = (unsigned short)atoi(pend);
	if ( ctx->port <= 0 ) {
		wtg_log("Get PORT from update command URL \"%s\" fail!\n", url);
		return -1;
	}

	if ( strlen(start) > WTG_DEF_STR_LEN - 1 ) {
		wtg_log("Wtg too long file server name \"%s\" 2!\n", start);
		return -1;
	}
	strcpy(ctx->hostname, start);

	return 0;
}

/*
 * wtg_master_handle_update():		Handle master connection packet update.
 * @conn:						Master connection.
 * Returns:						0 on success, -1 on error. 1 on error but not reset connection.
 */
static int
wtg_master_handle_update(wtg_conn_t *conn)
{
	wtg_update_context_t	*ctx = &wtg_info.update_ctx;
	wtg_buff_t				*buf = &conn->recv_buf;
	proto_header			*header = NULL;
	task_base				*task = NULL;
	
	header = (proto_header *)(buf->data + buf->offset);

	if ( header->length != sizeof(proto_header) + sizeof(task_base) ) {
		wtg_log("Wtg invalid update packet length - %d.\n", header->length);
		return -1;
	}

	task = (task_base *)(header->body);

	ctx->id = task->tid;

	if ( ctx->stat != WTG_UPDATE_STAT_IDLE ) {
		wtg_log("Wtg recive update command during updating!\n");
		wtg_update_send_response(UPDATE_RSTAT_OTHER_ERR, 0);
		return 1;
	}

	if ( strlen(task->url) == 0 ) {
		wtg_log("Wtg empty update command URI!\n");
		wtg_update_send_response(UPDATE_RSTAT_OTHER_ERR, 0);
		return 1;
	}

	if ( strlen(task->token) == 0 ) {
		wtg_log("Wtg empty update token!\n");
		wtg_update_send_response(UPDATE_RSTAT_OTHER_ERR, 0);
		return 1;
	}

	if ( task->factor == 0 ) {
		wtg_log("Wtg token factor could not be zero!\n");
		wtg_update_send_response(UPDATE_RSTAT_OTHER_ERR, 0);
		return 1;
	}

	if ( wtg_update_parse_cmd_url(task->url) ) {
		wtg_log("Wtg parse update command url fail!\n");
		wtg_update_send_response(UPDATE_RSTAT_OTHER_ERR, 0);
		return 1;
	}
	
	strcpy(ctx->token, task->token);
	ctx->token_factor = task->factor;

	ctx->restart = 1;	// Always restart.

	wtg_update_send_response(UPDATE_RSTAT_RECV_OK, 0);

	if ( wtg_update_init() ) {
		wtg_log("Wtg update initialize error!\n");
		wtg_update_roll_back();
		return 1;
	}

	return 0;
}

/*
 * wtg_master_fcontent_send_response():		Send file content response.
 * @status:								Response status.
 * @path:								File path.
 * @mtime:								File last modified time.
 * @file_len:								File length. When not OK, set to zero.
 * @file_content:							File content.
 * Returns:								0 on success, -1 on error.
 */
static int
wtg_master_fcontent_send_response(uint32_t status,
								const char *path,
								uint32_t mtime,
								uint32_t file_len,
								char *file_content)
{
	wtg_buff_t			*buf = &wtg_info.master_conn.send_buf;

	wtg_build_proto_header(CMD_REPORT_FILE_CONTENT_V2, sizeof(report_file_content_v2) + file_len);

	memset(&wtg_report_file_content, 0, sizeof(report_file_content_v2));

	wtg_report_file_content.ip = wtg_info.client_ip;
	wtg_report_file_content.port = wtg_info.client_port;
	wtg_report_file_content.status = status;
	wtg_report_file_content.mtime = mtime;
	strncpy(wtg_report_file_content.path, path, 255);
	wtg_report_file_content.file_len = file_len;

	if ( wtg_conn_buff_append(buf, (char *)&wtg_proto_header, sizeof(proto_header)) ) {
		wtg_log("Wtg add common protocol header fail when sned file content!\n");
		return -1;
	}

	if ( wtg_conn_buff_append(buf, (char *)&wtg_report_file_content, sizeof(report_file_content_v2)) ) {
		wtg_log("Wtg add file content protocol header to send buffer fail! May be full!\n");
		return -1;
	}

	if ( file_len > 0 ) {
		if ( wtg_conn_buff_append(buf, file_content, file_len) ) {
			wtg_log("Wtg add file content to send buffer fail! May be full!\n");
			return -1;
		}
	}

	return 0;
}

/*
 * wtg_master_send_file():		Send file content.
 * @name:					File name.
 * @type:					File type.
 * @st:						File status.
 * Returns:					0 on success, -1 on error. 1 on error but not reset the connection.
 */
static int
wtg_master_send_file(const char *name, uint32_t type, struct stat *st)
{
	int						fd = -1;
	uint32_t				send_len;
	char					*send_start = NULL;
	int						ret = 0;

	fd = open(name, O_RDONLY);
	if ( fd == -1 ) {
		wtg_log("Wtg open file \"%s\" fail when send file! %m\n", name);
		wtg_master_fcontent_send_response(REPORT_FILE_READ_ERR,
			name, (uint32_t)(st->st_mtime), 0, NULL);
		ret = 1;
		goto out;
	}

	if ( type == GET_FILE_CONF ) {
		// Config file.
		if ( read(fd, rep_file_content, st->st_size) != st->st_size ) {
			wtg_log("Wtg read config file \"%s\" fail! %m\n", name);
			wtg_master_fcontent_send_response(REPORT_FILE_READ_ERR,
				name, (uint32_t)(st->st_mtime), 0, NULL);
			ret = 1;
			goto out;
		}

		send_len = st->st_size;
		send_start = rep_file_content;
	} else {
		// Log file.
		if ( st->st_size <= WTG_LOG_REPORT_SIZE ) {
			if ( read(fd, rep_file_content, st->st_size) != st->st_size ) {
				wtg_log("Wtg read log file \"%s\" fail! 1 %m\n", name);
				wtg_master_fcontent_send_response(REPORT_FILE_READ_ERR,
					name, (uint32_t)(st->st_mtime), 0, NULL);
				ret = 1;
				goto out;
			}

			send_len = st->st_size;
			send_start = rep_file_content;
		} else {
			if ( lseek(fd, (-WTG_LOG_REPORT_SIZE), SEEK_END) == (off_t)(-1) ) {
				wtg_log("Wtg seek file \"%s\" fail! %m\n", name);
				wtg_master_fcontent_send_response(REPORT_FILE_READ_ERR,
					name, (uint32_t)(st->st_mtime), 0, NULL);
				ret = 1;
				goto out;
			}

			if ( read(fd, rep_file_content, WTG_LOG_REPORT_SIZE) != WTG_LOG_REPORT_SIZE ) {
				wtg_log("Wtg read log file \"%s\" fail! 2 %m\n", name);
				wtg_master_fcontent_send_response(REPORT_FILE_READ_ERR,
					name, (uint32_t)(st->st_mtime), 0, NULL);
				ret = 1;
				goto out;
			}

			rep_file_content[WTG_LOG_REPORT_SIZE] = 0;
			send_start = strchr(rep_file_content, '\n');
			if ( !send_start ) {
				send_start = rep_file_content;
				send_len = WTG_LOG_REPORT_SIZE;
			} else {
				send_start++;
				if ( send_start - rep_file_content >= WTG_LOG_REPORT_SIZE ) {
					send_start = rep_file_content;
					send_len = WTG_LOG_REPORT_SIZE;
				} else {
					send_len = WTG_LOG_REPORT_SIZE - (send_start - rep_file_content);
				}
			}
		}
	}

	if ( wtg_master_fcontent_send_response(REPORT_FILE_OK, name,
		st->st_mtime, send_len, send_start) ) {
		wtg_log("Wtg send file \"%s\" content fail! May be send buffer full!\n", name);
		ret = -1;
		goto out;
	}

out:
	if ( fd >= 0 ) {
		close(fd);
		fd = -1;
	}

	return ret;
}

/*
 * wtg_master_handle_get_file():		Get file content.
 * @conn:						Master connection.
 * Returns:						0 on success, -1 on error. 1 on error but not reset the connection.
 */
static int
wtg_master_handle_get_file(wtg_conn_t *conn)
{
	wtg_buff_t				*buf = &conn->recv_buf;
	proto_header			*header = NULL;
	get_file_content		*cmd = NULL;
	struct stat				st;
	int						ret = 0;

	header = (proto_header *)(buf->data + buf->offset);
	
	if ( header->length != sizeof(proto_header) + sizeof(get_file_content) ) {
		wtg_log("Wtg invalid get file content packet length - %d.\n", header->length);
		return -1;
	}
	
	cmd = (get_file_content *)(header->body);
	
	memset(fname, 0, sizeof(char) * (WTG_PATH_SIZE + 4));
	strncpy(fname, cmd->path, 255);

	if ( cmd->type != GET_FILE_CONF && cmd->type != GET_FILE_LOG ) {
		wtg_log("Wtg invalid get file type! \"%d\".\n", cmd->type);
		wtg_master_fcontent_send_response(REPORT_FILE_OTHER_ERR, fname, 0, 0, NULL);
		return 1;
	}

	if ( stat(fname, &st) ) {
		wtg_log("Wtg get file \"%s\" status fail! Maybe not existed! %m\n", fname);
		wtg_master_fcontent_send_response(REPORT_FILE_NFOUND_ERR, fname, 0, 0, NULL);
		return 1;
	}

	if ( st.st_size <= 0 ) {
		wtg_log("Wtg empty file \"%s\" when get file content!\n", fname);
		wtg_master_fcontent_send_response(REPORT_FILE_EMPTY_ERR,
			fname, (uint32_t)(st.st_mtime), 0, NULL);
		return 1;
	}

	if ( cmd->type == GET_FILE_CONF && st.st_size > WTG_DEF_FILE_CONTENT_SIZE ) {
		wtg_log("Wtg too large config file \"%s\" when get file content!\n", fname);
		wtg_master_fcontent_send_response(REPORT_FILE_LEAREG_ERR,
			fname, (uint32_t)(st.st_mtime), 0, NULL);
		return 1;
	}

	ret = wtg_master_send_file(fname, cmd->type, &st);
	if ( ret == -1 ) {
		// Send response in send method.
		wtg_log("Wtg master send file content \"%s\" fail! Reset master connection!\n", fname);
		return -1;
	} else if ( ret == 1 ) {
		wtg_log("Wtg master send file content \"%s\" fail! Not reset master connection!\n", fname);
		return 1;
	}

	return 0;
}

/*
 * wtg_master_handle_body():		Handle master connection packet body.
 * @conn:						Master connection.
 * Returns:						0 on success, -1 on error.
 */
static int
wtg_master_handle_body(wtg_conn_t *conn)
{
	int					ret = 0, len;
	wtg_buff_t			*buf = &conn->recv_buf;
	proto_header		*header = NULL;
	
	header = (proto_header *)(buf->data + buf->offset);
	len = header->length;

	if ( header->cmd == CMD_UPGRADE_NOTIFY ) {
		ret = wtg_master_handle_update(conn);
		goto out;
	} else if ( header->cmd == CMD_GET_FILE_CONTENT ) {
		ret = wtg_master_handle_get_file(conn);
		goto out;
	} else {
		ret = 0;
		goto out;
	}

out:
	wtg_conn_buff_use(buf, len);
	conn->stat = WTG_CSTAT_MASTER_RECV_HEAD;

	if ( ret == 1 ) {
		// Error but not reset the master connection.
		ret = 0;
	}
	
	return ret;
}

/*
 * wtg_master_conn_in():	EPOLLIN handler for master connection.
 */
static void
wtg_master_conn_in()
{
	int					rbytes;
	wtg_conn_t			*conn = &wtg_info.master_conn;
	int					ret;

again:
	if ( conn->recv_buf.size - conn->recv_buf.offset - conn->recv_buf.length <= 0 ) {
		wtg_log("Wtg master connection recive buffer full! Buffer size %d. Reset connection...\n",
			conn->recv_buf.size);
		if ( wtg_network_conn_reconnect(conn, wtg_info.master_name, wtg_info.master_port, EPOLLOUT) ) {
			wtg_log("Wtg reconnect to master fail when recive buff full! Try later!\n");
			conn->skt = -1;
		}
		return;
	}
	
	rbytes = recv(conn->skt, (conn->recv_buf.data + conn->recv_buf.offset + conn->recv_buf.length),
		(size_t)(conn->recv_buf.size - conn->recv_buf.offset - conn->recv_buf.length), MSG_DONTWAIT);
	
	if ( rbytes == 0 ) {
// #ifdef __DEBUG
		wtg_log("Wtg master connection be closed, maybe timeout.\n");
// #endif
		if ( wtg_network_conn_reconnect(conn, wtg_info.master_name, wtg_info.master_port, EPOLLOUT) ) {
			wtg_log("Wtg reconnect to master fail when be closed! Try later!\n");
			conn->skt = -1;
		}
		return;
	} else if ( rbytes < 0 ) {
		if ( errno == EWOULDBLOCK || errno == EAGAIN ) {
			// Only here return normaly.
			return;
		} else if ( errno == EINTR ) {
			goto again;
		} else {
			wtg_log("Wtg master connection recive error! %m\n");
			if ( wtg_network_conn_reconnect(conn, wtg_info.master_name, wtg_info.master_port, EPOLLOUT) ) {
				wtg_log("Wtg reconnect to master fail when recive error! Try later!\n");
				conn->skt = -1;
			}
			return;
		}
	} else {
		wtg_conn_touch(conn, wtg_info.master_timeout);
	
		// Handle data later.
		conn->recv_buf.length += rbytes;

handle:
		if ( conn->stat == WTG_CSTAT_MASTER_RECV_HEAD ) {
			ret = wtg_master_check_complete(conn);
			if ( ret > 0 ) {
				if ( wtg_master_handle_body(conn) ) {
					wtg_log("Wtg master connection body handle fail!\n");
					if ( wtg_network_conn_reconnect(conn, wtg_info.master_name, wtg_info.master_port, EPOLLOUT) ) {
						wtg_log("Wtg reconnect to master fail when body handle error! Try later!\n");
						conn->skt = -1;
					}
					return;
				} else {				
					goto handle;
				}
			} else if ( ret == 0) {
				goto again;
			} else {
				wtg_log("Wtg master connection recive head check error!\n");
				if ( wtg_network_conn_reconnect(conn, wtg_info.master_name, wtg_info.master_port, EPOLLOUT) ) {
					wtg_log("Wtg reconnect to master fail when recive head check error! Try later!\n");
					conn->skt = -1;
				}
				return;
			}
		} else {
			wtg_log("Wtg invalid master connection status \"%d\" when recive data!\n", conn->stat);
			if ( wtg_network_conn_reconnect(conn, wtg_info.master_name, wtg_info.master_port, EPOLLOUT) ) {
				wtg_log("Wtg reconnect to master fail when invalid status! Try later!\n");
				conn->skt = -1;
			}
			return;
		}
	}
}

/*
 * wtg_master_conn_out():	Handle EPOLLOUT event of master connection.
 */
static void
wtg_master_conn_out()
{
	int					sbytes;
	wtg_buff_t			*buf = &wtg_info.master_conn.send_buf;

again:
	if ( buf->length <= 0 ) {
		// Main loop will clean EPOLLOUT.
		return;
	}

	sbytes = send(wtg_info.master_conn.skt, buf->data + buf->offset, buf->length,
		MSG_DONTWAIT | MSG_NOSIGNAL);

	if ( sbytes < 0 ) {
		if ( errno == EWOULDBLOCK || errno == EAGAIN ) {
			return;
		} else if ( errno == EINTR ) {
			goto again;
		} else {
			if ( wtg_network_conn_reconnect(&wtg_info.master_conn,
					wtg_info.master_name, wtg_info.master_port, EPOLLOUT) ) {
				wtg_log("Wtg reconnect to master fail in master out event handler! Try later!\n");
				wtg_info.master_conn.skt = -1;
			}
			return;
		}
	} else {
		wtg_conn_buff_use(buf, sbytes);
		goto again;
	}
}

/*
 * wtg_err_event():		Handle EPOLLERR event.
 * @event:				EPOLL event information.
 */
static void
wtg_err_event(struct epoll_event *event)
{	
	if ( event->data.fd == wtg_info.master_conn.skt ) {
		if ( wtg_network_conn_reconnect(&wtg_info.master_conn,
				wtg_info.master_name, wtg_info.master_port, EPOLLOUT) ) {
			wtg_log("Wtg reconnect to master fail in error handler! Try later!\n");
			wtg_info.master_conn.skt = -1;
		}
	} else if ( event->data.fd == wtg_info.domain_server_skt ) {
		if ( wtg_domain_listen_reset() ) {
			wtg_log("Wtg UNIX domain server socket reset fail in error handler! Try later!\n");
			wtg_info.domain_server_skt = -1;
		}
	} else if ( event->data.fd != -1 && event->data.fd == wtg_info.download_conn.skt ) {
		wtg_log("Wtg download connection error! Roll back update!\n");
		wtg_update_roll_back();
	} else {
		// UNIX domain IPC socket.
		wtg_close_domain_by_fd(event->data.fd);
	}
}

/*
 * wtg_in_event():			Handle EPOLLIN event.
 * @event:				EPOLL event information.
 */
static void
wtg_in_event(struct epoll_event *event)
{
	// Should add download conn later.
	if ( event->data.fd == wtg_info.master_conn.skt ) {
		wtg_master_conn_in();
	} else if ( event->data.fd == wtg_info.domain_server_skt ) {
		wtg_domain_accept();
	} else if ( event->data.fd != -1 && event->data.fd == wtg_info.download_conn.skt ) {
		wtg_download_conn_in();	
	} else {
		wtg_domain_conn_in(event->data.fd);
	}
}

/*
 * wtg_out_event():		Handle EPOLLOUT event.
 * @event:				EPOLL event information.
 */
static void
wtg_out_event(struct epoll_event *event)
{
	// Should add download conn later.
	if ( event->data.fd == wtg_info.master_conn.skt ) {
		wtg_master_conn_out();
	} else if ( event->data.fd != -1 && event->data.fd == wtg_info.download_conn.skt ) {
		wtg_download_conn_out();
	} else {
		wtg_log("Invalid socket for EPOLLOUT event!\n");
	}
}

/*
 * wtg_event_handler():	Network and UNIX domain event handler.
 */
static void
wtg_event_handler()
{
	int							evt_cnt, i;
	static struct epoll_event	events[WTG_EPOLL_EVENT_CNT];
	
	if ( wtg_info.master_conn.send_buf.length > 0 ) {
		if ( wtg_info.master_conn.skt >= 0 ) {
			if ( wtg_epoll_mod(wtg_info.master_conn.skt, EPOLLOUT) ) {
				wtg_log("Wtg modified master socket EPOLL event to EPOLLOUT fail!\n");
			}
		}
	} else {
		if ( wtg_info.master_conn.skt >= 0 ) {
			if ( wtg_epoll_mod(wtg_info.master_conn.skt, 0) ) {
				wtg_log("Wtg modified master socket EPOLL event to no EPOLLOUT fail!\n");
			}
		}
	}

	evt_cnt = epoll_wait(wtg_info.epfd, events, WTG_EPOLL_EVENT_CNT, WTG_EPOLL_WAIT_TIME);
	if ( evt_cnt == -1 ) {
		wtg_log("EPOLL wait fail! %m\n");
		return;
	}

	for ( i = 0; i < evt_cnt; i++ ) {
		if ( events[i].events & EPOLLERR ) {
			wtg_err_event(&events[i]);
		} else {
			if ( events[i].events & EPOLLIN ) {
				wtg_in_event(&events[i]);
			}	
			if ( events[i].events & EPOLLOUT ) {
				wtg_out_event(&events[i]);
			}
		}
	}
}

/*
 * wtg_main_loop():		Wtg worker main loop.
 */
static void
wtg_main_loop()
{
	time_t						check_time;
	time_t						m_output_time = 0;
	time_t						l_output_time = 0;

	check_time = time(NULL) + WTG_CONN_CHECK_TIME;
	
	while ( !wtg_info.stop ) {
		if ( wtg_info.master_conn.skt == -1 ) {
			if ( wtg_network_conn_reconnect(&wtg_info.master_conn,
				wtg_info.master_name, wtg_info.master_port, EPOLLOUT) ) {
				if ( time(NULL) >= m_output_time ) {
					wtg_log("Wtg reconnect to master fail! Try later!\n");
					m_output_time = time(NULL) + WTG_RECONN_LOG_TIME;
				}
				wtg_info.master_conn.skt = -1;
			} else {
				m_output_time = 0;
			}
		}

		if ( wtg_info.domain_server_skt == -1 ) {
			if ( wtg_domain_listen_reset() ) {
				if ( time(NULL) >= l_output_time ) {
					wtg_log("Wtg UNIX domain server socket reset fail! Try later!\n");
					l_output_time = time(NULL) + WTG_RECONN_LOG_TIME;
				}
				wtg_info.domain_server_skt = -1;
			} else {
				l_output_time = 0;
			}
		}
		
		wtg_event_handler();

		if ( time(NULL) >= check_time ) {
			wtg_scan_domain_active_conns();
#ifdef __NWS
			if ( log_check(WTG_LOG_ROTATE_SIZE) ) {
				wtg_log("Wtg log check fail!\n");
			}
#endif
			check_time = time(NULL) + WTG_CONN_CHECK_TIME;
		}

		wtg_master_conn_check(&wtg_info.master_conn);
		wtg_download_conn_check(&wtg_info.download_conn);
	}
}

/*
 * wtg_update_cleanup():		Cleanup and send response when restart from update.
 */
static void
wtg_update_cleanup()
{
	struct stat			st;
	char				tmp_buf[WTG_INT_STR_MAX] = {0};
	int					fd = -1;
	
	// Weather restart by update command.
	if ( stat(WTG_UPDATE_FLAG_FILE, &st) == 0 ) {
		if ( st.st_size > 0 && st.st_size < WTG_INT_STR_MAX ) {
			fd = open(WTG_UPDATE_FLAG_FILE, O_RDONLY);
			if ( fd == -1 ) {
				wtg_log("Wtg open update flag file fail! Ignore it! %m\n");
				goto out;
			}
			if ( read(fd, tmp_buf, st.st_size) != st.st_size ) {
				wtg_log("Wtg read update flag file fail! Ignore it! %m\n");
				close(fd);
				goto out;
			}
			close(fd);
			wtg_info.update_ctx.id = (unsigned long)atol(tmp_buf);
			wtg_log("Wtg restart from update command successfully! tid - %lu. \n",
				wtg_info.update_ctx.id);
			// Update flag file existed!
			if ( wtg_update_send_response(UPDATE_RSTAT_OK, 1) ) {
				wtg_log("Wtg send update successful message fail! Ignore it!\n");
			}
			wtg_info.update_ctx.id = 0;
		} else {
			wtg_log("Wtg update flag file size \"%d\" error when restart from update command! Ignore it!\n",
				st.st_size);
		}
	}

out:
	unlink(WTG_UPDATE_FLAG_FILE);
}

/*
 * wtg_worker_thd_cleanup():	Wtg worker thread cleanup function.
 * @user:					Ignore.
 */
static void
wtg_worker_thd_cleanup(void *user)
{
	wtg_resource_destroy();
}

/*
 * wtg_worker_thd():		Wtg daemon worker thread.
 * @user:				Ignore.
 * Returns:				Ignore.
 */
static void*
wtg_worker_thd(void *user)
{
	pthread_cleanup_push(wtg_worker_thd_cleanup, NULL);

	wtg_update_cleanup();

	wtg_main_loop();
	
	pthread_cleanup_pop(1);

	pthread_exit(NULL);
}

/*
 * wtg_resource_init():	Initialize wtg module resource.
 * Returns:			0 on success, -1 on error.
 */
static int
wtg_resource_init()
{
	int					conn_fin_cnt = 0;

	wtg_info.epfd = -1;
	wtg_info.domain_server_skt = -1;
	
	if ( wtg_hash_init(&wtg_info.domain_conns.used_hash, wtg_info.domain_max_conn) ) {
		wtg_log("Wtg initialize used hash table fail!\n");
		goto err_out0;
	}

	// UNIX domain connection pool.
	wtg_info.domain_conns.conns = (wtg_conn_t *)calloc(wtg_info.domain_max_conn, sizeof(wtg_conn_t));
	if ( !wtg_info.domain_conns.conns ) {
		wtg_log("Wtg alloc memory for UNIX domain connections fail!\n");
		goto err_out1;
	}
	wtg_info.domain_conns.total_cnt = wtg_info.domain_max_conn;
	INIT_LIST_HEAD(&wtg_info.domain_conns.used);
	INIT_LIST_HEAD(&wtg_info.domain_conns.free);
	
	for ( conn_fin_cnt = 0; conn_fin_cnt < wtg_info.domain_max_conn; conn_fin_cnt++ ) {
		if ( wtg_conn_init(&wtg_info.domain_conns.conns[conn_fin_cnt], WTG_CONN_TYPE_DOMAIN) ) {
			wtg_log("Wtg initialize domain conn %d fail!\n", conn_fin_cnt);
			goto err_out2;
		}
	}

	// EPOLL initialize.
	if ( (wtg_info.epfd = epoll_create(wtg_info.domain_max_conn + WTG_EPOLL_EXT_CNT)) == -1 ) {
		wtg_log("Create EPOLL set fail! %m\n");
		goto err_out2;
	}

	// UNIX domain listen.	
	if ( wtg_domain_listen_reset() ) {
		wtg_log("UNIX domain listen socket initialize fail!\n");
		goto err_out3;
	}

	// Connection to master.
	if ( wtg_conn_init(&wtg_info.master_conn, WTG_CONN_TYPE_MASTER) ) {
		wtg_log("Wtg initialize master connection fail!\n");
		goto err_out4;
	}

	// printf("Wtg connecting to master...\n");
	// wtg_log("Wtg connecting to master...\n");
	
	// if ( wtg_network_conn_reconnect(&wtg_info.master_conn,
	//	wtg_info.master_name, wtg_info.master_port, EPOLLOUT) ) {
	//	printf("Wtg connect to master fail when launch!\n");
	//	wtg_log("Wtg connect to master fail when launch!\n");
	//	// goto err_out5;
	// }

	// Update connection.
	if ( wtg_conn_init(&wtg_info.download_conn, WTG_CONN_TYPE_DOWNLOAD) ) {
		wtg_log("Wtg initialize update connection fail!\n");
		goto err_out5;
	}

	return 0;

err_out5:
	wtg_conn_cleanup(&wtg_info.master_conn);

err_out4:
	wtg_domain_listen_close();

err_out3:
	if ( wtg_info.epfd >= 0 ) {
		close(wtg_info.epfd);
		wtg_info.epfd = -1;
	}

err_out2:
	if ( wtg_info.domain_conns.conns ) {
		for ( --conn_fin_cnt; conn_fin_cnt >= 0; conn_fin_cnt-- ) {
			wtg_conn_cleanup(&wtg_info.domain_conns.conns[conn_fin_cnt]);
		}

		free(wtg_info.domain_conns.conns);
		wtg_info.domain_conns.conns = NULL;
	}

err_out1:
	wtg_hash_destroy(&wtg_info.domain_conns.used_hash);

err_out0:
	return -1;
}

/*
 * wtg_local_ip_eth():	Get local IP by ethernet interface name.
 * @eth_name:		Ethernet interface name.
 * Returns:			Return integer IP on success, INADDR_NONE on error.
 */
static in_addr_t
wtg_local_ip_eth(const char *eth_name)
{
	int						fd = -1;
	struct ifreq			ifr;

	fd = socket(PF_INET, SOCK_DGRAM, 0);
	if ( fd == -1 ) {
		wtg_log("Wtg create tmp socket fail when get local IP. %m\n");
		return INADDR_NONE;
	}

	memset(&ifr, 0, sizeof(struct ifreq));
	strncpy(ifr.ifr_name, eth_name, IF_NAMESIZE - 1);

	if ( ioctl(fd, SIOCGIFADDR, &ifr) == -1 ) {
		wtg_log("Wtg ioctl fail when get \"%s\" IP. %m\n", eth_name);
	} else {
		if ( ((struct sockaddr_in *)(&ifr.ifr_addr))->sin_addr.s_addr == INADDR_NONE ) {
			wtg_log("Wtg get \"%s\" IP fail!\n", eth_name);
		} else {
			wtg_log("Wtg \"%s\" IP - %d \"%s\".\n", eth_name,
				((struct sockaddr_in *)(&ifr.ifr_addr))->sin_addr.s_addr,
				inet_ntoa(((struct sockaddr_in *)(&ifr.ifr_addr))->sin_addr));
			if ( fd >= 0 ) {
				close(fd);
				fd = -1;
			}
			return ((struct sockaddr_in *)(&ifr.ifr_addr))->sin_addr.s_addr;
		}
	}

	if ( fd >= 0 ) {
		close(fd);
		fd = -1;
	}

	return INADDR_NONE;
}

/*
 * wtg_local_ip():		Get local IP.
 * Returns:			Return integer IP on success, INADDR_NONE on error.
 */
static in_addr_t
wtg_local_ip()
{
	in_addr_t		local_ip = INADDR_NONE;

	local_ip = wtg_local_ip_eth("eth0");
	if ( local_ip != INADDR_NONE ) {
		return local_ip;
	}

	return wtg_local_ip_eth("eth1");
}

/*
 * wtg_load_conf():	Wtg module load configuration.
 * Returns:			0 on success, -1 on error.
 */
static int
wtg_load_conf()
{
	char		*str_val = NULL;
	int			i_val;

	i_val = myconfig_get_intval("wtg_enable", DEF_USE_FLAG);
	if ( i_val == DEF_USE_FLAG ) {
#ifdef __HARDCODE_CONF
		wtg_info.enabled = WTG_ENABLE;
		if ( wtg_info.enabled == 0 ) {
			return 0;
		}
#else
		wtg_info.enabled = 0;
		return 0;
#endif
	} else if ( i_val == 0 ) {
		wtg_info.enabled = 0;
		return 0;
	} else {
		wtg_info.enabled = 1;
	}

#ifdef __NWS
	str_val = myconfig_get_value("wtg_nws_log");
	if ( !str_val ) {
		strncpy(wtg_info.log_name, WTG_DEF_LOG_NAME, WTG_DEF_STR_LEN - 1);
		printf("Wtg \"wtg_nws_log\" not found in nws config file, use default file \"%s\".\n",
			wtg_info.log_name);
	} else {
		strncpy(wtg_info.log_name, str_val, WTG_DEF_STR_LEN - 1);
	}
	/*
	REGISTER_LOG(wtg_info.log_name,	WTG_LOG_LEVEL,
		(WTG_LOG_ROTATE_SIZE << 20), 0, 1, wtg_log_fd, update_wtg_log);
	*/
	if ( log_open(wtg_info.log_name) ) {
		printf("Wtg create log file fail!\n");
		return -1;
	}
#endif

/*
	str_val = myconfig_get_value("wtg_master_ip");
	if ( !str_val ) {
#ifdef __HARDCODE_CONF
		wtg_info.master_ip = inet_addr(WTG_MASTER_IP);
		if ( wtg_info.master_ip == INADDR_NONE ) {
			wtg_log("Wtg invalid hard code wtg master IP \"%s\".\n", WTG_MASTER_IP);
			goto err_out;
		}
		goto next0;
#else
		wtg_log("Wtg get config item \"wtg_master_ip\" fail!\n");
		goto err_out;
#endif
	}
	wtg_info.master_ip = inet_addr(str_val);
	if ( wtg_info.master_ip == INADDR_NONE ) {
		wtg_log("Wtg invalid wtg master IP \"%s\".\n", str_val);
		goto err_out;
	}
*/

	str_val = myconfig_get_value("wtg_master_name");
	if ( !str_val ) {
#ifdef __HARDCODE_CONF
		if ( strlen(WTG_MASTER_NAME) > WTG_DEF_STR_LEN - 1 ) {
			wtg_log("Wtg too long hardcode master name \"%s\".\n", WTG_MASTER_NAME);
			goto err_out;
		}
		strncpy(wtg_info.master_name, WTG_MASTER_NAME, WTG_DEF_STR_LEN - 1);
		goto next0;
#else
		wtg_log("Wtg get config item \"wtg_master_name\" fail!\n");
		goto err_out;
#endif
	}
	if ( strlen(str_val) > WTG_DEF_STR_LEN - 1 ) {
		wtg_log("Wtg too long master name \"%s\".\n", str_val);
		goto err_out;
	}
	strncpy(wtg_info.master_name, str_val, WTG_DEF_STR_LEN - 1);

#ifdef __HARDCODE_CONF
next0:
#endif

	i_val = myconfig_get_intval("wtg_master_port", WTG_DEF_MASTER_PORT);
	if ( i_val == WTG_DEF_MASTER_PORT ) {
#ifdef __HARDCODE_CONF
		wtg_info.master_port = (unsigned short)WTG_MASTER_PORT;
		if ( WTG_MASTER_PORT <= 0 || WTG_MASTER_PORT > 65535 ) {
			wtg_log("Wtg invalid hard code wtg master PORT \"%d\".\n",
				wtg_info.master_port);
			goto err_out;
		}
		goto next1;
#else
		wtg_log("Wtg invalid wtg master PORT \"%d\".\n", i_val);
		goto err_out;
#endif
	} else if ( i_val <= 0 || i_val > 65535 ) {
		wtg_log("Wtg invalid wtg master PORT \"%d\".\n", i_val);
		goto err_out;
	}
	wtg_info.master_port = (unsigned short)i_val;

#ifdef __HARDCODE_CONF
next1:
#endif

/*
	str_val = myconfig_get_value("wtg_file_server_ip");
	if ( !str_val ) {
		wtg_log("Wtg get config item \"wtg_file_server_ip\" fail!\n");
		goto err_out;
	}
	wtg_info.download_ip = inet_addr(str_val);
	if ( wtg_info.download_ip == INADDR_NONE ) {
		wtg_log("Wtg invalid wtg download file server IP \"%s\".\n", str_val);
		goto err_out;
	}

	i_val = myconfig_get_intval("wtg_file_server_port", WTG_DEF_DOWNLOAD_PORT);
	if ( i_val <= 0 || i_val > 65535 ) {
		wtg_log("Wtg invalid wtg download file server PORT \"%d\".\n", i_val);
		goto err_out;
	}
	wtg_info.download_port = (unsigned short)i_val;
*/

	/*
	str_val = myconfig_get_value("wtg_main_ip");
	if ( !str_val ) {
		wtg_log("Wtg get config item \"wtg_main_ip\" fail!\n");
		goto err_out;
	}
	wtg_info.client_ip = inet_addr(str_val);
	if ( wtg_info.client_ip == INADDR_NONE ) {
		wtg_log("Wtg invalid wtg client main IP \"%s\".\n", str_val);
		goto err_out;
	}
	*/

	wtg_info.client_ip = INADDR_NONE;
	str_val = myconfig_get_value("main_eth");
	if ( str_val ) {
		wtg_info.client_ip = wtg_local_ip_eth(str_val);
	} else {
		wtg_info.client_ip = wtg_local_ip();
	}
	if ( wtg_info.client_ip == INADDR_NONE ) {
		wtg_log("Wtg get client main IP fail!\n");
		goto err_out;
	}

	i_val = myconfig_get_intval("wtg_report_buf_size", WTG_DEF_REPORT_BUF_SIZE);
	if ( i_val < WTG_DEF_REPORT_BUF_SIZE ) {
		wtg_log("Wtg too little \"wtg_report_buf_size\" - \"%d\", set to \"%d\".\n",
			i_val, WTG_DEF_REPORT_BUF_SIZE);
		i_val = WTG_DEF_REPORT_BUF_SIZE;
	}
	wtg_info.report_buf_size = i_val;
	wtg_info.mrecv_buf_size = WTG_DEF_MRECV_SIZE;

	i_val = myconfig_get_intval("wtg_domain_rbuf_size", WTG_DEF_DOMAIN_RBUF_SIZE);
	if ( i_val < WTG_DEF_DOMAIN_RBUF_SIZE ) {
		wtg_log("Wtg too little \"wtg_domain_rbuf_size\" - \"%d\", set to \"%d\".\n",
			i_val, WTG_DEF_DOMAIN_RBUF_SIZE);
		i_val = WTG_DEF_DOMAIN_RBUF_SIZE;
	}
	wtg_info.domain_rbuf_size = i_val;

	i_val = myconfig_get_intval("wtg_domain_conn_count", WTG_DEF_DOMAIN_CONN_CNT);
	if ( i_val < WTG_DEF_DOMAIN_CONN_CNT ) {
		wtg_log("Wtg too little \"wtg_domain_conn_count\" - \"%d\", set to \"%d\".\n",
			i_val, WTG_DEF_DOMAIN_CONN_CNT);
		i_val = WTG_DEF_DOMAIN_CONN_CNT;
	}
	wtg_info.domain_max_conn = i_val;

	str_val = myconfig_get_value("wtg_domain_address");
	if ( !str_val ) {
		strncpy(wtg_info.domain_address, WTG_DEF_DOMAIN_ADDR, WTG_DEF_STR_LEN);
		wtg_log("Wtg no config item \"wtg_domain_address\" found, use default \"%s\"\n",
			wtg_info.domain_address);
	} else {
		strncpy(wtg_info.domain_address, str_val, WTG_DEF_STR_LEN);
	}

	i_val = myconfig_get_intval("wtg_domain_timeout", WTG_DEF_DOMAIN_TIMEOUT);
	if ( i_val < WTG_DOMAIN_TIMEOUT_MIN || i_val > WTG_DOMAIN_TIMEOUT_MAX ) {
		wtg_log("Wtg invalid \"wtg_domain_timeout\" - \"%d\", set to \"%d\".\n",
			i_val, WTG_DEF_DOMAIN_TIMEOUT);
		i_val = WTG_DEF_DOMAIN_TIMEOUT;
	}
	wtg_info.domain_timeout = i_val;

	i_val = myconfig_get_intval("wtg_master_timeout", WTG_DEF_MASTER_TIMEOUT);
	if ( i_val < WTG_MASTER_TIMEOUT_MIN ) {
		wtg_log("Wtg too little \"wtg_master_timeout\" - \"%d\", set to \"%d\".\n",
			i_val, WTG_MASTER_TIMEOUT_MIN);
		i_val = WTG_MASTER_TIMEOUT_MIN;
	}
	wtg_info.master_timeout = i_val;

	i_val = myconfig_get_intval("wtg_download_timeout", WTG_DEF_DOWNLOAD_TIMEOUT);
	if ( i_val < WTG_DOWNLOAD_TIMEOUT_MIN ) {
		wtg_log("Wtg too little \"wtg_download_timeout\" - \"%d\", set to \"%d\".\n",
			i_val, WTG_DOWNLOAD_TIMEOUT_MIN);
		i_val = WTG_DOWNLOAD_TIMEOUT_MIN;
	}
	wtg_info.download_timeout = i_val;
	
	wtg_info.download_sbuf_size = WTG_DEF_DOWNLOAD_SBUF_SIZE;
	wtg_info.download_rbuf_size = WTG_DEF_DOWNLOAD_RBUF_SIZE;

/*
	str_val = = myconfig_get_value("wtg_token");
	if ( !str_val ) {
		wtg_log("Wtg no config item \"wtg_token\" , use default \"%s\".\n",
			WTG_DEF_DOWNLOAD_TOKEN);
		strcpy(wtg_info.token, WTG_DEF_DOWNLOAD_TOKEN);
	} else {
		strncpy(wtg_info.token, str_val, WTG_DEF_STR_LEN - 1);
	}

	i_val = myconfig_get_intval("wtg_token_factor", WTG_DEF_DOWNLOAD_TOKEN_FACTOR);
	if ( i_val == 0 ) {
		wtg_log("Wtg config item \"wtg_token_factor\" could not be zero, set to \"%d\".\n",
			WTG_DEF_DOWNLOAD_TOKEN_FACTOR);
		i_val = WTG_DEF_DOWNLOAD_TOKEN_FACTOR;
	}
	wtg_info.token_factor = i_val;
*/

	return 0;

err_out:
#ifdef __NWS
	log_close();
#endif
	return -1;
}

/*
 * wtg_init():		Initialize wtg module.
 * @port:		Client PORT.
 * Returns:		0 on success, -1 on error.
 */
static int
wtg_init(unsigned short port)
{
	int					ret = 0;
	// struct stat			st;
	// char				tmp_buf[WTG_INT_STR_MAX] = {0};
	// int					fd = -1;

	memset(&wtg_info, 0, sizeof(wtg_daemon_info_t));
	wtg_info.client_port = port;
	wtg_info.thd_id = (unsigned long int)(-1);
	wtg_info.update_ctx.stat = WTG_UPDATE_STAT_IDLE;
	wtg_info.update_ctx.fd = -1;

#ifndef __MCP
#ifndef __NWS
	printf("Wtg module only for MCP or NWS.\n");
	ret = -1;
	goto out;
#endif
#endif

#ifdef __MCP
	wtg_info.type = WTG_D_MCP;
#else
#ifdef __NWS
	wtg_info.type = WTG_D_NWS;
#endif
#endif

	if ( wtg_load_conf() ) {
		printf("Wtg load configration fail!\n");
		ret = -1;
		goto out;
	}

	if ( !wtg_info.enabled ) {
		goto out;
	}

	wtg_log("Wtg load configuration successed!\n");

	if ( wtg_resource_init() ) {
		wtg_log("Wtg resource initialize fail!\n");
		ret = -1;
		goto out;
	}

/*
	// Weather restart by update command.
	if ( stat(WTG_UPDATE_FLAG_FILE, &st) == 0 ) {
		if ( st.st_size > 0 && st.st_size < WTG_INT_STR_MAX ) {
			fd = open(WTG_UPDATE_FLAG_FILE, O_RDONLY);
			if ( fd == -1 ) {
				wtg_log("Wtg open update flag file fail! Ignore it! %m\n");
				goto out;
			}
			if ( read(fd, tmp_buf, st.st_size) != st.st_size ) {
				wtg_log("Wtg read update flag file fail! Ignore it! %m\n");
				close(fd);
				goto out;
			}
			close(fd);
			wtg_info.update_ctx.id = (unsigned long)atol(tmp_buf);
			wtg_log("Wtg restart from update command successfully! tid - %lu. \n",
				wtg_info.update_ctx.id);
			// Update flag file existed!
			if ( wtg_update_send_response(UPDATE_RSTAT_OK, 0) ) {
				wtg_log("Wtg send update successful message fail! Ignore it!\n");
			}
			wtg_info.update_ctx.id = 0;
		} else {
			wtg_log("Wtg update flag file size \"%d\" error when restart from update command! Ignore it!\n",
				st.st_size);
		}
	}
	*/

out:
	// unlink(WTG_UPDATE_FLAG_FILE);

	return ret;
}

/*
 * wtg_dump_args():		Dump command args.
 * @pname:				Program name for execv().
 * @argc:				Command line argc.
 * @argv:				Command line argv.
 * Returns:				0 on success, -1 on error.
 */
int
wtg_dump_args(char *pname, int argc, char **argv)
{
	if ( pname == NULL ) {
		printf("Wtg no program name when dump command line args, restart not support!\n");
		strcpy(pro_name, "");
		return 0;
	}

	if ( strlen(pro_name) > WTG_PATH_SIZE - 1 ) {
		printf("Wtg too long command program name \"%s\"!\n", pname);
		return -1;
	}
	strcpy(pro_name, pname);

	if ( argc > WTG_ARG_CNT - 1 ) {
		printf("Wtg too large argc \"%d\"!\n", argc);
		return -1;
	}
	pro_argc = argc;

	pro_argv[argc] = NULL;
	while ( --argc >= 0 ) {
		pro_argv[argc] = pro_argv_str[argc];
		if ( strlen(argv[argc]) > WTG_PATH_SIZE - 1 ) {
			printf("Wtg too large args \"%d\" - \"%s\"!\n", argc, argv[argc]);
			return -1;
		}
		strcpy(pro_argv[argc], argv[argc]);
	}
	
	return 0;
}

/*
 * wtg_start():	Start wtg module.
 * @port:		Client PORT.
 * Returns:		0 on success, -1 on error.
 */
int
wtg_start(unsigned short port)
{
	struct in_addr		addr;
	
	if ( wtg_init(port) ) {
		printf("Wtg module initialize fail!\n");
		return -1;
	}

	if ( !wtg_info.enabled ) {
		printf("Wtg not enable!\n");
		return 0;
	}

	if ( pthread_create(&wtg_info.thd_id, NULL, wtg_worker_thd, NULL) ) {
		wtg_log("Wtg create worker thread fail!\n");
		wtg_info.thd_id = (unsigned long int)(-1);
		wtg_resource_destroy();
		return -1;
	}

	addr.s_addr = wtg_info.client_ip;
	printf("Wtg main IP \"%s\", PORT - %u.\n", inet_ntoa(addr), wtg_info.client_port);
	wtg_log("Wtg main PORT %u.\n", wtg_info.client_port);
	printf("Wtg daemon start! Version: \"%s\"\n", wtg_version_string);
	wtg_log("Wtg daemon start! Version: \"%s\"\n", wtg_version_string);
	
	return 0;
}

/*
 * wtg_stop():	Stop wtg module.
 */
void
wtg_stop()
{
	if ( !wtg_info.enabled ) {
		return;
	}
	
	wtg_info.stop = 1;

	if ( wtg_info.thd_id != (unsigned long int)(-1) ) {
		pthread_join(wtg_info.thd_id, NULL);
		wtg_info.thd_id = (unsigned long int)(-1);
	}
}

/*
 * wtg_release_in_child():		Release resource befor fork() in child process. NWS watchdog must call this befor fork.
 */
void
wtg_release_in_child()
{
	if ( !wtg_info.enabled ) {
		return;
	}
	
	wtg_info.is_sub = 1;
	wtg_info.stop = 1;
}

