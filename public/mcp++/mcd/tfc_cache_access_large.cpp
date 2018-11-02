#include "tfc_cache_access_large.h"
#include <vector>
#include <errno.h>

using namespace tfc::cache_large;

static char szBuff[MAX_BINLOG_ITEM_LEN];


//////////////////////////////////////////////////////////////////////////

//
//	return 0 on success
//	return >0 on cant find
//	return <0 on error
//

int CacheAccess::get(const char* key, char* buf, uint32_t buf_size
				   , uint32_t& data_len, bool& dirty_flag, long& time_stamp)
{
	//	find
	TMBHashKey hash_key;
	memcpy(hash_key._.md5_, key, TMBHashKey::C_DATA_LEN);
	THashNode* node = _cache.find_node(hash_key);

	//	if cached, return directly
	if(node == NULL)
		return 1;

	dirty_flag = node->flag_;
	time_stamp = node->add_info_1_;
	data_len = buf_size;
	return _cache.merge_node_data(node, buf, (uint32_t*)&data_len);
}

//
//	return 0 on success
//	return >0 on cant find
//	return <0 on error
//
int CacheAccess::get(const char* key, char* buf, uint32_t buf_size
			, uint32_t& data_len, bool& dirty_flag, long& time_stamp, long& expiretime)
{
	//	find
	TMBHashKey hash_key;
	memcpy(hash_key._.md5_, key, TMBHashKey::C_DATA_LEN);
	THashNode* node = _cache.find_node(hash_key);

	//	if cached, return directly
	if(node == NULL)
		return 1;

	dirty_flag = node->flag_;
	time_stamp = node->add_info_1_;
	expiretime = _cache.get_expiretime(node);
	data_len = buf_size;
	return _cache.merge_node_data(node, buf, (uint32_t*)&data_len);
}


//
//	return 0 on success
//	return >0 on cant find
//	return <0 on error
//
int CacheAccess::get_key(const char* key, uint32_t& data_len, bool& dirty_flag, long& time_stamp)
{
	//	find
	TMBHashKey hash_key;
	memcpy(hash_key._.md5_, key, TMBHashKey::C_DATA_LEN);
	THashNode* node = _cache.find_node(hash_key);

	//	if cached, return directly
	if(node == NULL)
		return 1;

	dirty_flag = node->flag_;
	time_stamp = node->add_info_1_;
	data_len = node->chunk_len_;
	return 0;
}

//
//	return 0, on set success
//	return >0, on set fail, no enough position
//
int CacheAccess::set(const char* key, const char* data, uint32_t data_len,ENodeFlag flag/*=NODE_FLAG_DIRTY*/, bool need_sync/*=false*/)
{
	//	find
	TMBHashKey hash_key;
	memcpy(hash_key._.md5_, key, TMBHashKey::C_DATA_LEN);

	//	if cached, return directly
	THashNode* node = _cache.find_node(hash_key);
	if(node == NULL)
		node = _cache.insert_node(hash_key, (void*)data, data_len);
	else
		node = _cache.update_node(node, (void*)data, data_len);

	//	full
	if (node == NULL)
		return 1;

	_cache.set_node_flag(node, flag);

	if ((_cache_dump_min >= 0) && (flag == NODE_FLAG_DIRTY))
	{
		int op = op_set;
		int bufflen = 0;

		memcpy(szBuff,&op,sizeof(int));
		bufflen += sizeof(int);

		memcpy(szBuff+bufflen,key,TMBHashKey::C_DATA_LEN);
		bufflen += TMBHashKey::C_DATA_LEN;

		memcpy(szBuff+bufflen,&data_len,sizeof(int));
		bufflen += sizeof(int);

		memcpy(szBuff+bufflen,data,data_len);
		bufflen += data_len;

		long expiretime=0;
		memcpy(szBuff+bufflen,&expiretime,sizeof(long));
		bufflen += sizeof(int);

		assert(bufflen < MAX_BINLOG_ITEM_LEN && bufflen >= 0);

		_binlog.WriteToBinLog(szBuff,bufflen,need_sync);
	}

	return 0;
}

//
//	return 0, on set success
//	return >0, on set fail, no enough position
//

int CacheAccess::set(const char* key, const char* data, uint32_t data_len, long expiretime, ENodeFlag flag/*=NODE_FLAG_DIRTY*/, bool need_sync/*=false*/)
{
	//	find
	TMBHashKey hash_key;
	memcpy(hash_key._.md5_, key, TMBHashKey::C_DATA_LEN);

	//	if cached, return directly
	THashNode* node = _cache.find_node(hash_key);
	if(node == NULL)
		node = _cache.insert_node(hash_key, (void*)data, data_len);
	else
		node = _cache.update_node(node, (void*)data, data_len);

	//	full
	if (node == NULL)
		return 1;

	_cache.set_node_flag(node, flag);
	_cache.set_expiretime(node, expiretime);

	if ((_cache_dump_min >= 0) && (flag == NODE_FLAG_DIRTY))
	{
		int op = op_set;
		int bufflen = 0;

		memcpy(szBuff,&op,sizeof(int));
		bufflen += sizeof(int);

		memcpy(szBuff+bufflen,key,TMBHashKey::C_DATA_LEN);
		bufflen += TMBHashKey::C_DATA_LEN;

		memcpy(szBuff+bufflen,&data_len,sizeof(int));
		bufflen += sizeof(int);

		memcpy(szBuff+bufflen,data,data_len);
		bufflen += data_len;

		memcpy(szBuff+bufflen,&expiretime,sizeof(int));
		bufflen += sizeof(int);

		assert(bufflen < MAX_BINLOG_ITEM_LEN && bufflen >= 0);

		_binlog.WriteToBinLog(szBuff,bufflen,need_sync);
	}

	return 0;
}

