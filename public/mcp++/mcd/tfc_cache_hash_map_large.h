
#ifndef _TFC_CACHE_HASH_MAP_LARGE_H_
#define _TFC_CACHE_HASH_MAP_LARGE_H_

//////////////////////////////////////////////////////////////////////////
//
//	author: sonicmao@tencent.com
//	create: 2006-02
//	mend: 2006-10
//
//
//////////////////////////////////////////////////////////////////////////

#include <cstring>
#include <cassert>
#include "tfc_cache_chunk_alloc_large.h"

namespace tfc{namespace cache_large
{
	typedef enum tagENodeFlag
	{
		NODE_FLAG_UNCHG = 0x00,
		NODE_FLAG_DIRTY = 0x01,
	}ENodeFlag;

#pragma pack(1)
	typedef struct tagTMBHashKey
	{
		static const unsigned C_DATA_LEN = 16;
		union un_key
		{
			char md5_[C_DATA_LEN];
			uint64_t uin_;
		};
		un_key _;

		tagTMBHashKey(){memset(_.md5_, 0, C_DATA_LEN);}
		tagTMBHashKey& operator =(const tagTMBHashKey& right)
		{
			memcpy(_.md5_, right._.md5_, C_DATA_LEN);
			return *this;
		}
	}TMBHashKey;

	typedef struct tagTHashNode
	{
		TMBHashKey key_;				//索引
		uint32_t chunk_len_;					//CHUNK中的数据长度
		BC_MEM_HANDLER_64 chunk_head_;		//CHUNK 句柄
		BC_MEM_HANDLER_64 node_prev_;		//节点链表前指针
		BC_MEM_HANDLER_64 node_next_;		//节点链表后指针
		BC_MEM_HANDLER_64 add_prev_;		//附加链表前指针
		BC_MEM_HANDLER_64 add_next_;		//附加链表后指针
		long add_info_1_;				//最后访问时间
		long add_info_2_;				//过期时间
		int flag_;						//脏标记
	}THashNode;

	typedef struct tagTHashMap
	{
// fix bug: if user compile so without this macro, pack will case below var. access failed...
// #ifdef _CACHE_COMPLETE_CHECK
		int complete_flag_;				//StartUp系列接口完整性标记
// #endif
		uint64_t node_total_;				//节点总数
		uint64_t bucket_size_;			   	//HASH桶的大小
		uint64_t used_node_num_;			 	//使用的节点数
		uint64_t used_bucket_num_;		   	//HASH桶使用数
		BC_MEM_HANDLER_64 add_head_;	 	//附加链表头指针
		BC_MEM_HANDLER_64 add_tail_;	 	//附加链表尾指针
		BC_MEM_HANDLER_64 free_list_;		//空间节点链表头指针
		BC_MEM_HANDLER_64 bucket[1];	 	//HASH桶
	}THashMap;

#pragma pack()

	inline bool operator== (const TMBHashKey &lhs, const TMBHashKey &rhs)
	{
		return !memcmp(lhs._.md5_, rhs._.md5_, TMBHashKey::C_DATA_LEN);
	}

	class CHashMap
	{
	public:
		enum HASH_MAP_ERROR
		{
			HASH_MAP_ERROR_BASE = -1000,
			HASH_MAP_ERROR_INVALID_PARAM = HASH_MAP_ERROR_BASE -1,	//非法参数
			HASH_MAP_ERROR_NODE_NOT_EXIST = HASH_MAP_ERROR_BASE -2,	//节点不存在
			HASH_MAP_ERROR_NODE_HAVE_EXIST = HASH_MAP_ERROR_BASE -3,	//节点已经存在
			HASH_MAP_ERROR_NO_FREE_NODE = HASH_MAP_ERROR_BASE -4,	//没有空闲节点
		};

	public:
		CHashMap();
		~CHashMap();

		//初始化 HASH_MAP 内存块
		int open(char* pool, bool init, uint64_t node_total, uint64_t bucket_size, uint64_t n_chunks, uint32_t chunk_size);

		// 使用 <key> 进行查询.
		THashNode* find_node(TMBHashKey &key);
		//插入节点, 如果旧节点存在, 则返回失败
		THashNode* insert_node(TMBHashKey &key, void* new_data, uint32_t new_len);
		//插入节点并附带超时信息, 如果旧节点存在, 则返回失败
		THashNode* insert_node_metadata(TMBHashKey &key, void* new_data, uint32_t new_len, long expiretime);

