/******************************************************************************

 索引对象管理器,配哈希表，队列，缓冲器，树


原理:固定块的拼接组合
 ___      ____     ____     ____
|__| --|___|--|___|--|___|


2007年9月, nekeyzhong written

实现管理不定长数据哈希,和定长数据哈希

 ******************************************************************************/
#ifndef _IDXOBJMNG_HPP
#define _IDXOBJMNG_HPP

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>

#ifndef ASSERT
#define ASSERT(COND){\
	assert(#COND);\
}
#endif

// 64 位的数据类型在 64 位系统上则要按照 64 位边界进行对齐
//linux使用ILP32,LP64数据模型,64位Windows使用LLP64
/*
 		ILP32 	LP64 	LLP64 	ILP64  
char 	8  		8  		8  		8  
short  	16  		16 		16  		16  
int  		32  		32  		32  		64  
long  	32  		64 		32 		64  
long long 64  		64  		64  		64  
指针  	32  		64  		64  		64  

*/

//int_t在32位系统为32位,在64位系统为64位

typedef enum
{
    emInit = 0,
    emRecover = 1
} emInitType;

#ifndef _IDX64
	#define idx_t int32_t
#else
	#define idx_t int64_t
#endif

//本块的索引定义
#define MAX_DSINFO_NUM	16

//块/对象管理器
class TIdxObjMng
{
public:
	/*
	在内部管理时，用0存储free指针链,分配后由外面使用，本类就不在管理这个节点了
	*/

#pragma pack(1)
	typedef struct
	{
		 //未分配时用0存储free链，分配后由外部使用
		idx_t        m_piDsInfo[0];  
	}TIdx;
#pragma pack()

	#define GETIDX(iIdx) ((TIdx*)((char*)m_pIdx+m_pIdxObjHead->m_iIdxSize*(iIdx)))

	enum
	{
		emMemory = 0,
		emDisk = 1
	};


	TIdxObjMng();
	virtual ~TIdxObjMng(){};
	
	static ssize_t CountMemSize(ssize_t iObjSize,ssize_t iIdxObjNum,ssize_t iDSNum=1);
	ssize_t AttachMem(char* pMemPtr,const ssize_t MEMSIZE,ssize_t iObjSize,ssize_t iIdxObjNum,
					ssize_t iInitType=emInit,ssize_t iDSNum=1);

public:	
	//申请链
	ssize_t GetOneFreeDS();
	
	TIdx* GetIdx(ssize_t iIdx);

	//与数据有关，可以被重载的操作
	virtual ssize_t CopyAttachObj(ssize_t iIdx,ssize_t iOffset,char* pObj,const ssize_t COPYSIZE);
	virtual char* GetAttachObj(ssize_t iIdx);
	virtual ssize_t SetAttachObj(ssize_t iIdx,ssize_t iOffset,char* pObj,ssize_t iLen=0);
	
	ssize_t CreateObject();
	ssize_t DestroyObject(ssize_t iIdx);

	ssize_t GetUsedCount(){return m_pIdxObjHead ? m_pIdxObjHead->m_iUsedCount : 0;};
	ssize_t GetFreeCount(){return m_pIdxObjHead ? m_pIdxObjHead->m_iIdxObjNum-m_pIdxObjHead->m_iUsedCount : 0;};
	ssize_t GetObjSize(){return m_pIdxObjHead ? m_pIdxObjHead->m_iObjSize : 0;}
	ssize_t GetObjNum(){return m_pIdxObjHead ? m_pIdxObjHead->m_iIdxObjNum : 0;}
	char* GetObjBaseMem(){return m_pObjMem;}

	ssize_t GetDsIdx(ssize_t iIdx,ssize_t iDsSufix);
	ssize_t SetDsIdx(ssize_t iIdx,ssize_t iDsSufix,ssize_t iVal);

	void Print(FILE *fpOut);
	ssize_t FormatIdx();                        //格式化索引区
	static ssize_t GetIdxSize(ssize_t iDSNum){return sizeof(TIdx) + sizeof(idx_t)*iDSNum;};
	
public:
	typedef struct
	{
		ssize_t m_iFreeHead;		//空闲头
		ssize_t m_iUsedCount;	//使用数	

		ssize_t m_iObjSize;
		ssize_t m_iIdxObjNum;
		ssize_t m_iIdxSize;
	}TIdxObjHead;
	
protected:
	//可计算的数据
	ssize_t m_iDSNum;
	ssize_t m_piDsUseFlag[MAX_DSINFO_NUM];
	
