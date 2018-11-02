/*
 * watchdog_network.h:	Watchdog network utility.
 * Date:					2011-03-28
 */

#ifndef __WATCHDOG_NETWORK_H
#define __WATCHDOG_NETWORK_H

#include <netinet/in.h>

/*
 * build_content_func_t():	Build network send content.
 * @data:				Set to the information buffer.
 * Returns:				Return content data length when success (> 0),
 *						0 means no error but not need to send anything,
 *						<0 on error.
 */
typedef int (*build_content_func_t)(char **);

typedef struct {
	in_addr_t				l_ip;					// Local Server (NWS or MCP++) IP.
	in_addr_t				m_ip;					// Managed Platform Master Server IP.
	unsigned short 			m_port;					// Managed Platform Master Server PORT.
	build_content_func_t	build_base_func;		// Build base information function.
	build_content_func_t	build_report_func;		// Build report information function.
} network_conf_t;

/*
 * network_init():			Initialize the network.
 * @conf:				Network configuration.
 * Returns:				0 on success, -1 on error.
 */
extern int
network_init(network_conf_t *conf);

/*
 * network_destroy():	Clean up network.
 */
extern void
network_destroy();

/*
 * network_send_base():	Send base information.
 * @ip:					Managed Platform IP.
 * @port:				Managed Platform PORT.
 * Returns:				0 on success, -1 on error.
 */
extern int
network_send_base(in_addr_t ip, unsigned short port);

/*
 * network_handler():	Handle network IPC.
 * @ip:				Managed Platform IP.
 * @port:			Managed Platform PORT.
 * Returns:			0 on success, -1 on error.
 */
int
network_handler(in_addr_t ip, unsigned short port);

/*
 * network_reset():		Reset the network.
 * @ip:					Maneged Platform IP.
 * @port:				Managed Platform PORT.
 * Returns:				0 on success, -1 on error.
 */
extern int
network_reset(in_addr_t ip, unsigned short port);

/*
 * network_lconn_reset():	Reset long connection to Managed Platform Master.
 * @ip:					Maneged Platform IP.
 * @port:				Managed Platform PORT.
 * Returns;				0 on success, -1 on error.
 */
extern int
network_lconn_reset(in_addr_t ip, unsigned short port);

/*
 * network_epoll_reset():	EPOLL reset.
 * Returns:				0 on success, -1 on error.
 */
extern int
network_epoll_reset();

/*
 * network_close_fd():	Close network fds.
 */
extern void
network_close_fd();

/*
 * network_close_skt():	Close network socket.
 */
extern void
network_close_skt();

/*
 * network_close_epoll():	Close EPOLL set.
 */
extern void
network_close_epoll();

#endif

