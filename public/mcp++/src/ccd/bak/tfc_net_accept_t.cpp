
#include "tfc_net_accept_t.h"
#include "tfc_base_str.h"

using namespace tfc::net;
using namespace tfc::base;
using namespace std;

//////////////////////////////////////////////////////////////////////////

CAcceptThread::CAcceptThread(	CConnSet& cc,
								CEPollFlow& epoll)
								: _cc(cc), _epoll(epoll)
{
	sran();
	_flow = ran(1<<31);
}

int CAcceptThread::open(const string& bind_ip, unsigned short bind_port)
{
	int ret = _socket.create();
	if (ret) return ret;
	
	_socket.set_reuseaddr();
	ret = _socket.bind(bind_ip, bind_port);
	if (ret) return ret;

	ret = _socket.listen();
	return ret;
}

void CAcceptThread::real_run()
{
	for(;;)
	{
		CSocketTCP sock;
		int ret = _socket.accept(sock);
		if (ret == 0)
		{
			sock.detach();
			sock.set_nonblock();
			_flow = ((_flow == 0 || _flow == ULONG_MAX) ? 1 : _flow+1);
			ret = _cc.AddConn(sock.fd(), _flow);
			if(ret != 0)
			{
				sock.close();
				continue;
			}

			//assert(ret == 0);
			ret = _epoll.add(sock.fd(), _flow, EPOLLET | EPOLLIN);
			assert(ret == 0);
		}
	}
}

void* CAcceptThread::run(void* instance)
{
	CAcceptThread* t = (CAcceptThread*) instance;
	t->real_run();
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
///:~
