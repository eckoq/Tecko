
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
		TMBHashKey key_;			   	//����
		int chunk_len_;				 	//CHUNK�е����ݳ���
		//BC_DISK_HANDLER chunk_head_;   	//CHUNK ���
		DISK_HANDLE chunk_head_;   		//CHUNK ���
		BC_DISK_HANDLER node_prev_;		//�ڵ�����ǰָ��
		BC_DISK_HANDLER node_next_;		//�ڵ�������ָ��
		BC_DISK_HANDLER add_prev_;	 	//��������ǰָ��
		BC_DISK_HANDLER add_next_;	 	//����������ָ��
		long add_info_1_;				//������ʱ��
		long add_info_2_;				//��ʱʱ��
		int flag_;						//����
#ifdef _TDC_DISKCACHE_
		unsigned int user_;				//�û��Զ����ֶΣ������������ݽ�ȫ
#endif
	}THashNode;
	
	typedef struct tagTHashMap
	{
		int node_total_;				//�ڵ�����
		int bucket_size_;			   	//HASHͰ�Ĵ�С
		int used_node_num_;			 	//ʹ�õĽڵ���
		int used_bucket_num_;		   	//HASHͰʹ����
		BC_DISK_HANDLER add_head_;	 	//��������ͷָ��
		BC_DISK_HANDLER add_tail_;	 	//��������βָ��
		BC_DISK_HANDLER free_list_;		//�ռ�ڵ�����ͷָ��
		BC_DISK_HANDLER bucket[1];	 	//HASHͰ
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
			HASH_MAP_ERROR_INVALID_PARAM = HASH_MAP_ERROR_BASE -1,	//�Ƿ�����
			HASH_MAP_ERROR_NODE_NOT_EXIST = HASH_MAP_ERROR_BASE -2,	//�ڵ㲻����
			HASH_MAP_ERROR_NODE_HAVE_EXIST = HASH_MAP_ERROR_BASE -3,//�ڵ��Ѿ�����
			HASH_MAP_ERROR_NO_FREE_NODE = HASH_MAP_ERROR_BASE -4,	//û�п��нڵ�
		};
		
	public:
		CHashMap();
		~CHashMap();	
		
		//��ʼ�� HASH_MAP �ڴ��
		//int open(char* pool, const std::string& file_path, bool init, int node_total, int bucket_size, int n_chunks, int chunk_size);
		int open(char* pool, const std::string& file_path, bool init, int node_total, int bucket_size, unsigned long long filesize, unsigned minchunksize);
		
		// ʹ�� <key> ���в�ѯ.
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
		//����ڵ�, ����ɽڵ����, �򷵻�ʧ��
		THashNode* insert_node(TMBHashKey &key, void* new_data, int new_len);
		//����ڵ�, ֻдԪ����,����ɽڵ����, �򷵻�ʧ��
		THashNode* insert_node_metadata(TMBHashKey &key, int new_len, long expiretime);
	
		//�޸Ľڵ�
		THashNode* update_node(THashNode* node, void* new_data, int new_len, char* old_data = NULL, int* old_len = NULL);
		//insert or update
		//�޸Ľڵ�,ֻдԪ����
		THashNode* update_node_metadata(THashNode* node, int new_len, long expiretime);

		THashNode* replace_node(TMBHashKey &key, void* new_data, int new_len, char* old_data = NULL, int* old_len = NULL);
		//ɾ�����. ͬʱ�Ὣ�ڵ�Ӹ������������
		//����ֵ = 0 ��ʾ�ɹ�, < 0 ��ʾʧ��(��ڵ㲻����,Ҳ����ʧ��)
		int delete_node(THashNode* node, char* data = NULL, int* data_len = NULL);
		
		int merge_node_data(THashNode* node, char* data, int* data_len);
		int map_node_data(THashNode* node, char** data, int* data_len);
		void unmap_node_data(char* data, int data_len);

		void set_expiretime(THashNode* node, long expiretime);
		long get_expiretime(THashNode* node);
		
		// ���ص�ǰ�ڵ�ʹ����
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
		
		// ����HASH_MAP��������ڴ��ߴ�
		static int get_pool_size(int node_total, int bucket_size)
		{
			int head_size = sizeof(THashMap) - sizeof(BC_DISK_HANDLER[1]);
			int bucket_total_size = bucket_size * sizeof(BC_DISK_HANDLER);
			int node_total_size = node_total * sizeof(THashNode);
			int pool_size = head_size + bucket_total_size + node_total_size;
			return pool_size;		
		}
		// ȡHASH_MAP ��CHUNK���ڴ��ߴ�
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
		//��ӡ��������, <num> ָ����ӡ����Ŀ. <num> = 0, ��ӡ���нڵ�.
		void print_add_list(int num = 0);
		//��ӡ������ͳ����Ϣ
		void print_stat();
		*/
		//transform handler to address
		THashNode *handler2ptr(BC_DISK_HANDLER handler);
		
		//transform address to handler
		BC_DISK_HANDLER ptr2handler(THashNode* ptr);
		
		//����������������
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
		
		//������������HASHͰֵ
		int get_bucket_id(TMBHashKey &key);
		int get_bucket_list_len(int bucket_id); //ȡHASHͰ����ײ��
		
		//���ڵ���뵽��������
		void free_list_insert(THashNode *node);
		//�ӿ���������ȡ�ڵ�
		THashNode *free_list_remove();
		
		//�ڵ�������������
		void insert_node_list(THashNode* node);
		void delete_from_node_list(THashNode* node);
		
		//��ʼ���ڵ�
		void init_node(THashNode* node);
		//���ڵ���Ϊ����ģʽ
		void free_node(THashNode *node);
		//���ڵ���Ϊʹ��ģʽ
		//void use_node(THashNode *node, TMBHashKey &key, int chunk_len, BC_DISK_HANDLER chunk_head);
		void use_node(THashNode *node, TMBHashKey &key, int chunk_len, DISK_HANDLE chunk_head);
		
		char *pool_;		//�ڴ����ʼ��ַ
		char *pool_tail_;   //�ڴ�������ַ
		
		THashMap* hash_map_;   //�ڴ���е�HASHMAP �ṹ
		THashNode* hash_node_; //�ڴ���е�HASH�ڵ�����
		//CDiskChunkAllocator allocator_; //Disk Chunk������
		mydiskalloc_head* myhead_; //Disk Chunk������
		
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