//
//	return 0 on success
//	return >0 on cant find
//	return <0 on error
//

int CacheAccess::mark_clean(const char* key, bool need_sync/*=false*/)
{
	//	find
	TMBHashKey hash_key;
	memcpy(hash_key._.md5_, key, TMBHashKey::C_DATA_LEN);

	//	if cached, return directly
	THashNode* node = _cache.find_node(hash_key);
	if(node == NULL)
		return 1;

	_cache.set_node_flag(node, NODE_FLAG_UNCHG);

	if (_cache_dump_min >= 0)
	{
		//char szBuff[MAX_BINLOG_ITEM_LEN];
		int op = op_mark_clean;
		int bufflen = 0;

		memcpy(szBuff,&op,sizeof(int));
		bufflen += sizeof(int);

		memcpy(szBuff+bufflen,key,TMBHashKey::C_DATA_LEN);
		bufflen += TMBHashKey::C_DATA_LEN;

		assert(bufflen<MAX_BINLOG_ITEM_LEN);

		_binlog.WriteToBinLog(szBuff,bufflen,need_sync);
	}

	return 0;
}

//
//	return 0 on success
//	return >0 on cant find
//	return <0 on error
//

int CacheAccess::get_lru_pre(const char* key, char* pre_key
							 , char* buf, uint32_t buf_size
							 , uint32_t& data_len, bool& dirty_flag, long& time_stamp)
{
	//	find
	TMBHashKey hash_key;
	memcpy(hash_key._.md5_, key, TMBHashKey::C_DATA_LEN);
	THashNode* node = _cache.find_node(hash_key);

	//	if cached, return directly
	if(node == NULL)
		return 1;

	THashNode* pre_node = _cache.get_add_list_next(node);
	if (pre_node == NULL)
		return 1;

	memcpy(pre_key, pre_node->key_._.md5_, TMBHashKey::C_DATA_LEN);
	dirty_flag = pre_node->flag_;
	time_stamp = pre_node->add_info_1_;
	data_len = buf_size;
	return _cache.merge_node_data(pre_node, buf, (uint32_t*)&data_len);
}

//
//	return 0 on success
//	return >0 on cant find
//	return <0 on error
//

int CacheAccess::del(const char* key, bool need_sync/*=false*/)
{
	//	find
	TMBHashKey hash_key;
	memcpy(hash_key._.md5_, key, TMBHashKey::C_DATA_LEN);

	//	if cached, return directly
	THashNode* node = _cache.find_node(hash_key);

	//	if cached, return directly
	if(node == NULL)
		return 1;

	if (_cache_dump_min >= 0)
	{
		//char szBuff[MAX_BINLOG_ITEM_LEN];
		int op = op_del;
		int bufflen = 0;

		memcpy(szBuff,&op,sizeof(int));
		bufflen += sizeof(int);

		memcpy(szBuff+bufflen,key,TMBHashKey::C_DATA_LEN);
		bufflen += TMBHashKey::C_DATA_LEN;

		assert(bufflen<MAX_BINLOG_ITEM_LEN);
		_binlog.WriteToBinLog(szBuff,bufflen,need_sync);
	}

	return _cache.delete_node(node);
}

//
//	return 0 on success
//	return >0 on cant find
//	return <0 on error
//

int CacheAccess::oldest(char* buf, uint32_t buf_size
					  , char* key, uint32_t& data_len, bool& dirty_flag, long& time_stamp)
{
	THashNode* node = _cache.get_add_list_tail();
	if (node == NULL)
		return 1;

	memcpy(key, node->key_._.md5_, TMBHashKey::C_DATA_LEN);
	dirty_flag = node->flag_;
	time_stamp = node->add_info_1_;
	data_len = buf_size;
	return _cache.merge_node_data(node, buf, (uint32_t*)&data_len);
}

//
//	return 0 on success
//	return >0 on cant find
//	return <0 on error
//

int CacheAccess::oldest_key(char* key, uint32_t& data_len , bool& dirty_flag, long& time_stamp)
{
	THashNode* node = _cache.get_add_list_tail();
	if (node == NULL)
		return 1;

	memcpy(key, node->key_._.md5_, TMBHashKey::C_DATA_LEN);
	dirty_flag = node->flag_;
	time_stamp = node->add_info_1_;
	data_len = node->chunk_len_;
	return 0;
}

