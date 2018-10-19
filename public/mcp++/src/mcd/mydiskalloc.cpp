#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "mydiskalloc.h"

#if !defined(__GNUC__) || (__GNUC__ == 2 && __GNUC_MINOR__ < 96)
#define __builtin_expect(x, expected_value) (x)
#endif
#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)

static const int pagesize = getpagesize();

static int init_free_chunk(mydiskalloc_head* myhead, unsigned size, unsigned long long addr, DISK_HANDLE cprev, DISK_HANDLE cnext) {
	
	DISK_HANDLE h;
	if((h = myhead->desclist) == NULL_HANDLE)
		return MYDISK_ERR_DESCOVER;

	mydiskalloc_chunk* cd = &myhead->chunks[h];
	myhead->desclist = cd->dnext;
	
	cd->size = size;
	cd->addr = addr;
	cd->inuse = 0;
	cd->cprev = cprev;
	cd->cnext = cnext;
	cd->dnext = NULL_HANDLE;

	unsigned sizeidx = size / myhead->minchunksize;
	if(sizeidx >= MAX_SIZE_NUM)
		sizeidx = 0; 

/*	cd->fprev = NULL_HANDLE;
	DISK_HANDLE hh = myhead->freelist[sizeidx];
	if(hh != NULL_HANDLE) {
		mydiskalloc_chunk* cd2 = &myhead->chunks[hh];
		cd2->fprev = h;
	}	
	cd->fnext = hh;
	myhead->freelist[sizeidx] = h;
	myhead->descchunknum--;
	myhead->freechunknum++;
*/
	mydiskalloc_chunk* cd_prev = NULL;
	DISK_HANDLE hh = myhead->freelist[sizeidx];
	while(hh != NULL_HANDLE) {
		cd_prev = &myhead->chunks[hh];
		hh = cd_prev->fnext;	
	}
	if(cd_prev == NULL) {
		cd->fprev = cd->fnext = NULL_HANDLE;
		myhead->freelist[sizeidx] = h;
	}
	else {
		cd_prev->fnext = h;
		cd->fprev = hh;
		cd->fnext = NULL_HANDLE;
	}
			
	return 0;
}
static inline int init_file(mydiskalloc_head* myhead) {

	if(myhead->diskfd > 0)
		close(myhead->diskfd);

	myhead->diskfd = open(myhead->diskfile, O_CREAT | O_RDWR| O_LARGEFILE, 0666);	
	
	return myhead->diskfd < 0 ? -1 : 0;
}

#ifndef _TDC_DISKCACHE_
unsigned long mydiskalloc_calcsize(unsigned long long disksize, unsigned minchunksize) {

	unsigned totalchunknum = disksize / minchunksize;
	if(totalchunknum > MAX_CHUNK_NUM)
		totalchunknum = MAX_CHUNK_NUM;
	
	return sizeof(mydiskalloc_head) + sizeof(mydiskalloc_chunk) * totalchunknum;
}
#else
unsigned long mydiskalloc_calcsize(unsigned chunktotal) {

	if(chunktotal > MAX_CHUNK_NUM)
		chunktotal = MAX_CHUNK_NUM;
	
	return sizeof(mydiskalloc_head) + sizeof(mydiskalloc_chunk) * chunktotal;
}
#endif

