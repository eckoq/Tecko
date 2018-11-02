
#ifndef _CHUNK_ALLOC_LARGE_HPP_
#define _CHUNK_ALLOC_LARGE_HPP_

//预分配内存区的数据存储分片类

//#include "comm.hpp"
#include <stdint.h>

namespace tfc{namespace cache_large{

#pragma pack(1)

	typedef long	BC_MEM_HANDLER_L;
	typedef int		BC_MEM_HANDLER_32;
	typedef uint64_t BC_MEM_HANDLER_64;

	static const BC_MEM_HANDLER_L INVALID_BC_MEM_HANDLER_L = (-1L);
	static const BC_MEM_HANDLER_32 INVALID_BC_MEM_HANDLER_32 = (-1);
	static const BC_MEM_HANDLER_64 INVALID_BC_MEM_HANDLER_64 = (-1);
#ifndef NULL
#define NULL 0
#endif

	typedef struct tagTChunkNode
	{
		BC_MEM_HANDLER_64 next_;     //CHUNK分片后指针
		char data_[1];            //数据区
	}TChunkNode;

	typedef struct tagTChunk
	{
		uint64_t chunk_total_;           //CHUNK总节点数
		uint32_t chunk_size_;            //CHUNK数据片尺寸
		uint64_t free_total_;            //空闲CHUNK总数
		BC_MEM_HANDLER_64 free_list_;  //空闲CHUNK链表头指针
		TChunkNode chunk_nodes_[1]; //CHUNK数组
	}TChunk;

#pragma pack()


	class CChunkAllocator
	{
	public:
		enum CHUNK_ALLOCATOR_ERROR
		{
			CHUNK_ALLOCATOR_ERROR_BASE = -1,
			CHUNK_ALLOCATOR_ERROR_INVALID_PARAM = CHUNK_ALLOCATOR_ERROR_BASE -1,    //非法参数
			CHUNK_ALLOCATOR_ERROR_FREE_CHUNK_LACK = CHUNK_ALLOCATOR_ERROR_BASE -2,    //空闲内存块不足
			CHUNK_ALLOCATOR_ERROR_DATA_VERIFY_FAIL = CHUNK_ALLOCATOR_ERROR_BASE -3,    //内存数据检查失败
		};

	public:
		//初始化CHUNK 内存块
		int open(char *pool, bool init, uint64_t n_chunks, uint32_t chunk_size);

		//从空闲链表中分配CHUNK.
		BC_MEM_HANDLER_64 malloc(uint32_t chunk_num=1);

		//将CHUNK插入到空闲链表中.
		void free(BC_MEM_HANDLER_64 head_chunk_hdr);

		//将CHUNK中的数据分片进行组合
		int merge(BC_MEM_HANDLER_64 chunk_node_hdr, uint32_t chunk_len, void* data_buf, uint32_t* data_len);

		//将数据分片存在到各CHUNK中.
		void split(BC_MEM_HANDLER_64 head_hdr, const void* data_buf, uint32_t data_len);

		//将偏移量转换成真实的内存地址
		TChunkNode *handler2ptr(BC_MEM_HANDLER_64 handler)
        {
            if (handler == INVALID_BC_MEM_HANDLER_64)
            {
                return NULL;
            }

            return (TChunkNode*)(pool_ + handler);
        }


		//将内存地址转换成偏移量
		BC_MEM_HANDLER_64 ptr2handler(TChunkNode* ptr)
        {
            char *tmp_ptr = (char *)ptr;
            if((tmp_ptr < pool_) || (tmp_ptr >= pool_tail_))
            {
                return INVALID_BC_MEM_HANDLER_64;
            }
            return (BC_MEM_HANDLER_64)(tmp_ptr - pool_);
        }


		//计算需要多少CHUNK进行数据存储.
		uint32_t get_chunk_num(uint32_t data_len);

		//返回chunk总数
		uint64_t get_chunk_total() { return chunk_->chunk_total_; }

		//返回使用的CHUNK数
		uint64_t get_used_chunk_num() { return (chunk_->chunk_total_ - chunk_->free_total_); };

		//返回空闲的CHUNK数
		uint64_t get_free_chunk_num() { return chunk_->free_total_; };

		uint32_t get_chunk_data_size() { return chunk_->chunk_size_; }

		//计算CHUNK 的内存块尺寸
		static uint32_t get_chunk_size(uint64_t n_chunks, uint32_t chunk_size)
		{
			return (sizeof(TChunkNode) - sizeof(char[1]) + chunk_size);
		}
		static uint64_t get_pool_size(uint64_t n_chunks, uint32_t chunk_size)
		{
			uint64_t chunk_total_size = n_chunks * get_chunk_size(n_chunks, chunk_size);
			uint32_t head_size = (sizeof(TChunk) - sizeof(TChunkNode));
			uint64_t pool_size = head_size + chunk_total_size;
			return pool_size;
		}

		//   void print_stat();

	protected:
		//将CHUNK插入到空闲链表中.
		void free_list_insert(TChunkNode *node);

		//从空闲链表中分配CHUNK.
		TChunkNode *free_list_remove();

		//初始化CHUNK 内存块中的数据结构
		void init_pool_data(uint64_t n_chunks, uint32_t chunk_size);

		//检查 CHUNK 内存块中的数据结构
		int verify_pool_data(uint64_t n_chunks, uint32_t chunk_size);

		char *pool_;        //CHUNK 内存块起始地址
		char *pool_tail_;   //CHUNK 内存块结束地址
		TChunk *chunk_; 	//内存块中的 TChunk 结构的指针
	};

}}
#endif