	TIdxObjHead* m_pIdxObjHead;
	TIdx* m_pIdx;		//索引区

private:	
	char* m_pObjMem;	//数据区
};

//---------------------------------------
class TDiskIdxObjMng: public TIdxObjMng
{
public:

	TDiskIdxObjMng();
	~TDiskIdxObjMng();

	//不能使用的函数
	ssize_t AttachMem(char* pMemPtr,const ssize_t MEMSIZE,ssize_t iObjSize,ssize_t iIdxObjNum,
							ssize_t iInitType=emInit,ssize_t iDSNum=1)
		{assert(0);return 0;}
	char* GetObjBaseMem(){assert(0);return 0;}
	
	static ssize_t CountMemSize(ssize_t iIdxObjNum,ssize_t iDSNum/*=0*/);
	static ssize_t CountFileSize(ssize_t iObjSize,ssize_t iIdxObjNum);
	static ssize_t CountObjNumByMem(ssize_t iObjSize,ssize_t iMemSize,ssize_t iDSNum/*=0*/);
	static ssize_t CountObjNumByFile(ssize_t iObjSize,ssize_t iFileSize);

	static ssize_t CountFileSize(ssize_t iObjSize,ssize_t iIdxObjNum,ssize_t iDSNum=0);
	
	ssize_t AttachMemFile(
		char* pMemPtr,const ssize_t MEMSIZE,ssize_t iInitType/*=emInit*/,
		char* pDiskFile,ssize_t iDiskFileStartPos/*=0*/,const ssize_t FILESIZE/*=0*/,
		ssize_t iObjSize,ssize_t iIdxObjNum,ssize_t iDSNum=0);

public:	

	//与数据有关，需要被重载的操作
	ssize_t CopyAttachObj(ssize_t iIdx,ssize_t iOffset,char* pObj,const ssize_t COPYSIZE);
	char* GetAttachObj(ssize_t iIdx);
	ssize_t SetAttachObj(ssize_t iIdx,ssize_t iOffset,char* pObj,ssize_t iLen=0);
	
private:
	ssize_t DetachFile();
	
	//数据文件句柄
	ssize_t m_iDataFileFD;
	ssize_t DISKFILE_OFFSET; 
		
	//临时数据
	char* m_pTmpObj;
};

//-----------------------以下为与对象管理器配合使用的外部件---------------------------------

//对象哈希表
typedef ssize_t (*SET_KEY_FUNC)(void* pObj,void* pKey,ssize_t iKeyLen);		//设置主键
typedef ssize_t (*GET_KEY_FUNC)(void* pObj,void* pKey,ssize_t &iKeyLen);		//获得主键

class CBuffMng;

//只支持key len <=1024的
class CHashTab
{
public:

#pragma pack(1)	
	//哈希节点定义
	typedef struct
	{
	    idx_t m_iFirstObjIdx;
	} THashItem;
#pragma pack()

	CHashTab();
	static ssize_t CountMemSize(ssize_t iBucketNum);
	ssize_t AttachMem(char* pMemPtr,const ssize_t MEMSIZE,ssize_t iBucketNum,ssize_t iInitType=emInit);	
	
	//挂接节点管理器
	ssize_t AttachIdxObjMng(TIdxObjMng *pIdxObjMng, SET_KEY_FUNC pfObjInitSetKey,GET_KEY_FUNC pfObjGetKey);
	//挂接数据空间管理器	,可选,从中获取key
	ssize_t AttachBuffMng(CBuffMng *pBuffMng);
public:
	char* CreateObjectByKey(void *pvKey, ssize_t iKeyLength, ssize_t& iObjIdx );    
	char* GetObjectByKey( void *pvKey, ssize_t iKeyLength, ssize_t& iObjIdx);
	ssize_t DeleteObjectByKey( void *pvKey, ssize_t iKeyLength );

	char* GetObjectByIdx(ssize_t iObjIdx);

	//遍历哈希桶内节点使用
	ssize_t GetBucketNum();
	ssize_t GetBucketNodeHead(ssize_t iBucketIdx);
	ssize_t GetBucketNodeNext(ssize_t iNodeIdx);
	
