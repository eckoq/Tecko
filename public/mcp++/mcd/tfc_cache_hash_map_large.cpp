#include <math.h>
#include <time.h>
#include <stdio.h>

#include "tfc_cache_hash_map_large.h"

using namespace tfc::cache_large;

CHashMap::CHashMap()
{
	pool_ = NULL;
	pool_tail_ = NULL;
	hash_map_ = NULL;
	hash_node_ = NULL;
//	right_rotate_ = 0;
}

// Clear things up.
CHashMap::~CHashMap()
{
	//NOTHING TO DO
}

int CHashMap::open(char* pool, bool init, uint64_t node_total, uint64_t bucket_size, uint64_t n_chunks, uint32_t chunk_size)
{
	int ret = 0;

	uint64_t hash_map_pool_size = get_pool_size(node_total, bucket_size);
	uint32_t head_size = sizeof(THashMap) - sizeof(BC_MEM_HANDLER_64[1]);
	uint64_t bucket_total_size = bucket_size * sizeof(BC_MEM_HANDLER_64);

	pool_ = pool;
	pool_tail_ = pool_ + hash_map_pool_size;
	hash_map_ = (THashMap*)pool_;
	hash_node_ = (THashNode*)(pool_ + head_size + bucket_total_size);

	if (init)
	{
		init_pool_data(node_total, bucket_size);
	}
	else
	{
		if ((ret = verify_pool_data(node_total, bucket_size)) != 0)
		{
            fprintf(stderr, "hash map verify_pool_data failed\n");
			return ret;
		}
	}

	if ((ret = allocator_.open(pool_tail_, init, n_chunks, chunk_size)) != 0)
	{
		return ret;
	}

	//bucket_size_应该是pow(2, x)
	//right_rotate_ = 32 -(int)( log((double)(hash_map_->bucket_size_ + 0.01)) / log(2.0));
	//DEBUG_P(LOG_TRACE, " CHashMap::open right_rotate %d\n", right_rotate_);

	return 0;
}

void CHashMap::init_pool_data(uint64_t node_total, uint64_t bucket_size)
{
#ifdef _CACHE_COMPLETE_CHECK
	this->set_complete_flag(false);
#endif

	hash_map_->node_total_ = node_total;
	hash_map_->bucket_size_ = bucket_size;
	hash_map_->used_node_num_ = 0;
	hash_map_->used_bucket_num_ = 0;

	hash_map_->add_head_ = INVALID_BC_MEM_HANDLER_64;
	hash_map_->add_tail_ = INVALID_BC_MEM_HANDLER_64;
	hash_map_->free_list_ = INVALID_BC_MEM_HANDLER_64;

	uint64_t i;
	for(i = 0; i < bucket_size; i++)
	{
		hash_map_->bucket[i] = INVALID_BC_MEM_HANDLER_64;
	}

	//将所有节点插入到空闲链表中
	THashNode* hash_node;
	BC_MEM_HANDLER_64 offset;
	for(i = 0; i < node_total; i++)
	{
		offset = i * (sizeof(THashNode));
		hash_node = (THashNode*)((char*)hash_node_ + offset);
		init_node(hash_node);
		free_list_insert(hash_node);
	}

	return;
}

int CHashMap::verify_pool_data(uint64_t node_total, uint64_t bucket_size)
{
	if (node_total != hash_map_->node_total_)
	{
		//ERROR_RETURN(HASH_MAP_ERROR_BASE, "pool data verify fail");
		return HASH_MAP_ERROR_BASE;
	}
	if (bucket_size != hash_map_->bucket_size_)
	{
		//ERROR_RETURN(HASH_MAP_ERROR_BASE, "pool data verify fail");
		return HASH_MAP_ERROR_BASE;
	}

//	int used_bucket_count = 0;
//	for (int i = 0; i < hash_map_->bucket_size_; i++)
//	{
//		if (hash_map_->bucket[i] != INVALID_BC_MEM_HANDLER)
//		{
//			used_bucket_count ++;
//		}
//	}
//	if (used_bucket_count != hash_map_->used_bucket_num_)
//	{
//		//ERROR_RETURN(HASH_MAP_ERROR_BASE, "pool data verify fail");
//		return HASH_MAP_ERROR_BASE;
//	}

	uint64_t free_node_count = 0;
	THashNode* free_node = handler2ptr(hash_map_->free_list_);
	while(free_node)
	{
		free_node_count++;
		free_node = handler2ptr(free_node->node_next_);
	}

	if ((hash_map_->used_node_num_ + free_node_count) != hash_map_->node_total_)
	{
		//ERROR_RETURN(HASH_MAP_ERROR_BASE, "pool data verify fail");
		return HASH_MAP_ERROR_BASE;
	}

	return 0;
}

