/*
 * tfc_net_socket_udp.cpp:	Encapsulation of the interface of UDP transport layer.
 * Date:					2012-05-21
 */

#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include "tfc_net_socket_udp.h"

using namespace std;

namespace tfc { namespace net {
	//
	//	return 0, on success
	//	return < 0, on -errno or unknown error
	//
	int CSocketUDP::create()
	{
		//	release fd first
		if (_close_protect)
			close();

		//	create and check
		errno = 0;
		int ret = ::socket(PF_INET, SOCK_DGRAM, 0);
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

	void CSocketUDP::close()
	{
		if (_socket_fd != INVALID_SOCKET)
		{
			::close(_socket_fd);
			_socket_fd = INVALID_SOCKET;
		}
	}
	
	//
	//	return 0, on success
	//	return < 0, on -errno or unknown error
	//
	int CSocketUDP::bind(const string &server_address, unsigned short port)
	{
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);

        if (server_address.empty()) {
            // add by kendywang, 2016/01/22
            // for client udp, bind local port only
            addr.sin_addr.s_addr = INADDR_ANY;

            errno = 0;
			int ret = ::bind(_socket_fd, (struct sockaddr *)(&addr), (socklen_t)(sizeof(struct sockaddr_in)));
			return (ret < 0) ? (errno ? -errno : ret) : 0;
        } else if(isdigit(server_address[0])) {
			in_addr_t ip = 0;
			int ret = in_s2n(server_address, ip);
			if (ret < 0)
				return ret;

			addr.sin_addr.s_addr = ip;

			errno = 0;
			ret = ::bind(_socket_fd, (struct sockaddr *)(&addr), (socklen_t)(sizeof(struct sockaddr_in)));
			return (ret < 0) ? (errno ? -errno : ret) : 0;
        } else {
			return bind_if(server_address, port);
        }
	}
	int CSocketUDP::bind_if(const string &ifname, unsigned short port)
	{
		in_addr_t ip = get_ip_by_if(ifname.c_str());
		struct sockaddr_in addr;
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		addr.sin_addr.s_addr = ip;
		
		errno = 0;
		int ret = ::bind(_socket_fd, (struct sockaddr *)(&addr), (socklen_t)(sizeof(struct sockaddr_in)));
		return (ret < 0) ? (errno ? -errno : ret) : 0;
	}

	//
	//	return 0, on success
	//	return < 0, on -errno or unknown error
	//
	//	to receive data buffer: buf/buf_size
	//	received data length: received_len
	//
	int CSocketUDP::recvfrom(void *buf, size_t buf_size, size_t &received_len,
								int flag, struct sockaddr &from, socklen_t &fromlen)
	{
		errno = received_len = 0;
		int bytes = ::recvfrom(_socket_fd, buf, buf_size, flag, &from, &fromlen);
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
	int CSocketUDP::sendto(const void *buf, size_t buf_size, size_t &sent_len,
							int flag, const struct sockaddr &to, socklen_t tolen)
	{
		errno = 0;
		int bytes = ::sendto(_socket_fd, buf, buf_size, flag, &to, tolen);
		
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

	int CSocketUDP::set_nonblock()
	{
		int ret = 1;
		ret = fcntl(_socket_fd, F_SETFL, O_RDWR | O_NONBLOCK);
		return (ret < 0) ? (errno ? -errno : ret) : 0;
	}

	int CSocketUDP::set_reuseaddr()
	{
		int optval = 1;
		size_t optlen = sizeof(optval);
		int ret = setsockopt(_socket_fd, SOL_SOCKET, SO_REUSEADDR, &optval, optlen);
		return (ret < 0) ? (errno ? -errno : ret) : 0;
	}

	int CSocketUDP::set_reuseport()
	{
		int optval = 1;
		size_t optlen = sizeof(optval);
#ifndef SO_REUSEPORT
#define SO_REUSEPORT 15
#endif
		int ret = setsockopt(_socket_fd, SOL_SOCKET, SO_REUSEPORT, &optval, optlen);
		return (ret < 0) ? (errno ? -errno : ret) : 0;
	}

	int CSocketUDP::in_s2n(const std::string& addr, in_addr_t &addr_4byte)
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
	
	unsigned CSocketUDP::get_ip_by_if(const char* ifname) {
		register int fd, intrface;
		struct ifreq buf[10];
		struct ifconf ifc;
		unsigned ip = 0;  

		if((fd = socket(AF_INET, SOCK_DGRAM, 0)) >= 0) {   
			ifc.ifc_len = sizeof(buf);
			ifc.ifc_buf = (caddr_t)buf;
			if(!ioctl(fd, SIOCGIFCONF, (char*)&ifc)) {    
				intrface = ifc.ifc_len / sizeof(struct ifreq); 
				while(intrface-- > 0)  {    
					if(strcmp(buf[intrface].ifr_name, ifname) == 0) {    
						if(!(ioctl(fd, SIOCGIFADDR, (char *)&buf[intrface]))) {
							ip = (unsigned)((struct sockaddr_in *)(&buf[intrface].ifr_addr))->sin_addr.s_addr;
						}
						break;  
					}    
				}    
			}    
			::close(fd);
		}
		return ip;  
	}
} }

