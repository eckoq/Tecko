#include "tfc_net_ipc_mq.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <cstring>
#include <stdio.h>

using namespace tfc::net;
using namespace tfc::ipc;
using namespace std;


int CShmMQ::init(int shm_key, unsigned shm_size) {
	assert(shm_size > C_HEAD_SIZE);
	try {
		try {
			_shm = CShm::create_only(shm_key, shm_size);
			memset(_shm->memory(), 0, C_HEAD_SIZE);	//	set head and tail
		}
		catch (ipc_ex& ex) {
			_shm = CShm::open(shm_key, shm_size);
		}
	}
	catch (...) {
		return -1;
	}

	_head = (unsigned*)_shm->memory();
	_tail = _head+1;
	// _block = (char*) (_tail+1);
	_block = (char *)( ((char *)(_shm->memory())) + C_HEAD_SIZE );
	_block_size = shm_size - C_HEAD_SIZE;
	return 0;
}

int CShmMQ::enqueue(const void* data, unsigned data_len, unsigned long long flow) {
	unsigned head = *_head;
	unsigned tail = *_tail;	
	unsigned free_len = head>tail? head-tail: head+_block_size-tail;
	unsigned tail_len = _block_size - tail;

	char sHead[C_HEAD_SIZE] = {0};
	unsigned total_len = data_len+C_HEAD_SIZE;

	//	first, if no enough space?
	if (free_len <= total_len)
		return -1;

	memcpy(sHead, &total_len, sizeof(unsigned));
	memcpy(sHead+sizeof(unsigned), &flow, sizeof(unsigned long long));

	//	second, if tail space > 12+len
	//	copy 12 byte, copy data
	if (tail_len >= total_len) {
		memcpy(_block+tail, sHead, C_HEAD_SIZE);
		memcpy(_block+tail+ C_HEAD_SIZE, data, data_len);
		*_tail += data_len + C_HEAD_SIZE;
	}

	//	third, if tail space > 12 && < 12+len
	else if (tail_len >= C_HEAD_SIZE && tail_len < C_HEAD_SIZE+data_len) {
		//	copy 12 byte
		memcpy(_block+tail, sHead, C_HEAD_SIZE);

		//	copy tail-12
		unsigned first_len = tail_len - C_HEAD_SIZE;
		memcpy(_block+tail+ C_HEAD_SIZE, data, first_len);

		//	copy left
		unsigned second_len = data_len - first_len;
		memcpy(_block, ((char*)data) + first_len, second_len);

     		//modify by felixxie 2010 3.3
		int itmp = *_tail + data_len + C_HEAD_SIZE - _block_size;
		*_tail = itmp;

		//*_tail += data_len + C_HEAD_SIZE;
		//*_tail -= _block_size;
	}

	//	fourth, if tail space < 12
	else {
		//	copy tail byte
		memcpy(_block+tail, sHead, tail_len);

		//	copy 12-tail byte
		unsigned second_len = C_HEAD_SIZE - tail_len;
		memcpy(_block, sHead + tail_len, second_len);

		//	copy data
		memcpy(_block + second_len, data, data_len);
		
		// 此处原来没有修改尾指针，已修改 tomxiao 2006.12.10
		*_tail = second_len + data_len;
	}

	//fix me: 如果mq在enqueue和dequeue的时候都不加锁，那么有可能这里free_len的判断是不准确的，
	//因为在enqueue的同时，可能有另外的进程在dequeue，而且是dequeue最后一个包，此时enqueue的判断
	//不会是第一个包，但是当enqueue完成的时候，实际上应该是第一个包。
	//要解决这个问题，一定要有一个原子变量来辅助这个判断，但是这样又会增加维护此变量的代价
	if(free_len == _block_size)	//第一个包
		return 1;
	else						//非第一个包
		return 0;
}

