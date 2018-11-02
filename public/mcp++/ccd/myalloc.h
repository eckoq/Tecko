/*
 * 基于共享内存的内存分配器，从QNF的内存分配器myalloc移植过来
 * 支持线程安全，且严格限制chunk长度的最小值和最大值，用于进程之间
 * 传输消息包数据
 */
#ifndef _MYALLOC_H_
#define _MYALLOC_H_

typedef unsigned long MEM_HANDLE;
#define ZERO_HANDLE 0
#define NULL_HANDLE ((unsigned long)-1)

typedef struct {
	unsigned shmkey;			//共享内存key
	unsigned long shmsize;		//共享内存大小
	char semname[128];			//posix信号量名字
	unsigned minchunksize;		//最小块大小
	unsigned maxchunksize;		//最大块大小
	unsigned maxchunknum;		//最大块数
}myalloc_alloc_conf;

typedef struct {
	unsigned long tsize;		//总使用内存
	unsigned long fsize;		//剩余可使用内存
	unsigned tnum;				//总可分配块数
	unsigned fnum;				//剩余可分配块数
}myalloc_alloc_stat;

typedef struct memhead {
	MEM_HANDLE mem;				//内存相对地址
	unsigned len;				//内存长度
};

//进程启动的时候调用一次
extern int myalloc_init(myalloc_alloc_conf* conf);
//分配内存
extern MEM_HANDLE myalloc_alloc(unsigned size);
//释放内存
extern void myalloc_free(MEM_HANDLE h);
//地址映射
extern void* myalloc_addr(MEM_HANDLE h);
//取内存分配器统计信息
extern void myalloc_stat(myalloc_alloc_stat* st);
//进程终止的时候调用一次
extern void myalloc_fini();

#endif
