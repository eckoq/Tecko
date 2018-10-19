/*
 * ����mem�����
 * ��Ҫ�Ƕ�glibcƵ�����ڴ�����ͷ�ʹ��mmap/munmapϵͳ���õ���Ч�ʵͽ������Ż�
 * �ۺ���glibc��Ч�ʵ�С�ڴ���������Զ���Ĵ��ڴ�������
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