THashNode* CHashMap::find_node(TMBHashKey &key)
{
	uint64_t bucket_id = get_bucket_id(key);
	BC_MEM_HANDLER_64 node_hdr = hash_map_->bucket[bucket_id];
	while(node_hdr != INVALID_BC_MEM_HANDLER_64)
	{
		THashNode* node = handler2ptr(node_hdr);
		if (node->key_ == key)
		{
			//将该节点插入到附加链表头部
			//node->add_info_1_ = time(NULL);
			insert_add_list_head(node);
			return node;
		}
		node_hdr = node->node_next_;
	}
	//ERROR_RETURN_NULL(HASH_MAP_ERROR_NODE_NOT_EXIST, "node not exist");
	return NULL;
}

THashNode* CHashMap::insert_node(TMBHashKey &key, void* new_data, uint32_t new_len)
{
	THashNode* node = free_list_remove();
	if (node == NULL)
	{
		return NULL;
	}

	uint32_t new_chunk_num = allocator_.get_chunk_num(new_len);
	BC_MEM_HANDLER_64 head_hdr = allocator_.malloc(new_chunk_num);
	if(head_hdr == INVALID_BC_MEM_HANDLER_64)
	{
		free_list_insert(node);
		return NULL;
	}
	allocator_.split(head_hdr, new_data, new_len);
	use_node(node, key, new_len, head_hdr);

	//将该节点插入到附加链表头部
	node->add_info_1_ = time(NULL);
	insert_add_list_head(node);
	return node;
}

THashNode* CHashMap::insert_node_metadata(TMBHashKey &key, void* new_data, uint32_t new_len, long expiretime)
{
	THashNode* node = free_list_remove();
	if (node == NULL)
	{
		return NULL;
	}

	uint32_t new_chunk_num = allocator_.get_chunk_num(new_len);
	BC_MEM_HANDLER_64 head_hdr = allocator_.malloc(new_chunk_num);
	if(head_hdr == INVALID_BC_MEM_HANDLER_64)
	{
		free_list_insert(node);
		return NULL;
	}
	allocator_.split(head_hdr, new_data, new_len);
	use_node(node, key, new_len, head_hdr);

	//将该节点插入到附加链表头部
	node->add_info_1_ = time(NULL);
	node->add_info_2_ = expiretime;
	insert_add_list_head(node);
	return node;
}

THashNode* CHashMap::update_node(THashNode* node, void* new_data, uint32_t new_len, char* old_data, uint32_t* old_len)
{
	if(old_data != NULL && old_len != NULL)
	{
		//返回旧数据
		if(allocator_.merge(node->chunk_head_, node->chunk_len_,  old_data, old_len) != 0)
		{
			return NULL;
		}
	}
	else if(old_len != NULL)
	{
		*old_len = node->chunk_len_;
	}

	uint32_t old_chunk_num = allocator_.get_chunk_num(node->chunk_len_);
	uint32_t new_chunk_num = allocator_.get_chunk_num(new_len);

	if (old_chunk_num != new_chunk_num)
	{
		//需要重新分配CHUNK. 先FREE再MALLOC.
		if (new_chunk_num > old_chunk_num)
		{
			if (allocator_.get_free_chunk_num() < (new_chunk_num - old_chunk_num))
			{
				//剩余CHUNK数不足
				//ERROR_RETURN_NULL(CChunkAllocator::CHUNK_ALLOCATOR_ERROR_FREE_CHUNK_LACK, "free chunk lack");
				return NULL;
			}
		}

		allocator_.free(node->chunk_head_);

		BC_MEM_HANDLER_64 head_hdr = allocator_.malloc(new_chunk_num);   //CHUNK数足够, 不会失败
		allocator_.split(head_hdr, new_data, new_len);

		node->chunk_len_ = new_len;
		node->chunk_head_ = head_hdr;
	}
	else
	{
		allocator_.split(node->chunk_head_, new_data, new_len);
		node->chunk_len_ = new_len;
	}

	//将该节点插入到附加链表头部
	node->add_info_1_ = time(NULL);
	insert_add_list_head(node);
	return node;
}