	ssize_t Print(FILE *fpOut);
	ssize_t GetUsage(ssize_t &iBucketNum,ssize_t &iUsedBucket);

private:
	typedef struct
	{
		//核心数据
		ssize_t m_iDSSuffix;
		ssize_t m_iBucketNum;
		ssize_t	m_iUsedBucket;
	}THashHead;

	THashHead* m_pHashHead;
	THashItem* m_pHashItems;	
	TIdxObjMng* m_pIdxObjMng;
	SET_KEY_FUNC m_pfObjInitSetKey;
	GET_KEY_FUNC m_pfObjGetKey;

	//用来遍历桶的数据,临时存在
	ssize_t m_iCurrGetIdx;
	ssize_t m_iCurrItemPage;		

	ssize_t m_iInitType;
};

//-------------------------------------------------------------
//对象队列,只记录索引,不控制具体对象
class CObjQueue
{
public:
	CObjQueue();
	static ssize_t CountMemSize();
	ssize_t AttachMem(char* pMemPtr,const ssize_t MEMSIZE,ssize_t iInitType=emInit);
	ssize_t AttachIdxObjMng(TIdxObjMng *pIdxObjMng,ssize_t iDSSuffix1=-1,ssize_t iDSSuffix2=-1);
	
public:
	ssize_t    GetHeadItem();
	ssize_t    GetTailItem();
	
	ssize_t    GetNextItem(ssize_t iItemIdx, ssize_t& iNextItemIdx);
	ssize_t    GetPrevItem(ssize_t iItemIdx, ssize_t& iPrevItemIdx);

	ssize_t    TakeHeadItem();	
	ssize_t    AppendToTail( ssize_t iItemIdx );
	ssize_t    PushToHead( ssize_t iItemIdx );

	ssize_t ItemInQueue(ssize_t iItemIdx);
	ssize_t InsertAfter(ssize_t iItemIdx, ssize_t iNewItemIdx);
	ssize_t	InsertBefore(ssize_t iItemIdx, ssize_t iNewItemIdx);

	ssize_t GetLength();
	ssize_t    DeleteItem( ssize_t iItemIdx );	
	ssize_t Print( FILE *fpOut );

	ssize_t Initialize();	
	ssize_t    SetNextItem(ssize_t iItemIdx, ssize_t iNextItemIdx);
	ssize_t    SetPrevItem(ssize_t iItemIdx, ssize_t iPrevItemIdx);

	typedef struct
	{
		ssize_t m_iHeadItem;
		ssize_t m_iTailItem;
		
		ssize_t m_iDSPrev;
		ssize_t m_iDSNext;
		
		ssize_t m_iTotalCount;
	}TObjQueueHead;
	
private:
	TObjQueueHead* m_pObjQueueHead;
	TIdxObjMng *m_pIdxObjMng;
	ssize_t m_iInitType;
};


//对象堆栈
#define CObjStack CObjQueue
//---------------------------------------------------------------------

/*
不定长缓冲区管理器
_BUFFMNG_APPEND_SKIP   是否提供append,skip能力，这需要多占用8字节节点数据
*/
class CBuffMng
{
public:

#pragma pack(1)	
	typedef struct
	{
		idx_t m_iBuffIdx;			//第一块位置
		idx_t m_iBuffSize;		//总数据长度

		//支持append,skip
#ifdef _BUFFMNG_APPEND_SKIP		
		idx_t m_iLastBuffIdx;		//最后一块位置	
		idx_t m_iBuffOffset;		//起点偏移
#endif		
	} TBuffItem;
#pragma pack()

	CBuffMng();
	static ssize_t CountMemSize(ssize_t iBuffNum);
	//[0 ... iBuffNum-1] 的独立buffer
	ssize_t AttachMem(char* pMemPtr,const ssize_t MEMSIZE,ssize_t iBuffNum,ssize_t iInitType=emInit);
	ssize_t AttachIdxObjMng(TIdxObjMng *pIdxObjMng);
	
public:

#ifdef _BUFFMNG_APPEND_SKIP		
	ssize_t AppendBuffer(ssize_t iBuffSuffix,char* pBuffer, ssize_t iBuffSize);
	ssize_t Skip(ssize_t iBuffSuffix, ssize_t iSkipLen);
	ssize_t SendBufferToSocket(ssize_t iSocketFD,ssize_t iBuffSuffix);
#endif

