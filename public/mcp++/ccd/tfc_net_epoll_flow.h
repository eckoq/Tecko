#ifndef _TFC_NET_EPOLL_FLOW_H_
#define _TFC_NET_EPOLL_FLOW_H_

#include <sys/epoll.h>

//////////////////////////////////////////////////////////////////////////
namespace tfc{	namespace net{

	inline void epoll_add(int epfd, int fd, void* user, int events) 
    {
		struct epoll_event ev;
		ev.events   = events | EPOLLERR;
		ev.data.ptr = user;
        
		epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);    
	}
    
	inline void epoll_mod(int epfd, int fd, void* user, int events) 
    {
		struct epoll_event ev;
		ev.events   = events | EPOLLERR;
		ev.data.ptr = user;

        epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);    
	}

    inline void epoll_del(int epfd, int fd) 
    {
		struct epoll_event ev;
	  //ev.data.u64 = fd ;

        epoll_ctl(epfd, EPOLL_CTL_DEL, fd, &ev);    
	}

}}

//////////////////////////////////////////////////////////////////////////
#endif//_TFC_NET_EPOLL_FLOW_H_
///:~