THashNode* CHashMap::update_node_metadata(THashNode* node, void* new_data, uint32_t new_len, long expiretime)
{
	uint32_t old_chunk_num = allocator_.get_chunk_num(node->chunk_len_);
	uint32_t new_chunk_num = allocator_.get_chunk_num(new_len);

	if (old_chunk_num != new_chunk_num)
	{
		//需要重新分配CHUNK. 先FREE再MALLOC.
		if (new_chunk_num > old_chunk_num)
		{
			if (allocator_.get_free_chunk_num() < (new_chunk_num - old_chunk_num))
			{
				//剩余CHUNK数不足
				//ERROR_RETURN_NULL(CChunkAllocator::CHUNK_ALLOCATOR_ERROR_FREE_CHUNK_LACK, "free chunk lack");
				return NULL;
			}
		}

		allocator_.free(node->chunk_head_);

		BC_MEM_HANDLER_64 head_hdr = allocator_.malloc(new_chunk_num);   //CHUNK数足够, 不会失败
		allocator_.split(head_hdr, new_data, new_len);

		node->chunk_len_ = new_len;
		node->chunk_head_ = head_hdr;
	}
	else
	{
		allocator_.split(node->chunk_head_, new_data, new_len);
		node->chunk_len_ = new_len;
	}

	//将该节点插入到附加链表头部
	node->add_info_1_ = time(NULL);
	node->add_info_2_ = expiretime;
	insert_add_list_head(node);
	return node;
}
THashNode* CHashMap::replace_node(TMBHashKey &key, void* new_data, uint32_t new_len, char* old_data, uint32_t* old_len)
{
	THashNode* node = find_node(key);
	if(node != NULL)
	{
		return update_node(node, new_data, new_len, old_data, old_len);
	}

	return insert_node(key, new_data, new_len);
}

int CHashMap::delete_node(THashNode* node, char* data, uint32_t* data_len)
{
	//旧节点存在
	if(data != NULL && data_len != NULL)
	{
		//返回旧数据
		if(allocator_.merge(node->chunk_head_, node->chunk_len_, data, data_len) != 0)
		{
			return -1;
		}
	}
	else if(data_len != NULL)
	{
		*data_len = node->chunk_len_;
	}

	delete_from_add_list(node);
	free_node(node);
	free_list_insert(node);

	return 0;
}

void CHashMap::insert_node_list(THashNode* node)
{
	//插入到节点链表头
	uint64_t bucket_id = get_bucket_id(node->key_);
	BC_MEM_HANDLER_64 node_hdr = ptr2handler(node);

	node->node_next_ = hash_map_->bucket[bucket_id];
	node->node_prev_ = INVALID_BC_MEM_HANDLER_64;
	hash_map_->bucket[bucket_id] = node_hdr;
	THashNode* next_node = handler2ptr(node->node_next_);
	if(next_node != NULL)
	{
		next_node->node_prev_ = node_hdr;
	}

	//stat
	hash_map_->used_node_num_ ++;
}

void CHashMap::delete_from_node_list(THashNode* node)
{
	BC_MEM_HANDLER_64 next_node_hdr = node->node_next_;
	BC_MEM_HANDLER_64 prev_node_hdr = node->node_prev_;

	if(prev_node_hdr != INVALID_BC_MEM_HANDLER_64)
	{
		THashNode* prev_node = handler2ptr(prev_node_hdr);
		prev_node->node_next_ = node->node_next_;
	}
	if(next_node_hdr != INVALID_BC_MEM_HANDLER_64)
	{
		THashNode* next_node = handler2ptr(next_node_hdr);
		next_node->node_prev_ = node->node_prev_;
	}

	BC_MEM_HANDLER_64 node_hdr = ptr2handler(node);

	uint64_t bucket_id = get_bucket_id(node->key_);
	if (node_hdr == hash_map_->bucket[bucket_id])
	{
		//当前节点为链表头节点
		hash_map_->bucket[bucket_id] = next_node_hdr;

	}

	//将前后链表指针清零
	node->node_next_ = INVALID_BC_MEM_HANDLER_64;
	node->node_prev_ = INVALID_BC_MEM_HANDLER_64;

	//stat
	hash_map_->used_node_num_ --;
}


void CHashMap::free_node(THashNode *node)
{
	//从链表中删除
	delete_from_node_list(node);

	//释放 chunk
	allocator_.free(node->chunk_head_);

	//stat
//	int bucket_list_len = get_bucket_list_len(get_bucket_id(node->key_));
//	if (bucket_list_len == 0)
//	{
//		//the bucket change to unused
//		hash_map_->used_bucket_num_ --;
//	}

	//reset member
	init_node(node);
}

