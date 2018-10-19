
#ifndef _TFC_DRIVER_SELECT_HPP_
#define _TFC_DRIVER_SELECT_HPP_

#include <deque>

#include "tfc_driver.hpp"

//////////////////////////////////////////////////////////////////////////

namespace tfc{namespace net{	//	namespace

typedef int fd_t;

class CDriverSelect : public CDriver<fd_t>
{
public:
	CDriverSelect(){}
	virtual ~CDriverSelect(){}
	
	virtual void drive();
	virtual void add(ptr< CReactor<fd_t> > reactor);
	virtual void del(fd_t fd);

protected:
	std::deque< ptr< CReactor<fd_t> > > _reactors;
	
	static const int C_SELECT_WAIT_TIMEOUT	= 200000;
	static const int C_SELECT_MAX_FD		= 1024;
	typedef std::deque< ptr< CReactor<fd_t> > >::iterator deque_it;
};

inline int SelectSingleRead(fd_t fd, float timeout, fd_set* r_set)
{
	assert(timeout >= 0);
	if (fd > 1024)
		return -1;
	
	//	fd_set prepare
	FD_SET(fd, r_set);
	
	//	time prepare
	struct timeval tv;
	tv.tv_sec = (int) timeout;
	tv.tv_usec = (int)((timeout - (int)timeout) * 1000000);
	
	//	select
	return select (fd+1, r_set, NULL, NULL, &tv);
}

inline int SelectSingleRead(fd_t fd, float timeout)
{
	fd_set r_set;
	FD_ZERO(&r_set);
	return SelectSingleRead(fd, timeout, &r_set);
}

inline int SelectSingleWrite(fd_t fd, float timeout, fd_set* w_set)
{
	assert(timeout >= 0);
	if (fd > 1024)
		return -1;
	
	//	fd_set prepare
	FD_SET(fd, w_set);
	
	//	time prepare
	struct timeval tv;
	tv.tv_sec = (int) timeout;
	tv.tv_usec = (int)((timeout - (int)timeout) * 1000000);
	
	//	select
	return select (fd+1, NULL, w_set, NULL, &tv);
}

inline int SelectSingleWrite(fd_t fd, float timeout)
{
	fd_set w_set;
	FD_ZERO(&w_set);
	return SelectSingleWrite(fd, timeout, &w_set);
}


//////////////////////////////////////////////////////////////////////////
//	implementations
//////////////////////////////////////////////////////////////////////////

inline void CDriverSelect::add(ptr< CReactor<fd_t> > reactor)
{
	fd_t fd = reactor->key();
	if (fd > C_SELECT_MAX_FD)
		throw bt_ex("select fd too big");
	_reactors.push_back(reactor);
}

inline void CDriverSelect::del(fd_t fd)
{
	for(deque_it it = _reactors.begin(); it != _reactors.end(); it++)
	{
		if ((*it)->key() == fd)
		{
			_reactors.erase(it);
			break;
		}
	}
}

inline void CDriverSelect::drive()
{
	//	fd_set prepare
	fd_set r_set;
	fd_set w_set;
	fd_set ex_set;
	
	FD_ZERO(&r_set);
	FD_ZERO(&w_set);
	FD_ZERO(&ex_set);

	int max_fd = 0;

	for(deque_it it = _reactors.begin(); it != _reactors.end(); it++)
	{
		ptr< CReactor<fd_t> > reactor = *it;
		fd_t fd = reactor->key();
		int able = reactor->reactable();
		if (able & MASK_READ)
		{
			FD_SET(fd, &r_set);
		}
		if (able & MASK_WRITE)
		{
			FD_SET(fd, &w_set);
		}
		if (able & MASK_EXCEPT)
		{
			FD_SET(fd, &ex_set);
		}
		max_fd = max_fd > fd ? max_fd : fd;
	}

	//	time prepare
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = C_SELECT_WAIT_TIMEOUT;

	//	select
	int ret = select (max_fd + 1, &r_set, &w_set, &ex_set, &tv);
	if (ret < 0)
	{
		return;	//	throw bt_ex(string("select return -1 ") + strerror(errno));
	}
	else if(ret == 0)
	{
		return;
	}

	//	get select result
	std::deque< ptr< CReactor<fd_t> > > restart_q;
	for(deque_it it = _reactors.begin(); it != _reactors.end(); it++)
	{
		fd_t fd = (*it)->key();
		int mask = 0;
		if (FD_ISSET(fd, &r_set))
		{
			mask = mask | MASK_READ;
		}
		if (FD_ISSET(fd, &w_set))
		{
			mask = mask | MASK_WRITE;
		}
		if (FD_ISSET(fd, &ex_set))
		{
			mask = mask | MASK_EXCEPT;
		}
		
		if (mask == 0)
			continue;

		ENUM_REACT_RESULT res = (*it)->react(mask);
		if (res == REACT_REMOVE)
		{
			it = _reactors.erase(it);
			if (it == _reactors.end()) break;
		}
		else if (res == REACT_RESTART)
		{
			restart_q.push_back(*it);
			it = _reactors.erase(it);
			if (it == _reactors.end()) break;
		}
	}

	//	restart
	for(deque_it it = restart_q.begin(); it < restart_q.end(); it++)
	{
		_reactors.push_back(*it);
	}
}

}}	//	namespace

//////////////////////////////////////////////////////////////////////////
#endif//_TFC_DRIVER_SELECT_HPP_
///:~
