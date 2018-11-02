#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <net/if.h>

#include "tfc_net_socket_tcp.h"

using namespace std;

//////////////////////////////////////////////////////////////////////////
namespace tfc
{
	namespace net
	{
		typedef sa_family_t family_t;	//	IF_INET

		class CSocketAddr
		{
		public:
			CSocketAddr():_len(sizeof(struct sockaddr_in))
			{memset(&_addr, 0, sizeof(struct sockaddr_in));}

			struct sockaddr * addr(){return (struct sockaddr *)(&_addr);}
			struct sockaddr_in * addr_in(){return &_addr;}
			socklen_t& length(){return _len;}

			ip_4byte_t get_numeric_ipv4(){return _addr.sin_addr.s_addr;}
			void set_numeric_ipv4(ip_4byte_t ip){_addr.sin_addr.s_addr = ip;}

			port_t get_port(){return ntohs(_addr.sin_port);}
			void set_port(port_t port){_addr.sin_port = htons(port);}

			family_t get_family(){return _addr.sin_family;}
			void set_family(family_t f){_addr.sin_family = f;}

			static string in_n2s(ip_4byte_t addr);
			static int in_s2n(const string& addr, ip_4byte_t& addr_4byte);

			private:
				struct sockaddr_in _addr;
				socklen_t _len;
		};

		string CSocketAddr::in_n2s(ip_4byte_t addr)
		{
			char buf[INET_ADDRSTRLEN];
			const char* p = inet_ntop(AF_INET, &addr, buf, sizeof(buf));
			return p ? p : string();
		}

		int CSocketAddr::in_s2n(const string& addr, ip_4byte_t& addr_4byte)
		{
			struct in_addr sinaddr;
			errno = 0;
			int ret = inet_pton(AF_INET, addr.c_str(), &sinaddr);

			if (ret < 0)
			{
				if (errno != 0)
					return 0-errno;
				else
					return ret;
			}
			else if (ret == 0)
			{
				return -1;
			}
			else
			{
				addr_4byte = sinaddr.s_addr;
				return 0;	//ret;
			}
		}

		//////////////////////////////////////////////////////////////////////////
		//
		//	return 0, on success
		//	return < 0, on -errno or unknown error
		//
		int CSocketTCP::create()
		{
			//	release fd first
			if (_close_protect)
			{
			    close();
             }

			//	create and check
			errno = 0;
			int ret =::socket(PF_INET, SOCK_STREAM, 0);
			if (ret < 0)
			{
				return errno ? -errno : ret;
			}
			else
			{
				_socket_fd = ret;
				return 0;
			}
		}

		void CSocketTCP::close()
		{
			if (_socket_fd != INVALID_SOCKET)
			{
				//::shutdown(_socket_fd, SHUT_RDWR);
				::close(_socket_fd);
				_socket_fd = INVALID_SOCKET;
			}
		}

		//
		//	return 0, on success
		//	return < 0, on -errno or unknown error
		//
		int CSocketTCP::bind(const string& server_address, port_t port)
		{
			if(isdigit(server_address[0]))
            {
				ip_4byte_t ip = 0;

				int ret = CSocketAddr::in_s2n(server_address, ip);
				if (ret < 0)
				{
				    return ret;
                }

				CSocketAddr addr;
				addr.set_family(AF_INET);
				addr.set_port(port);
				addr.set_numeric_ipv4(ip);

				errno = 0;
				ret = ::bind(_socket_fd, addr.addr(), addr.length());
				return (ret < 0) ? (errno ? -errno : ret) : 0;
			}
			else
			{
			    return bind_if(server_address, port);
            }
		}

		int CSocketTCP::bind_if(const string& ifname, port_t port)
		{
			ip_4byte_t ip = get_ip_by_if(ifname.c_str());

			CSocketAddr addr;
			addr.set_family(AF_INET);
			addr.set_port(port);
			addr.set_numeric_ipv4(ip);

			errno = 0;
			int ret = ::bind(_socket_fd, addr.addr(), addr.length());
			return (ret < 0) ? (errno ? -errno : ret) : 0;
		}

