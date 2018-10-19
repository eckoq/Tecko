
#ifndef _TFC_EPOLLER_HPP_
#define _TFC_EPOLLER_HPP_

//#include <string.h>
#include <errno.h>
#include <assert.h>
#include <epoll.h>

#include "tfc_ex.hpp"

namespace tfc{namespace net{

//////////////////////////////////////////////////////////////////////////

class CEPollResult;

class CEPoller
{
public:
	CEPoller() : _fd(-1), _events(NULL){}
	~CEPoller(){if (_events) delete[] _events; close(_fd);}
	
	void create(size_t iMaxFD);
	void add(int fd, int flag = EPOLLIN | EPOLLET);
	void del(int fd);
	CEPollResult wait(int iTimeout);
	
protected:
	void ctl(int fd, int epollAction, int flag);
	
	int _fd;
	epoll_event* _events;
	size_t _maxFD;
};

//////////////////////////////////////////////////////////////////////////

class CEPollResult
{
public:
	~CEPollResult(){}
	CEPollResult(const CEPollResult& right):_events(right._events), _size(right._size){}
	CEPollResult& operator=(const CEPollResult& right)
	{_events = right._events; _size = right._size; return *this;}
	
	class iterator;
	iterator begin(){return CEPollResult::iterator(0, *this);}
	iterator end(){return CEPollResult::iterator(_size, *this);}
	
	protected:
		CEPollResult(epoll_event* events, size_t size):_events(events), _size(size){}
		bool operator==(const CEPollResult& right){return (_events == right._events && _size == right._size);}
		
		epoll_event* _events;
		size_t _size;
		
	public:
		class iterator
		{
		public:
			iterator(const iterator& right):_index(right._index), _res(right._res){}
			iterator& operator ++(){_index++; return *this;}
			iterator& operator ++(int){_index++; return *this;}
			bool operator ==(const iterator& right){return (_index == right._index && _res == right._res);}
			bool operator !=(const iterator& right){return !(_index == right._index && _res == right._res);}
			epoll_event* operator->(){return &_res._events[_index];}
			epoll_event& operator* (){return  _res._events[_index];}
			
		protected:
			iterator(size_t index, CEPollResult& res): _index(index), _res(res){}
			size_t _index;
			CEPollResult& _res;
			friend class CEPollResult;
		};
		
		friend class CEPoller;
		friend class CEPollResult::iterator;
};

//////////////////////////////////////////////////////////////////////////
//	implementation
//////////////////////////////////////////////////////////////////////////

inline void CEPoller::create(size_t iMaxFD)
{
	_maxFD = iMaxFD;
	_fd = epoll_create(1024);
	if(_fd == -1)
		throw bt_ex("epoll_create fail [" + std::string(strerror(errno)) + "]");
	_events = new epoll_event[iMaxFD];
}

inline void CEPoller::add(int fd, int flag)
{
	ctl(fd, EPOLL_CTL_ADD, flag);
}

inline void CEPoller::del(int fd)
{
	ctl(fd, EPOLL_CTL_DEL, 0);
}

inline CEPollResult CEPoller::wait(int iTimeout)
{
	int nfds = epoll_wait(_fd, _events, _maxFD, iTimeout);
	if (nfds < 0)
	{
		if (errno != EINTR)
			throw bt_ex("epoll_wait fail [" + std::string(strerror(errno)) + "]");
		else
			nfds = 0;
	}
	return CEPollResult(_events, nfds);;
}

inline void CEPoller::ctl(int fd, int epollAction, int flag)
{
	assert(_fd != -1);
	epoll_event ev;
	ev.data.fd = fd;
	ev.events = flag;
	int ret = epoll_ctl(_fd, epollAction, fd, &ev);
	if (ret < 0)
		throw bt_ex("epoll_ctl fail [" + std::string(strerror(errno)) + "]");
}

//////////////////////////////////////////////////////////////////////////
}}	//	namespace tfc::net
#endif//_EPOLLER_H_
///:~
