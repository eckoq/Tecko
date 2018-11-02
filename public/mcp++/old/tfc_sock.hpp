
#ifndef _TFC_SOCK_HPP_
#define _TFC_SOCK_HPP_

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <cstdio>
#include <string>
#include <iostream>

#include "tfc_ex.hpp"
using namespace std;
namespace tfc{namespace net{
	
//////////////////////////////////////////////////////////////////////////
//	socket operation wrapper, united exception handling
//////////////////////////////////////////////////////////////////////////

class socket_error; class socket_nil; class CSocketAddr;

typedef in_addr_t numeric_ipv4_t;
typedef sa_family_t family_t;
typedef uint16_t port_t;

class CSocket
{
public:
	// Construction
	CSocket():_socket_fd(INVALID_SOCKET){}
	~CSocket(){close();}
	void create(int protocol_family = PF_INET, int socket_type = SOCK_STREAM);
	void attach(int fd){_socket_fd = fd;}
	
public:
	// Attributes, gettor & settor
	int fd() const {return _socket_fd;}
	bool socket_is_ok() const {return (_socket_fd != INVALID_SOCKET);}
	
	void get_peer_name(numeric_ipv4_t& peer_address, port_t& peer_port);
	void get_sock_name(numeric_ipv4_t& socket_address, port_t& socket_port);
	
public:
	// Operations
	void bind(port_t port);
	void bind(numeric_ipv4_t addr, port_t port);
	void listen(int backlog = 32);
	void connect(numeric_ipv4_t addr, port_t port);
	void accept(CSocket& client_socket);
	void shutdown(int how = SHUT_RDWR);
	void close();
	
	int receive	  (void * buf, size_t len, int flag = 0);
	int send(const void * buf, size_t len, int flag = 0);
	
	int receive_from (void* buf, size_t len,
		numeric_ipv4_t& from_address, port_t& from_port, int flags = 0);
	int send_to(const void* msg, size_t len,
		numeric_ipv4_t to_address, port_t to_port, int flags = 0);
	
	void set_nonblock();
	int  get_sockerror();
	void set_reuseaddr();
	static void ignore_pipe_signal();
	
protected:
	static const int INVALID_SOCKET = -1;
	int _socket_fd;
	
private:
	CSocket(const CSocket& sock); // no implementation
	CSocket& operator=(const CSocket& sock); // no implementation
};

//////////////////////////////////////////////////////////////////////////

class CSocketAddr
{
public:
	CSocketAddr():_len(sizeof(struct sockaddr_in))
	{memset(&_addr, 0, sizeof(struct sockaddr_in));}
	
	struct sockaddr * addr(){return (struct sockaddr *)(&_addr);}
	struct sockaddr_in * addr_in(){return &_addr;}
	socklen_t& length(){return _len;}
	
	numeric_ipv4_t get_numeric_ipv4(){return _addr.sin_addr.s_addr;}
	void set_numeric_ipv4(numeric_ipv4_t ip){_addr.sin_addr.s_addr = ip;}
	
	port_t get_port(){return ntohs(_addr.sin_port);}
	void set_port(port_t port){_addr.sin_port = htons(port);}
	
	family_t get_family(){return _addr.sin_family;}
	void set_family(family_t f){_addr.sin_family = f;}
	
	private:
		struct sockaddr_in _addr;
		socklen_t _len;
};

class socket_error: public bt_ex
{public:socket_error(const std::string& err_str) : bt_ex(err_str){}};

class socket_again: public socket_error
{public:socket_again(const std::string& err_str):socket_error(err_str){}};

class socket_intr : public socket_error
{public:socket_intr(const std::string& err_str):socket_error(err_str){}};

//////////////////////////////////////////////////////////////////////////
//	implementation
//////////////////////////////////////////////////////////////////////////

#define THROW_SOEXP(exp, err_str) (throw exp(__FILE__, __LINE__, errno, err_str))
#define THROW_SOEXP_SIMP(function) (THROW_SOEXP(socket_error, function + string(" ") + strerror(errno)))

//////////////////////////////////////////////////////////////////////////

inline void CSocket::create(int protocol_family /* = PF_INET */, int socket_type /* = SOCK_STREAM */)
{
	//	release fd first
	close();
	
	//	create and check
	_socket_fd =::socket(protocol_family, socket_type, 0);
	if (_socket_fd < 0)
	{
		throw socket_error(strerror(errno));
		_socket_fd = INVALID_SOCKET;
	}
}

inline void CSocket::bind(port_t port)
{
	bind(htonl(INADDR_ANY), port);
}

inline void CSocket::bind(numeric_ipv4_t ip, port_t port)
{
	CSocketAddr addr;
	addr.set_family(AF_INET);
	addr.set_port(port);
	addr.set_numeric_ipv4(ip);
	
	errno = 0;
	if (::bind(_socket_fd, addr.addr(), addr.length()) < 0)
		throw socket_error(strerror(errno));
}

inline void CSocket::listen(int backlog /*=32*/)
{
	if (::listen(_socket_fd, backlog) < 0)
		throw socket_error(strerror(errno));
}

inline void CSocket::accept(CSocket & client_socket)
{
	client_socket.close();
	int client_fd = INVALID_SOCKET;
	errno = 0;
	CSocketAddr addr;
	client_fd =::accept(_socket_fd, addr.addr(), &addr.length());
	if (client_fd < 0)	//	INVALID_SOCKET < 0
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			throw socket_again(strerror(errno));
		else if (errno == EINTR)
			throw socket_intr (strerror(errno));
		else
			throw socket_error(strerror(errno));
	}
	client_socket._socket_fd = client_fd;
}

inline void CSocket::connect(numeric_ipv4_t addr, port_t port)
{
	CSocketAddr add;
	add.set_family(AF_INET);
	add.set_port(port);
	add.set_numeric_ipv4(addr);
	errno = 0;
	if (::connect(_socket_fd, add.addr(), add.length()) < 0)
		throw socket_error(std::string("CSocket::connect") + strerror(errno));
}

inline void CSocket::shutdown(int how /*=SHUT_WR*/)
{
	if (::shutdown(_socket_fd, how) < 0)
		throw socket_error(strerror(errno));
}

inline void CSocket::close()
{
	if (_socket_fd != INVALID_SOCKET)
	{
		::shutdown(_socket_fd, SHUT_RDWR);
		::close(_socket_fd);
		_socket_fd = INVALID_SOCKET;
	}
}

//////////////////////////////////////////////////////////////////////////

inline int CSocket::receive(void *buf, size_t len, int flag /*=0*/)
{
	errno = 0;
	int bytes =::recv(_socket_fd, buf, len, flag);
	if (bytes < 0)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			throw socket_again(strerror(errno));
		else if (errno == EINTR)
			throw socket_intr (strerror(errno));
		else
			throw socket_error(strerror(errno));
	}
	return bytes;
}