//
//	return 0 on success
//	return >0 on cant find
//	return <0 on error
//

int CacheAccess::oldest_key(char* key, uint32_t& data_len , bool& dirty_flag, long& time_stamp, long& expiretime)
{
	THashNode* node = _cache.get_add_list_tail();
	if (node == NULL)
		return 1;

	memcpy(key, node->key_._.md5_, TMBHashKey::C_DATA_LEN);

	//以下几行用来更新附加链表
	TMBHashKey hash_key;
	memcpy(hash_key._.md5_, key, TMBHashKey::C_DATA_LEN);
	node = _cache.find_node(hash_key);
	if (node == NULL)
		return 1;

	dirty_flag = node->flag_;
	time_stamp = node->add_info_1_;
	expiretime = _cache.get_expiretime(node);
	data_len = node->chunk_len_;
	return 0;
}

//
//	return 0 on success
//	return <0 on error
//

int CacheAccess::open(char* mem, uint64_t mem_size, bool inited ,
                      uint64_t node_total, uint64_t bucket_size,
                      uint64_t n_chunks, uint32_t chunk_size)
{
    uint64_t total_pool_size = _cache.get_total_pool_size(node_total, bucket_size, n_chunks, chunk_size);
	if (mem_size < total_pool_size) {
        fprintf(stderr, "total need mem size: %ld\n", total_pool_size);
		return -1;
    }

	_cacheinit = inited;

	_mem = mem;
	_mem_size = mem_size;
	return _cache.open(mem, inited, node_total, bucket_size, n_chunks, chunk_size);
}

int CacheAccess::del_node(uint64_t modbase, uint64_t mobres)
{
	std::vector<THashNode*> _vec_node;

	THashNode* _cur_node = _cache.get_add_list_head();
	if (!_cur_node)
		return 0;

	uint64_t ikey=0;
	memcpy(&ikey,_cur_node->key_._.md5_,sizeof(uint64_t));
	if (ikey % modbase == mobres)
	{
		_vec_node.push_back(_cur_node);
	}

	for(_cur_node = _cache.get_add_list_next(_cur_node); _cur_node != NULL
				; _cur_node = _cache.get_add_list_next(_cur_node))
	{
		memcpy(&ikey,_cur_node->key_._.md5_,sizeof(uint64_t));
		if (ikey % modbase == mobres)
		{
			_vec_node.push_back(_cur_node);
		}
	}

	for (uint64_t i=0; i<(uint64_t)_vec_node.size();i++)
	{
		_cache.delete_node(_vec_node[i]);
	}

	return 0;
}

int CacheAccess::mark_clean(uint64_t modbase, uint64_t mobres, bool need_sync/*=false*/)
{
	uint32_t data_len = 0 ;
	char buff[102400];
	const uint32_t buf_size = sizeof(buff);
	TMBHashKey hash_key;
	char key[TMBHashKey::C_DATA_LEN];
	uint64_t ikey;

	for(CacheAccess::dirty_iterator it = begin(); it != end();it++)
	{
		if (0 != it.get(key, buff, buf_size, data_len))
		{
			return 0;
		}

		memcpy(&ikey,key,sizeof(uint64_t));
		if (ikey % modbase != mobres)
		{
			continue;
		}

		mark_clean(key, need_sync);
	}
	return 0;
}

//-1失败,返回字节数
//只倒脏数据,modbase=-1表示全部
int CacheAccess::core_dump_mem(char *buff, uint64_t maxsize, uint64_t modbase, uint64_t mobres)
{
	if (!buff || maxsize <= 0)
	{
		return -1;
	}

	const uint32_t HEAD_LEN = TMBHashKey::C_DATA_LEN + sizeof(uint32_t);
	uint32_t data_len = 0 ;
	uint64_t ipos = 0;
	char key[TMBHashKey::C_DATA_LEN];
	uint64_t ikey = 0;

	for(CacheAccess::dirty_iterator it = begin(); it != end();it++)
	{
		if (0 != it.get(key, buff+ipos+HEAD_LEN, maxsize-ipos-HEAD_LEN, data_len))
		{
			break;
		}

		if (modbase != (uint64_t)-1)
		{
			memcpy(&ikey,key,sizeof(uint64_t));
			if (ikey % modbase != mobres)
			{
				continue;
			}
		}

		memcpy(buff+ipos,key,TMBHashKey::C_DATA_LEN);
		memcpy(buff+ipos+TMBHashKey::C_DATA_LEN,&data_len,sizeof(uint32_t));

		ipos += HEAD_LEN;
		ipos += data_len;
	}
	return ipos;
}

