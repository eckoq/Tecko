
#ifndef _TFC_NETWORK_CONN_CONNECTOR_HPP_
#define _TFC_NETWORK_CONN_CONNECTOR_HPP_

#include <string>
#include <assert.h>

#include "tfc_driver.hpp"
#include "tfc_network.hpp"
#include "tfc_network_app.hpp"
#include "tfc_simple_sock.hpp"
#include "conn_reactor.hpp"

namespace tfc{namespace network{

//////////////////////////////////////////////////////////////////////////

class ConnectorImp : public Connector
{
public:
	ConnectorImp(ptr<ConnAllocator> allocator) : Connector(allocator){}
	virtual ~ConnectorImp(){}
	
	virtual bool SetSAP(unsigned short iPort, const std::string& sHost)
	{
		_port = iPort;
		_host = sHost;
		return true;
	}
	
	virtual ptr<Connection> AllocateConn()
	{
		ptr<Connection> conn = _allocator->AllocateConn();
		net::CSimpleSocketTcp tmp;
		tmp.create();
		tmp.connect(_host, _port);
		conn->AttachFD(tmp.fd());
		tmp.attach(-1);
		return conn;
	}

protected:
	unsigned short _port;
	std::string _host;
};

//////////////////////////////////////////////////////////////////////////
}}	//	namespace tfc::network
#endif//_TFC_NETWORK_CONN_CONNECTOR_HPP_
///:~