/*
int CShmMQ::enqueue(const void* data, unsigned data_len, unsigned flow) {
	unsigned head = *_head;
	unsigned tail = *_tail;	
	unsigned free_len = head>tail? head-tail: head+_block_size-tail;
	unsigned tail_len = _block_size - tail;

	char sHead[C_HEAD_SIZE] = {0};
	unsigned total_len = data_len+C_HEAD_SIZE;

	//	first, if no enough space?
	if (free_len <= total_len)
		return -1;

	memcpy(sHead, &total_len, sizeof(unsigned));
	memcpy(sHead+sizeof(unsigned), &flow, sizeof(unsigned));

	//	second, if tail space > 8+len
	//	copy 8 byte, copy data
	if (tail_len >= total_len) {
		memcpy(_block+tail, sHead, C_HEAD_SIZE);
		memcpy(_block+tail+ C_HEAD_SIZE, data, data_len);
		*_tail += data_len + C_HEAD_SIZE;
	}

	//	third, if tail space > 8 && < 8+len
	else if (tail_len >= C_HEAD_SIZE && tail_len < C_HEAD_SIZE+data_len) {
		//	copy 8 byte
		memcpy(_block+tail, sHead, C_HEAD_SIZE);

		//	copy tail-8
		unsigned first_len = tail_len - C_HEAD_SIZE;
		memcpy(_block+tail+ C_HEAD_SIZE, data, first_len);

		//	copy left
		unsigned second_len = data_len - first_len;
		memcpy(_block, ((char*)data) + first_len, second_len);

     		//modify by felixxie 2010 3.3
		int itmp = *_tail + data_len + C_HEAD_SIZE - _block_size;
		*_tail = itmp;

	}

	//	fourth, if tail space < 8
	else {
		//	copy tail byte
		memcpy(_block+tail, sHead, tail_len);

		//	copy 8-tail byte
		unsigned second_len = C_HEAD_SIZE - tail_len;
		memcpy(_block, sHead + tail_len, second_len);

		//	copy data
		memcpy(_block + second_len, data, data_len);
		
		// 此处原来没有修改尾指针，已修改 tomxiao 2006.12.10
		*_tail = second_len + data_len;
	}

	//fix me: 如果mq在enqueue和dequeue的时候都不加锁，那么有可能这里free_len的判断是不准确的，
	//因为在enqueue的同时，可能有另外的进程在dequeue，而且是dequeue最后一个包，此时enqueue的判断
	//不会是第一个包，但是当enqueue完成的时候，实际上应该是第一个包。
	//要解决这个问题，一定要有一个原子变量来辅助这个判断，但是这样又会增加维护此变量的代价
	if(free_len == _block_size)	//第一个包
		return 1;
	else						//非第一个包
		return 0;
}*/

