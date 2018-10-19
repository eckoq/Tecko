#ifndef _TFC_NET_SOCKET_TCP_H_
#define _TFC_NET_SOCKET_TCP_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string>
#include <stdio.h>

//////////////////////////////////////////////////////////////////////////
//	open for extension, open for modification
//////////////////////////////////////////////////////////////////////////

//
//	return value 的规则，
//	如果操作成功，返回 0，
//	如果操作失败，返回一个负数，代表一个errno的相反数或者-1不代表任何意义
//	没有返回正数的情况，所有的输出数据，都以输出参数的形式。
//	例如accept，是否成功在返回值中表示，accept产生的新fd，以参数形式传出
//	再如recv，如果失败，以返回值表示，如果是负数，表示errno，比如-EAGAIN
//	如果在recv中收到对方关闭的消息，recv操作仍然返回0，但是收到的数据长度是0
//

namespace tfc
{ 
	namespace net
	{
		typedef in_addr_t ip_4byte_t;	//	unsigned int
		typedef uint16_t port_t;		//	unsigned short
		
		class CSocketTCP
		{
			// Construction
		public:
			CSocketTCP(): _socket_fd(INVALID_SOCKET), _close_protect(true){}
			CSocketTCP(int fd, bool close_protect):_socket_fd(fd), _close_protect(close_protect){}
			~CSocketTCP(){if (_close_protect)close();}
			int create();
			
			inline int fd() const {return _socket_fd;};
			bool socket_is_ok() const {return (_socket_fd != INVALID_SOCKET);}
			void close();

			// Operations
			int bind(const std::string& server_address, port_t port);
			int bind_if(const std::string& ifname, port_t port);	
			int bind_any(port_t port);
			int listen(bool defer_accept = true);
			int accept(CSocketTCP& client_socket);

			int connect(ip_4byte_t addr, port_t port);
			int connect(const std::string & addr, port_t port);
			
			int receive	(void * buf, size_t buf_size, size_t& received_len, int flag = 0);
			int send	(const void * buf, size_t buf_size, size_t& sent_len, int flag = 0);

			int shutdown();
			int set_nonblock(bool tcp_nodelay = true);
			int set_reuseaddr();

			void detach(){_close_protect = false;}
			void attach(int fd){if (_close_protect){close();_close_protect = true;} _socket_fd = fd;}
			
			int get_peer_name(ip_4byte_t& peer_address, port_t& peer_port);
			int get_sock_name(ip_4byte_t& socket_address, port_t& socket_port);
			
			int get_peer_name(std::string& peer_address, port_t& peer_port);
			int get_sock_name(std::string& socket_address, port_t& socket_port);
			//根据适配器名字（eth0，eth1等）获取其ip地址	
			unsigned get_ip_by_if(const char* ifname);	
		private:
			CSocketTCP(const CSocketTCP& sock); 		// no implementation
			void operator = (const CSocketTCP& sock); 	// no implementation

			int _socket_fd;
			bool _close_protect;
			static const int INVALID_SOCKET = -1;
		};
	}
}

#endif