		//修改节点
		THashNode* update_node(THashNode* node, void* new_data, uint32_t new_len, char* old_data = NULL, uint32_t* old_len = NULL);
		//修改节点并附带超时信息
		THashNode* update_node_metadata(THashNode* node, void* new_data, uint32_t new_len, long expiretime);

		//insert or update
		THashNode* replace_node(TMBHashKey &key, void* new_data, uint32_t new_len, char* old_data = NULL, uint32_t* old_len = NULL);
		//删除结点. 同时会将节点从附加链表中清除
		//返回值 = 0 表示成功, < 0 表示失败(如节点不存在,也返回失败)
		int delete_node(THashNode* node, char* data = NULL, uint32_t* data_len = NULL);

		int merge_node_data(THashNode* node, char* data, uint32_t* data_len);

		void set_expiretime(THashNode* node, long expiretime);
		long get_expiretime(THashNode* node);

		// 返回当前节点使用数
		uint64_t used_node_num() { return hash_map_->used_node_num_; }
		uint64_t free_node_num() { return hash_map_->node_total_ - hash_map_->used_node_num_; }
		uint64_t get_node_total() { return hash_map_->node_total_; }
		uint64_t get_bucket_used() { return hash_map_->used_bucket_num_; }
		uint64_t free_bucket_num() {return hash_map_->bucket_size_ - hash_map_->used_bucket_num_; }
		uint64_t get_bucket_size() {return hash_map_->bucket_size_;}
		uint64_t get_used_chunk_num() { return allocator_.get_used_chunk_num(); }
		uint64_t get_chunk_total() { return allocator_.get_chunk_total(); }
		uint32_t get_chunk_data_size() { return allocator_.get_chunk_data_size(); }

		CChunkAllocator* chunks() {return &allocator_; };

		// 计算HASH_MAP所需求的内存块尺寸
		static uint64_t get_pool_size(uint64_t node_total, uint64_t bucket_size)
		{
			uint32_t head_size = sizeof(THashMap) - sizeof(BC_MEM_HANDLER_64[1]);
			uint64_t bucket_total_size = bucket_size * sizeof(BC_MEM_HANDLER_64);
			uint64_t node_total_size = node_total * sizeof(THashNode);
			uint64_t pool_size = head_size + bucket_total_size + node_total_size;
			return pool_size;
		}
		// 取HASH_MAP 和CHUNK的内存块尺寸
		static uint64_t get_total_pool_size(uint64_t node_total, uint64_t bucket_size, uint64_t n_chunks, uint32_t chunk_size)
		{
			return get_pool_size(node_total, bucket_size) + CChunkAllocator::get_pool_size(n_chunks, chunk_size);
		}

#ifdef _CACHE_COMPLETE_CHECK
		void set_complete_flag(bool flag)
		{
			if ( flag ) {
				hash_map_->complete_flag_ = 1;
			} else {
				hash_map_->complete_flag_ = 0;
			}
		}

		bool get_complete_flag()
		{
			return hash_map_->complete_flag_ ? true : false;
		}
#endif

		/*
		//打印附加链表, <num> 指定打印的数目. <num> = 0, 打印所有节点.
		void print_add_list(int num = 0);
		//打印基本的统计信息
		void print_stat();
		*/
		//transform handler to address
		THashNode *handler2ptr(BC_MEM_HANDLER_64 handler);

		//transform address to handler
		BC_MEM_HANDLER_64 ptr2handler(THashNode* ptr);

		//附加链表操作方法
		void insert_add_list_head(THashNode* node);
		void insert_add_list_tail(THashNode* node);
		void delete_from_add_list(THashNode* node);
		THashNode* get_add_list_prev(THashNode* node);
		THashNode* get_add_list_next(THashNode* node);
		THashNode* get_add_list_head();
		THashNode* get_add_list_tail();
		////////////////

		void set_node_flag(THashNode * node, ENodeFlag f){assert(node); node->flag_ = (int)f;}
		ENodeFlag get_node_flag(THashNode *node){assert(node); return (ENodeFlag)node->flag_;}
		THashNode* get_bucket_list_head(uint64_t bucket_id);
		THashNode* get_bucket_list_prev(THashNode* node);
		THashNode* get_bucket_list_next(THashNode* node);

	protected:

		void init_pool_data(uint64_t node_total, uint64_t bucket_size);
		int verify_pool_data(uint64_t node_total, uint64_t bucket_size);

		//根据索引计算HASH桶值
		uint64_t get_bucket_id(TMBHashKey &key);
		uint64_t get_bucket_list_len(uint64_t bucket_id); //取HASH桶的碰撞数

