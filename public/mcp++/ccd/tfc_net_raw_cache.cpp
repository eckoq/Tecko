#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include "tfc_net_raw_cache.h"
#include "tfc_net_cconn.h"
#include "fastmem.h"

using namespace tfc::net;
using namespace std;


int CRawCache::append(const char* data0, unsigned data_len0, const char* data1, unsigned data_len1)
{
	unsigned data_len = data_len0 + data_len1;
	if (0 == data_len)
    {
		printf("empty content to append!\n");
		return -1;
	}

	if(NULL == _data)
    {
		unsigned size = (data_len < _buff_size ? _buff_size : data_len);
		_data = (char*)fastmem_get(size, &_size);

		if(NULL == _data)
        {
			//分配内存失败，丢弃当前操作
			printf("out of memory[size:%u], %s:%d\n", size, __FILE__, __LINE__);
			return -1;
		}

		_len    = 0;
		_offset = 0;
	}

	if(_size - _len - _offset >= data_len)
    {
		memcpy(_data + _offset + _len, data0, data_len0);
		memcpy(_data + _offset + _len + data_len0, data1, data_len1);
		_len += data_len;
	}
	else if(_len + data_len <= _size)
    {
		memmove(_data, _data + _offset, _len);
		memcpy(_data + _len, data0, data_len0);
		memcpy(_data + _len + data_len0, data1, data_len1);

		_offset = 0;
		_len += data_len;
	}
	else
    {
		unsigned tmp_len = calculate_new_buffer_size(data_len);
		unsigned tmp_size;

		char* tmp = (char*)fastmem_get(tmp_len, &tmp_size);
		if(NULL == tmp)
        {
			//分配内存失败，丢弃当前操作
			printf("out of memory[size:%u], %s:%d\n", tmp_len, __FILE__, __LINE__);
			return -1;
		}

		memcpy(tmp, _data + _offset, _len);
		memcpy(tmp + _len, data0, data_len0);
		memcpy(tmp + _len + data_len0, data1, data_len1);

        fastmem_put(_data, _size);

		_data   = tmp;
		_size   = tmp_size;
		_len    = _len + data_len;
		_offset = 0;
	}

	return 0;
}

int CRawCache::append(const char* data, unsigned data_len)
{
	if(NULL == _data)
    {
		unsigned size = (data_len < _buff_size ? _buff_size : data_len);
		_data = (char*)fastmem_get(size, &_size);

		if(NULL == _data)
        {
			//分配内存失败，丢弃当前操作
			printf("out of memory[size:%u], %s:%d\n", size, __FILE__, __LINE__);
			return -1;
		}

		_len    = 0;
		_offset = 0;
	}

	if(_size - _len - _offset >= data_len)
    {
		memcpy(_data + _offset + _len, data, data_len);
		_len += data_len;
	}
	else if(_len + data_len <= _size)
    {
		memmove(_data, _data + _offset, _len);
		memcpy(_data + _len, data, data_len);

		_offset = 0;
		_len   += data_len;
	}
	else
    {
		unsigned tmp_len = calculate_new_buffer_size(data_len);
		unsigned tmp_size;

		char* tmp = (char*)fastmem_get(tmp_len, &tmp_size);
		if(!tmp)
        {
			//分配内存失败，丢弃当前操作
			printf("out of memory[size:%u], %s:%d\n", tmp_len, __FILE__, __LINE__);
			return -1;
		}

		memcpy(tmp, _data + _offset, _len);
		memcpy(tmp + _len, data, data_len);

		fastmem_put(_data, _size);

		_data   = tmp;
		_size   = tmp_size;
		_len    = _len + data_len;
		_offset = 0;
	}

	return 0;
}

int CRawCache::calculate_new_buffer_size(unsigned append_size) {
    unsigned linear_increment_len = _len + append_size;

    // current buffer is not too long, use liner increment first
    if (_len < (unsigned)LINEAR_MALLOC_THRESHOLD) {
        return linear_increment_len;
    }

    // current buffer is long enough, try exponent increment, and return the
    // bigger one
    unsigned exponent_increment_len = _len + _len / EXPONENT_INCREMENT_PERCENT;
    if (exponent_increment_len > linear_increment_len) {
        return exponent_increment_len;
    } else {
        return linear_increment_len;
    }
}

void CRawCache::skip(unsigned length)
{
	if(NULL == _data)
    {
        return;
    }

    if(length >= _len)
    {
		if ( _size > _buff_size )
        {
			reinit();
		}
        else
        {
			clean_data();
		}
	}
	else
    {
		_len    -= length;
		_offset += length;
	}
}

void CRawCache::reinit()
{
	fastmem_put(_data, _size);

	_data   = NULL;
	_size   = 0;
	_len    = 0;
	_offset = 0;
}


void CTimeRawCache::skip(unsigned length)
{
	// skip may call reinit and clean_data,
    // in reinit _total_bytes_cached and _total_bytes_skip will be reset
	CRawCache::skip(length);
	if (_head == _tail)
		return;

	_total_bytes_skip += length;

	int new_head = (_head + 1) % TIME_QUEUE_LEN;
	while (new_head != _tail &&
		   _msg_time_queue[new_head].offset <= _total_bytes_skip)
	{
		_head = new_head;
		new_head = (new_head + 1) % TIME_QUEUE_LEN;
	}
}

void CTimeRawCache::add_new_msg_time(time_t sec, time_t msec)
{
	int new_tail = (_tail + 1) % TIME_QUEUE_LEN;

	if (_head == _tail)  // No record
	{
		_msg_time_queue[_tail].offset = _total_bytes_cached;
		_msg_time_queue[_tail].sec = sec;
		_msg_time_queue[_tail].msec = msec;
		_tail = new_tail;
		return;
	}

    // msg_time_queue full, do nothing, keep the time stamp at _head
	if (_head == new_tail)
	{
		return;
	}

	// 如果和前一次放进来的时间相差不足100ms，则不添加新的时间戳
	// 因此时间控制的精度是100ms
	int valid_tail = (_tail + TIME_QUEUE_LEN - 1) % TIME_QUEUE_LEN;
	int64_t diff =
		(sec * 1000 + msec) -
		(_msg_time_queue[valid_tail].sec * 1000 + _msg_time_queue[valid_tail].msec);

	if (diff < INTERVAL_PER_GRID)
	{
		return;
	}

    // No data is cached between last add and this add
    if (_msg_time_queue[valid_tail].offset == _total_bytes_cached)
    {
        _msg_time_queue[valid_tail].offset = _total_bytes_cached;
        _msg_time_queue[valid_tail].sec = sec;
        _msg_time_queue[valid_tail].msec = msec;
        return;
    }

	_msg_time_queue[_tail].offset = _total_bytes_cached;
	_msg_time_queue[_tail].sec = sec;
	_msg_time_queue[_tail].msec = msec;
	_tail = new_tail;
}

bool CTimeRawCache::is_msg_timeout(int expire_time_ms, const struct timeval* now)
{
    // no message in queue or no data is cached
	if (_head == _tail || _total_bytes_skip == _total_bytes_cached)
	{
		return false;
	}

	int64_t diff =
		(now->tv_sec * 1000 + now->tv_usec / 1000) -
		(_msg_time_queue[_head].sec * 1000 + _msg_time_queue[_head].msec);
	return (diff > expire_time_ms);
}