		//
		//	bind on *:port
		//	return 0, on success
		//	return < 0, on -errno or unknown error
		//
		int CSocketTCP::bind_any(port_t port)
		{
			CSocketAddr addr;
			addr.set_family(AF_INET);
			addr.set_port(port);
			addr.set_numeric_ipv4(htonl(INADDR_ANY));

			errno = 0;
			int ret = ::bind(_socket_fd, addr.addr(), addr.length());
			return (ret < 0) ? (errno ? -errno : ret) : 0;
		}

		int CSocketTCP::listen(bool defer_accept) {
            return listen(defer_accept, 1024);
        }

		//
		//	return 0, on success
		//	return < 0, on -errno or unknown error
		//
		//  modified by edenhuang, 20110215
		int CSocketTCP::listen(bool defer_accept, int backlog)
		{
			if (_socket_fd == INVALID_SOCKET)
			{
			    return -1;
            }

			int ret;
			if(defer_accept)
            {
				setsockopt(_socket_fd, SOL_TCP, TCP_DEFER_ACCEPT, (ret=10, &ret), sizeof(ret));
            }

			errno = 0;
			ret = ::listen(_socket_fd, backlog);
			return (ret < 0) ? (errno ? -errno : ret) : 0;
		}

		//
		//	accept a new connection, and attach it into the client_socket parameter
		//	return 0, on success
		//	return < 0, on -errno or unknown error
		//
		int CSocketTCP::accept(CSocketTCP& client_socket)
		{
			//client_socket.close();

			errno = 0;
			int ret =::accept(_socket_fd, NULL, NULL);

			if (ret < 0)
			{
			    return errno ? -errno : ret;
            }
			else
			{
				client_socket._socket_fd = ret;
				return 0;
			}
		}

		//
		//	return 0, on success
		//	return < 0, on -errno or unknown error
		//
		int CSocketTCP::connect(const string& address, port_t port)
		{
			ip_4byte_t ip = 0;

			int ret = CSocketAddr::in_s2n(address, ip);
			if (ret < 0)
			{
			    return ret;
            }

			return connect(ip, port);
		}

		//
		//	return 0, on success
		//	return < 0, on -errno or unknown error
		//
		int CSocketTCP::connect(ip_4byte_t address, port_t port)
		{
			CSocketAddr addr;
			addr.set_family(AF_INET);
			addr.set_port(port);
			addr.set_numeric_ipv4(address);

			errno = 0;
			int ret = ::connect(_socket_fd, addr.addr(), addr.length());
			return (ret < 0) ? (errno ? -errno : ret) : 0;
		}

		//
		//	return 0, on success
		//	return < 0, on -errno or unknown error
		//
		//	to receive data buffer: buf/buf_size
		//	received data length: received_len
		//
		int CSocketTCP::receive(void* buf, size_t buf_size, size_t& received_len, int flag)
		{
			errno = received_len = 0;

			int bytes =::recv(_socket_fd, buf, buf_size, flag);
			if(bytes < 0)
			{
				return errno ? -errno : bytes;
			}
			else
			{
				received_len = bytes;
				return 0;
			}
		}

		//
		//	return 0, on success
		//	return < 0, on -errno or unknown error
		//
		//	to be sent data buffer: buf/buf_size
		//	done data length: sent_len
		//
		int CSocketTCP::send(const void* buf, size_t buf_size, size_t& sent_len, int flag)
		{
			errno = sent_len = 0;

			int bytes = ::send(_socket_fd, buf, buf_size, flag);
			if(bytes < 0)
			{
				return errno ? -errno : bytes;
			}
			else
			{
				sent_len = bytes;
				return 0;
			}
		}

