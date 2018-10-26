
#ifndef _TFC_DRIVER_EPOLL_HPP_
#define _TFC_DRIVER_EPOLL_HPP_

#include <map>

#include "tfc_epoll.hpp"
#include "tfc_driver.hpp"

//////////////////////////////////////////////////////////////////////////

namespace tfc{namespace net{	//	namespace

typedef int fd_t;

class CDriverEpoll : public CDriver<fd_t>
{
public:
	CDriverEpoll(int iMaxFD);
	virtual ~CDriverEpoll(){}
	
	virtual void drive();
	virtual void add(ptr< CReactor<fd_t> > reactor);
	virtual void del(fd_t fd);
	
protected:
	static int epoll_to_react(int epoll_mask);
	static int react_to_epoll(int react_mask);
	
	CEPoller _epoller;
	std::map< fd_t, ptr< CReactor<fd_t> > > _relation;
	
	static const int C_EPOLL_WAIT_TIMEOUT = 200;
	typedef  std::map<fd_t, ptr< CReactor<fd_t> > >::iterator map_it;
};

//////////////////////////////////////////////////////////////////////////
//	implementations
//////////////////////////////////////////////////////////////////////////

inline CDriverEpoll::CDriverEpoll(int iMaxFD)
{
	_epoller.create(iMaxFD);
}

inline void CDriverEpoll::add(ptr< CReactor<fd_t> > reactor)
{
	_relation.insert(std::map< fd_t, ptr< CReactor<fd_t> > >::value_type(reactor->key(), reactor));
	_epoller.add(reactor->key());
}

inline void CDriverEpoll::del(fd_t fd)
{
	map_it it = _relation.find(fd);
	if (it != _relation.end())
		_relation.erase(it);
}

inline void CDriverEpoll::drive()
{
	try
	{
		CEPollResult result = _epoller.wait(C_EPOLL_WAIT_TIMEOUT);
		for(CEPollResult::iterator it = result.begin(); it != result.end(); it++)
		{
			fd_t fd = it->data.fd;
			map_it im = _relation.find(fd);
			
			if (im == _relation.end())
			{
				_epoller.del(fd);	//	??
				continue;
			}
			
			ENUM_REACT_RESULT ret = im->second->react(epoll_to_react(it->events));
			if (ret == REACT_REMOVE)
			{
				_relation.erase(im);
			}
			else if (ret == REACT_RESTART)
			{
				ptr< CReactor<fd_t> > reactor = im->second;
				_relation.erase(im);
				_relation.insert(std::map< fd_t, ptr< CReactor<fd_t> > >::value_type(reactor->key(), reactor));
				_epoller.del(reactor->key());
				_epoller.add(reactor->key(), reactor->reactable());
			}
			else
			{
				continue;
			}
		}
	}
	catch (std::exception& ex)
	{
		//	??
	}
}

//////////////////////////////////////////////////////////////////////////

inline int CDriverEpoll::epoll_to_react(int epoll_mask)
{
	int react_mask = 0;
	if (epoll_mask & EPOLLIN)
		react_mask |= MASK_READ;
	if (epoll_mask & EPOLLOUT)
		react_mask |= MASK_WRITE;
	if (epoll_mask & EPOLLERR)
		react_mask |= MASK_EXCEPT;
	if (epoll_mask & EPOLLHUP)
		react_mask |= MASK_HUNGUP;
	if (epoll_mask & EPOLLPRI)
		react_mask |= MASK_PRIORITY;
	return react_mask;
}

inline int CDriverEpoll::react_to_epoll(int react_mask)
{
	int epoll_mask = 0;
	if (react_mask & MASK_READ)
		epoll_mask |= EPOLLIN;
	if (react_mask & MASK_WRITE)
		epoll_mask |= EPOLLOUT;
	if (react_mask & MASK_EXCEPT)
		epoll_mask |= EPOLLERR;
	if (react_mask & MASK_HUNGUP)
		epoll_mask |= EPOLLHUP;
	if (react_mask & MASK_PRIORITY)
		epoll_mask |= EPOLLPRI;
	return react_mask;
}

}}	//	namespace tfc::net

//////////////////////////////////////////////////////////////////////////
#endif//_TFC_DRIVER_EPOLL_HPP_
///:~
