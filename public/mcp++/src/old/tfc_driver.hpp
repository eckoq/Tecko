
#ifndef _TFC_DRIVER_HPP_
#define _TFC_DRIVER_HPP_

#include "tfc_heap_guardian.hpp"

//////////////////////////////////////////////////////////////////////////

//
//	three element: 
//	1 key,			every reactor has a corresponding key
//	2 event,		the things happened on the key, and captured by driver
//	3 react result, stay in driver, removed from driver or restart a key-event-mapping
//

namespace tfc
{
	//
	//	mask the difference between [select r/w/e]
	//	and  [epoll/poll IN/OUT/ERR/HUP/PRI
	//	EPOLLET as a control flag
	//
	
	enum ENUM_REACT_MASK
	{
		MASK_READ		= 0x00000001,
			MASK_WRITE		= 0x00000002,
			MASK_EXCEPT		= 0x00000004,
			MASK_HUNGUP		= 0x00000008,
			MASK_PRIORITY	= 0x00000010,
	};
	
	enum ENUM_REACT_RESULT
	{
		REACT_OK		= 0,
			REACT_REMOVE	= -1,
			REACT_RESTART	= 1,
	};
	
	template<typename T> class CReactor
	{
	public:
		virtual ~CReactor(){}
		virtual T key() = 0;
		//	return 0 for nothing, -1 for del, 1 for del and add
		virtual ENUM_REACT_RESULT react(int action_mask) = 0;
		virtual int reactable() = 0;				//	return -1 for del
	};
	
	//	active object
	template<typename T> class CDriver
	{
	public:
		virtual ~CDriver(){}
		virtual void drive() = 0;					//	passive/active method
		virtual void add(ptr< CReactor<T> >) = 0;	//	passive method, dont care if existed any more
		virtual void del(T) = 0;					//	passive method
	};

}

//////////////////////////////////////////////////////////////////////////
#endif//_TFC_DRIVER_HPP_
///:~
