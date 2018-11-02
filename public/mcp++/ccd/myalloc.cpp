#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "myalloc.h"

#if !defined(__GNUC__) || (__GNUC__ == 2 && __GNUC_MINOR__ < 96)
#define __builtin_expect(x, expected_value) (x)
#endif
#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)

#define HARD_MAX_SIZE_NUM  (1<<14) 		//物理块大小分类数目
#define HARD_MIN_CHUNK_SIZE  (1<<10)	//物理最小块大小
#define HARD_MAX_CHUNK_SIZE	 (1<<30) 	//物理最大块大小

//数据块头部
typedef struct {
	unsigned psize;			//前一块的块大小
	unsigned csize:31;		//当前块的块大小
	unsigned inuse:1;		//1-使用中,0-空闲
	unsigned index;			//myalloc_chunk_desc的序号
}myalloc_chunk_head;

//数据块描述
typedef struct {
	unsigned size;			//块大小
	MEM_HANDLE addr;		//数据块地址
	union {
		struct {
			MEM_HANDLE prev;	//空闲链表前一个节点
			MEM_HANDLE next;	//空闲链表后一个节点
		};
		MEM_HANDLE fnext;		//chunk描述链表的下一个节点
	};
}myalloc_chunk_desc;

//内存分配器头
typedef struct {
	unsigned long tsize;			//总使用内存
	unsigned long fsize;			//剩余空闲内存
	unsigned long datasec;			//数据区偏移量
	unsigned maxchunksize;			//最大块大小
	unsigned minchunksize;			//最小块大小
	unsigned maxchunknum;			//最大块数
	unsigned freechunknum;			//剩余可分配块数
	unsigned flsize;				//空闲链表数组大小
	MEM_HANDLE freelist[HARD_MAX_SIZE_NUM];
	MEM_HANDLE desclist;			//空闲块描述链表
	myalloc_chunk_desc chunks[0];	//数据块区域
}myalloc_alloc_head;

//内存分配器头部
static myalloc_alloc_head* myallochead = NULL;
//内存分配器互斥信号量
static sem_t* myallocsem = NULL;

