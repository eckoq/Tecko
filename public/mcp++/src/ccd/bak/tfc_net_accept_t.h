
#ifndef _TFC_NET_ACCEPT_H_
#define _TFC_NET_ACCEPT_H_

#include <string>
#include "tfc_net_cconn.h"
#include "tfc_net_epoll_flow.h"

//////////////////////////////////////////////////////////////////////////
//class CHandleConnectImpl;
namespace tfc { namespace net {

	class CAcceptThread
	{
	public:
		static void* run(void* instance);
		
		CAcceptThread(	CConnSet& cc,
						CEPollFlow& epoll);

		int open(const std::string& bind_ip, unsigned short bind_port);
		void real_run();

		//CHandleConnect* _handle;
	private:
		CSocketTCP _socket;
		CConnSet& _cc;
		CEPollFlow& _epoll;
		unsigned _flow;
	};
}}

//////////////////////////////////////////////////////////////////////////
#endif//_TFC_NET_ACCEPT_H_
///:~
