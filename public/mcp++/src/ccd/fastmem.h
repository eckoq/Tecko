/*
 * 快速mem分配池
 * 主要是对glibc频繁大内存分配释放使用mmap/munmap系统调用导致效率低进行了优化
 * 综合了glibc高效率的小内存分配管理和自定义的大内存分配管理
 */
#ifndef _FASTMEM_H_
#define _FASTMEM_H_

extern void fastmem_init(unsigned long thresh_size);
extern void* fastmem_get(unsigned size, unsigned* real_size);
extern void fastmem_put(void* mem, unsigned size);
extern void* fastmem_mod(void* mem, unsigned old_size, unsigned new_size, unsigned* real_size);
extern void fastmem_fini();
extern void fastmem_shrink();
#endif