//-1失败,返回已恢复字节数
int CacheAccess::core_recover_mem(char *buff, uint64_t buffsize)
{
	if (!buff || buffsize < 0)
	{
		return -1;
	}

	const uint32_t HEAD_LEN = TMBHashKey::C_DATA_LEN + sizeof(uint32_t);
	char key[TMBHashKey::C_DATA_LEN];
	uint32_t data_len=0;
	uint64_t ipos=0;
	while(ipos < buffsize)
	{
		if (buffsize - ipos <= HEAD_LEN)
		{
			break;
		}

		memcpy(key,buff+ipos,TMBHashKey::C_DATA_LEN);
		memcpy(&data_len,buff+ipos+TMBHashKey::C_DATA_LEN,sizeof(uint32_t));

		if (data_len > buffsize - ipos - HEAD_LEN)
		{
			break;
		}
		//非磁盘数据，需要binlog
		int ret = set(key, buff+ipos+HEAD_LEN, data_len);
		assert(ret == 0);

		ipos += HEAD_LEN;
		ipos += data_len;
	}

	return ipos;
}

//-1失败, -2 buff长度不够(此时core_size为已dump字节数，core_size == 0 说明为空桶)，0 成功
int CacheAccess::core_dump_bucket_mem(char *buff, uint64_t maxsize, uint64_t bucket_id, uint64_t&core_size)
{
	const uint32_t		HEAD_LEN = TMBHashKey::C_DATA_LEN + sizeof(int);
	uint32_t			data_len = 0;
	uint64_t			ipos = 0;
	THashNode		*node = NULL;

	core_size = 0;

	if ( !buff || maxsize <= 0 ) {
		return -1;
	}

	node = _cache.get_bucket_list_head(bucket_id);
	while ( node ) {
		data_len = maxsize - ipos - HEAD_LEN;
		if ( _cache.merge_node_data(node, buff + ipos + HEAD_LEN, &data_len) ) {
			return -2;
		}

		memcpy(buff + ipos, node->key_._.md5_, TMBHashKey::C_DATA_LEN);
		memcpy(buff + ipos + TMBHashKey::C_DATA_LEN, &data_len, sizeof(uint32_t));

		ipos += HEAD_LEN;
		ipos += data_len;
		core_size = ipos;

		node = _cache.get_bucket_list_next(node);
	}

	core_size = ipos;

	return 0;
}

//0成功，-1失败，-2部分失败(即在恢复过程中失败，此时recover_size为已恢复字节数)
int CacheAccess::core_recover_bucket_mem(char *buff, uint64_t buffsize, uint64_t &recover_size)
{
	const uint32_t	HEAD_LEN = TMBHashKey::C_DATA_LEN + sizeof(uint32_t);
	char			key[TMBHashKey::C_DATA_LEN];
	uint32_t		data_len = 0;
	uint64_t		ipos = 0;
	int				ret = 0;

	recover_size = 0;

	if ( !buff || buffsize < 0 ) {
		return -1;
	}

	while ( ipos < buffsize ) {
		if ( buffsize - ipos <= HEAD_LEN ) {
			break;
		}

		memcpy(key, buff + ipos, TMBHashKey::C_DATA_LEN);
		memcpy(&data_len, buff + ipos + TMBHashKey::C_DATA_LEN, sizeof(uint32_t));

		if ( data_len > buffsize - ipos - HEAD_LEN ) {
			return -2;
		}

		ret = set(key, buff + ipos + HEAD_LEN, data_len);
		if ( ret ) {
			return -2;
		}

		ipos += HEAD_LEN;
		ipos += data_len;

		recover_size = ipos;
	}

	recover_size = ipos;

	return 0;
}

//-1失败,返回字节数
uint64_t CacheAccess::core_dump(char *szcorefile)
{
	char last_file[256], tmp_name[256];
	snprintf(last_file, sizeof(last_file), "%s.last", szcorefile);
	snprintf(tmp_name, sizeof(tmp_name), "%s.tmp", szcorefile);

	FILE *fp =fopen(last_file, "w+");
	if (!fp)
	{
		return -1;
	}

	// 每次写入128M, 加快dump速度
	unsigned buf_size = 128*1024*1024;
	char *buf = new char[buf_size];
	char *node_buf = new char[MAX_BINLOG_ITEM_LEN];

	uint32_t data_len = 0;
	uint32_t write_len = 0;

	char key[TMBHashKey::C_DATA_LEN];
	for(CacheAccess::dirty_iterator it = begin(); it != end();it++)
	{
		if (0 != it.get(key, node_buf, MAX_BINLOG_ITEM_LEN, data_len))
		{
			break;
		}

		if (buf_size < (write_len + TMBHashKey::C_DATA_LEN + sizeof(uint32_t) + data_len))
		{
			// 当前的节点不能入buf了，先把buf写入文件,buf已使用长度归0
			fwrite(buf, write_len, 1, fp);
			write_len = 0;
		}

		memcpy(buf + write_len, key, TMBHashKey::C_DATA_LEN);
		write_len += TMBHashKey::C_DATA_LEN;
		memcpy(buf + write_len, &data_len, sizeof(uint32_t));
		write_len += sizeof(uint32_t);
		memcpy(buf + write_len, node_buf, data_len);
		write_len += data_len;
	}

	// 最后再写一次
	if (write_len > 0)
	{
		fwrite(buf, write_len, 1, fp);
		write_len = 0;
	}

	uint64_t len = ftell(fp);
	fclose(fp);

	rename(szcorefile, tmp_name);
	rename(last_file, szcorefile);
	rename(tmp_name, last_file);

	delete[] buf;
	delete[] node_buf;

	return len;
}