static int getshmmemory(unsigned key, unsigned long size, void** mem) {

	int shmid = -1;
	int isnew = 0;
	if((shmid = shmget(key, size, 0)) != -1)
		isnew = 0;
	else {
		shmid = shmget(key, size, IPC_CREAT | 0644);
		if(shmid != -1)
			isnew = 1;
		else
			return -1;
	}
	//assign memory here
	*mem = shmat(shmid, NULL, 0);
	if(((long)*mem != -1))
		return isnew;
	else
		return -1;
}
static int insert_free_chunk(unsigned psize, unsigned csize, unsigned long addr) {

	MEM_HANDLE h;
	if((h = myallochead->desclist) == NULL_HANDLE)
		return -1;

	myalloc_chunk_desc* cd = &myallochead->chunks[h];
	myallochead->desclist = cd->fnext;

	cd->size = csize;
	cd->addr = addr;
	cd->fnext = NULL_HANDLE;

	unsigned sizeidx = (csize - sizeof(myalloc_chunk_head)) / myallochead->minchunksize;
	if(sizeidx >= myallochead->flsize)
		sizeidx = 0;

	cd->prev = NULL_HANDLE;
	MEM_HANDLE hh = myallochead->freelist[sizeidx];
	if(hh != NULL_HANDLE) {
		myalloc_chunk_desc* cd2 = &myallochead->chunks[hh];
		cd2->prev = h;
	}
	cd->next = hh;
	myallochead->freelist[sizeidx] = h;

	myalloc_chunk_head* ch = (myalloc_chunk_head*)((char*)myallochead + addr);
	ch->psize = 0;
	ch->csize = csize;
	ch->inuse = 0;
	ch->index = h;

	myallochead->freechunknum--;

	return 0;
}
int myalloc_init(myalloc_alloc_conf* conf) {

	if((conf->maxchunksize < HARD_MIN_CHUNK_SIZE) ||
		((conf->maxchunksize & (conf->maxchunksize - 1)) != 0) ||
		conf->maxchunknum < 1 ||
		conf->maxchunksize / conf->minchunksize >= HARD_MAX_SIZE_NUM ||
		conf->maxchunksize < conf->minchunksize ||
		conf->shmkey == 0	||
		conf->semname[0] != '/'
		) {
		printf("MYALLOC INVALID CONF\n");
		return -1;
	}
	unsigned long need_size = sizeof(myalloc_alloc_head);
	need_size += sizeof(myalloc_chunk_desc) * conf->maxchunknum;
	need_size += (sizeof(myalloc_chunk_head) + conf->minchunksize) * conf->maxchunknum;

	if(conf->shmsize < need_size) {
		printf("MEMORY NOT ENOUGH, shmsize=%lu,needsize=%lu\n", conf->shmsize, need_size);
		return -1;
	}

	void* mem;
	int ret = getshmmemory(conf->shmkey, conf->shmsize, &mem);
	if(ret >= 0) {
		myallochead = (myalloc_alloc_head*)mem;
	}
	else {
		printf("GET SHM MEMORY FAIL, shmkey=%u,shmsize=%lu,%m\n", conf->shmkey, conf->shmsize);
		return -1;
	}

	if(!ret) {
		if(myallochead->maxchunksize != conf->maxchunksize ||
		   myallochead->minchunksize != conf->minchunksize ||
		   myallochead->maxchunknum != conf->maxchunknum  ||
		   myallochead->tsize != conf->shmsize
		   ) {
			// printf("MYALLOC CONFLICT CONF\n");
			// return -1;

			// Double check for sync.
			sleep(1);
			if ( myallochead->maxchunksize != conf->maxchunksize ||
		  		 myallochead->minchunksize != conf->minchunksize ||
		  		 myallochead->maxchunknum != conf->maxchunknum  ||
		  		 myallochead->tsize != conf->shmsize ) {

		  		printf("MYALLOC CONFLICT CONF\n");
				return -1;
			}
		}
	}
	else {
		myallochead->tsize = conf->shmsize;
		myallochead->fsize = conf->shmsize - sizeof(myalloc_alloc_head) - sizeof(myalloc_chunk_desc) * conf->maxchunknum;
		myallochead->datasec = sizeof(myalloc_alloc_head) + sizeof(myalloc_chunk_desc) * conf->maxchunknum;
		myallochead->maxchunksize = conf->maxchunksize;
		myallochead->minchunksize = conf->minchunksize;
		myallochead->maxchunknum = conf->maxchunknum;
		myallochead->freechunknum = conf->maxchunknum;
		myallochead->flsize = conf->maxchunksize / conf->minchunksize + 1;
		unsigned i;
		for(i = 0; i < myallochead->flsize; ++i)
			myallochead->freelist[i] = NULL_HANDLE;
		myallochead->desclist = ZERO_HANDLE;

		myalloc_chunk_desc* cd = &myallochead->chunks[0];
		for(i = 0; i < myallochead->maxchunknum; ++i) {
			memset(cd, 0x0, sizeof(myalloc_chunk_desc));
			cd->prev = cd->next = NULL_HANDLE;
			cd->fnext = i + 1;
			cd++;
		}
		(--cd)->fnext = NULL_HANDLE;

		unsigned long phy_free_size = myallochead->fsize;
		unsigned long phy_free_offset = myallochead->datasec;
		unsigned psize, csize;
		for(i = 0; ; ++i) {
			if(i == 0)
				psize = 0;
			else
				psize = HARD_MAX_CHUNK_SIZE;

			if(phy_free_size < (unsigned long)HARD_MAX_CHUNK_SIZE)
				csize = (unsigned)phy_free_size;
			else
				csize = HARD_MAX_CHUNK_SIZE;

			insert_free_chunk(psize, csize, phy_free_offset);
			//printf("insert free chunk, phy_free_size=%lu, phy_free_offset=%lu, psize=%u, csize=%u, offset=%lu\n", phy_free_size, phy_free_offset, psize, csize, phy_free_offset);
			phy_free_size -= csize;
			phy_free_offset += csize;

			if(phy_free_size == 0)
				break;
		}
	}

	myallocsem = sem_open(conf->semname, O_CREAT, 0644, 1);
	if(myallocsem == SEM_FAILED) {
		printf("SEM OPEN FAIL, %s,%m\n", conf->semname);
		return -1;
	}

	return 0;
}
static inline myalloc_chunk_desc* get_free_chunk(unsigned sizeidx, MEM_HANDLE* nh) {
	MEM_HANDLE h = myallochead->freelist[sizeidx];
	myalloc_chunk_desc* cd = NULL;
	if(h != NULL_HANDLE) {
		cd = &myallochead->chunks[h];
		myallochead->freelist[sizeidx] = cd->next;
		if(cd->next != NULL_HANDLE) {
			myalloc_chunk_desc* tmp_cd = &myallochead->chunks[cd->next];
			tmp_cd->prev = NULL_HANDLE;
		}
		*nh = h;
		cd->prev = cd->next = NULL_HANDLE;
	}
	return cd;
}
static inline void add_free_chunk(myalloc_chunk_desc* cd, MEM_HANDLE h) {

	unsigned sizeidx = (cd->size - sizeof(myalloc_chunk_head)) / myallochead->minchunksize;

	if(sizeidx >= myallochead->flsize)
		sizeidx = 0;

	MEM_HANDLE hh = myallochead->freelist[sizeidx];
	if(hh != NULL_HANDLE) {
		myalloc_chunk_desc* tmp_cd = &myallochead->chunks[hh];
		tmp_cd->prev = h;
	}
	cd->prev = NULL_HANDLE;
	cd->next = hh;
	myallochead->freelist[sizeidx] = h;
}
static inline void del_free_chunk(myalloc_chunk_desc* cd) {

	myalloc_chunk_desc* tmp_cd = NULL;
	if(cd->prev != NULL_HANDLE) {
		tmp_cd = &myallochead->chunks[cd->prev];
		tmp_cd->next = cd->next;
	}
	else {
		unsigned sizeidx = (cd->size - sizeof(myalloc_chunk_head)) / myallochead->minchunksize;
		if(sizeidx >= myallochead->flsize)
			sizeidx = 0;
		myallochead->freelist[sizeidx] = cd->next;
	}
	if(cd->next != NULL_HANDLE) {
		tmp_cd = &myallochead->chunks[cd->next];
		tmp_cd->prev = cd->prev;
	}
	cd->prev = cd->next = NULL_HANDLE;
}
MEM_HANDLE myalloc_alloc(unsigned size) {
	if ( !myallochead ) {
		return NULL_HANDLE;
	}

	if(unlikely(size > myallochead->maxchunksize))
		return NULL_HANDLE;

	if(size < myallochead->minchunksize)
		return NULL_HANDLE;
//	if(unlikely(size < myallochead->minchunksize))
//		size = myallochead->minchunksize;

	//查找合适大小的chunk块
	sem_wait(myallocsem);
	unsigned sizeidx = ((size + myallochead->minchunksize - 1) / myallochead->minchunksize);
	MEM_HANDLE h;
	myalloc_chunk_desc* cd = NULL;
	while(sizeidx < myallochead->flsize) {

		if((cd = get_free_chunk(sizeidx, &h)) != NULL)
			break;
		else
			sizeidx++;
	}
	if(cd == NULL) {
		if((cd = get_free_chunk(0, &h)) == NULL) {
			sem_post(myallocsem);
			return NULL_HANDLE;
		}
	}
// #ifdef MYALLOC_DEBUG
	if(cd->size < size + sizeof(myalloc_chunk_head)) {
		printf("ASSERT[%s:%d] %u %u %u\n", __FILE__, __LINE__, cd->size, size, sizeidx);
        sem_post(myallocsem);
        return NULL_HANDLE;
		// exit(-1);
	}
// #endif
	myalloc_chunk_head* ch = (myalloc_chunk_head*)((char*)myallochead + cd->addr);
	ch->inuse = 1;
	ch->index = h;
	if(ch->csize > (size + sizeof(myalloc_chunk_head) * 2 + myallochead->minchunksize)) {

		//申请一个新的chunk块描述，分割原来的chunk块
		MEM_HANDLE hh = myallochead->desclist;
		if(hh != NULL_HANDLE) {
			myallochead->freechunknum--;

			myalloc_chunk_desc* cd2 = &myallochead->chunks[hh];
			myallochead->desclist = cd2->fnext;

			//改变原来chunk块的描述信息和信息头中的大小
			unsigned csize = ch->csize;
			ch->csize = sizeof(myalloc_chunk_head) + size;
			cd->size = ch->csize;

			//新的chunk
			cd2->size = csize - cd->size;
			cd2->addr = cd->addr + cd->size;

			add_free_chunk(cd2, hh);

			myalloc_chunk_head* ch2 = (myalloc_chunk_head*)((char*)ch + ch->csize);
			ch2->psize = ch->csize;
			ch2->csize = cd2->size;
			ch2->inuse = 0;
			ch2->index = hh;

			//ch2不是最后一块，需要修改下一块的psize
			if(cd2->addr + cd2->size < myallochead->tsize) {
				myalloc_chunk_head* ch3 = (myalloc_chunk_head*)((char*)ch2 + ch2->csize);
				ch3->psize = ch2->csize;
			}
		}
		else {
#ifdef MYALLOC_DEBUG
			printf("NO FREE CHUNKDESC\n");
#endif
		}
	}
	myallochead->fsize -= cd->size;
#ifdef MYALLOC_DEBUG
	printf("ALLOC %u->%u\n", size, cd->size);
#endif
	sem_post(myallocsem);
	return cd->addr + sizeof(myalloc_chunk_head);
}
void myalloc_free(MEM_HANDLE h) {

	if ( !myallochead ) {
		return;
	}

	if(unlikely(h < myallochead->datasec || h > (myallochead->tsize - sizeof(myalloc_chunk_head))))
		return;

	myalloc_chunk_head* ch = (myalloc_chunk_head*)((char*)myallochead + h - sizeof(myalloc_chunk_head));
	if(unlikely(ch->inuse != 1))
		return;

	sem_wait(myallocsem);
	myalloc_chunk_desc* cd = &myallochead->chunks[ch->index];
	ch->inuse = 0;
	myallochead->fsize += cd->size;

	myalloc_chunk_desc* new_cd = cd;
	myalloc_chunk_head* new_ch = ch;
	myalloc_chunk_head* tmp_ch = NULL;

	if(ch->psize > 0) {
		myalloc_chunk_head* ch_prev = (myalloc_chunk_head*)((char*)ch - ch->psize);
		if(ch_prev->inuse == 0 && (new_cd->size + ch_prev->csize) <= HARD_MAX_CHUNK_SIZE) {
			myalloc_chunk_desc* cd_prev = &myallochead->chunks[ch_prev->index];

			//把该空闲块先从freelist去掉（后面会重新挂到不同的freelist）
			del_free_chunk(cd_prev);

			//修改该块的信息
			new_ch = ch_prev;
			new_cd = cd_prev;
			new_cd->size += cd->size;

			//释放原来的chunk（用户释放的那块）
			del_free_chunk(cd);

			cd->size = 0;
			cd->addr = 0;
			cd->fnext = myallochead->desclist;
			myallochead->desclist = ch->index;
			myallochead->freechunknum++;
		}
	}

	if(h < myallochead->tsize - ch->csize) {
		myalloc_chunk_head* ch_next = (myalloc_chunk_head*)((char*)ch + ch->csize);
		if(ch_next->inuse == 0 && (new_cd->size + ch_next->csize) <= HARD_MAX_CHUNK_SIZE) {
			myalloc_chunk_desc* cd_next = &myallochead->chunks[ch_next->index];
			new_cd->size += cd_next->size;

			//释放原来的chunk描述信息
			del_free_chunk(cd_next);

			cd_next->size = 0;
			cd_next->addr = 0;
			cd_next->fnext = myallochead->desclist;
			myallochead->desclist = ch_next->index;
			myallochead->freechunknum++;
		}
	}

	add_free_chunk(new_cd, new_ch->index);

	//合并了相邻至少一个空闲快，需要修改下一个非空闲块的psize
	if(new_cd->size != ch->csize) {
		//确保不是最后一个chunk块
		if(new_cd->size + new_cd->addr < myallochead->tsize) {
			tmp_ch = (myalloc_chunk_head*)((char*)new_ch + new_cd->size);
			//if(tmp_ch->inuse == 1)
				tmp_ch->psize = new_cd->size;
			//else {}	//不可能出现
		}
	}
	new_ch->csize = new_cd->size;
#ifdef MYALLOC_DEBUG
	printf("FREE %u->%u\n", ch->csize, new_cd->size);
#endif
	sem_post(myallocsem);
}
void* myalloc_addr(MEM_HANDLE h) {
	if ( !myallochead ) {
		return NULL;
	}

	if(h <= (myallochead->tsize - sizeof(myalloc_chunk_head)) && h >= (myallochead->datasec + sizeof(myalloc_chunk_head)))
		return (char*)myallochead + h;
	else
		return NULL;
}
void myalloc_stat(myalloc_alloc_stat* st) {

	memset(st, 0x0, sizeof(myalloc_alloc_stat));

	if ( !myallochead ) {
		return;
	}

	st->tsize = myallochead->tsize;
	st->fsize = myallochead->fsize;
	st->tnum = myallochead->maxchunknum;
	st->fnum = myallochead->freechunknum;

#ifdef MYALLOC_DEBUG
	printf("STAT %lu %lu %u %u\n", st->tsize, st->fsize, st->tnum, st->fnum);

	unsigned i;
	MEM_HANDLE h;
	myalloc_chunk_desc* cd = NULL;
	for(i = 0; i < myallochead->flsize; ++i) {
		h = myallochead->freelist[i];
		if(h == NULL_HANDLE)
			continue;
		else {
			printf("FREELIST[%u]", i);
			while(h != NULL_HANDLE) {
				cd = &myallochead->chunks[h];
				printf("->(%lu,%u,%lu)", h, cd->size, cd->addr);
				h = cd->next;
			}
			printf("\n");
		}
	}
#endif
}
void myalloc_fini() {
	if(myallochead != NULL) {
		shmdt(myallochead);
	}
	if(myallocsem != NULL) {
		sem_close(myallocsem);
	}
}
