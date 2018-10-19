/*
 * ���ڹ����ڴ���ڴ����������QNF���ڴ������myalloc��ֲ����
 * ֧���̰߳�ȫ�����ϸ�����chunk���ȵ���Сֵ�����ֵ�����ڽ���֮��
 * ������Ϣ������
 */
#ifndef _MYALLOC_H_
#define _MYALLOC_H_

typedef unsigned long MEM_HANDLE;
#define ZERO_HANDLE 0
#define NULL_HANDLE ((unsigned long)-1)

typedef struct {
	unsigned shmkey;			//�����ڴ�key
	unsigned long shmsize;		//�����ڴ��С
	char semname[128];			//posix�ź�������
	unsigned minchunksize;		//��С���С
	unsigned maxchunksize;		//�����С
	unsigned maxchunknum;		//������
}myalloc_alloc_conf;

typedef struct {
	unsigned long tsize;		//��ʹ���ڴ�
	unsigned long fsize;		//ʣ���ʹ���ڴ�
	unsigned tnum;				//�ܿɷ������
	unsigned fnum;				//ʣ��ɷ������
}myalloc_alloc_stat;

typedef struct memhead {
	MEM_HANDLE mem;				//�ڴ���Ե�ַ
	unsigned len;				//�ڴ泤��
};

//����������ʱ�����һ��
extern int myalloc_init(myalloc_alloc_conf* conf);
//�����ڴ�
extern MEM_HANDLE myalloc_alloc(unsigned size);
//�ͷ��ڴ�
extern void myalloc_free(MEM_HANDLE h);
//��ַӳ��
extern void* myalloc_addr(MEM_HANDLE h);
//ȡ�ڴ������ͳ����Ϣ
extern void myalloc_stat(myalloc_alloc_stat* st);
//������ֹ��ʱ�����һ��
extern void myalloc_fini();

#endif