int CShmMQ::dequeue(void* buf, unsigned buf_size, unsigned& data_len, unsigned long long &flow) {
	unsigned head = *_head;
	unsigned tail = *_tail;
	if (head == tail) {
		data_len = 0;
		return 0;
	}
	unsigned used_len = tail>head ? tail-head : tail+_block_size-head;
	char sHead[C_HEAD_SIZE];
	
	//	if head + 12 > block_size
	if (head+C_HEAD_SIZE > _block_size) {
		unsigned first_size = _block_size - head;
		unsigned second_size = C_HEAD_SIZE - first_size;
		memcpy(sHead, _block + head, first_size);
		memcpy(sHead + first_size, _block, second_size);
		head = second_size;
	}
	else {
		memcpy(sHead, _block + head, C_HEAD_SIZE);
		head += C_HEAD_SIZE;
	}
	
	//	get meta data
	unsigned total_len  = *(unsigned*) (sHead);
	flow = *(unsigned long long*) (sHead+sizeof(unsigned));
	assert(total_len <= used_len);
	
	data_len = total_len-C_HEAD_SIZE;
	if (data_len > buf_size)
		return -1;

	if (head+data_len > _block_size) {
		unsigned first_size = _block_size - head;
		unsigned second_size = data_len - first_size;
		memcpy(buf, _block + head, first_size);
		memcpy(((char*)buf) + first_size, _block, second_size);
		*_head = second_size;
	}
	else {
		memcpy(buf, _block + head, data_len);
		*_head = head+data_len;
	}

	return 0;
}
/*
int CShmMQ::dequeue(void* buf, unsigned buf_size, unsigned& data_len, unsigned& flow) {
	unsigned head = *_head;
	unsigned tail = *_tail;
	if (head == tail) {
		data_len = 0;
		return 0;
	}
	unsigned used_len = tail>head ? tail-head : tail+_block_size-head;
	char sHead[C_HEAD_SIZE];
	
	//	if head + 8 > block_size
	if (head+C_HEAD_SIZE > _block_size) {
		unsigned first_size = _block_size - head;
		unsigned second_size = C_HEAD_SIZE - first_size;
		memcpy(sHead, _block + head, first_size);
		memcpy(sHead + first_size, _block, second_size);
		head = second_size;
	}
	else {
		memcpy(sHead, _block + head, C_HEAD_SIZE);
		head += C_HEAD_SIZE;
	}
	
	//	get meta data
	unsigned total_len  = *(unsigned*) (sHead);
	flow = *(unsigned*) (sHead+sizeof(unsigned));
	assert(total_len <= used_len);
	
	data_len = total_len-C_HEAD_SIZE;
	if (data_len > buf_size)
		return -1;

	if (head+data_len > _block_size) {
		unsigned first_size = _block_size - head;
		unsigned second_size = data_len - first_size;
		memcpy(buf, _block + head, first_size);
		memcpy(((char*)buf) + first_size, _block, second_size);
		*_head = second_size;
	}
	else {
		memcpy(buf, _block + head, data_len);
		*_head = head+data_len;
	}

	return 0;
}*/

//////////////////////////////////////////////////////////////////////////

//CSemLockMQ::CSemLockMQ(CShmMQ& mq) : _mq(mq), _sem_index(0){}
CSemLockMQ::CSemLockMQ(CShmMQ& mq) : _mq(mq), _sem(NULL){}
CSemLockMQ::~CSemLockMQ() {
	if(_sem)
		sem_close(_sem);
}

//static const int C_UNINIT_VALUE = (1<<16) - 1;

//int CSemLockMQ::init(int sem_key, unsigned sem_size, unsigned sem_index, int rlock,int wlock)
int CSemLockMQ::init(const char* sem_name, int rlock,int wlock) {
	_rlock = rlock;
	_wlock = wlock;

	_sem = sem_open(sem_name, O_CREAT, 0644, 1);	
	if(_sem == SEM_FAILED) {
		printf("sem_open fail, %s, %d,%m\n", sem_name,errno);
		return -1;
	}	
	else
		return 0;	
}

int CSemLockMQ::enqueue(const void* data, unsigned data_len, unsigned long long flow) {
	int ret = 0;
	
	if(_wlock) {
		sem_wait(_sem);
		ret = _mq.enqueue(data, data_len, flow);
		sem_post(_sem);
	
	}
	else {
		ret = _mq.enqueue(data, data_len, flow);
	}
	return ret;
}

/*
int CSemLockMQ::enqueue(const void* data, unsigned data_len, unsigned flow) {
	int ret = 0;
	
	if(_wlock) {
		//_sem->wait(_sem_index);
		sem_wait(_sem);
		ret = _mq.enqueue(data, data_len, flow);
		sem_post(_sem);
		//_sem->post(_sem_index);	
	
	}
	else {
		ret = _mq.enqueue(data, data_len, flow);
	}
	return ret;
}*/

int CSemLockMQ::dequeue(void* buf, unsigned buf_size, unsigned& data_len, unsigned long long &flow) {
	int ret = 0;
	if(_rlock) {
		sem_wait(_sem);
		ret = _mq.dequeue(buf, buf_size, data_len, flow);
		sem_post(_sem);
	}
	else {
		ret = _mq.dequeue(buf, buf_size, data_len, flow);
	}
	return ret;
}

