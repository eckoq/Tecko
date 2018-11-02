
#include <cstring>
#include "tfc_cache_chunk_alloc.h"

using namespace tfc::cache;

BC_MEM_HANDLER_L CChunkAllocator::malloc (int chunk_num)
{
    if (chunk_->free_total_ < chunk_num)
    {
        //SET_ERROR(CHUNK_ALLOCATOR_ERROR_FREE_CHUNK_LACK, "no enough chunk");
        return INVALID_BC_MEM_HANDLER_L;
    }
    TChunkNode* node = NULL;
    TChunkNode* head = NULL;
    for(int i = 0; i < chunk_num; i++)
    {
        node = free_list_remove();
        BC_MEM_HANDLER_L head_hdr = ptr2handler(head);
        node->next_ = head_hdr;
        head = node;
    }

    chunk_->free_total_ -= chunk_num;
    return ptr2handler(head);
}

void CChunkAllocator::free(BC_MEM_HANDLER_L head_chunk_hdr)
{
    BC_MEM_HANDLER_L chunk_hdr = head_chunk_hdr;
    while(chunk_hdr != INVALID_BC_MEM_HANDLER_L)
    {
        TChunkNode* node = handler2ptr(chunk_hdr);
        BC_MEM_HANDLER_L next_chunk_hdr = node->next_;

        free_list_insert(node);
        chunk_->free_total_ ++;

        chunk_hdr = next_chunk_hdr;
    }
}

void CChunkAllocator::init_pool_data(int n_chunks, int chunk_size)
{
    chunk_->free_list_ = INVALID_BC_MEM_HANDLER_L;
	long tmp_chunk_size = get_chunk_size(n_chunks, chunk_size);

    TChunkNode* chunk_node = NULL;
    for(long i = 0; i < n_chunks; i++)
    {
        BC_MEM_HANDLER_L offset = (long)( i * tmp_chunk_size );
        chunk_node = (TChunkNode*)((char*)(chunk_->chunk_nodes_) + offset);
        chunk_node->next_ = INVALID_BC_MEM_HANDLER_L;
        free_list_insert(chunk_node);
    }

    chunk_->chunk_total_ = n_chunks;
    chunk_->chunk_size_ = chunk_size;
    chunk_->free_total_ = n_chunks;
}

int CChunkAllocator::verify_pool_data(int n_chunks, int chunk_size)
{
    //检查内存块的数据的有效性
    if((chunk_->chunk_size_ != chunk_size) ||
        (chunk_->chunk_total_ != n_chunks))
    {
        //ERROR_RETURN_2(CHUNK_ALLOCATOR_ERROR_DATA_VERIFY_FAIL, "data verify fail[total:%d, size:%d]", chunk_->chunk_total_, chunk_->chunk_size_);
		return CHUNK_ALLOCATOR_ERROR_DATA_VERIFY_FAIL;
    }

    //检查空闲链
    TChunkNode* free_node = handler2ptr(chunk_->free_list_);
    int free_total = 0;
    while (free_node != NULL)
    {
        free_total ++;
        free_node = handler2ptr(free_node->next_);
    }

    if (free_total != chunk_->free_total_)
    {
        //ERROR_RETURN_2(CHUNK_ALLOCATOR_ERROR_DATA_VERIFY_FAIL, "data verify fail[free_total:%d, free_list:%d]", chunk_->free_total_, free_total);
        return CHUNK_ALLOCATOR_ERROR_DATA_VERIFY_FAIL;
    }

    return 0;
}

int CChunkAllocator::open(char *pool, bool init, int n_chunks, int chunk_size)
{
    int ret = 0;
    long pool_size = get_pool_size(n_chunks, chunk_size);

    pool_ = pool;
    pool_tail_ = pool + pool_size;
    chunk_ = (TChunk *)pool_;

    if(init)
    {
        //初始化内存块
        init_pool_data(n_chunks, chunk_size);
    }
    else
    {
        //检查内存块
        if ((ret = verify_pool_data(n_chunks, chunk_size)) != 0)
        {
            return ret;
        }
    }
    return 0;
}

inline void CChunkAllocator::free_list_insert(TChunkNode *node)
{
    //插入到空闲链表头
    node->next_ = chunk_->free_list_;
    BC_MEM_HANDLER_L node_hdr = ptr2handler(node);
    chunk_->free_list_ = node_hdr;
}

inline TChunkNode* CChunkAllocator::free_list_remove()
{
    if(chunk_->free_list_ == INVALID_BC_MEM_HANDLER_L)
    {
        //没有空闲CHUNK
        return NULL;
    }
    //从空闲链表头分配CHUNK
    TChunkNode* head_node = handler2ptr(chunk_->free_list_);
    chunk_->free_list_ = head_node->next_;

    head_node->next_ = INVALID_BC_MEM_HANDLER_L;
    return head_node;
}


int CChunkAllocator::get_chunk_num(int data_len)
{
//	int num = data_len / chunk_->chunk_size_;
//	if ((data_len % chunk_->chunk_size_) != 0)
//	{
//		num++;
//	}
//	return num;
	return (data_len + chunk_->chunk_size_ - 1) / chunk_->chunk_size_;
}

int CChunkAllocator::merge(BC_MEM_HANDLER_L chunk_node_hdr, int chunk_len, void* data_buf, int* data_len)
{
    if(*data_len < chunk_len)
    {
        //输入的缓存区过短
        //ERROR_RETURN_2(CHUNK_ALLOCATOR_ERROR_INVALID_PARAM, "input date_len too short[%d < %d]", *data_len, chunk_len);
		*data_len = chunk_len;
		return CHUNK_ALLOCATOR_ERROR_INVALID_PARAM;
    }

    int remain_len = chunk_len;
    char* write_pos = (char *)data_buf;

    while(chunk_node_hdr != INVALID_BC_MEM_HANDLER_L)
    {
        TChunkNode* chunk_node = handler2ptr(chunk_node_hdr);

        if (remain_len < chunk_->chunk_size_)
        {
            memcpy(write_pos, chunk_node->data_, remain_len);
            break;
        }

        memcpy(write_pos, chunk_node->data_, chunk_->chunk_size_);
        write_pos += chunk_->chunk_size_;
        remain_len -= chunk_->chunk_size_;

        chunk_node_hdr = chunk_node->next_;    //to next
    }

    *data_len = chunk_len;

    return 0;
}

void CChunkAllocator::split(BC_MEM_HANDLER_L head_hdr, const void* data_buf, int data_len)
{
    TChunkNode* chunk_node = handler2ptr(head_hdr);

    char* read_pos = (char*)data_buf;
    int remain_len = data_len;
    while (chunk_node != NULL)
    {
        if (remain_len < chunk_->chunk_size_)
        {
            memcpy(chunk_node->data_, read_pos, remain_len);
            break;
        }

        memcpy(chunk_node->data_, read_pos, chunk_->chunk_size_);
        read_pos += chunk_->chunk_size_;
        remain_len -= chunk_->chunk_size_;

        //to next
        chunk_node = handler2ptr(chunk_node->next_);
    }

}
/*
void CChunkAllocator::print_stat()
{
    DEBUG_P_NO_TIME(LOG_NORMAL, "%20s:%10d;\n", "chunk total", chunk_->chunk_total_);
    DEBUG_P_NO_TIME(LOG_NORMAL, "%20s:%10d;\n", "chunk size", chunk_->chunk_size_);
    DEBUG_P_NO_TIME(LOG_NORMAL, "%20s:%10d;\n", "chunk used", get_used_chunk_num());
}
 */
