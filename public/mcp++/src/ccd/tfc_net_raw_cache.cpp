#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "tfc_net_raw_cache.h"
#include "fastmem.h"

using namespace tfc::net;
using namespace std;

int CRawCache::append(const char* data, unsigned data_len) {
	if(_data == NULL) {
		unsigned size = (data_len < _buff_size ? _buff_size : data_len);
		_data = (char*)fastmem_get(size, &_size);

		if(_data == NULL) {
			//分配内存失败，丢弃当前操作
			printf("out of memory[size:%u], %s:%d\n", size, __FILE__, __LINE__);
			return -1; 
		}   
		_len = 0;
		_offset = 0;	
	}   

	if(_size - _len - _offset >= data_len) {
		memcpy(_data + _offset + _len, data, data_len);
		_len += data_len;
	}   
	else if(_len + data_len <= _size) {
		memmove(_data, _data + _offset, _len);	
		memcpy(_data + _len, data, data_len);
		_offset = 0;
		_len += data_len;
	}
	else {
		unsigned tmp_len = _len + data_len;
		unsigned tmp_size;
		char* tmp = (char*)fastmem_get(tmp_len, &tmp_size);
		if(!tmp) {
			//分配内存失败，丢弃当前操作
			printf("out of memory[size:%u], %s:%d\n", tmp_len, __FILE__, __LINE__);
			return -1; 
		}   
		memcpy(tmp, _data + _offset, _len);
		memcpy(tmp + _len, data, data_len);
		fastmem_put(_data, _size);
		_data = tmp;
		_size = tmp_size;
		_len = tmp_len;
		_offset = 0;
	}
	return 0;   
}
void CRawCache::skip(unsigned length) {
	if(_data) {
		if(length >= _len) {
			reinit();
		}
		else {
			_len -= length;
			_offset += length;
		}
	}
}
void CRawCache::reinit() {
	if(_size > _buff_size) {
		fastmem_put(_data, _size);
		_data = NULL;
		_size = 0;
	}
	_len = 0;
	_offset = 0;
}