int CacheAccess::core_recover(char *szcorefile)
{
	FILE *fp =fopen(szcorefile,"r+");
	if (!fp)
	{
		return -1;
	}

	int buf_size = 32*1024*1024;
	char* buf = new char[buf_size];

	char key[TMBHashKey::C_DATA_LEN];
	uint32_t data_len=0;
	uint32_t read_len = 0;
	TMBHashKey hash_key;
	THashNode* node = NULL;
	while(!feof(fp))
	{
		fread(key,TMBHashKey::C_DATA_LEN,1,fp);
		fread(&data_len,sizeof(uint32_t),1,fp);

		if (data_len > (uint32_t)buf_size)
		{
			fclose(fp);
			delete[] buf;
			return -1;
		}

		read_len = fread(buf,1,data_len,fp);
		if (read_len != data_len)
		{
			break;
		}

		memcpy(hash_key._.md5_, key, TMBHashKey::C_DATA_LEN);
		node = _cache.find_node(hash_key);
		if(node == NULL)
			node = _cache.insert_node(hash_key, (void*)buf, data_len);
		else
			node = _cache.update_node(node, (void*)buf, data_len);

		if (node != NULL)
			_cache.set_node_flag(node, NODE_FLAG_DIRTY);
	}

	delete[] buf;
	fclose(fp);
	return 0;
}

//每隔coredump_min 分钟进行core, 位置coredump_dir
//coredump_min == 0不core，只log
//coredump_min == -1无core,无log
//such as CoreInit(3,"../log/smcd1.dump","../log/smcd1bin_",10000000,5,0);
//default is no core
int CacheAccess::CoreInit(int coredump_min, unsigned coredump_point,
		char *coredump_file,char * sBinLogBaseName,
		long lMaxBinLogSize, int iMaxBinLogNum)
{
	if (coredump_point >= 24*60)
	{
		assert(coredump_point < 1440);
	}

	_cache_dump_point = coredump_point;
	_cache_dump_min = coredump_min;
	init_lastdumptime();

	memset(_cache_dump_file,0,sizeof(_cache_dump_file));
	strncpy(_cache_dump_file,coredump_file,sizeof(_cache_dump_file)-1);

	_binlog.Init(sBinLogBaseName, lMaxBinLogSize, iMaxBinLogNum);
	return 0;
}


//ret 0: 恢复了dump file
//    1：没有恢复
int CacheAccess::StartUp()
{
	//非新建的共享内存,不用恢复
#ifdef _CACHE_COMPLETE_CHECK
	if (!_cacheinit && _cache.get_complete_flag())
#else
	if (!_cacheinit)
#endif
	{
		return 1;
	}

	int len = 0;
	// _cache_dump_min == 0代表不需要core_recover
	if (access(_cache_dump_file, F_OK) == 0 && _cache_dump_min > 0)
	{
		//恢复core
		core_recover(_cache_dump_file);
	}

	//恢复日志流水
	_binlog.SetReadRecordStartTime(-1);

	char key[TMBHashKey::C_DATA_LEN];
	uint32_t data_len = 0;
	int op = 0;
	TMBHashKey hash_key;
	THashNode* node = NULL;
	char* data = NULL;
	while(0<(len = _binlog.ReadRecordFromBinLog(szBuff, MAX_BINLOG_ITEM_LEN)))
	{
		memcpy(&op,szBuff,sizeof(int));
		memcpy(key,szBuff+sizeof(int),TMBHashKey::C_DATA_LEN);
		memcpy(hash_key._.md5_, key, TMBHashKey::C_DATA_LEN);
		if (op == op_set)
		{
			memcpy(&data_len,szBuff+sizeof(int)+TMBHashKey::C_DATA_LEN,sizeof(int));
			//set(key,szBuff+sizeof(int)+TMBHashKey::C_DATA_LEN+sizeof(int),data_len);
			data = szBuff+sizeof(int)+TMBHashKey::C_DATA_LEN+sizeof(int);

			node = _cache.find_node(hash_key);
			if(node == NULL)
				node = _cache.insert_node(hash_key, (void*)data, data_len);
			else
				node = _cache.update_node(node, (void*)data, data_len);

			if (node != NULL)
				_cache.set_node_flag(node, NODE_FLAG_DIRTY);
		}
		else if  (op == op_del)
		{
			node = _cache.find_node(hash_key);
			if(node != NULL)
				_cache.delete_node(node);
			//del(key);
		}
		else if (op == op_mark_clean)
		{
			//mark_clean(key);
			node = _cache.find_node(hash_key);
			if(node != NULL)
				_cache.set_node_flag(node, NODE_FLAG_UNCHG);
		}
	}

#ifdef _CACHE_COMPLETE_CHECK
	_cache.set_complete_flag(true);
#endif

	return 0;
}

