#ifndef _TFC_NET_IPC_MQ_H_
#define _TFC_NET_IPC_MQ_H_

#include "tfc_ipc_sv.hpp"	//	use tfc_ipc_sv temporarily, in a month
#include <semaphore.h>

//////////////////////////////////////////////////////////////////////////

//
//	
//

namespace tfc{namespace net
{
	static const int E_DEQUEUE_BUF_NOT_ENOUGH = -13001;
	class CShmMQ
	{
	public:
		typedef struct tagMQStat
		{
			unsigned _used_len;
			unsigned _free_len;
			unsigned _total_len;
			unsigned _shm_key;
			unsigned _shm_id;
			unsigned _shm_size;
		}TMQStat;

	public:
		CShmMQ(){}
		~CShmMQ(){}
		int init(int shm_key, unsigned shm_size);
		int enqueue(const void* data, unsigned data_len, unsigned long long flow);
		int dequeue(void* buf, unsigned buf_size, unsigned& data_len, unsigned long long &flow);
		
		void get_stat(TMQStat& mq_stat)
		{
			unsigned head = *_head;
			unsigned tail = *_tail;
			
			mq_stat._used_len = (tail>=head) ? tail-head : tail+_block_size-head;
			mq_stat._free_len = head>tail? head-tail: head+_block_size-tail;
			mq_stat._total_len = _block_size;
			mq_stat._shm_key = _shm->key();
			mq_stat._shm_id = _shm->id();
			mq_stat._shm_size = _shm->size();
		}

	private:
		tfc::ptr<tfc::ipc::CShm> _shm;

		unsigned* _head;
		unsigned* _tail;
		char* _block;
		unsigned _block_size;

		// static const unsigned C_HEAD_SIZE = 8;
		static const unsigned C_HEAD_SIZE = sizeof(unsigned) + sizeof(unsigned long long);
	};
	
	//posix实现的sem比systemv5实现的sem更简单高效
	class CSemLockMQ
	{
	public:
		typedef struct tagSemLockMQStat
		{
			CShmMQ::TMQStat _mq_stat;
			int _rlock;
			int _wlock;
		}TSemLockMQStat;

	public:
		CSemLockMQ(CShmMQ& mq);
		~CSemLockMQ();
		
		int init(const char* sem_name, int rlock=0,int wlock=0);
		int enqueue(const void* data, unsigned data_len, unsigned long long flow);
		int dequeue(void* buf, unsigned buf_size, unsigned& data_len, unsigned long long &flow);
		void get_stat(TSemLockMQStat& mq_stat)
		{
			_mq.get_stat(mq_stat._mq_stat);
			mq_stat._rlock = _rlock;
			mq_stat._wlock = _wlock;
		}
		
	public:
		CShmMQ& _mq;
		sem_t* _sem;
		int _rlock;
		int _wlock;
	};
	
	class CFifoSyncMQ
	{
	public:
		typedef struct tagFifoSyncMQStat
		{
			CSemLockMQ::TSemLockMQStat _semlockmq_stat;
			int _fd;
		}TFifoSyncMQStat;

	public:
		CFifoSyncMQ(CSemLockMQ& mq);
		~CFifoSyncMQ();
		
		//此实现是采用fifo作为通知机制
		int init(const std::string& fifo_path);
		//此实现是采用udp包作为通知机制
		//int init(unsigned short port, bool to_read);
		
		int enqueue(const void* data, unsigned data_len, unsigned long long flow);
		int enqueue(const void* data, unsigned data_len, unsigned flow);
#if 0
		//当无数据的时候会阻塞（不建议使用，除非进程只dequeue仅仅一个mq）
		int dequeue(void* buf, unsigned buf_size, unsigned& data_len, unsigned long long &flow);
		int dequeue(void* buf, unsigned buf_size, unsigned& data_len, unsigned& flow);
#endif	
		//当无数据的时候不会阻塞，立即返回
		int try_dequeue(void* buf, unsigned buf_size, unsigned& data_len, unsigned long long &flow);
		int try_dequeue(void* buf, unsigned buf_size, unsigned& data_len, unsigned& flow);
		//当被epoll或者select等激活读的时候需要调用此函数
		void clear_flag();
		
		void get_stat(TFifoSyncMQStat& mq_stat)
		{
			_mq.get_stat(mq_stat._semlockmq_stat);
			mq_stat._fd = _fd;
		}
		int fd(){ return _fd; };
		
	private:
#if 0		
		int select_fifo();
#endif
		CSemLockMQ& _mq;
		int _fd;
#ifdef _MQ_OPT	
		int _buff_read_count;
#endif
	};
}}

//////////////////////////////////////////////////////////////////////////
#endif//_TFC_NET_IPC_MQ_H_
///:~
