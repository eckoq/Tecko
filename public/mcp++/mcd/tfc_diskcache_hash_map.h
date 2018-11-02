
#ifndef _TFC_DISKCACHE_HASH_MAP_H_
#define _TFC_DISKCACHE_HASH_MAP_H_
#include <string>
#include <cstring>
#include <cassert>
#ifdef _TDC_DISKCACHE_
#include <stdlib.h>
#endif
//#include "tfc_diskcache_chunk_alloc.h"
#include "mydiskalloc.h"
typedef long BC_DISK_HANDLER;
const BC_DISK_HANDLER INVALID_BC_DISK_HANDLER = -1;

namespace tfc{namespace diskcache
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
			unsigned uin_;
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
		TMBHashKey key_;			   	//索引
		int chunk_len_;				 	//CHUNK中的数据长度
		//BC_DISK_HANDLER chunk_head_;   	//CHUNK 句柄
		DISK_HANDLE chunk_head_;   		//CHUNK 句柄
		BC_DISK_HANDLER node_prev_;		//节点链表前指针
		BC_DISK_HANDLER node_next_;		//节点链表后指针
		BC_DISK_HANDLER add_prev_;	 	//附加链表前指针
		BC_DISK_HANDLER add_next_;	 	//附加链表后指针
		long add_info_1_;				//最后访问时间
		long add_info_2_;				//超时时间
		int flag_;						//脏标记
#ifdef _TDC_DISKCACHE_
		unsigned int user_;				//用户自定义字段，可以用作数据健全
#endif
	}THashNode;
	
	typedef struct tagTHashMap
	{
		int node_total_;				//节点总数
		int bucket_size_;			   	//HASH桶的大小
		int used_node_num_;			 	//使用的节点数
		int used_bucket_num_;		   	//HASH桶使用数
		BC_DISK_HANDLER add_head_;	 	//附加链表头指针
		BC_DISK_HANDLER add_tail_;	 	//附加链表尾指针
		BC_DISK_HANDLER free_list_;		//空间节点链表头指针
		BC_DISK_HANDLER bucket[1];	 	//HASH桶
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
			HASH_MAP_ERROR_NODE_HAVE_EXIST = HASH_MAP_ERROR_BASE -3,//节点已经存在
			HASH_MAP_ERROR_NO_FREE_NODE = HASH_MAP_ERROR_BASE -4,	//没有空闲节点
		};
		
	public:
		CHashMap();
		~CHashMap();	
		
		//初始化 HASH_MAP 内存块
		//int open(char* pool, const std::string& file_path, bool init, int node_total, int bucket_size, int n_chunks, int chunk_size);
		int open(char* pool, const std::string& file_path, bool init, int node_total, int bucket_size, unsigned long long filesize, unsigned minchunksize);
		
		// 使用 <key> 进行查询.
		THashNode* find_node(TMBHashKey &key);	
#ifdef _TDC_DISKCACHE_
		// For TDC diskcache, not modify the LRU list.
		THashNode* find_node_rdonly(TMBHashKey &key);
		// Malloc memory in function. Caller must call free() to release the memory when data not be used.
		int merge_node_data_v2(THashNode *node, char **data, int *data_len);

		THashMap* get_hash_map() { return hash_map_; }
		mydiskalloc_head* get_disk_alloc_head() { return myhead_; }
		int data_dump(const char *dump_name);
		int data_recover(const char *dump_name, char *bak_name);
		mydiskalloc_chunk* node_2_chunk(THashNode *node);
#endif
		//插入节点, 如果旧节点存在, 则返回失败
		THashNode* insert_node(TMBHashKey &key, void* new_data, int new_len);
		//插入节点, 只写元数据,如果旧节点存在, 则返回失败
		THashNode* insert_node_metadata(TMBHashKey &key, int new_len, long expiretime);
	
		//修改节点
		THashNode* update_node(THashNode* node, void* new_data, int new_len, char* old_data = NULL, int* old_len = NULL);
		//insert or update
		//修改节点,只写元数据
		THashNode* update_node_metadata(THashNode* node, int new_len, long expiretime);

		THashNode* replace_node(TMBHashKey &key, void* new_data, int new_len, char* old_data = NULL, int* old_len = NULL);
		//删除结点. 同时会将节点从附加链表中清除
		//返回值 = 0 表示成功, < 0 表示失败(如节点不存在,也返回失败)
		int delete_node(THashNode* node, char* data = NULL, int* data_len = NULL);
		
		int merge_node_data(THashNode* node, char* data, int* data_len);
		int map_node_data(THashNode* node, char** data, int* data_len);
		void unmap_node_data(char* data, int data_len);

		void set_expiretime(THashNode* node, long expiretime);
		long get_expiretime(THashNode* node);
		
		// 返回当前节点使用数
		int used_node_num() { return hash_map_->used_node_num_; }
		int free_node_num() { return hash_map_->node_total_ - hash_map_->used_node_num_; }
		int get_node_total() { return hash_map_->node_total_; }
		int get_bucket_used() { return hash_map_->used_bucket_num_; }
		int free_bucket_num() {return hash_map_->bucket_size_ - hash_map_->used_bucket_num_; }
		int get_bucket_size() {return hash_map_->bucket_size_;}
