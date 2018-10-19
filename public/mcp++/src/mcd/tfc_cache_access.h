#ifndef _TFC_CACHE_ACCESS_H_
#define _TFC_CACHE_ACCESS_H_

#include "tfc_cache_hash_map.h"
#include "tfc_binlog.hpp"

//////////////////////////////////////////////////////////////////////////

namespace tfc{namespace cache
{
	typedef struct {
		unsigned				hash_node_used;
		unsigned				hash_node_total;
		unsigned				hash_bucket_used;
		unsigned				hash_bucket_total;
		
		unsigned				chunk_node_used;
		unsigned				chunk_node_total;
		unsigned				chunk_data_size;		// Not include chunk head.
	} stat_memcache;
	
	class CacheAccess
	{
	public:
		CacheAccess()
		{
			_lastdumptime = time(NULL);
			_cache_dump_min= -1;
			_cache_dump_point = 0;
			strcpy(_cache_dump_file, "./cache.dump");
			_cacheinit = true;
		}
		~CacheAccess(){}
		
		int open(char* mem, long mem_size, bool inited , int node_total, int bucket_size, int n_chunks, int chunk_size);
		
		int set(const char* key, const char* data, unsigned data_len, ENodeFlag flag=NODE_FLAG_DIRTY);
		int set(const char* key, const char* data, unsigned data_len, long expiretime, ENodeFlag flag=NODE_FLAG_DIRTY);		
		int del(const char* key);
		int get(const char* key, char* buf, unsigned buf_size , unsigned& data_len, bool& dirty_flag, long& time_stamp);
		int get(const char* key, char* buf, unsigned buf_size , unsigned& data_len, bool& dirty_flag, long& time_stamp, long& expiretime);
		
		int get_key(const char* key, unsigned& data_len, bool& dirty_flag, long& time_stamp);
		int mark_clean(const char* key);
		
		int get_lru_pre(const char* key, char* pre_key , char* buf, unsigned buf_size , unsigned& data_len, bool& dirty_flag, long& time_stamp);
		int oldest(char* buf, unsigned buf_size , char* key, unsigned& data_len, bool& dirty_flag, long& time_stamp);
		int oldest_key(char* key, unsigned& data_len, bool& dirty_flag, long& time_stamp);
		int oldest_key(char* key, unsigned& data_len, bool& dirty_flag, long& time_stamp, long& expiretime);	

		int mark_clean(int modbase, int mobres);
		int del_node(int modbase, int mobres);
		int get_hash_bucket_size() { return _cache.get_bucket_size(); }
		int core_dump_mem(char *buff, int maxsize,int modbase, int mobres);
		int core_recover_mem(char *buff, int buffsize);
		//-1失败, -2 buff长度不够(此时core_size为已dump字节数，core_size == 0 说明为空桶)，0 成功
		int core_dump_bucket_mem(char *buff, int maxsize, unsigned bucket_id, unsigned &core_size);
		//0成功，-1失败，-2部分失败(即在恢复过程中失败，此时recover_size为已恢复字节数)
		int core_recover_bucket_mem(char *buff, int buffsize, unsigned &recover_size);
		int core_dump(char *szcorefile);
		int core_recover(char *szcorefile);
		int StartUp();
		int CoreInit(int coredump_interval/* 间隔时间，单位：min */,
					unsigned coredump_point/* min of day, from 0 - 1439 */,
				char *coredump_file, char *sBinLogBaseName, long lMaxBinLogSize, int iMaxBinLogNum);

		int time_check();

		// check if need dump
		bool is_need_dump();
		// init _last_dump_time
		void init_lastdumptime();

		// for mirror dump
		int mirror_dump(char *mirror_file);
		int mirror_recover(char *mirror_file);
		int StartUp_mirror();
		// ret 0: 进行了dump
		//     1: 没有进行dump
		int time_check_mirror();
		
		// HashNode的使用率超过80％
		bool warning_80persent();
		float get_used_percent();
		
		// HashNode, ChunkNode的使用情况
		void get_node_num(unsigned &hash_node_used, unsigned &hash_node_total, 
                            unsigned &chunk_node_used, unsigned &chunk_node_total);

		void get_stat(stat_memcache &st)
		{
			memset(&st, 0, sizeof(stat_memcache));
			
			st.hash_node_used = (unsigned)_cache.used_node_num();
			st.hash_node_total = (unsigned)_cache.get_node_total();
			st.hash_bucket_used = (unsigned)_cache.get_bucket_used();
			st.hash_bucket_total = (unsigned)_cache.get_bucket_size();

			st.chunk_node_used = (unsigned)_cache.get_used_chunk_num();
			st.chunk_node_total = (unsigned)_cache.get_chunk_total();
			st.chunk_data_size = (unsigned)_cache.get_chunk_data_size();
		}
		