		int CSocketTCP::shutdown()
		{
			int ret = ::shutdown(_socket_fd, SHUT_RDWR);
			return (ret < 0) ? (errno ? -errno : ret) : 0;
		}

		int CSocketTCP::get_peer_name(ip_4byte_t& peer_address, port_t& peer_port)
		{
			CSocketAddr addr;

			int ret = ::getpeername(_socket_fd, addr.addr(), &addr.length());
			if (ret < 0)
			{
			    return errno ? -errno : ret;
            }

			peer_address = addr.get_numeric_ipv4();
			peer_port    = addr.get_port();

			return 0;
		}

		int CSocketTCP::get_peer_name(string& peer_address, port_t& peer_port)
		{
			ip_4byte_t ip = 0;

			int ret = get_peer_name(ip, peer_port);
			if (ret < 0)
			{
			    return ret;
            }

			peer_address = CSocketAddr::in_n2s(ip);

			return 0;
		}

		int CSocketTCP::get_sock_name(ip_4byte_t& socket_address, port_t& socket_port)
		{
			CSocketAddr addr;

			int ret = ::getsockname(_socket_fd, addr.addr(), &addr.length());
			if (ret < 0)
			{
			    return errno ? -errno : ret;
            }

			socket_address = addr.get_numeric_ipv4();
			socket_port    = addr.get_port();

			return 0;
		}

		int CSocketTCP::get_sock_name(string& socket_address, port_t& socket_port)
		{
			ip_4byte_t ip = 0;

			int ret = get_sock_name(ip, socket_port);
			if (ret < 0)
			{
			    return ret;
            }

			socket_address = CSocketAddr::in_n2s(ip);

			return 0;
		}

		int CSocketTCP::set_reuseaddr()
		{
			int    optval = 1;
			size_t optlen = sizeof(optval);

			int ret = setsockopt(_socket_fd, SOL_SOCKET, SO_REUSEADDR, &optval, optlen);
			return (ret < 0) ? (errno ? -errno : ret) : 0;
		}

		int CSocketTCP::set_reuseport()
		{
			int    optval = 1;
			size_t optlen = sizeof(optval);
            int ret = 0;

#ifndef SO_REUSEPORT
#define SO_REUSEPORT 15
#endif
			ret = setsockopt(_socket_fd, SOL_SOCKET, SO_REUSEPORT, &optval, optlen);
			return (ret < 0) ? (errno ? -errno : ret) : 0;
		}

		int CSocketTCP::set_nonblock(bool tcp_nodelay)
		{
			int ret = 1;

			if(tcp_nodelay)
			{
			    setsockopt(_socket_fd, SOL_TCP, TCP_NODELAY, &ret, sizeof(ret));
            }

            ret = fcntl(_socket_fd, F_SETFL, O_RDWR | O_NONBLOCK);
			return (ret < 0) ? (errno ? -errno : ret) : 0;
		}

		unsigned CSocketTCP::get_ip_by_if(const char* ifname)
        {
            int fd = socket(AF_INET, SOCK_DGRAM, 0);
            if( fd < 0)
            {
                return 0;
            }

            struct ifreq  buf[10];
            struct ifconf ifc;
			ifc.ifc_len = sizeof(buf);
			ifc.ifc_buf = (caddr_t)buf;

            unsigned ip = 0;
			if(!ioctl(fd, SIOCGIFCONF, (char*)&ifc))
            {
				register int intrface = ifc.ifc_len / sizeof(struct ifreq);

				while(intrface-- > 0)
                {
					if(0 != strcmp(buf[intrface].ifr_name, ifname))
                    {
                        continue;
                    }

                    if(!(ioctl(fd, SIOCGIFADDR, (char *)&buf[intrface])))
                    {
						ip = (unsigned)((struct sockaddr_in *)(&buf[intrface].ifr_addr))->sin_addr.s_addr;
			    	}
					break;
				}
			}

            ::close(fd);

			return ip;
		}
	}
}
//中文测试
//////////////////////////////////////////////////////////////////////////
