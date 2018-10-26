#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include "list.h"
#include "fastmem.h"

#if !defined(__GNUC__) || (__GNUC__ == 2 && __GNUC_MINOR__ < 96)
#define __builtin_expect(x, expected_value) (x)
#endif
#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)

#define MEM_MIN_SIZE		(1<<13)		//8KB
//#ifdef _NS_								//in ccd，短连接居多，缓冲区较小
#define MEM_MAX_SIZE		(1<<23)		//32MB
#define MEM_STEP_SIZE		(1<<13)		//8KB
//#else									//in dcc，长连接居多，缓冲区较大
//#define MEM_MAX_SIZE		(1<<25)		//32MB
//#define MEM_STEP_SIZE		(1<<14)		//16KB
//#endif
#define MEM_MAP_SIZE		((MEM_MAX_SIZE - MEM_MIN_SIZE) / MEM_STEP_SIZE + 1)  
#define MAX_SCAN_TIMES		(MEM_MAP_SIZE / 4)

struct mobj {
	unsigned size;		//mem size
	list_head_t list;	//listhead in freelist
	list_head_t vlist;	//listhead in lrulist
	char addr[0];		//mem addr
};

list_head_t mem_maps[MEM_MAP_SIZE];
list_head_t used_list;
unsigned long free_mem_size;
unsigned long free_thresh;
#ifdef _FASTMEM_DEBUG
unsigned na_get_cnt;
unsigned na_put_cnt;
unsigned get_cnt;
unsigned get_hit_cnt;
unsigned put_cnt;
unsigned purge_cnt;
#ifdef _FASTMEM_EXT
unsigned get_lru_hit_cnt;
#endif
#endif