//		int get_used_chunk_num() { return allocator_.get_used_chunk_num(); }
//		int get_chunk_total() { return allocator_.get_chunk_total(); }
		unsigned long long get_free_size() { return myhead_->freesize; }
		unsigned long long get_total_size() { return myhead_->totalsize; } 
		
//		CDiskChunkAllocator* chunks() {return &allocator_; };
		
		// 计算HASH_MAP所需求的内存块尺寸
		static int get_pool_size(int node_total, int bucket_size)
		{
			int head_size = sizeof(THashMap) - sizeof(BC_DISK_HANDLER[1]);
			int bucket_total_size = bucket_size * sizeof(BC_DISK_HANDLER);
			int node_total_size = node_total * sizeof(THashNode);
			int pool_size = head_size + bucket_total_size + node_total_size;
			return pool_size;		
		}
		// 取HASH_MAP 和CHUNK的内存块尺寸
		//static long get_total_pool_size(int node_total, int bucket_size, int n_chunks, int chunk_size)
		static long get_total_pool_size(int node_total, int bucket_size, unsigned long long filesize, unsigned minchunksize)
		{
			//return get_pool_size(node_total, bucket_size) + CDiskChunkAllocator::get_pool_size(n_chunks);
#ifndef _TDC_DISKCACHE_
			return get_pool_size(node_total, bucket_size) + mydiskalloc_calcsize(filesize, minchunksize);
#else
			return get_pool_size(node_total, bucket_size) + mydiskalloc_calcsize((unsigned)node_total);
#endif
		}
		/*
		//打印附加链表, <num> 指定打印的数目. <num> = 0, 打印所有节点.
		void print_add_list(int num = 0);
		//打印基本的统计信息
		void print_stat();
		*/
		//transform handler to address
		THashNode *handler2ptr(BC_DISK_HANDLER handler);
		
		//transform address to handler
		BC_DISK_HANDLER ptr2handler(THashNode* ptr);
		
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
		THashNode* get_bucket_list_head(unsigned bucket_id);
		THashNode* get_bucket_list_prev(THashNode* node);
		THashNode* get_bucket_list_next(THashNode* node);

#ifdef _TDC_DISKCACHE_
		void set_node_user(THashNode *node, unsigned int user) { assert(node); node->user_ = user; }
		unsigned int get_node_user(THashNode *node) { assert(node); return node->user_; }
#endif
		
	protected:
		
		void init_pool_data(int node_total, int bucket_size);
		int verify_pool_data(int node_total, int bucket_size);
		
		//根据索引计算HASH桶值
		int get_bucket_id(TMBHashKey &key);
		int get_bucket_list_len(int bucket_id); //取HASH桶的碰撞数
		
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
		//void use_node(THashNode *node, TMBHashKey &key, int chunk_len, BC_DISK_HANDLER chunk_head);
		void use_node(THashNode *node, TMBHashKey &key, int chunk_len, DISK_HANDLE chunk_head);
		
		char *pool_;		//内存块起始地址
		char *pool_tail_;   //内存块结束地址
		
		THashMap* hash_map_;   //内存块中的HASHMAP 结构
		THashNode* hash_node_; //内存块中的HASH节点数组
		//CDiskChunkAllocator allocator_; //Disk Chunk分配器
		mydiskalloc_head* myhead_; //Disk Chunk分配器
		
};

//////////////////////////////////////////////////////////////////////////

inline int CHashMap::get_bucket_id(TMBHashKey &key)
{
	return ((unsigned)key._.uin_) % ((unsigned)hash_map_->bucket_size_);
}

inline THashNode* CHashMap::handler2ptr(BC_DISK_HANDLER handler)
{
	if (handler == INVALID_BC_DISK_HANDLER)
		return NULL;
	
	return (THashNode*)(pool_ + handler);
}

