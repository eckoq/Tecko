
#ifndef _TFC_QUEUE_CACHE_HPP_
#define _TFC_QUEUE_CACHE_HPP_

#include <cstring>

namespace tfc{
	
//////////////////////////////////////////////////////////////////////////

//	
//	auto extend cache
//	cant be used as data array directly, what a pity...
//

class CQueueCache	//	be dealt with as a queue
{
public:
	CQueueCache(size_t buf_size = C_DEFAULT_CACHE_SIZE);
	virtual ~CQueueCache();
	
	//	what in cache?
	virtual char*  data(){return _buf;}
	virtual size_t data_len() const {return _data_len;}
	
	//	add/remove from cache
	virtual void enqueue(const char* data, size_t data_len);
	virtual void dequeue(size_t dq_len);	//	return left	data size
	
	//	cache metadata info
	virtual void resize(size_t new_size);	//	return left data size
	virtual void clear();

protected:
	size_t _buf_size;
	size_t _data_len;
	char*  _buf;
	
	static const size_t C_DEFAULT_CACHE_SIZE	= 0x00001000;	//	4k
	static const size_t C_CRITICAL_BUFFER_SIZE	= 0x00010000;	//	64k
};

//////////////////////////////////////////////////////////////////////////
//	implementations
//////////////////////////////////////////////////////////////////////////

inline CQueueCache::CQueueCache(size_t buf_size):_buf_size(buf_size),_data_len(0),_buf(NULL)
{
	assert(buf_size < C_CRITICAL_BUFFER_SIZE);
	_buf = new char[buf_size];
	//	without memset
}

inline CQueueCache::~CQueueCache()
{
	delete[] _buf;
	_buf = NULL;
}

inline void CQueueCache::enqueue(const char* data, size_t data_len)
{
	if (_buf_size < _data_len + data_len)
		resize(_data_len + data_len);
	
	memcpy(_buf + _data_len, data, data_len);
	_data_len += data_len;
}

inline void CQueueCache::dequeue(size_t dq_len)
{
	if (dq_len > _data_len)
	{
		clear();
		return;
	}

	memmove(_buf, _buf + dq_len, _data_len - dq_len);
	return _data_len = _data_len- dq_len;
}

inline void CQueueCache::resize(size_t new_size)
{
	//	no change
	if (new_size == _buf_size)
		return;

	//	extend
	else if (new_size > _buf_size)
	{
		char* p = new char[new_size];
		memcpy(p, _buf, _data_len);
		delete [] _buf;

		_buf = p;
		_buf_size = new_size;
		//	_data_len no change
	}

	//	truncate
	else
	{
		char* p = new char[new_size];
		if (new_size >= _data_len)
		{
			memcpy(p, _buf, _data_len);
			//	_data_len no change
		}
		else
		{
			memcpy(p, _buf, new_size);
			data_len = new_size;
		}
		delete [] _buf;
		
		_buf = p;
		_buf_size = new_size;
	}
}

inline size_t CQueueCache::clear()
{
	_data_len = 0;
}

}

//////////////////////////////////////////////////////////////////////////
#endif//_TFC_QUEUE_CACHE_HPP_
///:~