static void purge_mem(int shrink) {
	
	struct mobj* mem;
	list_head_t* tmp;
#ifdef _FASTMEM_DEBUG
	purge_cnt++;
#endif
	//如果是shrink被设置，则每次减半空闲内存占用量，直到清空为止，否则按照预设置的阀值，减少到阀值的80%
	unsigned long water_mark;
	if(shrink)
		water_mark = free_mem_size / 2;	
	else
		water_mark = free_thresh / 5 * 4;
	
	list_for_each_entry_safe_l(mem, tmp, &used_list, vlist) {

		free_mem_size -= mem->size;
		list_del(&mem->vlist);
		list_del(&mem->list);
		free(mem);
		if(free_mem_size < water_mark)
			return;
	}
}
#ifdef _FASTMEM_EXT
static inline struct mobj* try_lru_mem(unsigned size) {
	
	struct mobj* mem;
	list_head_t* ptt;
	if(free_mem_size > 0) {
		ptt = used_list.prev;
		mem  = list_entry(ptt, struct mobj, vlist);
		if(mem->size >= size && mem->size < (size << 1)) {
			return mem;
		}
	}
	return NULL;
}
#endif
void fastmem_init(unsigned long thresh_size) {
	
	int i;
	for(i = 0; i < MEM_MAP_SIZE; ++i)
		INIT_LIST_HEAD(&mem_maps[i]);

	INIT_LIST_HEAD(&used_list);
	free_thresh = thresh_size;	
	free_mem_size = 0;
#ifdef _FASTMEM_DEBUG	
	na_get_cnt = na_put_cnt = get_cnt = get_hit_cnt = put_cnt = purge_cnt = 0;	
#endif
}
void fastmem_fini() {

#ifdef _FASTMEM_DEBUG
	printf("free_mem_size=%lu,na_get=%u,na_put=%u,get=%u,get_hit=%u,get_lru_hit=%u,hit_ratio=%f,put=%u,purge=%u\n", 
		free_mem_size, na_get_cnt, na_put_cnt, get_cnt, get_hit_cnt, get_lru_hit_cnt, (float)get_hit_cnt / (float)get_cnt, put_cnt, purge_cnt);	
#endif
	struct mobj* mem;
	list_head_t* tmp;
	list_for_each_entry_safe_l(mem, tmp, &used_list, vlist) {
		list_del(&mem->list);
		list_del(&mem->vlist);
		free(mem);
	}	
}
void* fastmem_get(unsigned size, unsigned* real_size) {
	
	if(size < MEM_MIN_SIZE || size > MEM_MAX_SIZE) {
#ifdef _FASTMEM_DEBUG
		na_get_cnt++;
#endif				
		*real_size = size;
		return malloc(size);
	}	
	else {
#ifdef _FASTMEM_DEBUG
		get_cnt++;
		if((get_cnt % 100000) == 0) {
			printf("free_mem_size=%lu,na_get=%u,na_put=%u,get=%u,get_hit=%u,get_lru_hit=%u,hit_ratio=%f,put=%u,purge=%u\n", 
			free_mem_size, na_get_cnt, na_put_cnt, get_cnt, get_hit_cnt, get_lru_hit_cnt, (float)get_hit_cnt / (float)get_cnt, put_cnt, purge_cnt);	
		}
#endif	
		struct mobj* mem = NULL;
#ifdef _FASTMEM_EXT		
		mem = try_lru_mem(size);
		if(mem) {
			list_del(&mem->list);		
			list_del(&mem->vlist);
			free_mem_size -= mem->size;
			*real_size = mem->size;		
#ifdef _FASTMEM_DEBUG
			get_lru_hit_cnt++;			
			get_hit_cnt++;
#endif
			return mem->addr;
		}	
#endif				
		unsigned new_size = size + MEM_STEP_SIZE - 1;
		if(unlikely(new_size > MEM_MAX_SIZE))
			new_size = MEM_MAX_SIZE;
		int i = (new_size - MEM_MIN_SIZE) / MEM_STEP_SIZE;	
		int j = i;
		int k;
		for(k = 0; (k < MAX_SCAN_TIMES) && (j < MEM_MAP_SIZE); ++k, ++j) {
			if(!list_empty(&mem_maps[j])) {
				mem  = list_entry(mem_maps[j].next, struct mobj, list);
				if(unlikely(mem->size < size)) {
					printf("mempool FAULT, %s:%d, %u < %u\n", __FILE__, __LINE__, mem->size, size);
					return NULL;
				}
				list_del(&mem->list);		
				list_del(&mem->vlist);
				free_mem_size -= mem->size;
#ifdef _FASTMEM_DEBUG
				get_hit_cnt++;
#endif
				*real_size = mem->size;		
				return mem->addr;
			}
		}
		mem = (struct mobj*)malloc(sizeof(struct mobj) + new_size);
		if(unlikely(mem == NULL)) {
			purge_mem(1);
			mem = (struct mobj*)malloc(sizeof(struct mobj) + new_size);
			if(mem == NULL)
				return NULL;
		}	
		*real_size = mem->size = new_size;
		return mem->addr;
	}
}
void fastmem_put(void* addr, unsigned size) {
	
	if(size < MEM_MIN_SIZE || size > MEM_MAX_SIZE) {
#ifdef _FASTMEM_DEBUG
		na_put_cnt++;
#endif		
		return free(addr);
	}	
	else {
		struct mobj* mem = (struct mobj*)((char*)addr - sizeof(struct mobj));
		if(unlikely(mem->size < size)) {
			printf("mempool FAULT, %s:%d, %u < %u\n", __FILE__, __LINE__, mem->size, size);
			return;
		}
		int i = (mem->size - MEM_MIN_SIZE) / MEM_STEP_SIZE;
		list_add(&mem->list, &mem_maps[i]);	
		list_add_tail(&mem->vlist, &used_list);
		free_mem_size += mem->size;
		
#ifdef _FASTMEM_DEBUG
		put_cnt++;
#endif		
		if(unlikely(free_mem_size > free_thresh)) {
			purge_mem(0);		
		}
	}
}
//fastmem_mod的语义与realloc的语义不一样，这里不拷贝老数据
void* fastmem_mod(void* mem, unsigned old_size, unsigned new_size, unsigned* real_size) {
	//这里的实现还可以做更多优化...	
	fastmem_put(mem, old_size);
	return fastmem_get(new_size, real_size);	
}
void fastmem_shrink() {
	purge_mem(1);
}