/*
int CSemLockMQ::dequeue(void* buf, unsigned buf_size, unsigned& data_len, unsigned& flow) {
	int ret = 0;
	if(_rlock) {
		//_sem->wait(_sem_index);
		sem_wait(_sem);
		ret = _mq.dequeue(buf, buf_size, data_len, flow);
		sem_post(_sem);
		//_sem->post(_sem_index);	
	}
	else {
		ret = _mq.dequeue(buf, buf_size, data_len, flow);
	}
	return ret;
}*/

//////////////////////////////////////////////////////////////////////////

CFifoSyncMQ::CFifoSyncMQ(CSemLockMQ& mq) : _mq(mq), _fd(-1){}
CFifoSyncMQ::~CFifoSyncMQ(){}

int CFifoSyncMQ::init(const std::string& fifo_path) {

#ifdef _MQ_OPT
	_buff_read_count = 0;
#endif
    int mode = 0666 | O_NONBLOCK | O_NDELAY;
    
	errno = 0;
    if ((mkfifo(fifo_path.c_str(), mode)) < 0) {
		if (errno != EEXIST)
			return -1;
    }

	if (_fd != -1) {
		close(_fd);
		_fd = -1;
	}

    if ((_fd = open(fifo_path.c_str(), O_RDWR)) < 0) {
		return -1;
    }
	if (_fd > 1024) {
		close(_fd);
		return -1;
	}
    
	int val = fcntl(_fd, F_GETFL, 0);
	
	if (val == -1)
		return errno ? -errno : val;
	
	if (val & O_NONBLOCK)
		return 0;
	
	int ret = fcntl(_fd, F_SETFL, val | O_NONBLOCK | O_NDELAY);
	return (ret < 0) ? (errno ? -errno : ret) : 0;
    return 0;
}
#if 0
int CFifoSyncMQ::init(unsigned short port, bool to_read) {

	struct sockaddr_in addr;
	int n;

	addr.sin_family = AF_INET;
	inet_aton("127.0.0.1", &addr.sin_addr);
	addr.sin_port = htons(port);

	_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if(_fd < 0) {
		printf("socket(AF_INET, SOCK_DGRAM, 0): %m\n");
		return -1;
	}

	if(to_read) {
		setsockopt(_fd, SOL_SOCKET, SO_REUSEADDR, (n=1,&n), sizeof(n));
		n = bind(_fd, (const struct sockaddr *)&addr, sizeof(addr));
		if(n < 0) {
			printf("bind(127.0.0.1:%d): %m\n", port);
			close(_fd);
			return -1;
		}
	}
	else {
		n = connect(_fd, (const struct sockaddr *)&addr, sizeof(addr));
		if(n < 0) {
			printf("connect(127.0.0.1:%d): %m\n", port);
			close(_fd);
			return -1;
		}
	}
	n = fcntl(_fd, F_SETFL, fcntl(_fd, F_GETFL) | O_NONBLOCK);
	if(n < 0) {
		printf("fcntl non_blocking(127.0.0.1:%d) fail, %m\n", port);
		close(_fd);
		return -1;
	}	
	return 0;
}
#endif
int CFifoSyncMQ::enqueue(const void* data, unsigned data_len, unsigned long long flow) {
	int ret = _mq.enqueue(data, data_len, flow);
	if(ret <= 0)
		return ret;
	
	//只有第一个包才发送通知（判断是否第一个包有缺陷，见CShmMQ的enqueue实现说明）
//	static struct timeval ss, ee;
//	static int diff = 0, c1 = 0, c2 = 0;
//	static long long sum = 0;
//	static int max = 0;
//	gettimeofday(&ss, NULL);
	write(_fd, "\0", 1);
//	gettimeofday(&ee, NULL);
//	diff = (ee.tv_sec - ss.tv_sec) * 1000000 + ee.tv_usec - ss.tv_usec;
//	c1++;
//	sum += diff;
//	if(diff > 1000) {
//		c2++;
//	}
//	if(max < diff)
//		max = diff;
//	if(c1 > 100000) {
//		printf("c1=%d,c2=%d,avg=%lf,max=%d\n", c1, c2, (double)sum / (double)c1, max);
//		c1 = c2 = 0;
//		sum = 0;
//		max = 0;
//	}
	return 0;
}

