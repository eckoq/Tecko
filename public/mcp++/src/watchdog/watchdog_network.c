/*
 * watchdog_network.h:	Watchdog network utility.
 * Date:					2011-03-28
 */

#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>

#include "wsproto.h"
#include "watchdog_network.h"

#define MCP_PLUS_PLUS						// Define this macro only in MCP++ watchdog.

#ifdef MCP_PLUS_PLUS
#include "watchdog_log.h"
#else
static void
log_sys(const char *fmt, ...)
{
}
#endif

#define SOCK_TIMEOUT			10								// Socket send and recive timeout.
#define EPOLL_S_SIZE			32								// EPOLL set size.

#define HUNGUP_TIME				1								// Sleep time to release CPU.

#define REPORT_HEADER_LEN		(sizeof(proto_header) + sizeof(stat_report_req))	// Report header length.

#define TYPE_BASE				1								// Report type base infromation.
#define TYPE_ATTR				2								// Report type attribute.

#define BIN_NWS					1								// NWS watchdog.
#define BIN_CCD					2								// MCP++ watchdog.

static in_addr_t				l_ip;							// Store local server IP.

static int						m_skt = -1;						// Fd of managed platform connection socket.
static int						m_epfd = -1;					// Epoll fd of managed platform connection.

static build_content_func_t		network_build_base = NULL;		// Build base information function.
static build_content_func_t		network_build_report = NULL;	// Build report information function.

static char						report_header[REPORT_HEADER_LEN];	// Report header.

static proto_header				*header_pro = NULL;				// Protocol header.
static stat_report_req			*header_rep = NULL;				// Report header.

/*
 * network_comm_header():	Reset header buffer common part.
 * @content_len:			Packet data length.
 */
static inline void
network_comm_header(int content_len)
{
	header_pro->length = (unsigned int)(REPORT_HEADER_LEN + content_len);
	header_pro->cmd = CMD_STAT_REPORT;

	header_rep->ip = l_ip;
#ifdef MCP_PLUS_PLUS
	header_rep->pid = BIN_CCD;
#else
	header_rep->pid = BIN_NWS;
#endif
	header_rep->length = (unsigned short)content_len;
}

/*
 * network_base_header():	Reset base information header buffer.
 * @content_len:			Base information data length.
 */
static inline void
network_base_header(int content_len)
{
	header_rep->type = TYPE_BASE;
	network_comm_header(content_len);
}

/*
 * network_rep_header():	Reset report header buffer.
 * @content_len:			Report information data length.
 */
static inline void
network_rep_header(int content_len)
{
	header_rep->type = TYPE_ATTR;
	network_comm_header(content_len);
}

/*
 * network_close_skt():	Close network socket.
 */
void
network_close_skt()
{
	if ( m_skt >= 0 ) {
		close(m_skt);
		m_skt = -1;
	}
}

/*
 * network_close_epoll():	Close EPOLL set.
 */
void
network_close_epoll()
{
	if ( m_epfd >= 0 ) {
		close(m_epfd);
		m_epfd = -1;
	}
}

/*
 * network_close_fd():	Close network fds.
 */
void
network_close_fd()
{
	network_close_skt();
	network_close_epoll();
}

/*
 * network_lconn_reset():	Reset long connection to Managed Platform Master.
 * @ip:					Maneged Platform IP.
 * @port:				Managed Platform PORT.
 * Returns;				0 on success, -1 on error.
 */
int
network_lconn_reset(in_addr_t ip, unsigned short port)
{
	struct sockaddr_in		addr;
	struct timeval			s_timeout;

	network_close_skt();
	
	m_skt = socket(PF_INET, SOCK_STREAM, 0);
	if ( m_skt == -1 ) {
		log_sys("Create socket fail! %m\n");
		return -1;
	}

	s_timeout.tv_sec = SOCK_TIMEOUT;
	s_timeout.tv_usec = 0;
	if ( setsockopt(m_skt, SOL_SOCKET, SO_RCVTIMEO, &s_timeout, sizeof(struct timeval)) ) {
		log_sys("Set socket recive timeout time fail! %m\n");
		goto err_out;
	}
	if ( setsockopt(m_skt, SOL_SOCKET, SO_SNDTIMEO, &s_timeout, sizeof(struct timeval)) ) {
		log_sys("Set socket send timeout time fail! %m\n");
		goto err_out;
	}

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = ip;
	addr.sin_port = htons(port);
	if ( connect(m_skt, (struct sockaddr *)&addr, (socklen_t)sizeof(struct sockaddr_in)) ) {
		log_sys("Connect Managed Flatform server fail! %m\n");
		goto err_out;
	}

	return 0;

err_out:
	network_close_skt();

	return -1;
}

