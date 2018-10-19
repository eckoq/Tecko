
#ifndef _TFC_NETWORK_APP_HPP_
#define _TFC_NETWORK_APP_HPP_

#include "tfc_network.hpp"

namespace tfc{namespace network{

//////////////////////////////////////////////////////////////////////////

class CApp
{
public:
	virtual ~CApp(){}
	virtual bool AccessService(ptr<Connection> conn) = 0;
};

class CAppFactory
{
public:
	virtual ~CAppFactory(){}
	virtual ptr<CApp> CreateApp() = 0;
};

//////////////////////////////////////////////////////////////////////////
}}	//	namespace tfc::network

#endif//_TFC_NETWORK_APP_HPP_
///:~
