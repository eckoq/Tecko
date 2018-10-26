
#ifndef _CHUNK_ALLOC_HPP_
#define _CHUNK_ALLOC_HPP_

//Ԥ�����ڴ��������ݴ洢��Ƭ��

//#include "comm.hpp"

namespace tfc{namespace cache{
	
#pragma pack(1)
	
	typedef long	BC_MEM_HANDLER_L;
	typedef int		BC_MEM_HANDLER_32;

	static const BC_MEM_HANDLER_L INVALID_BC_MEM_HANDLER_L = (-1L);
	static const BC_MEM_HANDLER_32 INVALID_BC_MEM_HANDLER_32 = (-1);
#ifndef NULL
#define NULL 0
#endif
	
	typedef struct tagTChunkNode
	{
		BC_MEM_HANDLER_L next_;     //CHUNK��Ƭ��ָ��
		char data_[1];            //������
	}TChunkNode;
	
	typedef struct tagTChunk
	{
		int chunk_total_;           //CHUNK�ܽڵ���
		int chunk_size_;            //CHUNK����Ƭ�ߴ�
		int free_total_;            //����CHUNK����
		BC_MEM_HANDLER_L free_list_;  //����CHUNK����ͷָ��
		TChunkNode chunk_nodes_[1]; //CHUNK����
	}TChunk;
	
#pragma pack()
	
	
	class CChunkAllocator 
	{
	public:
		enum CHUNK_ALLOCATOR_ERROR
		{
			CHUNK_ALLOCATOR_ERROR_BASE = -1,    
			CHUNK_ALLOCATOR_ERROR_INVALID_PARAM = CHUNK_ALLOCATOR_ERROR_BASE -1,    //�Ƿ�����
			CHUNK_ALLOCATOR_ERROR_FREE_CHUNK_LACK = CHUNK_ALLOCATOR_ERROR_BASE -2,    //�����ڴ�鲻��
			CHUNK_ALLOCATOR_ERROR_DATA_VERIFY_FAIL = CHUNK_ALLOCATOR_ERROR_BASE -3,    //�ڴ����ݼ��ʧ��
		};
		
	public:
		//��ʼ��CHUNK �ڴ��    
		int open(char *pool,bool init, int n_chunks, int chunk_size);
		
		//�ӿ��������з���CHUNK.  
		BC_MEM_HANDLER_L malloc (int chunk_num=1);
		
		//��CHUNK���뵽����������.
		void free(BC_MEM_HANDLER_L head_chunk_hdr);
		
		//��CHUNK�е����ݷ�Ƭ�������
		int merge(BC_MEM_HANDLER_L chunk_node_hdr, int chunk_len, void* data_buf, int* data_len);
        
		//�����ݷ�Ƭ���ڵ���CHUNK��.
		void split(BC_MEM_HANDLER_L head_hdr, const void* data_buf, int data_len);        
		
		//��ƫ����ת������ʵ���ڴ��ַ
		TChunkNode *handler2ptr(BC_MEM_HANDLER_L handler);
		
		//���ڴ��ַת����ƫ����
		BC_MEM_HANDLER_L ptr2handler(TChunkNode* ptr);
		
		//������Ҫ����CHUNK�������ݴ洢. 
		int get_chunk_num(int data_len);    
		
		//����chunk����
		int get_chunk_total() { return chunk_->chunk_total_; }
		
		//����ʹ�õ�CHUNK��
		int get_used_chunk_num() { return (chunk_->chunk_total_ - chunk_->free_total_); };
		
		//���ؿ��е�CHUNK��
		int get_free_chunk_num() { return chunk_->free_total_; };

		int get_chunk_data_size() { return chunk_->chunk_size_; }
		
		//����CHUNK ���ڴ��ߴ�
		static int get_chunk_size(int n_chunks, int chunk_size)
		{
			return (sizeof(TChunkNode) - sizeof(char[1]) + chunk_size); 
		}
		static long get_pool_size(int n_chunks, int chunk_size)
		{
			long chunk_total_size = (long)n_chunks * (long)get_chunk_size(n_chunks, chunk_size);
			int head_size = (sizeof(TChunk) - sizeof(TChunkNode));
			long pool_size = head_size + chunk_total_size;
			return pool_size;        
		}
		
		//   void print_stat();
		
	protected:    
		//��CHUNK���뵽����������.
		void free_list_insert(TChunkNode *node);
		
		//�ӿ��������з���CHUNK.  
		TChunkNode *free_list_remove();
		
		//��ʼ��CHUNK �ڴ���е����ݽṹ
		void init_pool_data(int n_chunks, int chunk_size);
		
		//��� CHUNK �ڴ���е����ݽṹ
		int verify_pool_data(int n_chunks, int chunk_size);
		
		char *pool_;        //CHUNK �ڴ����ʼ��ַ
		char *pool_tail_;   //CHUNK �ڴ�������ַ
		TChunk *chunk_; 	//�ڴ���е� TChunk �ṹ��ָ��
	};
	
}}
#endif