	ssize_t SetBufferSpace(ssize_t iBuffSuffix,ssize_t iBuffSize);
	ssize_t SetDataToSpace(ssize_t iBuffSuffix,char* pBuffer, ssize_t iBuffSize);
#if 0	
	ssize_t AppendBufferSpace(ssize_t iBuffSuffix,ssize_t iBuffSize);
	ssize_t AppendDataToSpace(ssize_t iBuffSuffix,char* pBuffer, ssize_t iBuffSize);
#endif
	ssize_t SetBuffer(ssize_t iBuffSuffix,char* pBuffer, ssize_t iBuffSize);
	ssize_t GetBuffer(ssize_t iBuffSuffix,char* pBuffer,const ssize_t BUFFSIZE);
	ssize_t FreeBuffer(ssize_t iBuffSuffix);
	char* GetFirstBlockPtr(ssize_t iBuffSuffix);

	//是否还有空闲空间
	bool HaveFreeSpace(ssize_t iSize);

	ssize_t GetBufferSize(ssize_t iBuffSuffix);
	ssize_t GetBlockSize();	
	ssize_t GetBufferUsage(ssize_t &iBufferUsed,ssize_t &iBufferCount,ssize_t &iObjUsed,ssize_t &iObjCount);
	ssize_t Print(FILE *fpOut);
	
public:
	typedef struct
	{
		ssize_t m_iBuffNum;			//缓冲块个数
		ssize_t m_iFreeBuffNum;		//空闲缓冲块数
		ssize_t m_iDSSuffix;			//在对象管理器中使用的链		
	}TBuffMngHead;
	
private:
	ssize_t FreeObjList(ssize_t iObjIdx);

	TBuffMngHead* m_pBuffMngHead;
	TBuffItem* m_pBuffItem;	//记录每个桶缓冲区长
	
	TIdxObjMng* m_pIdxObjMng;	
	ssize_t m_iInitType;
};

//---------------------------------------------------------------------
//定长对象管理器
/*
 ___      ____     ____     ____
|__| --|___|--|___|--|___|

*/

//判等函数，find使用eq return 0
typedef ssize_t (*BLOCK_FIND_EQU_FUNC)(void* pBlock,void* pFindArg);	
//选出函数
typedef ssize_t (*BLOCK_SELECT_FUNC)(void* pBlock,void* pSelectArg);	//block 比较0相等
//判等,删除时使用eq return 0
typedef ssize_t (*BLOCK_DEL_EQU_FUNC)(void* pBlock,void* pDeleteArg);

/*
不定长缓冲区管理器
_BLOCKMNG_APPEND_SKIP   是否提供append,skip能力，这需要多占用8字节节点数据
*/

class CBlockMng
{
public:

#pragma pack(1)	
	//节点定义
	typedef struct
	{
		idx_t m_iBlockIdx;		//第一块位置
		idx_t m_iBlockNum;		//块数

#ifdef _BLOCKMNG_APPEND_SKIP			
		idx_t m_iLastBlockIdx;		//最后一块位置			
#endif
	} TBlockItem;
#pragma pack()

	CBlockMng();
	static ssize_t CountMemSize(ssize_t iItemNum);
	//[0 ... iBlockNum-1] 的独立buffer
	ssize_t AttachMem(char* pMemPtr,const ssize_t MEMSIZE,ssize_t iItemNum,ssize_t iInitType=emInit);
	ssize_t AttachIdxObjMng(TIdxObjMng *pIdxObjMng);
	
public:
	ssize_t AppendBlock(ssize_t iItemSuffix,char* pBlockData);
	ssize_t InsertBlock(ssize_t iItemSuffix,char* pBlockData,ssize_t iInsertPos=0);

	//遍历方案
	ssize_t GetBlockNum(ssize_t iItemSuffix);
	char* GetBlockData(ssize_t iItemSuffix,ssize_t iPos);

	//遍历方案
	char* GetFirstBlockData(ssize_t iItemSuffix);
	char* GetNextBlockData(char* pBlockData);
	ssize_t GetBlockPos(ssize_t iItemSuffix,char* pBlockData);
	