// ret 0: 完成dump
//     1；没有dump
int CacheAccess::time_check()
{
	// cache_dump_min == 0代表只写binlog，不dump
	if (_cache_dump_min <= 0)
		return 0;

	time_t tNow = time(NULL);
	struct tm stTmNow;
	struct tm stTmLast;

	localtime_r(&tNow,&stTmNow);
	localtime_r(&_lastdumptime,&stTmLast);

	bool bNeedCore = false;
	unsigned min_of_day = 60 * stTmNow.tm_hour + stTmNow.tm_min;
	//新的一天，且到达dump_point，必须core一次
	if ((stTmLast.tm_mday != stTmNow.tm_mday) && (min_of_day == _cache_dump_point))
	{
		bNeedCore = true;
	}

	if(tNow -_lastdumptime >=  (_cache_dump_min*60))
	{
		bNeedCore = true;
	}

	if (bNeedCore)
	{
		if (0 <= core_dump(_cache_dump_file))
		{
			//delete binlog
			_binlog.ClearAllBinLog();
		}
		_lastdumptime = tNow;
		return 0;
	}

	return 1;
}

// for mirror dump
int CacheAccess::mirror_dump(char *mirror_file)
{
	// tmp file format: ***.dump.pid
	char last_file[256], tmp_name[256];
	snprintf(last_file, sizeof(last_file), "%s.last", mirror_file);
	snprintf(tmp_name, sizeof(tmp_name), "%s.tmp", mirror_file);

	//64位系统下fopen,fread,fwrite默认是64位操作
	// get share memory
	// 写到file
	FILE *fp = fopen(last_file, "w");
	if (fp == NULL)
	{
		return -1;
	}
	fwrite(_mem, _mem_size, 1, fp);
	fclose(fp);

	// 重命名旧的dump file
	rename(mirror_file, tmp_name);
	// 重命名新的dump file
	rename(last_file, mirror_file);
	// 重命名旧的dump file
	rename(tmp_name, last_file);

	return 0;
}

int CacheAccess::mirror_recover(char *mirror_file)
{
	// 检查dump file是否存在
	struct stat st_stat;
	int ret = ::stat(mirror_file, &st_stat);
	if (ret == -1)
	{
		assert(errno == ENOENT);
		return 0;
	}

	// 检查dump file大小是否符合share memory size
	assert(st_stat.st_size == (off_t)_mem_size);

	FILE *fp = fopen(mirror_file, "r");
	assert(fp != NULL);

	fread(_mem, _mem_size, 1, fp);
	fclose(fp);

	return 0;
}

//ret 0: 恢复了dump file
//    1：没有恢复
int CacheAccess::StartUp_mirror()
{
	//非新建的共享内存,不用恢复
#ifdef _CACHE_COMPLETE_CHECK
	if (!_cacheinit && _cache.get_complete_flag())
#else
	if (!_cacheinit)
#endif
	{
		return 1;
	}

	int len = 0;
	// _cache_dump_min == 0代表不需要dump_recover
	if (access(_cache_dump_file, F_OK) == 0 && _cache_dump_min > 0)
	{
		//恢复mirror
		mirror_recover(_cache_dump_file);
#ifdef _CACHE_COMPLETE_CHECK
		_cache.set_complete_flag(false);
#endif
	}

	//恢复日志流水
	_binlog.SetReadRecordStartTime(-1);

	char key[TMBHashKey::C_DATA_LEN];
	uint32_t data_len = 0;
	int op = 0;
	TMBHashKey hash_key;
	THashNode* node = NULL;
	char* data = NULL;
	long expiretime=0;
	while(0<(len = _binlog.ReadRecordFromBinLog(szBuff, MAX_BINLOG_ITEM_LEN)))
	{
		memcpy(&op,szBuff,sizeof(int));
		memcpy(key,szBuff+sizeof(int),TMBHashKey::C_DATA_LEN);
		memcpy(hash_key._.md5_, key, TMBHashKey::C_DATA_LEN);
		if (op == op_set)
		{
			memcpy(&data_len,szBuff+sizeof(int)+TMBHashKey::C_DATA_LEN,sizeof(int));
			//set(key,szBuff+sizeof(int)+TMBHashKey::C_DATA_LEN+sizeof(int),data_len);
			data = szBuff+sizeof(int)+TMBHashKey::C_DATA_LEN+sizeof(int);
			memcpy(&expiretime, szBuff+sizeof(int)+TMBHashKey::C_DATA_LEN+sizeof(int)+data_len, sizeof(long));

			node = _cache.find_node(hash_key);
			if(node == NULL)
				node = _cache.insert_node_metadata(hash_key, (void*)data, data_len, expiretime);
			else
				node = _cache.update_node_metadata(node, (void*)data, data_len, expiretime);

			if (node != NULL)
				_cache.set_node_flag(node, NODE_FLAG_DIRTY);
		}
		else if  (op == op_del)
		{
			node = _cache.find_node(hash_key);
			if(node != NULL)
				_cache.delete_node(node);
			//del(key);
		}
		else if (op == op_mark_clean)
		{
			//mark_clean(key);
			node = _cache.find_node(hash_key);
			if(node != NULL)
				_cache.set_node_flag(node, NODE_FLAG_UNCHG);
		}
	}

#ifdef _CACHE_COMPLETE_CHECK
	_cache.set_complete_flag(true);
#endif

	return 0;
}