/*
 * network_epoll_reset():	EPOLL reset.
 * Returns:				0 on success, -1 on error.
 */
int
network_epoll_reset()
{
	network_close_epoll();

	// Create EPOLL set. EPOLL only be used with download socket. Actually, only to check timeout.
	m_epfd = epoll_create(EPOLL_S_SIZE);
	if ( m_epfd == -1 ) {
		log_sys("Create EPOLL set fail! %m\n");
		return -1;
	}
	
	return 0;
}

/*
 * network_reset():		Reset the network.
 * @ip:					Maneged Platform IP.
 * @port:				Managed Platform PORT.
 * Returns:				0 on success, -1 on error.
 */
int
network_reset(in_addr_t ip, unsigned short port)
{
	if ( network_lconn_reset(ip, port) ) {
		log_sys("Reset long connection to Managed Platform Master fail!\n");
		return -1;
	}

	if ( network_epoll_reset() ) {
		log_sys("Epoll reset fail!\n");
		return -1;
	}

	return 0;
}

/*
 * network_init():			Initialize the network.
 * @conf:				Network configuration.
 * Returns:				0 on success, -1 on error.
 */
int
network_init(network_conf_t *conf)
{
	memset(report_header, 0, REPORT_HEADER_LEN);
	header_pro = (proto_header *)report_header;
	header_rep = (stat_report_req *)(report_header + sizeof(proto_header));

	l_ip = conf->l_ip;

	if ( !conf->build_base_func || !conf->build_report_func ) {
		log_sys("Build content function should not be NULL!\n");
		return -1;
	}

	network_build_base = conf->build_base_func;
	network_build_report = conf->build_report_func;

	return network_reset(conf->m_ip, conf->m_port);
}

/*
 * network_destroy():	Clean up network.
 */
void
network_destroy()
{
	network_close_fd();
}

/*
 * network_send_base():	Send base information.
 * @ip:					Managed Platform IP.
 * @port:				Managed Platform PORT.
 * Returns:				0 on success, -1 on error.
 */
int
network_send_base(in_addr_t ip, unsigned short port)
{
	char					*base_data = NULL;
	int						base_len, err = 0;

	if ( m_skt < 0 ) {
		log_sys("Socket not create when send base!\n");
		return -1;
	}
	
	base_len = network_build_base(&base_data);
	if ( base_len < 0 ) {
		log_sys("Build base information error!\n");
		return -1;
	}

	if ( base_len != 0 ) {
		network_base_header(base_len);
		
		if ( send(m_skt, report_header, REPORT_HEADER_LEN, 0) != REPORT_HEADER_LEN ) {
			log_sys("Send protocol header error when send base information! %m Retrying.\n");
			err = 1;
			goto retry;
		}
		if ( send(m_skt, base_data, base_len, 0) != base_len ) {
			log_sys("Send base information Managed Platform Master fail! %m Retrying.\n");
			err = 1;
			goto retry;
		}
		
retry:
		if ( err == 1 ) {
			if ( network_lconn_reset(ip, port) ) {
				log_sys("Reset connection fail when retry send base!\n");
				return -1;
			}
			
			if ( send(m_skt, report_header, REPORT_HEADER_LEN, 0) != REPORT_HEADER_LEN ) {
				log_sys("Send protocol header error when send base information! %m\n");
				return -1;
			}
			if ( send(m_skt, base_data, base_len, 0) != base_len ) {
				log_sys("Send base information Managed Platform Master fail! %m\n");
				return -1;
			}
		}

		log_sys("Base information has been send!\n");
	}

	return 0;
}

/*
 * network_handler():	Handle network IPC.
 * @ip:				Managed Platform IP.
 * @port:			Managed Platform PORT.
 * Returns:			0 on success, -1 on error.
 */
int
network_handler(in_addr_t ip, unsigned short port)
{
	char				*rep_data = NULL;
	int					rep_len;
	
	if ( m_skt < 0 ) {
		log_sys("Socket not create when report!\n");
		return -1;
	}

	if ( network_send_base(ip, port) ) {
		log_sys("Send base information fail in network handler!\n");
		return -1;
	}

	rep_len = network_build_report(&rep_data);
	if ( rep_len < 0 ) {
		log_sys("Build reprot information error!\n");
		return -1;
	}

	if ( rep_len != 0 ) {
		network_rep_header(rep_len);
		if ( send(m_skt, report_header, REPORT_HEADER_LEN, 0) != REPORT_HEADER_LEN ) {
			log_sys("Send protocol header error when send report information! %m\n");
			return -1;
		}
		if ( send(m_skt, rep_data, rep_len, 0) != rep_len ) {
			log_sys("Send report informaion error! %m\n");
			return -1;
		}
	}

	sleep(HUNGUP_TIME);		// Release CPU here.
	
	return 0;
}