inline BC_DISK_HANDLER CHashMap::ptr2handler(THashNode* ptr)
{
	char *tmp_ptr = (char *)ptr;
	if((tmp_ptr < pool_) || (tmp_ptr >= pool_tail_))
		return INVALID_BC_DISK_HANDLER;
	else
		return (BC_DISK_HANDLER)(tmp_ptr - pool_);	
}

inline void CHashMap::free_list_insert(THashNode *node)
{
	//insert to free list's head
	node->node_next_ = hash_map_->free_list_;
	BC_DISK_HANDLER node_hdr = ptr2handler(node);
	hash_map_->free_list_ = node_hdr;
}

inline THashNode* CHashMap::free_list_remove()
{
	//get head node from free list
	if(hash_map_->free_list_ == INVALID_BC_DISK_HANDLER)
		//ERROR_RETURN_NULL(HASH_MAP_ERROR_NO_FREE_NODE, "no free node");
		return NULL;
	
	THashNode* head_node = handler2ptr(hash_map_->free_list_);
	hash_map_->free_list_ = head_node->node_next_;
	head_node->node_next_ = INVALID_BC_DISK_HANDLER;	
	return head_node;
}

inline void CHashMap::init_node(THashNode* node)
{
	memset(node->key_._.md5_, 0, TMBHashKey::C_DATA_LEN);
	node->chunk_len_ = 0;
	node->add_info_1_ = 0;
	node->add_info_2_ = 0;
	node->flag_ = NODE_FLAG_UNCHG;
	
	//node->chunk_head_ = INVALID_BC_DISK_HANDLER;
	node->chunk_head_ = NULL_HANDLE;
	node->node_next_= INVALID_BC_DISK_HANDLER;
	node->node_prev_= INVALID_BC_DISK_HANDLER;
	node->add_next_= INVALID_BC_DISK_HANDLER;
	node->add_prev_= INVALID_BC_DISK_HANDLER;
}

inline THashNode*  CHashMap::get_bucket_list_head(unsigned bucket_id)
{
	assert(bucket_id < (unsigned)hash_map_->bucket_size_);
	BC_DISK_HANDLER node_hdr = hash_map_->bucket[bucket_id];
	return node_hdr != INVALID_BC_DISK_HANDLER ? handler2ptr(node_hdr) : NULL; 
}
inline THashNode*  CHashMap::get_bucket_list_prev(THashNode* node)
{
	assert(node);
	return node->node_prev_!= INVALID_BC_DISK_HANDLER ? handler2ptr( node->node_prev_) : NULL;
}
inline THashNode*  CHashMap::get_bucket_list_next(THashNode* node)
{
	assert(node);
	return node->node_next_!= INVALID_BC_DISK_HANDLER ? handler2ptr( node->node_next_) : NULL;
}
inline int CHashMap::merge_node_data(THashNode* node, char* data, int* data_len)
{
	//return allocator_.merge(node->chunk_head_, node->chunk_len_, data, data_len);
	if(*data_len < node->chunk_len_)
		return -1;
	*data_len = node->chunk_len_;
	return mydiskalloc_read(myhead_, node->chunk_head_, data, node->chunk_len_);
}
#ifdef _TDC_DISKCACHE_
inline int CHashMap::merge_node_data_v2(THashNode *node, char **data, int *data_len)
{
	int			ret = 0;

	*data_len = 0;
	*data = NULL;
	*data = (char *)malloc(node->chunk_len_);
	if ( !(*data) ) {
		return -1;
	}
	*data_len = node->chunk_len_;

	ret = mydiskalloc_read(myhead_, node->chunk_head_, *data, node->chunk_len_);
	if ( ret ) {
		free(*data);
		*data = NULL;
		*data_len = 0;
	}

	return ret;
}
#endif
inline int CHashMap::map_node_data(THashNode* node, char** data, int* data_len)
{
	if(*data_len < node->chunk_len_)
		return -1;
	*data_len = node->chunk_len_;
	return mydiskalloc_mmap(myhead_, node->chunk_head_, data, node->chunk_len_);
}
inline void CHashMap::unmap_node_data(char* data, int data_len) 
{
	mydiskalloc_unmmap(NULL, 0, data, data_len);
}
}}
//////////////////////////////////////////////////////////////////////////
#endif//_TFC_DISKCACHE_HASH_MAP_H_
///:~