// 0: 进行了dump
// 1: 没有进行dump
int CacheAccess::time_check_mirror()
{
	bool need_dump = is_need_dump();

	if (need_dump == true)
	{
		if (0 <= mirror_dump(_cache_dump_file))
		{
			//delete binlog
			_binlog.ClearAllBinLog();
		}
		_lastdumptime = time(NULL);

		return 0;
	}

	return 1;
}

bool CacheAccess::is_need_dump()
{
	// cache_dump_min == 0代表只写binlog，不dump
	if (_cache_dump_min <= 0)
	{
		return false;
	}

	time_t tNow = time(NULL);
	struct tm stTmNow;
	struct tm stTmLast;

	localtime_r(&tNow,&stTmNow);
	localtime_r(&_lastdumptime,&stTmLast);

	unsigned min_of_day = 60 * stTmNow.tm_hour + stTmNow.tm_min;
	//新的一天，且到达dump_point，必须dump一次
	if ((stTmLast.tm_mday != stTmNow.tm_mday) && (min_of_day == _cache_dump_point))
	{
		return true;
	}

	if(tNow -_lastdumptime >= (_cache_dump_min*60))
	{
		return true;
	}

	return false;
}

void CacheAccess::init_lastdumptime()
{
	// 通过dump_point, dump_min计算程序启动时的_lastdumptime
	// 计算原则：
	// 1、若n满足 dump_point + n*dump_min <= now < dump_point + (n+1)*dump_min
	// 2、_lastdumptime = dump_point + n*dump_min

	time_t now = time(NULL);
	struct tm st_now;

	if (_cache_dump_min == 0)
	{
		_lastdumptime = now;
		return;
	}

	if (_cache_dump_min >= 60 * 24) // 间隔时间超过1天
	{
		_lastdumptime = now;
		return;
	}

	localtime_r(&now, &st_now);
	int min_of_day = 60 * st_now.tm_hour + st_now.tm_min;
	int last_dump_min = 0;
	if ((int)_cache_dump_point <= min_of_day)
	{
		last_dump_min = min_of_day - (min_of_day - _cache_dump_point) % _cache_dump_min;
	}
	else if ((int)_cache_dump_point > min_of_day)
	{
		// 可能为负数，表示昨天
		last_dump_min = min_of_day-_cache_dump_min+(_cache_dump_point-min_of_day)%_cache_dump_min;
	}

	//  把last_dump_min换算成_lastdumptime
	_lastdumptime = now - (min_of_day - last_dump_min) * 60;

	printf("min_of_day:%d last_dump_min:%d\n", min_of_day, last_dump_min);
}

bool CacheAccess::warning_80persent()
{
    uint64_t used_node = _cache.used_node_num();
    uint64_t total_node = _cache.get_node_total();
    uint64_t used_chunk = _cache.get_used_chunk_num();
    uint64_t total_chunk = _cache.get_chunk_total();
    double node_percentage = used_node * 1.0 / total_node;
    double chunk_percentage = used_chunk * 1.0 / total_chunk;
    return ((node_percentage > 0.8) || (chunk_percentage > 0.8));
}

float CacheAccess::get_used_percent()
{
    float used_node = _cache.used_node_num()*1.0 / _cache.get_node_total();
    float used_chunk = _cache.get_used_chunk_num() * 1.0 /_cache.get_chunk_total();

    if (used_node > used_chunk) return used_node;
    return used_chunk;
}

void CacheAccess::get_node_num(
    uint64_t &hash_node_used, uint64_t &hash_node_total,
    uint64_t &chunk_node_used, uint64_t &chunk_node_total)
{
    hash_node_used = _cache.used_node_num();
    hash_node_total = _cache.get_node_total();
    chunk_node_used = _cache.get_used_chunk_num();
    chunk_node_total = _cache.get_chunk_total();
}
//////////////////////////////////////////////////////////////////////////

int CacheAccessUin::set(uint64_t uin, const char* data, uint32_t data_len,ENodeFlag flag/*=NODE_FLAG_DIRTY*/, bool need_sync/*=false*/)
{
	//	find
	TMBHashKey hash_key;
	hash_key._.uin_ = uin;

	return _da.set(hash_key._.md5_, data, data_len,flag,need_sync);
}

int CacheAccessUin::del(uint64_t uin, bool need_sync/*=false*/)
{
	TMBHashKey hash_key;
	hash_key._.uin_ = uin;

	return _da.del(hash_key._.md5_, need_sync);
}

