
#ifndef _TFC_SIMPLE_SOCK_HPP_
#define _TFC_SIMPLE_SOCK_HPP_

#include "tfc_sock.hpp"

//////////////////////////////////////////////////////////////////////////

namespace tfc{namespace net{

class CSimpleSocketTcp : public CSocket
{
public:
	// Construction
	CSimpleSocketTcp(){}
	~CSimpleSocketTcp(){}
	
	void create(){CSocket::create(PF_INET, SOCK_STREAM);}
	
	// Operations
	void bind   (const std::string& addr, port_t  port);
	void connect(const std::string& addr, port_t  port);
	
	//	recv & send existed, use CSocket version
	
	// Attributes, gettor
	std::string get_peer_ip();
	std::string get_sock_ip();
	port_t get_peer_port();
	port_t get_sock_port();
};

class CSimpleSocketUdp : public CSocket
{
public:
	CSimpleSocketUdp(){}
	~CSimpleSocketUdp(){}
	
	void create(){CSocket::create(PF_INET, SOCK_DGRAM);}
	void bind   (const std::string& addr, port_t  port);
	
	int receive_from (void* buf, size_t len,
		std::string& from_address, port_t& from_port, int flags=0);
	int send_to(const void* msg, size_t len,
		const std::string& to_address, port_t to_port, int flags=0);
};

//	unix socket?

static std::string in_n2s(numeric_ipv4_t addr);
static numeric_ipv4_t in_s2n(const std::string& addr);

//////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////

inline void CSimpleSocketTcp::bind(const std::string& addr, port_t port)
{
	numeric_ipv4_t ip = (addr.empty() || addr== "*") ?
		htonl(INADDR_ANY) : in_s2n(addr);
	CSocket::bind(ip, port);
}

inline void CSimpleSocketTcp::connect(const std::string& addr, port_t port)
{
	CSocket::connect(in_s2n(addr), port);
}

//////////////////////////////////////////////////////////////////////////

inline std::string CSimpleSocketTcp::get_peer_ip()
{
	numeric_ipv4_t ip;
	port_t port;
	get_peer_name(ip, port);
	return in_n2s(ip);
}

inline std::string CSimpleSocketTcp::get_sock_ip()
{
	numeric_ipv4_t ip;
	port_t port;
	get_sock_name(ip, port);
	return in_n2s(ip);
}

inline port_t CSimpleSocketTcp::get_peer_port()
{
	numeric_ipv4_t ip;
	port_t port;
	get_peer_name(ip, port);
	return port;
}

inline port_t CSimpleSocketTcp::get_sock_port()
{
	numeric_ipv4_t ip;
	port_t port;
	get_sock_name(ip, port);
	return port;
}

//////////////////////////////////////////////////////////////////////////

inline void CSimpleSocketUdp::bind(const std::string& addr, port_t port)
{
	numeric_ipv4_t ip = (addr.empty() || addr== "*") ?
		htonl(INADDR_ANY) : in_s2n(addr);
	CSocket::bind(ip, port);
}

inline int CSimpleSocketUdp::receive_from(void * buf, size_t len,
										  std::string& addr, port_t& port, int flags/* = 0 */)
{
	numeric_ipv4_t ip;
	int ret = CSocket::receive_from(buf, len, ip, port, flags);
	addr = in_n2s(ip);
	return ret;
}

inline int CSimpleSocketUdp::send_to(const void *msg, size_t len,
									 const std::string& addr, port_t port, int flags /* = 0 */)
{
	return CSocket::send_to(msg, len, in_s2n(addr), port, flags);
}

//////////////////////////////////////////////////////////////////////////

inline std::string in_n2s(numeric_ipv4_t addr)
{
	char buf[INET_ADDRSTRLEN];
	const char* p = inet_ntop(AF_INET, &addr, buf, sizeof(buf));
	return p ? p : std::string();
}

inline numeric_ipv4_t in_s2n(const std::string& addr)
{
	struct in_addr sinaddr;
	errno = 0;
	int ret = inet_pton(AF_INET, addr.c_str(), &sinaddr);
	
	if (ret < 0)
		throw socket_error("inet_pton: " + addr + strerror(errno));
	else if (ret == 0)
		throw socket_error(std::string("CSocketAddr::in_s2n: inet_pton: "
		"does not contain a character string representing a valid "
		"network address in: ") + addr);
	
	return sinaddr.s_addr;
}

}}	//	namespace tfc::net

//////////////////////////////////////////////////////////////////////////
#endif//_TFC_SIMPLE_SOCK_HPP_
///:~
