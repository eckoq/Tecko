
#ifndef _TFC_BUFFER_HPP_
#define _TFC_BUFFER_HPP_

#include <cstring>

//////////////////////////////////////////////////////////////////////////

namespace tfc{

class CBuffer
{
public:
	CBuffer() : _buf(NULL), _buf_size(0), _data_len(0){}
	virtual ~CBuffer(){if (_buf) delete [] _buf;}
	virtual unsigned size()		{return _data_len;}
	virtual void append(const void* buf, unsigned len);
	virtual void skip(unsigned len);

protected:
	char* _buf;
	unsigned _buf_size;
	unsigned _data_len;

public:
	
	class iterator
	{
	public:
		iterator(CBuffer& buf, unsigned pos): _buf(buf), _pos(pos){}
		iterator(const iterator& right): _buf(right._buf), _pos(right._pos){}
		iterator& operator=(const iterator& i){_buf = i._buf; _pos = i._pos; return *this;}
		
		iterator& operator += (unsigned len){_pos += len; return *this;}
		iterator operator++(int) {iterator ret = *this; (*this)+=1; return ret;}
		
		char operator*(){return *(_buf._buf + _pos);}
		template<typename T> T get(){return *(T*)(_buf._buf + _pos);}
		unsigned get(char* buf, size_t size)
		{
			if (size + _pos <= _buf._data_len)
			{
				memmove(buf, _buf._buf + _pos, size);
				return size;
			}
			else
			{
				unsigned len = _buf._data_len - _pos;
				memmove(buf, _buf._buf + _pos, len);
				return len;
			}
		}
		
		bool operator == (const iterator& right)
		{return (&_buf == & (right._buf)) && (_pos == right._pos);}
		bool operator != (const iterator& right)
		{return (&_buf != & (right._buf)) || (_pos != right._pos);}
		
		void* operator()(){return _buf._buf + _pos;}
	
	protected:
		CBuffer& _buf;
		unsigned _pos;
	};
	friend class iterator;	
	
	iterator begin()	{return iterator(*this, 0);}
	iterator end()		{return iterator(*this, _data_len);}

private:
	CBuffer(const CBuffer&);
	CBuffer& operator = (const CBuffer&);
};

inline void CBuffer::append(const void* buf, unsigned len)
{
	if (_buf_size >= _data_len + len)
	{
		memmove(_buf + _data_len, buf, len);
		_data_len += len;
	}
	else if(_buf_size * 2 >= _data_len + len)
	{
		char* p = new char[_buf_size * 2];
		memmove(p, _buf, _data_len);
		memmove(p + _data_len, buf, len);
		_data_len += len;
		_buf_size *= 2;
		delete [] _buf;
		_buf = p;
	}
	else
	{
		char* p = new char[_data_len + len];
		memmove(p, _buf, _data_len);
		memmove(p + _data_len, buf, len);
		_data_len += len;
		_buf_size = _data_len;
		delete [] _buf;
		_buf = p;
	}
}

inline void CBuffer::skip(unsigned len)
{
	if (len >= _data_len)
	{
		_data_len = 0;
	}
	else
	{
		memmove(_buf, _buf + len, _data_len - len);
		_data_len -= len;
	}
}

//////////////////////////////////////////////////////////////////////////
}	//	namespace tfc
#endif//_TFC_BUFFER_HPP_
///:~