#ifndef _TDC_DISKCACHE_
int mydiskalloc_init(mydiskalloc_head** myhead, void* mem, unsigned long memsize, char init, const char* diskfile, unsigned long long disksize, unsigned minchunksize) {

	unsigned totalchunknum = disksize / minchunksize;
	if(totalchunknum > MAX_CHUNK_NUM)
		totalchunknum = MAX_CHUNK_NUM;
#else
int mydiskalloc_init(mydiskalloc_head** myhead, void* mem, unsigned long memsize, char init, const char* diskfile, unsigned long long disksize, unsigned minchunksize, unsigned chunktotal) {

	unsigned totalchunknum = chunktotal;
	if(totalchunknum > MAX_CHUNK_NUM)
		totalchunknum = MAX_CHUNK_NUM;
#endif			
	unsigned long needsize = sizeof(mydiskalloc_head) + sizeof(mydiskalloc_chunk) * totalchunknum;
	
	if(memsize < needsize) {
		printf("mydiskalloc MEMORY NOT ENOUGH, %lu<%lu\n", memsize, needsize);
		return MYDISK_ERR_NOTENOUGH;
	}	
			
	mydiskalloc_head* head = (mydiskalloc_head*)mem;
	head->diskfd = -1;
	if(!init) {
		//校验头部数据
		if(head->magic != MYDISK_MAGIC ||
		   strncmp(head->diskfile, diskfile, 256) != 0 || 
		   head->totalsize != disksize ||
		   head->totalchunknum != totalchunknum || 
		   head->minchunksize != minchunksize
		   ) {
			
			printf("mydiskalloc CONFLICT CONF\n");
			return MYDISK_ERR_CONFERROR;
		}			
	}
	else {
		//初始化头部
		head->magic = MYDISK_MAGIC;
		strncpy(head->diskfile, diskfile, 256);
		head->totalsize = disksize;
		head->freesize = disksize;
		head->totalchunknum = totalchunknum;
		head->minchunksize = minchunksize;
		head->descchunknum = totalchunknum;
		head->freechunknum = 0;
#ifdef _MYDISKALLOC_DEBUG
		head->alloc_cnt = head->free_cnt = head->split_cnt = head->merge_front_cnt = head->merge_after_cnt = 0;
#endif
		unsigned long i;
		for(i = 0; i < MAX_SIZE_NUM; ++i) {
#ifdef _MYDISKALLOC_FREEINDEX
			head->freeindex[i] = 0;
#endif
			head->freelist[i] = NULL_HANDLE;
		}	
		head->desclist = ZERO_HANDLE;
		
		mydiskalloc_chunk* cd = &head->chunks[0];
		for(i = 0; i < head->totalchunknum; ++i) {
			memset(cd, 0x0, sizeof(mydiskalloc_chunk));
			cd->cprev = cd->cnext = NULL_HANDLE;
			cd->fprev = cd->fnext = NULL_HANDLE;
			cd->dnext = i + 1;
			cd++;	
		}
		(--cd)->dnext = NULL_HANDLE;

		//初始化空闲块，且限制每个块最大不能超过MAX_CHUNK_SIZE
		unsigned long long phy_free_size = head->freesize;
		unsigned long long phy_free_offset = 0;
		unsigned size;
		DISK_HANDLE cprev, cnext;
		for(i = 0; ; ++i) {	
		
			cprev = (DISK_HANDLE)(i - 1);
			cnext = (DISK_HANDLE)(i + 1);
			
			if(phy_free_size < (unsigned long long)MAX_CHUNK_SIZE) {
				size = (unsigned)phy_free_size;
				cnext = NULL_HANDLE;
			}
			else  {
				size = MAX_CHUNK_SIZE;	
				if(phy_free_size == (unsigned long long)MAX_CHUNK_SIZE)
					cnext = NULL_HANDLE;
			}
			
			init_free_chunk(head, size, phy_free_offset, cprev, cnext);
			phy_free_size -= size;
			phy_free_offset += size;

			if(phy_free_size == 0) {
				break;
			}	
		}
	}
	
	//打开文件	
	if(init_file(head) < 0) {
		printf("mydiskalloc OPEN FILE ERROR, %m\n");
		return MYDISK_ERR_OPENFILE;
	}
	*myhead = head;
	
	return 0;
}
static inline mydiskalloc_chunk* get_free_chunk(mydiskalloc_head* myhead, unsigned sizeidx, DISK_HANDLE* nh) {
	
	DISK_HANDLE h = myhead->freelist[sizeidx];
	mydiskalloc_chunk* cd = NULL;
	if(h != NULL_HANDLE) {
		cd = &myhead->chunks[h];	
		myhead->freelist[sizeidx] = cd->fnext;
		if(cd->fnext != NULL_HANDLE) {
			mydiskalloc_chunk* tmp_cd = &myhead->chunks[cd->fnext];
			tmp_cd->fprev = NULL_HANDLE;
		}
		*nh = h;
		cd->fprev = cd->fnext = NULL_HANDLE;
		
		myhead->freechunknum--;
	}

	return cd;
}
static inline mydiskalloc_chunk* get_free_chunk_0(mydiskalloc_head* myhead, unsigned size, DISK_HANDLE* nh) {
	
	DISK_HANDLE h = myhead->freelist[0];
	mydiskalloc_chunk* cd = NULL;
	while(h != NULL_HANDLE) {
		cd = &myhead->chunks[h];	
		if(cd->size >= size) {			
			myhead->freelist[0] = cd->fnext;
			if(cd->fnext != NULL_HANDLE) {
				mydiskalloc_chunk* tmp_cd = &myhead->chunks[cd->fnext];
				tmp_cd->fprev = NULL_HANDLE;
			}
			*nh = h;
			cd->fprev = cd->fnext = NULL_HANDLE;
			myhead->freechunknum--;
			break;
		}
		else {
			h = cd->fnext;
		}
	}

	return cd;
}
static inline void add_free_chunk(mydiskalloc_head* myhead, mydiskalloc_chunk* cd, DISK_HANDLE h) {

	unsigned sizeidx = cd->size / myhead->minchunksize;   

	if(sizeidx >= MAX_SIZE_NUM)
		sizeidx = 0;
#ifdef _MYDISKALLOC_FREEINDEX
	else {
		int i;
		for(i = (int)sizeidx; i >= 1; --i) {
			if((myhead->freeindex[i] == 0) || (myhead->freeindex[i] > sizeidx))
				myhead->freeindex[i] = sizeidx;
			else
				break;
		}
	}
#endif		

	DISK_HANDLE hh = myhead->freelist[sizeidx];
	if(hh != NULL_HANDLE) {
		mydiskalloc_chunk* tmp_cd = &myhead->chunks[hh];
		tmp_cd->fprev = h;
	}
	cd->fprev = NULL_HANDLE;
	cd->fnext = hh;
	myhead->freelist[sizeidx] = h;
	myhead->freechunknum++;
}
static inline void del_free_chunk(mydiskalloc_head* myhead, mydiskalloc_chunk* cd) {

	mydiskalloc_chunk* tmp_cd = NULL;
	if(cd->fprev != NULL_HANDLE) {
		tmp_cd = &myhead->chunks[cd->fprev];
		tmp_cd->fnext = cd->fnext;
	}
	else {
		unsigned sizeidx = cd->size / myhead->minchunksize;
		if(sizeidx >= MAX_SIZE_NUM)
			sizeidx = 0;
		myhead->freelist[sizeidx] = cd->fnext;
	}
	if(cd->fnext != NULL_HANDLE) {
		tmp_cd = &myhead->chunks[cd->fnext];
		tmp_cd->fprev = cd->fprev;
	}
	cd->fprev = cd->fnext = NULL_HANDLE;
	myhead->freechunknum--;
}
DISK_HANDLE mydiskalloc_alloc(mydiskalloc_head* myhead, unsigned size) {
	
	//查找合适大小的chunk块
	if(size < myhead->minchunksize)
		size = myhead->minchunksize;
	
/*	按pagesize对齐可能会轻微提高一点性能，但是很可能造成空间浪费
	int align = size % pagesize;
	if(align != 0) {
		size += (pagesize - align);
	}
*/
	unsigned sizeidx = ((size + myhead->minchunksize - 1) / myhead->minchunksize);
	DISK_HANDLE h = NULL_HANDLE;
	mydiskalloc_chunk* cd = NULL;
	unsigned i = sizeidx;
		
	while(i < MAX_SIZE_NUM) {
		
#ifdef _MYDISKALLOC_FREEINDEX
		i = myhead->freeindex[i];
		if(i == 0) {
			break;
		}
#endif	
		if((cd = get_free_chunk(myhead, i, &h)) != NULL) {
#ifdef _MYDISKALLOC_FREEINDEX
			if(i != sizeidx) {
				myhead->freeindex[sizeidx] = i;
			}
#endif						
			break;
		}	
		else
			i++;
	}
	if(cd == NULL) {
#ifdef _MYDISKALLOC_FREEINDEX
		if(sizeidx < MAX_SIZE_NUM)
			myhead->freeindex[sizeidx] = 0;	
#endif			
		if((cd = get_free_chunk_0(myhead, size, &h)) == NULL) {
			printf("ALLOC FAULT: OUT OF MEMORY: freesize=%llu,freechunknum=%u,alloc_size=%u\n", myhead->freesize, myhead->freechunknum, size);
			return NULL_HANDLE;
		}	
	}

	if(unlikely(cd->size < size || cd->inuse == 1)) {
		printf("ALLOC FAULT: alloc_size=%u,need_size=%u,sizeidx=%u,inuse=%d\n", cd->size, size, sizeidx, cd->inuse);
		return NULL_HANDLE;
	}

	cd->inuse = 1;
	
	if(cd->size >= (size + myhead->minchunksize)) {	//可以分裂至少一个块
		
		//申请一个新的chunk块描述，分割原来的chunk块
		DISK_HANDLE hh = myhead->desclist;	
		if(hh != NULL_HANDLE) {
			myhead->descchunknum--;
			
			mydiskalloc_chunk* cd2 = &myhead->chunks[hh];
			myhead->desclist = cd2->dnext;
		
			//新的chunk
			cd2->size = cd->size - size;
			cd2->addr = cd->addr + size;
			cd2->inuse = 0;
			cd2->cprev = h;
			cd2->cnext = cd->cnext;
			if(cd->cnext != NULL_HANDLE) {
				mydiskalloc_chunk* tmp_cd = &myhead->chunks[cd->cnext];
				tmp_cd->cprev = hh;
			}
				
			add_free_chunk(myhead, cd2, hh); 
		
			//改变原来chunk块的描述信息
			cd->size = size;
			cd->cnext = hh;
		
#ifdef _MYDISKALLOC_DEBUG
			myhead->split_cnt++;	
#endif				
		}
		else {
			//分裂不成功，浪费一部分空间，不影响逻辑
		}
	}
#ifdef _MYDISKALLOC_DEBUG
	myhead->alloc_cnt++;	
#endif				
	myhead->freesize -= cd->size;
	return h;
}
void mydiskalloc_free(mydiskalloc_head* myhead, DISK_HANDLE h) {
	
	if(unlikely(h >= myhead->totalchunknum)) {
		printf("FREE FAULT: h=%lu,totalchunknum=%u\n", h, myhead->totalchunknum);
		return;
	}	
				
	mydiskalloc_chunk* cd = &myhead->chunks[h];
	if(unlikely(cd->inuse == 0)) {
		printf("FREE WARNNING: h=%lu,inuse=0\n", h);
		return;
	}	

	cd->inuse = 0;
	myhead->freesize += cd->size;

	mydiskalloc_chunk* cd_new = cd;
	DISK_HANDLE h_new = h;

	mydiskalloc_chunk* cd_prev = NULL;
	DISK_HANDLE h_prev = cd->cprev;
	mydiskalloc_chunk* cd_next = NULL;
	DISK_HANDLE h_next = cd->cnext; 
	
	if(h_prev != NULL_HANDLE) {
		//尝试向前合并空闲块
		cd_prev = &myhead->chunks[h_prev];	
		if((cd_prev->inuse == 0) && (MAX_CHUNK_SIZE - cd_prev->size >= cd->size)) {
			
			del_free_chunk(myhead, cd_prev);
			cd_prev->size += cd->size;
			cd_prev->cnext = h_next;
			if(h_next != NULL_HANDLE) {
				cd_next = &myhead->chunks[h_next];
				cd_next->cprev = h_prev;
			}
				
			cd_new = cd_prev;
			h_new = h_prev;

			cd->size = cd->addr = cd->inuse = 0;
			cd->cprev = cd->cnext = NULL_HANDLE;
			cd->dnext = myhead->desclist;
			myhead->desclist = h;
			myhead->descchunknum++;	
#ifdef _MYDISKALLOC_DEBUG
			myhead->merge_front_cnt++;	
#endif				
		}
		else {
		}
	}
	
	if(h_next != NULL_HANDLE) {
		//尝试向后合并空闲块
		cd_next = &myhead->chunks[h_next];
		if((cd_next->inuse == 0) && (MAX_CHUNK_SIZE - cd_new->size >= cd_next->size)) {
			
			cd_new->size += cd_next->size;
			cd_new->cnext = cd_next->cnext;
		
			if(cd_next->cnext != NULL_HANDLE) {
				mydiskalloc_chunk* cd_next_next = &myhead->chunks[cd_next->cnext];
				cd_next_next->cprev = h_new;
			}
			
			del_free_chunk(myhead, cd_next);
			cd_next->dnext = myhead->desclist;
			myhead->desclist = h_next;
			myhead->descchunknum++;	
			cd_next->cprev = cd_next->cnext = NULL_HANDLE;	
			cd_next->size = cd_next->addr = cd_next->inuse = 0;
#ifdef _MYDISKALLOC_DEBUG
			myhead->merge_after_cnt++;	
#endif				
		}
		else {
		}
	}
	
#ifdef _MYDISKALLOC_DEBUG
	myhead->free_cnt++;	
#endif				
	//加入合并后的空闲块	
	add_free_chunk(myhead, cd_new, h_new);	
}

inline mydiskalloc_chunk* get_chunk(mydiskalloc_head* myhead, DISK_HANDLE h) {
	
	if(unlikely(h >= myhead->totalchunknum)) {
		printf("GET FAULT: h=%lu,totalchunknum=%u\n", h, myhead->totalchunknum);
		return NULL;
	}	
	
	mydiskalloc_chunk* cd = &myhead->chunks[h];
	if(unlikely(cd->inuse == 0)) {
		printf("GET WARNNING: h=%lu,inuse=0\n", h);
		return NULL;
	}	
	
	return cd;		
}

#ifdef _TDC_DISKCACHE_
mydiskalloc_chunk* get_chunk_extern(mydiskalloc_head* myhead, DISK_HANDLE h)
{
	return get_chunk(myhead, h);
}
#endif

int mydiskalloc_read(mydiskalloc_head* myhead, DISK_HANDLE h, void* data_buf, unsigned data_len) {
	
	mydiskalloc_chunk* cd = get_chunk(myhead, h);
	if(unlikely(cd == NULL))
		return MYDISK_ERR_CHUNKERR;
	if(unlikely(cd->size < data_len))
		return MYDISK_ERR_LENGTHERR;
		
try_again:
	if(pread64(myhead->diskfd, data_buf, data_len, cd->addr) == (int)data_len) {
		return 0;
	}	
	else {		
		//防止diskfd被外部应用异常关闭，EBADF应该不会循环出现
		if(errno == EBADF) {
			if(init_file(myhead) == 0) {
				goto try_again;
			}
		}
		return MYDISK_ERR_READFAIL;
	}
}
int mydiskalloc_write(mydiskalloc_head* myhead, DISK_HANDLE h, const void* data_buf, unsigned data_len) {

	mydiskalloc_chunk* cd = get_chunk(myhead, h);
	if(unlikely(cd == NULL))
		return MYDISK_ERR_CHUNKERR;
	if(unlikely(cd->size < data_len))
		return MYDISK_ERR_LENGTHERR;

try_again:			
	if(pwrite64(myhead->diskfd, data_buf, data_len, cd->addr) == (int)data_len) {
		return 0;
	}	
	else {
		//防止diskfd被外部应用异常关闭，EBADF应该不会循环出现
		if(errno == EBADF) {
			if(init_file(myhead) == 0) {
				goto try_again;
			}
		}		
		return MYDISK_ERR_WRITEERR;
	}
}
int mydiskalloc_mmap(mydiskalloc_head* myhead, DISK_HANDLE h, char** addr, unsigned data_len) {

	mydiskalloc_chunk* cd = get_chunk(myhead, h);
	if(unlikely(cd == NULL))
		return MYDISK_ERR_CHUNKERR;
	if(unlikely(cd->size < data_len))
		return MYDISK_ERR_LENGTHERR;

	int align = cd->addr % pagesize;
	char* maddr = (char*)mmap64(NULL, data_len + align, PROT_READ, MAP_PRIVATE, myhead->diskfd, cd->addr - align);
	if(maddr != MAP_FAILED)	{
		*addr = maddr + align;
		return 0;
	}
	else {
		return MYDISK_ERR_MMAP;
	}
}
void mydiskalloc_unmmap(mydiskalloc_head* myhead, DISK_HANDLE h, char* addr, unsigned data_len) {
	
/*	
	这里目前实现不关心myhead和h，可以随便填写
	mydiskalloc_chunk* cd = get_chunk(myhead, h);
	if(unlikely(cd == NULL))
		return MYDISK_ERR_CHUNKERR;
	if(unlikely(cd->size < data_len))
		return MYDISK_ERR_LENGTHERR;
*/
	int align = ((unsigned long)addr) % pagesize;
	munmap(addr - align, data_len + align);
}

#ifdef _TDC_DISKCACHE_
// Only read lock.
int mydiskalloc_data_dump(mydiskalloc_head* myhead, const char *dump_name)
{
	const int				FILE_BUF_SIZE = (1<<20);
	char					name0[256];
	char					file_buf[FILE_BUF_SIZE];
	int						dfd = -1;
	unsigned long long		left = 0;
	int						rw_bytes = 0;
	struct stat				st;

	memset(name0, 0, sizeof(char) * 256);

	snprintf(name0, 255, "%s.old", dump_name);
	rename(dump_name, name0);

	dfd = open(dump_name, O_RDWR | O_CREAT | O_TRUNC | O_LARGEFILE, 0644);
	if ( dfd == -1 ) {
		printf("Open dump file %s fail! %m\n", dump_name);
		goto err_out;
	}

	if ( fstat(myhead->diskfd, &st) ) {
		printf("Get cache file status fail! %m\n");
		goto err_out;
	}

	left = (unsigned long long)st.st_size;

	if ( lseek64(myhead->diskfd, 0, SEEK_SET) == (off_t)-1 ) {
		printf("Seek file fail! %m\n");
		goto err_out;
	}

	while ( left > 0 ) {
		if ( left >= (unsigned)FILE_BUF_SIZE ) {
			rw_bytes = FILE_BUF_SIZE;
		} else {
			rw_bytes = left;
		}

		if ( read(myhead->diskfd, file_buf, rw_bytes) != rw_bytes ) {
			printf("Read cache file fail! %m\n");
			goto err_out;
		}

		if ( write(dfd, file_buf, rw_bytes) != rw_bytes ) {
			printf("Write dump file fail! %m\n");
			goto err_out;
		}

		left -= rw_bytes;
	}

	if ( dfd >= 0 ) {
		close(dfd);
		dfd = -1;
	}

	return 0;

err_out:
	if ( dfd >= 0 ) {
		close(dfd);
		dfd = -1;
	}

	return -1;
}

// Must write lock.
int mydiskalloc_data_recover(mydiskalloc_head* myhead, const char *dump_name, char *bak_name)
{
	const int				FILE_BUF_SIZE = (1<<20);
	char					name0[256];
	char					file_buf[FILE_BUF_SIZE];
	int						dumpfd = -1;
	unsigned long long		left = 0;
	int						rw_bytes = 0;
	struct stat				st;

	if ( myhead->diskfd >= 0 ) {
		close(myhead->diskfd);
		myhead->diskfd = -1;
	}

	memset(name0, 0, sizeof(char) * 256);
	snprintf(name0, 255, "%s.bak", myhead->diskfile);	
	if ( rename(myhead->diskfile, name0) ) {
		printf("Rename disk file %s fail! %m\n", myhead->diskfile);
		return -1;
	}

	myhead->diskfd = open(myhead->diskfile, O_CREAT | O_RDWR | O_TRUNC | O_LARGEFILE, 0666);
	if ( myhead->diskfd == -1 ) {
		printf("Create cache file %s fail! %m\n", myhead->diskfile);
		return -1;
	}

	dumpfd = open(dump_name, O_RDONLY | O_LARGEFILE);
	if ( dumpfd == -1 ) {
		printf("Open dump file %s fail! %m\n", myhead->diskfile);
		return -1;
	}

	if ( fstat(dumpfd, &st) ) {
		printf("Get dump file %s status fail! %m\n", dump_name);
		goto err_out;
	}

	left = (unsigned long long)st.st_size;

	while ( left > 0 ) {
		if ( left >= (unsigned)FILE_BUF_SIZE ) {
			rw_bytes = FILE_BUF_SIZE;
		} else {
			rw_bytes = left;
		}

		if ( read(dumpfd, file_buf, rw_bytes) != rw_bytes ) {
			printf("Read dump file %s fail! %m\n", dump_name);
			goto err_out;
		}

		if ( write(myhead->diskfd, file_buf, rw_bytes) != rw_bytes ) {
			printf("Write cache file %s fail! %m\n", myhead->diskfile);
			goto err_out;
		}

		left -= rw_bytes;
	}

	if ( dumpfd >= 0 ) {
		close(dumpfd);
		dumpfd = -1;
	}

	if ( bak_name ) {
		if ( rename(name0, bak_name) ) {
			printf("Rename cache backup file fail! %m\n");
			return -1;
		}
	} else {
		unlink(name0);
	}

	return 0;

err_out:
	if ( dumpfd >= 0 ) {
		close(dumpfd);
		dumpfd = -1;
	}

	return -1;
}
#endif

void mydiskalloc_fini(mydiskalloc_head* myhead) {
	if(myhead->diskfd >= 0) {
		close(myhead->diskfd);
		myhead->diskfd = -1;
	}
}