void CHashMap::use_node(THashNode *node, TMBHashKey &key, uint32_t chunk_len, BC_MEM_HANDLER_64 chunk_head)
{
	//set member
	node->key_ = key;
	node->chunk_len_ = chunk_len;
	node->chunk_head_ = chunk_head;
	node->add_info_1_ = 0;
	node->add_info_2_ = 0;


//	int bucket_list_len = get_bucket_list_len(get_bucket_id(node->key_));
//	if (bucket_list_len == 0)
//	{
//		//the bucket change from unused
//		hash_map_->used_bucket_num_ ++;
//	}

	insert_node_list(node);
	return;
}

uint64_t CHashMap::get_bucket_list_len(uint64_t bucket_id)
{
	uint64_t num = 0;

	BC_MEM_HANDLER_64 node_hdr;
	node_hdr = hash_map_->bucket[bucket_id];

	while (node_hdr != INVALID_BC_MEM_HANDLER_64)
	{
		num ++;
		THashNode* node = handler2ptr(node_hdr);
		node_hdr = node->node_next_;
	}

	return num;
}
void CHashMap::insert_add_list_head(THashNode* node)
{
	delete_from_add_list(node);
	BC_MEM_HANDLER_64 node_hdr = ptr2handler(node);

	//insert node into head of add list
	node->add_next_ = hash_map_->add_head_;
	hash_map_->add_head_ = node_hdr;

	if (hash_map_->add_tail_ == INVALID_BC_MEM_HANDLER_64)
	{
		hash_map_->add_tail_ = node_hdr;
	}

	node->add_prev_ = INVALID_BC_MEM_HANDLER_64;
	THashNode* next_node = handler2ptr(node->add_next_);
	if(next_node != NULL)
	{
		next_node->add_prev_ = node_hdr;
	}

}

void CHashMap::insert_add_list_tail(THashNode* node)
{
	delete_from_add_list(node);
	//reform add list, insert to head
	BC_MEM_HANDLER_64 node_hdr = ptr2handler(node);

	node->add_prev_ = hash_map_->add_tail_;
	hash_map_->add_tail_ = node_hdr;

	if (hash_map_->add_head_ == INVALID_BC_MEM_HANDLER_64)
	{
		hash_map_->add_head_ = node_hdr;
	}

	node->add_next_ = INVALID_BC_MEM_HANDLER_64;
	THashNode* prev_node = handler2ptr(node->add_prev_);
	if(prev_node != NULL)
	{
		prev_node->add_next_ = node_hdr;
	}
}

void CHashMap::delete_from_add_list(THashNode* node)
{
	//link the prev add node and the next add node
	BC_MEM_HANDLER_64 node_hdr = ptr2handler(node);
	BC_MEM_HANDLER_64 next_add_hdr = node->add_next_;
	BC_MEM_HANDLER_64 prev_add_hdr = node->add_prev_;

	if ((next_add_hdr == INVALID_BC_MEM_HANDLER_64) &&
		(prev_add_hdr == INVALID_BC_MEM_HANDLER_64) &&
		(hash_map_->add_head_ != node_hdr) &&
		(hash_map_->add_tail_ != node_hdr))
	{
		//不在链表中
		return ;
	}

	if(prev_add_hdr != INVALID_BC_MEM_HANDLER_64)
	{
		THashNode* prev_add = handler2ptr(prev_add_hdr);
		prev_add->add_next_ = node->add_next_;
	}
	if(next_add_hdr != INVALID_BC_MEM_HANDLER_64)
	{
		THashNode* next_add = handler2ptr(next_add_hdr);
		next_add->add_prev_ = node->add_prev_;
	}


	if (hash_map_->add_head_ == node_hdr)
	{
		hash_map_->add_head_ =  next_add_hdr;
	}
	if (hash_map_->add_tail_ == node_hdr)
	{
		hash_map_->add_tail_ =  prev_add_hdr;
	}

	//将前后链表指针清零
	node->add_prev_ = INVALID_BC_MEM_HANDLER_64;
	node->add_next_ = INVALID_BC_MEM_HANDLER_64;

}

THashNode* CHashMap::get_add_list_head()
{
	return handler2ptr(hash_map_->add_head_);
}

THashNode* CHashMap::get_add_list_tail()
{
	return handler2ptr(hash_map_->add_tail_);
}

THashNode* CHashMap::get_add_list_prev(THashNode* node)
{
	return handler2ptr(node->add_prev_);
}

THashNode* CHashMap::get_add_list_next(THashNode* node)
{
	return handler2ptr(node->add_next_);
}

void CHashMap::set_expiretime(THashNode* node, long expiretime)
{
	node->add_info_2_ = expiretime;
}

long CHashMap::get_expiretime(THashNode* node)
{
	return node->add_info_2_;
}