int CFifoSyncMQ::enqueue(const void* data, unsigned data_len, unsigned flow) {
	return enqueue(data, data_len, (unsigned long long)flow);
}
#if 0
int CFifoSyncMQ::dequeue(void* buf, unsigned buf_size, unsigned& data_len, unsigned long long &flow) {
	//	first, try to get data from Q
	int ret = _mq.dequeue(buf, buf_size, data_len, flow);
	if (ret || data_len)
		return ret;

	//	second, if no data, wait on fifo
	ret = select_fifo();
	if (ret == 0) {
		data_len = 0;
		return ret;
	}
	else if (ret < 0) {
		return -1;
	}

	//	third, if fifo activated, read the signals
	static const unsigned buf_len = 1<<10;
	char buffer[buf_len];
	ret = read(_fd, buffer, buf_len);
	if (ret < 0 && errno != EAGAIN)
		return -1;
	
	//	fourth, get data
	return _mq.dequeue(buf, buf_size, data_len, flow);

}

int CFifoSyncMQ::dequeue(void* buf, unsigned buf_size, unsigned& data_len, unsigned& flow) {
	unsigned long long ll_flow;
	//	first, try to get data from Q
	int ret = _mq.dequeue(buf, buf_size, data_len, ll_flow);
	flow = (unsigned)ll_flow;
	if (ret || data_len)
		return ret;

	//	second, if no data, wait on fifo
	ret = select_fifo();
	if (ret == 0) {
		data_len = 0;
		return ret;
	}
	else if (ret < 0) {
		return -1;
	}

	//	third, if fifo activated, read the signals
	static const unsigned buf_len = 1<<10;
	char buffer[buf_len];
	ret = read(_fd, buffer, buf_len);
	if (ret < 0 && errno != EAGAIN)
		return -1;
	
	//	fourth, get data
	ret = _mq.dequeue(buf, buf_size, data_len, ll_flow);
	flow = (unsigned)ll_flow;
	return ret;

}
int CFifoSyncMQ::select_fifo() {
	errno = 0;
    fd_set readfd;
    FD_ZERO(&readfd);
    FD_SET(_fd, &readfd);
    struct timeval tv;
    //tv.tv_sec = _wait_sec;
    //tv.tv_usec = _wait_usec;
    tv.tv_sec = 0;
    tv.tv_usec = 10;
    
    int ret = select(_fd+1, &readfd, NULL, NULL, &tv);
    if(ret > 0) {
        if(FD_ISSET(_fd, &readfd))
			return ret;
		else
			return -1;
    }
	else if (ret == 0) {
		return 0;
	}
	else {
		// select函数可能被USR信号中断，此时不应该close。 tomxiao 2006.10.26. 
		if (errno != EINTR) {
			close(_fd);
		}
		return -1;
	}
}
#endif
int CFifoSyncMQ::try_dequeue(void* buf, unsigned buf_size, unsigned& data_len, unsigned long long &flow) {
	return _mq.dequeue(buf, buf_size, data_len, flow);
}

int CFifoSyncMQ::try_dequeue(void* buf, unsigned buf_size, unsigned& data_len, unsigned& flow) {
	unsigned long long ll_flow;
	int ret = _mq.dequeue(buf, buf_size, data_len, ll_flow);
	flow = (unsigned)ll_flow;
	return ret;
}
void CFifoSyncMQ::clear_flag() {
#ifdef _MQ_OPT	
	static char buffer[64];
	if(++_buff_read_count == 64) {
		read(_fd, buffer, 64);
		_buff_read_count = 0;
	}
#else	
	static char buffer[1];
	read(_fd, buffer, 1);
#endif
}