		// 获得cache所对应的share memory指针，memory大小
		void get_memory(char **mem, long& mem_size)
		{
			*mem = _mem;
			mem_size = _mem_size;
		};
		// 获得是否新建共享内存的标志
		bool is_new_init(){ return _cacheinit; };
		/////////////////////////////////
		
		enum
		{
			op_set = 0,
			op_mark_clean,
			op_del
		};
		
	class dirty_iterator
	{
	public:
		dirty_iterator(const dirty_iterator& right)
			: _cache(right._cache)
			, _cur_node(right._cur_node)
		{}
		dirty_iterator& operator ++(int)
		{
			if (_cur_node == NULL)
				return *this;	//	error tolerance
			
			for(_cur_node = _cache->get_add_list_next(_cur_node)
				; _cur_node != NULL && _cur_node->flag_ == NODE_FLAG_UNCHG
				; _cur_node = _cache->get_add_list_next(_cur_node))
				;
			
			return *this;
		}
		bool operator ==(const dirty_iterator& right)
		{return _cur_node == right._cur_node;}
		bool operator !=(const dirty_iterator& right)
		{return _cur_node != right._cur_node;}
		int get(char* key, char* buf, unsigned buf_size, unsigned& data_len)
		{
			if (_cur_node == NULL)
				return -1;

			memcpy(key,_cur_node->key_._.md5_,TMBHashKey::C_DATA_LEN);
			data_len = buf_size;
			return  _cache->merge_node_data(_cur_node, buf, (int*)&data_len);
		}
	
	protected:
		dirty_iterator(CHashMap* cache, THashNode* cur_node)
			: _cache(cache)
			, _cur_node(cur_node)
		{}
		CHashMap * const _cache;
		THashNode* _cur_node;

	private:
		dirty_iterator& operator ++();
		void* operator->();
		int operator* ();
		friend class CacheAccess;
	};

	dirty_iterator begin(){return dirty_iterator(&_cache, _cache.get_add_list_head());}
	dirty_iterator end(){return dirty_iterator(&_cache, NULL);}
	
		CHashMap _cache;
		
		time_t _lastdumptime;
		int _cache_dump_min;
		unsigned _cache_dump_point;
		char _cache_dump_file[256];

		CBinLog _binlog;

		bool _cacheinit;
		char *_mem;
		long _mem_size;
	};
	
	//////////////////////////////////////////////////////////////////////////
	
	class CacheAccessUin
	{
	public:
		CacheAccessUin(CacheAccess& da) : _da(da){}
		~CacheAccessUin(){}
		
		int set(unsigned uin, const char* data, unsigned data_len,ENodeFlag flag=NODE_FLAG_DIRTY);
		int del(unsigned uin);
		int get(unsigned uin, char* buf, unsigned buf_size , unsigned& data_len, bool& dirty_flag, long& time_stamp);
		int get_key(unsigned uin, unsigned& data_len, bool& dirty_flag, long& time_stamp);
		int mark_clean(unsigned uin);

		int get_lru_pre(unsigned key, unsigned& pre_key , char* buf, unsigned buf_size , unsigned& data_len, bool& dirty_flag, long& time_stamp);
		int oldest(char* buf, unsigned buf_size , unsigned& uin, unsigned& data_len, bool& dirty_flag, long& time_stamp);
		int oldest_key(unsigned& uin, unsigned& data_len, bool& dirty_flag, long& time_stamp);

		int mark_clean(int modbase, int mobres);
		int del_node(int modbase, int mobres);
		int core_dump_mem(char *buff, int maxsize,int modbase, int mobres);
		int core_recover_mem(char *buff, int buffsize);
		int get_hash_bucket_size() { return _da.get_hash_bucket_size(); }
		//-1失败, -2 buff长度不够(此时core_size为已dump字节数，core_size == 0 说明为空桶)，0 成功
		int core_dump_bucket_mem(char *buff, int maxsize, unsigned bucket_id, unsigned &core_size);
		//0成功，-1失败，-2部分失败(即在恢复过程中失败，此时recover_size为已恢复字节数)
		int core_recover_bucket_mem(char *buff, int buffsize, unsigned &recover_size);
		
		int core_dump(char *szcorefile);
		int core_recover(char *szcorefile);
		int mirror_dump(char *szcorefile);
		int mirror_recover(char *szcorefile);
		int CoreInit(int coredump_min, unsigned coredump_point, char *coredump_file,
				char * sBinLogBaseName, long lMaxBinLogSize, int iMaxBinLogNum);
		int StartUp();
		int time_check();
	    
	    // HashNode的使用率超过80％
		bool warning_80persent();
		float get_used_percent();
		
		// HashNode, ChunkNode的使用情况
		void get_node_num(unsigned &hash_node_used, unsigned &hash_node_total, 
                            unsigned &chunk_node_used, unsigned &chunk_node_total);
	
		bool is_new_init(){ return _da.is_new_init(); };
		CacheAccess& _da;
	};
}}

//////////////////////////////////////////////////////////////////////////
#endif//_TFC_CACHE_ACCESS_H_
///:~
