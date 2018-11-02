/*
 * 一个磁盘数据块分配器，索引全部在内存中，数据在磁盘上，数据块不定长，每次读取或者写入只有一次磁盘IO
 * 从QNF的内存分配器myalloc移植改造
 */
#ifndef _MYDISKALLOC_H_
#define _MYDISKALLOC_H_

typedef unsigned long DISK_HANDLE;
#define ZERO_HANDLE 0
#define NULL_HANDLE ((unsigned long)-1)

#define MAX_FILE_LEN	256

#ifdef _DISKALLOC_LARGE_INDEX_
#define MAX_SIZE_NUM  	(4<<10) 	//块大小分类最大数目
#else
#define MAX_SIZE_NUM  	(1<<10)		//块大小分类最大数目
#endif

#define MAX_CHUNK_NUM	(1<<25)		//块的最大数目
#define MAX_CHUNK_SIZE	(1<<30) 	//最大块大小
#define MYDISK_MAGIC	0x94470099	

#define MYDISK_ERR_NOTENOUGH	-2001	//提供的内存不足以容纳所有索引数据
#define MYDISK_ERR_CONFERROR	-2002	//配置参数有误或者与已经存在冲突
#define MYDISK_ERR_DESCOVER		-2003	//块描述符用完
#define MYDISK_ERR_FREEOVER		-2004	//空闲块用完
#define MYDISK_ERR_OPENFILE		-2005	//打开文件失败
#define MYDISK_ERR_HANDLEERR	-2006	//DISKHANDLE无效
#define MYDISK_ERR_STATUSERR	-2007	//块状态不符合要求
#define MYDISK_ERR_READFAIL		-2008	//读数据失败
#define MYDISK_ERR_WRITEERR		-2009	//写数据失败
#define MYDISK_ERR_LENGTHERR	-2010	//数据块长度不符合要求
#define MYDISK_ERR_CHUNKERR		-2011	//找不到对应的数据块
#define MYDISK_ERR_MMAP			-2012	//映射数据块失败

//磁盘块描述
//注意：磁盘数据块没有加任何checksum，不能保证数据的完整性
typedef struct {
	unsigned size;				//块大小
	unsigned long long addr;	//数据块地址
	union {
		struct {
			unsigned inuse:1;	//0-空闲，1-使用中
		};
		unsigned flag;
	};
	DISK_HANDLE cprev;			//逻辑上相邻的前一个块
	DISK_HANDLE cnext;			//逻辑上相邻的后一个块
	union {
		struct {
			DISK_HANDLE fprev;	//freelist空闲链表前一个节点
			DISK_HANDLE fnext;	//freelist空闲链表后一个节点
		};
		DISK_HANDLE dnext;		//desclist链表的下一个节点
	};
}mydiskalloc_chunk;

//磁盘块分配器
typedef struct {
	unsigned magic;						//magic number
	char diskfile[MAX_FILE_LEN];		//disk文件路径
	int diskfd;							//disk文件句柄
	unsigned long long totalsize;		//总的可使用空间	
	unsigned long long freesize;		//剩余可使用空间
#ifdef _MYDISKALLOC_DEBUG
	unsigned long long alloc_cnt;		//分配次数
	unsigned long long free_cnt;		//释放次数
	unsigned long long split_cnt;		//分配时分裂次数
	unsigned long long merge_front_cnt;	//释放时向前合并次数
	unsigned long long merge_after_cnt;	//释放时向后合并次数
#endif
	unsigned totalchunknum;				//chunks总块数
	unsigned minchunksize;				//最小chunk大小
	unsigned descchunknum;				//desclist里面的chunks块数
	unsigned freechunknum;				//freelist里面的chunks块数
	DISK_HANDLE desclist;				//可使用的chunks块描述符链表
	DISK_HANDLE freelist[MAX_SIZE_NUM];	//free状态的chunks块哈希表
#ifdef _MYDISKALLOC_FREEINDEX
	unsigned freeindex[MAX_SIZE_NUM];	//快速空闲块索引
#endif	
	mydiskalloc_chunk chunks[0];		//数据块区域
}mydiskalloc_head;

//初始化分配器资源
#ifndef _TDC_DISKCACHE_
extern int mydiskalloc_init(mydiskalloc_head** myhead, void* mem, unsigned long memsize, char init, const char* diskfile, unsigned long long disksize, unsigned minchunksize);
#else
extern int mydiskalloc_init(mydiskalloc_head** myhead, void* mem, unsigned long memsize, char init, const char* diskfile, unsigned long long disksize, unsigned minchunksize, unsigned chunktotal);
#endif
//分配数据块，调用者保存DISK_HANDLE以便写入或者读取数据块内容
//注意：DISK_HANDLE到真正数据块的物理地址映射（即文件的偏移量）是单向映射
extern DISK_HANDLE mydiskalloc_alloc(mydiskalloc_head* myhead, unsigned size);
//释放数据块
extern void mydiskalloc_free(mydiskalloc_head* myhead, DISK_HANDLE h);
//读取数据块内容
extern int mydiskalloc_read(mydiskalloc_head* myhead, DISK_HANDLE h, void* data_buf, unsigned data_len);
//写入数据块内容
extern int mydiskalloc_write(mydiskalloc_head* myhead, DISK_HANDLE h, const void* data_buf, unsigned data_len); 
//采用mmap的方式读取数据块内容
extern int mydiskalloc_mmap(mydiskalloc_head* myhead, DISK_HANDLE h, char** addr, unsigned data_len);
//unmap数据块
extern void mydiskalloc_unmmap(mydiskalloc_head* myhead, DISK_HANDLE h, char* addr, unsigned data_len);
//释放分配器资源
extern void mydiskalloc_fini(mydiskalloc_head* myhead);
//计算索引需要使用的内存大小
#ifndef _TDC_DISKCACHE_
extern unsigned long mydiskalloc_calcsize(unsigned long long disksize, unsigned minchunksize);
#else
extern unsigned long mydiskalloc_calcsize(unsigned chunktotal);
#endif

#ifdef _TDC_DISKCACHE_
extern int mydiskalloc_data_dump(mydiskalloc_head* myhead, const char *dump_name);
extern int mydiskalloc_data_recover(mydiskalloc_head* myhead, const char *dump_name, char *bak_name);
#endif

#ifdef _TDC_DISKCACHE_
extern mydiskalloc_chunk* get_chunk_extern(mydiskalloc_head* myhead, DISK_HANDLE h);
#endif

#endif

