	
#ifndef _CONN_ACCEPTOR_HPP_
#define _CONN_ACCEPTOR_HPP_

#include <string>
#include <assert.h>

#include "tfc_driver.hpp"
#include "tfc_network.hpp"
#include "tfc_network_app.hpp"
#include "tfc_simple_sock.hpp"
#include "conn_reactor.hpp"

namespace tfc{namespace network{

//////////////////////////////////////////////////////////////////////////

class CAcceptorImp : public CAcceptor
{
public:
	CAcceptorImp(ptr<ConnAllocator> allocator) : CAcceptor(allocator){}
	virtual ~CAcceptorImp(){}

	virtual bool SetSAP(unsigned short iPort, const std::string& sHost)
	{
		_sock.create();
		_sock.set_reuseaddr();
		_sock.bind(sHost, iPort);
		_sock.listen();
		return true;
	}

	virtual ptr<Connection> AllocateConn()
	{
		ptr<Connection> conn = _allocator->AllocateConn();
		net::CSocket tmp;
		_sock.accept(tmp);
		conn->AttachFD(tmp.fd());
		tmp.attach(-1);
		return conn;
	}

	int fd(){return _sock.fd();}

protected:
	net::CSimpleSocketTcp _sock;
};

class ConnAcceptor : public CReactor<int>
{
public:
	ConnAcceptor(ptr<CAppFactory> app_factory,
		ptr< CDriver<int> > driver,
		ptr<CAcceptorImp> acceptor)
		:_app_factory(app_factory),
		_driver(driver),
		_acceptor(acceptor){}
	virtual ~ConnAcceptor(){}
	virtual int key(){return _acceptor->fd();}
	virtual ENUM_REACT_RESULT react(int action_mask)
	{
		if (action_mask & MASK_READ)
		{
			ptr<Connection> conn = _acceptor->AllocateConn();
			ptr<CApp> app = _app_factory->CreateApp();
			ptr< CReactor<int> > reactor = new ConnReactor(conn, app);
			_driver->add(reactor);
		}
		else
		{
			assert(false);
		}
		return REACT_OK;
	}

	virtual int reactable(){return MASK_READ;}

protected:
	ptr<CAppFactory> _app_factory;
	ptr< CDriver<int> > _driver;
	ptr<CAcceptorImp> _acceptor;
};

//////////////////////////////////////////////////////////////////////////
}}	//	namespace tfc::network
#endif//_CONN_ACCEPTOR_HPP_
///:~
