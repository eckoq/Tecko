/*
 * tfc_net_socket_udp.h:	Encapsulation of the interface of UDP transport layer.
 * Date:					2012-05-21
 */

#ifndef _TFC_NET_SOCKET_UDP_H_
#define _TFC_NET_SOCKET_UDP_H_

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <string>

namespace tfc
{ 
	namespace net
	{		
		class CSocketUDP
		{			
		public:
			// Construction
			CSocketUDP(): _socket_fd(INVALID_SOCKET), _close_protect(true){}
			CSocketUDP(int fd, bool close_protect):_socket_fd(fd), _close_protect(close_protect){}
			~CSocketUDP(){if (_close_protect)close();}
			int create();
			
			inline int fd() const {return _socket_fd;};
			bool socket_is_ok() const {return (_socket_fd != INVALID_SOCKET);}
			void close();

			// Operations
			int bind(const std::string& server_address, unsigned short port);
			int bind_if(const std::string& ifname, unsigned short port);
			
			int recvfrom(void *buf, size_t buf_size, size_t &received_len,
				int flag, struct sockaddr &from, socklen_t &fromlen);
			int sendto(const void *buf, size_t buf_size, size_t &sent_len,
				int flag, const struct sockaddr &to, socklen_t tolen);

			int set_nonblock();
			int set_reuseaddr();
			int set_reuseport();

			void detach(){_close_protect = false;}
			void attach(int fd){if (_close_protect){close();_close_protect = true;} _socket_fd = fd;}
			
			//根据适配器名字（eth0，eth1等）获取其ip地址	
			unsigned get_ip_by_if(const char* ifname);
			int in_s2n(const std::string& addr, in_addr_t &addr_4byte);
			
		private:
			int _socket_fd;
			bool _close_protect;
			static const int INVALID_SOCKET = -1;
		};
	}
}

#endif