inline int CSocket::send(const void *buf, size_t len, int flag /*=0*/)
{
	errno = 0;
	int bytes = ::send(_socket_fd, buf, len, flag);
	if (bytes < 0)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			throw socket_again(strerror(errno));
		else if (errno == EINTR)
			throw socket_intr (strerror(errno));
		else
			throw socket_error(strerror(errno));
	}
	return bytes;
}

inline int CSocket::receive_from(void * buf, size_t len, numeric_ipv4_t& from_address,
								 port_t& from_port, int flags /*=0*/)
{
	CSocketAddr addr;
	errno = 0;
	int bytes =::recvfrom(_socket_fd, buf, len, flags, addr.addr(), &addr.length());
	if (bytes < 0)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			throw socket_again(strerror(errno));
		else if (errno == EINTR)
			throw socket_intr (strerror(errno));
		else
			throw socket_error(strerror(errno));
	}
	from_address = addr.get_numeric_ipv4();
	from_port = addr.get_port();
	return bytes;
}

inline int CSocket::send_to(const void *msg, size_t len,
							numeric_ipv4_t to_address, port_t to_port, int flags /*=0*/)
{
	CSocketAddr addr;
	addr.set_family(AF_INET);
	addr.set_numeric_ipv4(to_address);
	addr.set_port(to_port);
	errno = 0;
	int bytes =::sendto(_socket_fd, msg, len, flags, addr.addr(), addr.length());
	if (bytes < 0)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			throw socket_again(strerror(errno));
		else if (errno == EINTR)
			throw socket_intr (strerror(errno));
		else
			throw socket_error(strerror(errno));
	}
	return bytes;
}

//////////////////////////////////////////////////////////////////////////

inline void CSocket::get_peer_name(numeric_ipv4_t & peer_address, port_t & peer_port)
{
	CSocketAddr addr;
	if (::getpeername(_socket_fd, addr.addr(), &addr.length()) < 0)
		throw socket_error(strerror(errno));
	peer_address = addr.get_numeric_ipv4();
	peer_port = addr.get_port();
}

inline void CSocket::get_sock_name(numeric_ipv4_t& socket_address, port_t & socket_port)
{
	CSocketAddr addr;
	if (::getsockname(_socket_fd, addr.addr(), &addr.length()) < 0)
		throw socket_error(strerror(errno));
	socket_address = addr.get_numeric_ipv4();
	socket_port = addr.get_port();
}

inline void CSocket::set_reuseaddr()
{
	int optval = 1;
	size_t optlen = sizeof(optval);
	if (::setsockopt(_socket_fd, SOL_SOCKET, SO_REUSEADDR, &optval, optlen) < 0)
		throw socket_error(strerror(errno));
}

inline int CSocket::get_sockerror()
{
	int error = 0;
	size_t optlen = sizeof(error);
	if (::getsockopt(_socket_fd, SOL_SOCKET, SO_ERROR, &error, &optlen) < 0)
		throw socket_error(strerror(errno));
	return error;
}

inline void CSocket::set_nonblock()
{
	int val = fcntl(_socket_fd, F_GETFL, 0);
	if (val == -1)
		throw socket_error(strerror(errno));
	if (val & O_NONBLOCK)
		return;
	
	if (fcntl(_socket_fd, F_SETFL, val | O_NONBLOCK | O_NDELAY) == -1)
		throw socket_error(strerror(errno));
}

inline void CSocket::ignore_pipe_signal()
{
	signal(SIGPIPE, SIG_IGN);
}

//////////////////////////////////////////////////////////////////////////

class big_fd_set
{
public:
	big_fd_set(){FD_0();}
	~big_fd_set(){}
	fd_set* get_fd_set(){return (fd_set*)&_guardian;}
	void FD_0(){memset(_guardian, 0, C_GUARD_SIZE);}
	void FD_UNSET(int fd)	//	efficient for few, than SET_ZERO()
	{
		long int* offset = &((__fd_mask*)_guardian)[__FDELT (fd)];
		int mask_opposit = int(1) << (fd % __NFDBITS);
		unsigned mask = 0xFFFFFFFF - *(unsigned*)&mask_opposit;
		*offset &= mask;
	}
	
protected:
	static const size_t C_GUARD_SIZE = 15000;	//	120000 fd protected, enough?
	char _guardian[C_GUARD_SIZE];
};


//////////////////////////////////////////////////////////////////////////
}}	//	namespace tfc::net

#endif//_TFC_SOCK_H_
///:~