		//将节点插入到空闲链表
		void free_list_insert(THashNode *node);
		//从空闲链表中取节点
		THashNode *free_list_remove();

		//节点链表操作方法
		void insert_node_list(THashNode* node);
		void delete_from_node_list(THashNode* node);

		//初始化节点
		void init_node(THashNode* node);
		//将节点置为空闲模式
		void free_node(THashNode *node);
		//将节点置为使用模式
		void use_node(THashNode *node, TMBHashKey &key, uint32_t chunk_len, BC_MEM_HANDLER_64 chunk_head);

		char *pool_;		//内存块起始地址
		char *pool_tail_;   //内存块结束地址

		THashMap* hash_map_;   //内存块中的HASHMAP 结构
		THashNode* hash_node_; //内存块中的HASH节点数组
		tfc::cache_large::CChunkAllocator allocator_; //CHUNK分配器
	private:
		//原来在hash function里计算，现在在初始化时计算一次。
		//int right_rotate_;
};

//////////////////////////////////////////////////////////////////////////

inline uint64_t CHashMap::get_bucket_id(TMBHashKey &key)
{
	return key._.uin_ % hash_map_->bucket_size_;
}

inline THashNode* CHashMap::handler2ptr(BC_MEM_HANDLER_64 handler)
{
	if (handler == INVALID_BC_MEM_HANDLER_64)
		return NULL;

	return (THashNode*)(pool_ + handler);
}

inline BC_MEM_HANDLER_64 CHashMap::ptr2handler(THashNode* ptr)
{
	char *tmp_ptr = (char *)ptr;
	if((tmp_ptr < pool_) || (tmp_ptr >= pool_tail_))
		return INVALID_BC_MEM_HANDLER_64;
	else
		return (BC_MEM_HANDLER_64)(tmp_ptr - pool_);
}

inline void CHashMap::free_list_insert(THashNode *node)
{
	//insert to free list's head
	node->node_next_ = hash_map_->free_list_;
	BC_MEM_HANDLER_64 node_hdr = ptr2handler(node);
	hash_map_->free_list_ = node_hdr;
}

inline THashNode* CHashMap::free_list_remove()
{
	//get head node from free list
	if(hash_map_->free_list_ == INVALID_BC_MEM_HANDLER_64)
		//ERROR_RETURN_NULL(HASH_MAP_ERROR_NO_FREE_NODE, "no free node");
		return NULL;

	THashNode* head_node = handler2ptr(hash_map_->free_list_);
	hash_map_->free_list_ = head_node->node_next_;
	head_node->node_next_ = INVALID_BC_MEM_HANDLER_64;
	return head_node;
}

inline void CHashMap::init_node(THashNode* node)
{
	memset(node->key_._.md5_, 0, TMBHashKey::C_DATA_LEN);
	node->chunk_len_ = 0;
	node->add_info_1_ = 0;
	node->add_info_2_ = 0;
	node->flag_ = NODE_FLAG_UNCHG;

	node->chunk_head_ = INVALID_BC_MEM_HANDLER_64;
	node->node_next_= INVALID_BC_MEM_HANDLER_64;
	node->node_prev_= INVALID_BC_MEM_HANDLER_64;
	node->add_next_= INVALID_BC_MEM_HANDLER_64;
	node->add_prev_= INVALID_BC_MEM_HANDLER_64;
}

inline THashNode*  CHashMap::get_bucket_list_head(uint64_t bucket_id)
{
	assert(bucket_id < hash_map_->bucket_size_);
	BC_MEM_HANDLER_64 node_hdr = hash_map_->bucket[bucket_id];
	return node_hdr != INVALID_BC_MEM_HANDLER_64 ? handler2ptr(node_hdr) : NULL;
}

inline THashNode*  CHashMap::get_bucket_list_prev(THashNode* node)
{
	assert(node);
	return node->node_prev_!= INVALID_BC_MEM_HANDLER_64 ? handler2ptr( node->node_prev_) : NULL;
}
inline THashNode*  CHashMap::get_bucket_list_next(THashNode* node)
{
	assert(node);
	return node->node_next_!= INVALID_BC_MEM_HANDLER_64 ? handler2ptr( node->node_next_) : NULL;
}

inline int CHashMap::merge_node_data(THashNode* node, char* data, uint32_t* data_len)
{
	return allocator_.merge(node->chunk_head_, node->chunk_len_, data, data_len);
}

}}

//////////////////////////////////////////////////////////////////////////
#endif//_TFC_CACHE_HASH_MAP_H_
///:~
