
#ifndef _CONN_REACTOR_HPP_
#define _CONN_REACTOR_HPP_

#include "tfc_driver.hpp"
#include "tfc_network.hpp"
#include "tfc_network_app.hpp"

namespace tfc{namespace network{

//////////////////////////////////////////////////////////////////////////

typedef int fd_t;

class ConnReactor : public CReactor<fd_t>
{
public:
	ConnReactor(ptr<Connection> conn, ptr<CApp> app)
		: _conn(conn), _app(app){}
	virtual ~ConnReactor(){}
	
	virtual fd_t key(){return _conn->FD();}
	
	virtual int reactable()
	{
		if (_conn->SendAvailable())
			return MASK_READ | MASK_WRITE;
		else
			return MASK_READ;
	}
	
	virtual ENUM_REACT_RESULT react(int mask)
	{
		//	first, if readable
		if (mask & MASK_READ)
			if (!_app->AccessService(_conn))
				return REACT_REMOVE;

		//	second, if writable
		if ((mask & MASK_WRITE) && (_conn->SendAvailable()))
			_conn->SendCache();

		//	third, if not readable neither writable
		if (!(mask & MASK_READ) && !(mask & MASK_WRITE)) 
			return REACT_REMOVE;

		return REACT_OK;
	}
	
protected:
	ptr<Connection> _conn;
	ptr<CApp> _app;
};

//////////////////////////////////////////////////////////////////////////
}}	//	namespce tfc::network
#endif//_CONN_REACTOR_HPP_
//:~