int CacheAccessUin::get(uint64_t uin, char* buf, uint32_t buf_size
		, uint32_t& data_len, bool& dirty_flag, long& time_stamp)
{
	TMBHashKey hash_key;
	hash_key._.uin_ = uin;

	return _da.get(hash_key._.md5_, buf, buf_size, data_len, dirty_flag, time_stamp);
}

int CacheAccessUin::get_key(uint64_t uin, uint32_t& data_len, bool& dirty_flag, long& time_stamp)
{
	TMBHashKey hash_key;
	hash_key._.uin_ = uin;

	return _da.get_key(hash_key._.md5_, data_len, dirty_flag, time_stamp);
}

int CacheAccessUin::oldest(char* buf, uint32_t buf_size
						   , uint64_t& uin, uint32_t& data_len
						   , bool& dirty_flag, long& time_stamp)
{
	TMBHashKey hash_key;
	int ret = _da.oldest(buf, buf_size, hash_key._.md5_, data_len, dirty_flag, time_stamp);
	uin = hash_key._.uin_;
	return ret;
}

int CacheAccessUin::oldest_key(uint64_t& uin, uint32_t& data_len
						   , bool& dirty_flag, long& time_stamp)
{
	TMBHashKey hash_key;
	int ret = _da.oldest_key(hash_key._.md5_, data_len, dirty_flag, time_stamp);
		uin = hash_key._.uin_;
	return ret;
}

int CacheAccessUin::get_lru_pre(uint64_t uin, uint64_t& pre_uin
								, char* buf, uint32_t buf_size, uint32_t& data_len
								, bool& dirty_flag, long& time_stamp)
{
	TMBHashKey hash_key;
	hash_key._.uin_ = uin;
	char pre_key[16] = {0};

	int ret = _da.get_lru_pre(hash_key._.md5_, pre_key, buf, buf_size, data_len, dirty_flag, time_stamp);
	pre_uin = *(uint64_t*)pre_key;
	return ret;
}

int CacheAccessUin::mark_clean(uint64_t uin, bool need_sync/*=false*/)
{
	TMBHashKey hash_key;
	hash_key._.uin_ = uin;

	return _da.mark_clean(hash_key._.md5_, need_sync);
}

int CacheAccessUin::mark_clean(uint64_t modbase, uint64_t mobres, bool need_sync/*=false*/)
{
	return _da.mark_clean(modbase, mobres, need_sync);
}

int CacheAccessUin::del_node(uint64_t modbase, uint64_t mobres)
{
	return _da.del_node(modbase, mobres);
}

int CacheAccessUin::core_dump_mem(char *buff, uint64_t maxsize,uint64_t modbase, uint64_t mobres)
{
	return _da.core_dump_mem(buff,maxsize,modbase,mobres);
}
int CacheAccessUin::core_recover_mem(char *buff, uint64_t buffsize)
{
	return _da.core_recover_mem(buff,buffsize);
}
int CacheAccessUin::core_dump_bucket_mem(char *buff, uint64_t maxsize, uint64_t bucket_id, uint64_t &core_size)
{
	return _da.core_dump_bucket_mem(buff, maxsize, bucket_id, core_size);
}
int CacheAccessUin::core_recover_bucket_mem(char *buff, uint64_t buffsize, uint64_t &recover_size)
{
	return _da.core_recover_bucket_mem(buff, buffsize, recover_size);
}
int CacheAccessUin::core_dump(char *szcorefile)
{
	return _da.core_dump(szcorefile);
}
int CacheAccessUin::core_recover(char *szcorefile)
{
	return _da.core_recover(szcorefile);
}
int CacheAccessUin::mirror_dump(char *szcorefile)
{
	return _da.mirror_dump(szcorefile);
}
int CacheAccessUin::mirror_recover(char *szcorefile)
{
	return _da.mirror_recover(szcorefile);
}
int CacheAccessUin::CoreInit(int coredump_min, unsigned coredump_point, char *coredump_file,
		char * sBinLogBaseName, long lMaxBinLogSize, int iMaxBinLogNum)
{
	return _da.CoreInit(coredump_min,coredump_point,coredump_file,
			sBinLogBaseName,lMaxBinLogSize,iMaxBinLogNum);
}
int CacheAccessUin::StartUp()
{
	return _da.StartUp();
}
int CacheAccessUin::time_check()
{
	return _da.time_check();
}

bool CacheAccessUin::warning_80persent()
{
    return _da.warning_80persent();
}

float CacheAccessUin::get_used_percent()
{
	return _da.get_used_percent();
}

void CacheAccessUin::get_node_num(
    uint64_t &hash_node_used, uint64_t &hash_node_total,
    uint64_t &chunk_node_used, uint64_t &chunk_node_total)
{
    _da.get_node_num(hash_node_used, hash_node_total, chunk_node_used, chunk_node_total);
}
//////////////////////////////////////////////////////////////////////////
///:~