	ssize_t SelectBlock(ssize_t iItemSuffix,char* pBuffer,const ssize_t iBUFFSIZE,BLOCK_SELECT_FUNC fBlockSelectFunc=NULL,void* pSelectArg=NULL,ssize_t iStartBlockPos=0,ssize_t iBlockNum=-1);
	ssize_t FindBlock(ssize_t iItemSuffix,BLOCK_FIND_EQU_FUNC fFindEquFunc,void* pFindArg=NULL);
	ssize_t DeleteBlock(ssize_t iItemSuffix,BLOCK_DEL_EQU_FUNC fDelEquFunc,void* pDeleteArg=NULL);
	ssize_t DeleteBlock(ssize_t iItemSuffix,ssize_t iPos);
	ssize_t FreeBlock(ssize_t iItemSuffix);
	ssize_t TrimTail(ssize_t iItemSuffix,ssize_t iBlockNum);

	ssize_t GetUsage(ssize_t &iItemUsed,ssize_t &iItemCount,ssize_t &iBlockUsed,ssize_t &iBlockCount);
	ssize_t Print(FILE *fpOut);

	ssize_t GetBlockObjSize();
private:
	ssize_t FreeObjList(ssize_t iObjIdx);

	typedef struct
	{
		ssize_t m_iItemNum;			//缓冲块个数
		ssize_t m_iFreeItemNum;		//空闲缓冲块数
		ssize_t m_iDSSuffix;			//在对象管理器中使用的链		
	}TBlockMngHead;

	TBlockMngHead* m_pBlockMngHead;
	TBlockItem* m_pBlockItem;	//记录每个桶缓冲区长
	
	TIdxObjMng* m_pIdxObjMng;
	ssize_t m_iInitType;
};
//---------------------------------------------------------------------

class CTree
{
public:
	CTree();	
	~CTree();
	static ssize_t CountMemSize();
	ssize_t AttachMem(char* pMemPtr,const ssize_t MEMSIZE,ssize_t iInitType=emInit);
	ssize_t AttachIdxObjMng(TIdxObjMng *pIdxObjMng);
	
private:
	typedef struct
	{
		idx_t m_iRootIdx;

		idx_t m_iChildDS;
		idx_t m_iPrevBrotherDS;
		idx_t m_iNextBrotherDS;
		idx_t m_iParentDS;

	}TTreeRoot;
	
	TTreeRoot* m_pTreeRoot;
	ssize_t m_iInitType;
	TIdxObjMng* m_pIdxObjMng;

	ssize_t *m_piNodeIdxArray;

public:
	ssize_t BuildRoot();					    //创建根结点	
	ssize_t GetRoot();
	ssize_t BuildNode();				//获得节点
	char* GetNode(ssize_t iNodeObjIdx);				//获得节点
	
	//插入一个子女,iOrderPos为孩子位置,从0开始,-1表示最后	
	ssize_t InsertChild(ssize_t iNodeObjIdx,ssize_t iChildObjIdx, ssize_t iOrderPos=0);	
	//回收掉以iNodeObjIdx为根的树,包括iNodeObjIdx,-1为包括根的全树
	ssize_t RemoveTree(ssize_t iNodeObjIdx = -1);
	//将以iNodeObjIdx为根的树摘下
	ssize_t TakeTree(ssize_t iNodeObjIdx);

	ssize_t IsLeaf(ssize_t iNodeObjIdx);
	void PrintTree(ssize_t iNodeObjIdx=-1);
	ssize_t Height(ssize_t iRootNodeIdx=-1);

	ssize_t LeafNum(ssize_t iNodeObjIdx=-1);

	//获取直接孩子
	ssize_t* GetChild(ssize_t iNodeObjIdx,ssize_t &iChildNum);

	//得到深度优先序列	
	ssize_t* GetDeepSequeue(ssize_t iNodeObjIdx,ssize_t &iSeqObjNum);
	//得到广度优先序列
	ssize_t* GetExpandSequeue(ssize_t iNodeObjIdx,ssize_t &iSeqObjNum);

private:
	void RecursionPrint(ssize_t iNodeObjIdx,ssize_t iLevel);
	ssize_t GetDeepSequeue(ssize_t iNodeObjIdx,ssize_t* piSeqObj,ssize_t &iSeqObjNum);
	ssize_t GetExpandSequeue(ssize_t iNodeObjIdx,ssize_t* piSeqObj,ssize_t &iSeqObjNum);	
};

#endif	/*_IDXOBJMNG_HPP*/

