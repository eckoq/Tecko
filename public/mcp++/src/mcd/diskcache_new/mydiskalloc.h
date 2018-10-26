/*
 * һ���������ݿ������������ȫ�����ڴ��У������ڴ����ϣ����ݿ鲻������ÿ�ζ�ȡ����д��ֻ��һ�δ���IO
 * ��QNF���ڴ������myalloc��ֲ����
 */
#ifndef _MYDISKALLOC_H_
#define _MYDISKALLOC_H_

typedef unsigned long DISK_HANDLE;
#define ZERO_HANDLE 0
#define NULL_HANDLE ((unsigned long)-1)

#define MAX_FILE_LEN	256
#define MAX_SIZE_NUM  	(1<<10) 	//���С���������Ŀ
#define MAX_CHUNK_NUM	(1<<25)		//��������Ŀ
#define MAX_CHUNK_SIZE	(1<<30) 	//�����С
#define MYDISK_MAGIC	0x94470099	

#define MYDISK_ERR_NOTENOUGH	-2001	//�ṩ���ڴ治��������������������
#define MYDISK_ERR_CONFERROR	-2002	//���ò�������������Ѿ����ڳ�ͻ
#define MYDISK_ERR_DESCOVER		-2003	//������������
#define MYDISK_ERR_FREEOVER		-2004	//���п�����
#define MYDISK_ERR_OPENFILE		-2005	//���ļ�ʧ��
#define MYDISK_ERR_HANDLEERR	-2006	//DISKHANDLE��Ч
#define MYDISK_ERR_STATUSERR	-2007	//��״̬������Ҫ��
#define MYDISK_ERR_READFAIL		-2008	//������ʧ��
#define MYDISK_ERR_WRITEERR		-2009	//д����ʧ��
#define MYDISK_ERR_LENGTHERR	-2010	//���ݿ鳤�Ȳ�����Ҫ��
#define MYDISK_ERR_CHUNKERR		-2011	//�Ҳ�����Ӧ�����ݿ�
#define MYDISK_ERR_MMAP			-2012	//ӳ�����ݿ�ʧ��

//���̿�����
//ע�⣺�������ݿ�û�м��κ�checksum�����ܱ�֤���ݵ�������
typedef struct {
	unsigned size;				//���С
	unsigned long long addr;	//���ݿ��ַ
	union {
		struct {
			unsigned inuse:1;	//0-���У�1-ʹ����
		};
		unsigned flag;
	};
	DISK_HANDLE cprev;			//�߼������ڵ�ǰһ����
	DISK_HANDLE cnext;			//�߼������ڵĺ�һ����
	union {
		struct {
			DISK_HANDLE fprev;	//freelist��������ǰһ���ڵ�
			DISK_HANDLE fnext;	//freelist���������һ���ڵ�
		};
		DISK_HANDLE dnext;		//desclist�������һ���ڵ�
	};
}mydiskalloc_chunk;

//���̿������
typedef struct {
	unsigned magic;						//magic number
	char diskfile[MAX_FILE_LEN];		//disk�ļ�·��
	int diskfd;							//disk�ļ����
	unsigned long long totalsize;		//�ܵĿ�ʹ�ÿռ�	
	unsigned long long freesize;		//ʣ���ʹ�ÿռ�
#ifdef _MYDISKALLOC_DEBUG
	unsigned long long alloc_cnt;		//�������
	unsigned long long free_cnt;		//�ͷŴ���
	unsigned long long split_cnt;		//����ʱ���Ѵ���
	unsigned long long merge_front_cnt;	//�ͷ�ʱ��ǰ�ϲ�����
	unsigned long long merge_after_cnt;	//�ͷ�ʱ���ϲ�����
#endif
	unsigned totalchunknum;				//chunks�ܿ���
	unsigned minchunksize;				//��Сchunk��С
	unsigned descchunknum;				//desclist�����chunks����
	unsigned freechunknum;				//freelist�����chunks����
	DISK_HANDLE desclist;				//��ʹ�õ�chunks������������
	DISK_HANDLE freelist[MAX_SIZE_NUM];	//free״̬��chunks���ϣ��
#ifdef _MYDISKALLOC_FREEINDEX
	unsigned freeindex[MAX_SIZE_NUM];	//���ٿ��п�����
#endif	
	mydiskalloc_chunk chunks[0];		//���ݿ�����
}mydiskalloc_head;

//��ʼ����������Դ
#ifndef _TDC_DISKCACHE_
extern int mydiskalloc_init(mydiskalloc_head** myhead, void* mem, unsigned long memsize, char init, const char* diskfile, unsigned long long disksize, unsigned minchunksize);
#else
extern int mydiskalloc_init(mydiskalloc_head** myhead, void* mem, unsigned long memsize, char init, const char* diskfile, unsigned long long disksize, unsigned minchunksize, unsigned chunktotal);
#endif
//�������ݿ飬�����߱���DISK_HANDLE�Ա�д����߶�ȡ���ݿ�����
//ע�⣺DISK_HANDLE���������ݿ�������ַӳ�䣨���ļ���ƫ�������ǵ���ӳ��
extern DISK_HANDLE mydiskalloc_alloc(mydiskalloc_head* myhead, unsigned size);
//�ͷ����ݿ�
extern void mydiskalloc_free(mydiskalloc_head* myhead, DISK_HANDLE h);
//��ȡ���ݿ�����
extern int mydiskalloc_read(mydiskalloc_head* myhead, DISK_HANDLE h, void* data_buf, unsigned data_len);
//д�����ݿ�����
extern int mydiskalloc_write(mydiskalloc_head* myhead, DISK_HANDLE h, const void* data_buf, unsigned data_len); 
//����mmap�ķ�ʽ��ȡ���ݿ�����
extern int mydiskalloc_mmap(mydiskalloc_head* myhead, DISK_HANDLE h, char** addr, unsigned data_len);
//unmap���ݿ�
extern void mydiskalloc_unmmap(mydiskalloc_head* myhead, DISK_HANDLE h, char* addr, unsigned data_len);
//�ͷŷ�������Դ
extern void mydiskalloc_fini(mydiskalloc_head* myhead);
//����������Ҫʹ�õ��ڴ��С
#ifndef _TDC_DISKCACHE_
extern unsigned long mydiskalloc_calcsize(unsigned long long disksize, unsigned minchunksize);
#else
extern unsigned long mydiskalloc_calcsize(unsigned chunktotal);
#endif

#ifdef _TDC_DISKCACHE_
extern int mydiskalloc_data_dump(mydiskalloc_head* myhead, const char *dump_name);
extern int mydiskalloc_data_recover(mydiskalloc_head* myhead, const char *dump_name, char *bak_name);
#endif

#endif

