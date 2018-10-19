#include "IdxObjMng.hpp"
#include "HashFunc.h"
#include <sys/socket.h>

//--------------------------------------------------------------------
TIdxObjMng::TIdxObjMng()
{
	m_pIdxObjHead = NULL;
	m_pIdx = NULL;
	m_pObjMem = NULL;
	m_iDSNum = 1;

	memset(m_piDsUseFlag,0,sizeof(m_piDsUseFlag));
}	
ssize_t TIdxObjMng::GetOneFreeDS()
{
	for (ssize_t i=0; i<m_iDSNum;i++)
	{
		if (m_piDsUseFlag[i] == 0)
		{
			m_piDsUseFlag[i] = 1;
			return i;
		}
	}
	return -1;
}

//计算总占用内存量
ssize_t TIdxObjMng::CountMemSize(ssize_t iObjSize,ssize_t iIdxObjNum,ssize_t iDSNum/*=1*/)
{
	//自己至少要用一个存储free list 指针
	iDSNum = iDSNum<1?1:iDSNum;
	
	ssize_t iIdxSize = sizeof(TIdx) + sizeof(idx_t)*iDSNum;
	return sizeof(TIdxObjHead) + iIdxObjNum*(iIdxSize + iObjSize);
}

//挂接内存,返回占用大小
//iIdxObjNum=0表示自动计算
//iObjSize=0表示没有对象，只有索引
ssize_t TIdxObjMng::AttachMem(char* pMemPtr,const ssize_t MEMSIZE,ssize_t iObjSize,ssize_t iIdxObjNum,
					ssize_t iInitType/*=emInit*/,ssize_t iDSNum/*=1*/)
{
	if ( !pMemPtr || (MEMSIZE<=0))
		return -1;

	//自己至少要用一个存储free list 指针
	m_iDSNum = (iDSNum < 1) ? 1 : iDSNum;
	
	ssize_t iIdxSize = sizeof(TIdx) + sizeof(idx_t)*m_iDSNum;

	//自动计算Num
	if (iIdxObjNum <= 0)
	{
		ssize_t iLeftLen = MEMSIZE-sizeof(TIdxObjHead);
		ssize_t iIdxObjSize = iIdxSize+iObjSize;
		iIdxObjNum = (ssize_t)(iLeftLen/(float)iIdxObjSize);
	}

	if (iIdxObjNum<=0)
	{
		return -2;
	}
	
	m_pIdxObjHead = (TIdxObjHead*)pMemPtr;
	m_pIdx = (TIdx*)(pMemPtr+sizeof(TIdxObjHead));
	m_pObjMem = pMemPtr+sizeof(TIdxObjHead)+iIdxSize*iIdxObjNum;

	if (iInitType == emInit)
	{
		m_pIdxObjHead->m_iObjSize = iObjSize;
		m_pIdxObjHead->m_iIdxObjNum = iIdxObjNum;
		m_pIdxObjHead->m_iIdxSize = iIdxSize;
		FormatIdx();
	}
	else
	{
		if ((m_pIdxObjHead->m_iObjSize != iObjSize)||
			(m_pIdxObjHead->m_iIdxObjNum != iIdxObjNum) ||
			(m_pIdxObjHead->m_iIdxSize != iIdxSize))
		{
			return -3;
		}
	}
	return CountMemSize(iObjSize,iIdxObjNum,m_iDSNum);
}
ssize_t TIdxObjMng::FormatIdx()
{
	m_pIdxObjHead->m_iFreeHead = 0;
	m_pIdxObjHead->m_iUsedCount = 0;

	memset(m_pIdx,-1,m_pIdxObjHead->m_iIdxSize*m_pIdxObjHead->m_iIdxObjNum);	
		
	for(ssize_t i = 1 ; i < m_pIdxObjHead->m_iIdxObjNum; i++ )
	{
		GETIDX(i-1)->m_piDsInfo[0] = i;		
	}

	GETIDX(m_pIdxObjHead->m_iIdxObjNum-1)->m_piDsInfo[0] = -1;
	return 0;
}

TIdxObjMng::TIdx* TIdxObjMng::GetIdx(ssize_t iIdx)
{
	if( iIdx < 0 || iIdx >= m_pIdxObjHead->m_iIdxObjNum )
		return NULL;
	
	return (TIdx*)((char*)m_pIdx+m_pIdxObjHead->m_iIdxSize*iIdx);
}

//return iCopyLen
ssize_t TIdxObjMng::CopyAttachObj(ssize_t iIdx,ssize_t iOffset,char* pObj,const ssize_t COPYSIZE)
{
	if( iIdx < 0 || iIdx >= m_pIdxObjHead->m_iIdxObjNum)
	{
		return -1;
	}

	ssize_t iCopyLen = COPYSIZE>(m_pIdxObjHead->m_iObjSize-iOffset)?(m_pIdxObjHead->m_iObjSize-iOffset):COPYSIZE;
	memcpy(pObj,m_pObjMem+m_pIdxObjHead->m_iObjSize*iIdx+iOffset,iCopyLen);
	return iCopyLen;
}
char* TIdxObjMng::GetAttachObj(ssize_t iIdx)
{
	if (iIdx >= m_pIdxObjHead->m_iIdxObjNum || iIdx < 0)
		return NULL;

	if(m_pIdxObjHead->m_iObjSize <= 0)
		return NULL;
		
	return (m_pObjMem + m_pIdxObjHead->m_iObjSize*iIdx);
}
ssize_t TIdxObjMng::SetAttachObj(ssize_t iIdx,ssize_t iOffset,char* pObj,ssize_t iLen/*=0*/)
{
	if(iIdx >= m_pIdxObjHead->m_iIdxObjNum || !pObj || iIdx<0)
	{
		return -1;
	}

	if(iLen <= 0)
		iLen = m_pIdxObjHead->m_iObjSize;

	if(iOffset+iLen > m_pIdxObjHead->m_iObjSize)
		return -2;

	ssize_t iObjlen = iLen>m_pIdxObjHead->m_iObjSize?m_pIdxObjHead->m_iObjSize:iLen;
	memcpy(m_pObjMem+m_pIdxObjHead->m_iObjSize*iIdx+iOffset,pObj,iObjlen);
	return 0;
}
ssize_t TIdxObjMng::GetDsIdx(ssize_t iIdx,ssize_t iDsSufix)
{
	if (iIdx < 0 || iIdx >= m_pIdxObjHead->m_iIdxObjNum)
		return -1;
	if (iDsSufix >= m_iDSNum)
		return -2;
	
	return GETIDX(iIdx)->m_piDsInfo[iDsSufix];
}

ssize_t TIdxObjMng::SetDsIdx(ssize_t iIdx,ssize_t iDsSufix,ssize_t iVal)
{
	if (iIdx <0 || iIdx >= m_pIdxObjHead->m_iIdxObjNum)
		return -1;
	if (iDsSufix >= m_iDSNum)
		return -2;

	GETIDX(iIdx)->m_piDsInfo[iDsSufix] = iVal;
	return 0;
}

ssize_t TIdxObjMng::CreateObject()
{
	if(m_pIdxObjHead->m_iFreeHead < 0)
	{
		//printf("ERR:TIdxObjMng::%s no free node! UsedCount=%lld,ObjNum=%lld[%s:%d]\n",__FUNCTION__,
		//	(long long)m_pIdxObjHead->m_iUsedCount,(long long)m_pIdxObjHead->m_iIdxObjNum,
		//	__FILE__,__LINE__);
		return -1;
	}

	ssize_t iTempIdx = m_pIdxObjHead->m_iFreeHead;
	TIdx* pIdx = GETIDX(iTempIdx);
	m_pIdxObjHead->m_iFreeHead = pIdx->m_piDsInfo[0];

	//clear
	memset(pIdx->m_piDsInfo,-1,m_iDSNum*sizeof(idx_t));
	
	m_pIdxObjHead->m_iUsedCount++;
	return iTempIdx;
}

//为了节约空间,这里无法容忍double free
ssize_t TIdxObjMng::DestroyObject(ssize_t iIdx)
{
	if(iIdx < 0 || iIdx >= m_pIdxObjHead->m_iIdxObjNum )
		return -1;

	TIdx* pIdx = GETIDX(iIdx);
	
	pIdx->m_piDsInfo[0] = m_pIdxObjHead->m_iFreeHead;
	m_pIdxObjHead->m_iFreeHead = iIdx;
	m_pIdxObjHead->m_iUsedCount--;
	return iIdx;
}
void TIdxObjMng::Print(FILE *fpOut)
{
	if (!fpOut)
		return;

	fprintf(fpOut,"IDXOBJ [ USE %lld ,OBJSIZE %lld ,IDXOBJNUM %lld ]",
		(long long)m_pIdxObjHead->m_iUsedCount,
		(long long)m_pIdxObjHead->m_iObjSize,
		(long long)m_pIdxObjHead->m_iIdxObjNum);

	//只有free链，无法打印节点
}	

//-----------------------------------------------------------------------------------
TDiskIdxObjMng::TDiskIdxObjMng()
{
	m_iDataFileFD = -1;
	DISKFILE_OFFSET = 0; 
	m_pTmpObj = NULL;	
}	

TDiskIdxObjMng::~TDiskIdxObjMng()
{
	DetachFile();
}	
ssize_t TDiskIdxObjMng::DetachFile()
{
	if (m_iDataFileFD>=0)
	{
		close(m_iDataFileFD);
		m_iDataFileFD = -1;
	}

	if (m_pTmpObj)
	{
		delete []m_pTmpObj;
		m_pTmpObj = NULL;
	}

	return 0;
}
ssize_t TDiskIdxObjMng::CountMemSize(ssize_t iIdxObjNum,ssize_t iDSNum/*=0*/)
{
	//自己至少要用一个存储free list 指针
	iDSNum = iDSNum<1?1:iDSNum;
	
	ssize_t iIdxSize = sizeof(TIdx) + sizeof(idx_t)*iDSNum;
	return sizeof(TIdxObjHead) + iIdxObjNum*iIdxSize;
}
ssize_t TDiskIdxObjMng::CountFileSize(ssize_t iObjSize,ssize_t iIdxObjNum)
{
	return  iIdxObjNum*iObjSize;
}
ssize_t TDiskIdxObjMng::CountObjNumByMem(ssize_t iObjSize,ssize_t iMemSize,ssize_t iDSNum/*=0*/)
{
	iDSNum = iDSNum<1?1:iDSNum;
	
	ssize_t iLeftSize = iMemSize - sizeof(TIdxObjHead);
	ssize_t iIdxSize = sizeof(TIdx) + sizeof(idx_t)*iDSNum;
	return iLeftSize/iIdxSize;
}
ssize_t TDiskIdxObjMng::CountObjNumByFile(ssize_t iObjSize,ssize_t iFileSize)
{
	return iFileSize/iObjSize;
}

//return attached mem size
ssize_t TDiskIdxObjMng::AttachMemFile(
	char* pMemPtr,const ssize_t MEMSIZE,ssize_t iInitType/*=emInit*/,
	char* pDiskFile,ssize_t iDiskFileStartPos/*=0*/,const ssize_t FILESIZE/*=0*/,
	ssize_t iObjSize,ssize_t iIdxObjNum,ssize_t iDSNum/*=0*/)
{
	if ( !pDiskFile || !pMemPtr)
	{
		return -1;
	}

	m_iDSNum = (iDSNum < 1) ? 1 : iDSNum;
	
	ssize_t iTheryObjNumByMem = CountObjNumByMem(iObjSize,MEMSIZE,m_iDSNum);
	ssize_t iTheryObjNumByFile = CountObjNumByFile(iObjSize,FILESIZE);
	ssize_t iTheryMaxObjNum = iTheryObjNumByMem<iTheryObjNumByFile?iTheryObjNumByMem:iTheryObjNumByFile;

	if(iIdxObjNum <= 0)
		iIdxObjNum = iTheryMaxObjNum;
	
	if(iIdxObjNum > iTheryMaxObjNum)
	{
		if(iIdxObjNum > iTheryObjNumByMem)
		{
			printf("ERR: TDiskIdxObjMng need more MEMSIZE to support %llu Objs\n",(unsigned long long)iIdxObjNum);
			return -1;
		}
		else
		{
			printf("ERR: TDiskIdxObjMng need more FILESIZE to support %llu Objs\n",(unsigned long long)iIdxObjNum);
			return -1;
		}	
	}

	ssize_t iIdxSize = sizeof(TIdx) + sizeof(idx_t)*m_iDSNum;

	m_pIdxObjHead = (TIdxObjHead*)pMemPtr;
	m_pIdx = (TIdx*)(pMemPtr+sizeof(TIdxObjHead));
	
	if (iInitType == emInit)
	{
		m_pIdxObjHead->m_iObjSize = iObjSize;
		m_pIdxObjHead->m_iIdxObjNum = iIdxObjNum;
		m_pIdxObjHead->m_iIdxSize = iIdxSize;
		FormatIdx();
	}
	else
	{
		if ((m_pIdxObjHead->m_iObjSize != iObjSize)||
			(m_pIdxObjHead->m_iIdxObjNum != iIdxObjNum) ||
			(m_pIdxObjHead->m_iIdxSize != iIdxSize))
		{
			return -3;
		}
	}
	
	//打开数据文件
	if(m_iDataFileFD >= 0)
		close(m_iDataFileFD);
		
	m_iDataFileFD = open(pDiskFile,O_RDWR| O_LARGEFILE,0666);
	if (m_iDataFileFD < 0)
	{
		printf("Can not open file %s\n",pDiskFile);
		return -3;
	}

	DISKFILE_OFFSET = (ssize_t)iDiskFileStartPos;

	if(m_pTmpObj)
		delete m_pTmpObj;

	m_pTmpObj = new char[iObjSize];
	
	return CountMemSize(iIdxObjNum,m_iDSNum);
}
ssize_t TDiskIdxObjMng::CopyAttachObj(ssize_t iIdx,ssize_t iOffset,char* pObj,const ssize_t COPYSIZE)
{
	if( iIdx < 0 || iIdx >= m_pIdxObjHead->m_iIdxObjNum)
	{
		return -1;
	}

	ssize_t iCopyLen = COPYSIZE>(m_pIdxObjHead->m_iObjSize-iOffset)?(m_pIdxObjHead->m_iObjSize-iOffset):COPYSIZE;
	ssize_t iRet = pread64(m_iDataFileFD,pObj,iCopyLen,DISKFILE_OFFSET+m_pIdxObjHead->m_iObjSize*iIdx+iOffset);
	if (iRet != iCopyLen)
	{
		return -2;
	}
	
	return iCopyLen;
}
char* TDiskIdxObjMng::GetAttachObj(ssize_t iIdx)
{
	if( iIdx < 0 || iIdx >= m_pIdxObjHead->m_iIdxObjNum)
	{
		return NULL;
	}
	
	ssize_t iRet = pread64(m_iDataFileFD,m_pTmpObj,m_pIdxObjHead->m_iObjSize,DISKFILE_OFFSET+m_pIdxObjHead->m_iObjSize*iIdx);
	if (iRet != m_pIdxObjHead->m_iObjSize)
	{
		return NULL;
	}
	
	return m_pTmpObj;
}
ssize_t TDiskIdxObjMng::SetAttachObj(ssize_t iIdx,ssize_t iOffset,char* pObj,ssize_t iLen/*=0*/)
{
	if( iIdx < 0 || iIdx >= m_pIdxObjHead->m_iIdxObjNum)
	{
		return -1;
	}

	if(iLen <= 0)
		iLen = m_pIdxObjHead->m_iObjSize;

	ssize_t iObjlen = iLen>(m_pIdxObjHead->m_iObjSize-iOffset)?(m_pIdxObjHead->m_iObjSize-iOffset):iLen;
	ssize_t iRet = pwrite64(m_iDataFileFD,pObj,iObjlen,DISKFILE_OFFSET+m_pIdxObjHead->m_iObjSize*iIdx+iOffset);
	if (iRet != iObjlen)
	{
		return -2;
	}
	
	return 0;
}

//-------------------------------------------------------------------------------
#define HashKeyToIdx(pKey,iLen)	SuperFastHash((char*)pKey,(u_int32_t)iLen)

CHashTab::CHashTab()
{
	m_pHashHead = NULL;
	m_pHashItems = NULL;
	m_pIdxObjMng = NULL;	
	m_pfObjInitSetKey = NULL;
	m_pfObjGetKey = NULL;
	m_iInitType = emInit;
}
ssize_t CHashTab::CountMemSize(ssize_t iBucketNum)
{
	return sizeof(THashHead) + iBucketNum*sizeof(THashItem);
}
	
//绑定内存
ssize_t CHashTab::AttachMem(char* pMemPtr,const ssize_t MEMSIZE,ssize_t iBucketNum,ssize_t iInitType/*=emInit*/)
{
	if ((MEMSIZE < CountMemSize(iBucketNum)) || !pMemPtr || (MEMSIZE<=0) ||(iBucketNum<=0))
		return -1;

	m_pHashHead = (THashHead*)pMemPtr;
	m_pHashItems = (THashItem*)(pMemPtr+sizeof(THashHead));

	m_iInitType = iInitType;
	if (iInitType == emInit)
	{
		memset(m_pHashHead,0,sizeof(THashHead));	
		memset(m_pHashItems,-1,sizeof(THashItem)*iBucketNum);
		m_pHashHead->m_iBucketNum = iBucketNum;
	}
	else
	{
		if (m_pHashHead->m_iBucketNum != (ssize_t)iBucketNum)
			return -2;
	}
	
	return CountMemSize(iBucketNum);
}

//绑定对象管理器
ssize_t CHashTab::AttachIdxObjMng(TIdxObjMng *pIdxObjMng, SET_KEY_FUNC pfObjInitSetKey,GET_KEY_FUNC pfObjGetKey)
{
	if (!m_pHashHead ||!pIdxObjMng||!pfObjInitSetKey || !pfObjGetKey)
		return -1;

	m_pIdxObjMng = pIdxObjMng;
	
	//在块管理器中申请一个链
	ssize_t iDSSuffix = m_pIdxObjMng->GetOneFreeDS();
	if (iDSSuffix < 0)
		return -2;
	
	if (m_iInitType == emInit)
	{
		m_pHashHead->m_iDSSuffix = iDSSuffix;
	}
	else
	{
		if(m_pHashHead->m_iDSSuffix != iDSSuffix)
			return -3;
	}
	
	m_pfObjInitSetKey = pfObjInitSetKey;
	m_pfObjGetKey = pfObjGetKey;
	return 0;
}
ssize_t CHashTab::GetBucketNum()
{
	return m_pHashHead->m_iBucketNum;
}

ssize_t CHashTab::GetBucketNodeHead(ssize_t iBucketIdx)
{
	if (iBucketIdx < 0 || iBucketIdx >= m_pHashHead->m_iBucketNum)
		return -1;

	return m_pHashItems[iBucketIdx].m_iFirstObjIdx;
}

ssize_t CHashTab::GetBucketNodeNext(ssize_t iNodeIdx)
{
	//指向下一个
	return m_pIdxObjMng->GetDsIdx(iNodeIdx,m_pHashHead->m_iDSSuffix);	
}

char* CHashTab::CreateObjectByKey(void *pvKey, ssize_t iKeyLength, ssize_t& iObjIdx)
{
    ssize_t iTempIdx = -1;
    char abyTempKey[1024];
    ssize_t  iTempKeyLen = 0;

    iObjIdx = 0;

    if( !pvKey || iKeyLength <= 0 || !m_pIdxObjMng ||!m_pfObjGetKey || m_pHashHead->m_iDSSuffix < 0)
    {
        iObjIdx = -1;
        return NULL;
    }

    u_int32_t unHashIdx = HashKeyToIdx(pvKey, iKeyLength);
    unHashIdx %= m_pHashHead->m_iBucketNum;
	
    char *pObj = NULL;	

    //没有发生冲突，直接分配该节点
    if( m_pHashItems[unHashIdx].m_iFirstObjIdx >= 0 )
    {
        //查找有无相同节点存在
        iTempIdx = m_pHashItems[unHashIdx].m_iFirstObjIdx;
        while( iTempIdx >= 0 )
        {
			pObj = m_pIdxObjMng->GetAttachObj(iTempIdx);
			if( !pObj )
			{
				iObjIdx = -3;
				return NULL;
			}
			
			if (m_pfObjGetKey((void*)pObj,(void*)abyTempKey,iTempKeyLen) == 0)
			{
		            if( iTempKeyLen == iKeyLength &&
		                !memcmp((const void *)abyTempKey, pvKey, iKeyLength) )
		            {
		                break;
		            }
			}

			iTempIdx = m_pIdxObjMng->GetDsIdx(iTempIdx,m_pHashHead->m_iDSSuffix);
        }

        //如果该节点已经存在，则直接返回找到的节点
        if( iTempIdx >= 0 )
        {
        	iObjIdx = iTempIdx;
			return m_pIdxObjMng->GetAttachObj(iTempIdx);
        }
    }
	
	//否则创建节点，并挂入冲突列表头
	iTempIdx = m_pIdxObjMng->CreateObject();
	if( iTempIdx < 0 )
	{
		iObjIdx = -4;
		//printf("ERR:CHashTab::%s Create IdxObjMng Object failed![%s:%d]\n",
		//	__FUNCTION__,__FILE__,__LINE__);
		return NULL;
	}

	if( m_pHashItems[unHashIdx].m_iFirstObjIdx < 0 )
		m_pHashHead->m_iUsedBucket++;
	
	m_pIdxObjMng->SetDsIdx(iTempIdx,m_pHashHead->m_iDSSuffix,m_pHashItems[unHashIdx].m_iFirstObjIdx);	
	m_pHashItems[unHashIdx].m_iFirstObjIdx = iTempIdx;

	//设入主键
	pObj = m_pIdxObjMng->GetAttachObj(iTempIdx);
	if (!pObj)
	{
		m_pIdxObjMng->DestroyObject(iTempIdx);
		return NULL;
	}
	m_pfObjInitSetKey(pObj,pvKey,iKeyLength);

	iObjIdx = iTempIdx;
	return pObj;
}
char* CHashTab::GetObjectByIdx(ssize_t iObjIdx)
{
	if(iObjIdx < 0 || !m_pIdxObjMng ||!m_pfObjGetKey || m_pHashHead->m_iDSSuffix < 0)
		return NULL;

	return m_pIdxObjMng->GetAttachObj(iObjIdx);
}
char* CHashTab::GetObjectByKey(void *pvKey, ssize_t iKeyLength, ssize_t& iObjIdx)
{
    ssize_t iTempIdx = -1;
    
    char abyTempKey[1024];
    ssize_t  iTempKeyLen = 0;

    if( !pvKey || iKeyLength <= 0 || !m_pIdxObjMng ||!m_pfObjGetKey || m_pHashHead->m_iDSSuffix < 0)
        return NULL;
    
    u_int32_t unHashIdx = HashKeyToIdx(pvKey, iKeyLength);
    unHashIdx %= m_pHashHead->m_iBucketNum;

    if( m_pHashItems[unHashIdx].m_iFirstObjIdx < 0 )
        return NULL;
	
    char *pObj;
    //查找有无相同节点存在
    iTempIdx = m_pHashItems[unHashIdx].m_iFirstObjIdx;
    while( iTempIdx >= 0 )
    {
		pObj = m_pIdxObjMng->GetAttachObj(iTempIdx);
		if( !pObj )
		{
		    return NULL;
		}
		
		if (m_pfObjGetKey((void*)pObj,(void*)abyTempKey,iTempKeyLen) == 0)
		{
	            if( iTempKeyLen == iKeyLength &&
	                !memcmp((const void *)abyTempKey, pvKey, iKeyLength) )
	            {
	                break;
	            }
		}
		
		iTempIdx = m_pIdxObjMng->GetDsIdx(iTempIdx,m_pHashHead->m_iDSSuffix);
    }

    //如果该节点存在，则返回找到的节点
    if( iTempIdx >= 0 )
    {
        iObjIdx = iTempIdx;
        return m_pIdxObjMng->GetAttachObj(iTempIdx);
    }
    else
    {
        return NULL;
    }
}

ssize_t CHashTab::DeleteObjectByKey(void *pvKey, ssize_t iKeyLength)
{
    ssize_t iTempIdx = -1;
    ssize_t iTempPrevIdx = -1;
    ssize_t iTempNextIdx = -1;
    
    char abyTempKey[1024];
    ssize_t  iTempKeyLen = 0;

    if( !pvKey || iKeyLength <= 0 || !m_pIdxObjMng ||!m_pfObjGetKey || m_pHashHead->m_iDSSuffix < 0)
        return -1;

    u_int32_t unHashIdx = HashKeyToIdx(pvKey, iKeyLength);
    unHashIdx %= m_pHashHead->m_iBucketNum;

    if( m_pHashItems[unHashIdx].m_iFirstObjIdx < 0 )
        return -3;

    char *pObj;
	
    //查找有无相同节点存在
    iTempIdx = m_pHashItems[unHashIdx].m_iFirstObjIdx;
    while( iTempIdx >= 0 )
    {
		pObj = m_pIdxObjMng->GetAttachObj(iTempIdx);
		if( !pObj )
		    return -4;
		
		if (m_pfObjGetKey(pObj,abyTempKey,iTempKeyLen) == 0)
		{
	            if( iTempKeyLen == iKeyLength &&
	                !memcmp((const void *)abyTempKey, pvKey, iKeyLength) )
	            {
	                break;
	            }
		}
		
		iTempPrevIdx = iTempIdx;
		iTempIdx = m_pIdxObjMng->GetDsIdx(iTempIdx,m_pHashHead->m_iDSSuffix);
    }

	if( iTempIdx < 0 )
		return -5;

	//如果该节点存在，则从哈希表中删除找到的节点
	iTempNextIdx =  m_pIdxObjMng->GetDsIdx(iTempIdx,m_pHashHead->m_iDSSuffix);

	if (iTempPrevIdx >= 0)
	{
		m_pIdxObjMng->SetDsIdx(iTempPrevIdx,m_pHashHead->m_iDSSuffix,iTempNextIdx);
	}

	m_pIdxObjMng->SetDsIdx(iTempIdx,m_pHashHead->m_iDSSuffix,-1);

	if( iTempIdx == (ssize_t)m_pHashItems[unHashIdx].m_iFirstObjIdx )
	{
		m_pHashItems[unHashIdx].m_iFirstObjIdx = iTempNextIdx;
	}

	m_pIdxObjMng->DestroyObject(iTempIdx);

	if(m_pHashItems[unHashIdx].m_iFirstObjIdx < 0)
		m_pHashHead->m_iUsedBucket--;
	
	return iTempIdx;
}

ssize_t CHashTab::Print(FILE *fpOut)
{
    ssize_t iTempIdx = -1;
    ssize_t iHashIdx = -1;
    
    char abyTempKey[1024];
    ssize_t  iTempKeyLen = 0;

    char *pObj = NULL;

    if( !fpOut )
        return -1;

    fprintf(fpOut, "\nSHOW HASH TABLE(%lld objs):\n",(long long)m_pIdxObjMng->GetUsedCount());

    for( iHashIdx = 0; iHashIdx < m_pHashHead->m_iBucketNum; iHashIdx++ )
    {
        if( m_pHashItems[iHashIdx].m_iFirstObjIdx < 0 )
        {
            continue;
        }

        fprintf(fpOut, "HASH[%06lld]->", (long long)iHashIdx);

        //查找有无相同节点存在
        iTempIdx = m_pHashItems[iHashIdx].m_iFirstObjIdx;
        while( iTempIdx >= 0 )
        {
        	pObj = m_pIdxObjMng->GetAttachObj(iTempIdx);
            if( !pObj )
            {
                fprintf(fpOut, "\nError in show, can't get idx %lld.\n", (long long)iTempIdx);
                fflush(fpOut);
                return -1;
            }

		m_pfObjGetKey(pObj,abyTempKey,iTempKeyLen);
		
            fprintf(fpOut, "OBJ[%lld].KEY=0x", (long long)iTempIdx);
			for (ssize_t i=0; i<iTempKeyLen; i++)
			{
				fprintf(fpOut, "%02x", abyTempKey[i]);
			}
		fprintf(fpOut, "\n");

             iTempIdx = m_pIdxObjMng->GetDsIdx(iTempIdx,m_pHashHead->m_iDSSuffix);
        }
        fprintf(fpOut, "END\n");
    }

    fflush(fpOut);

    return 0;
}
ssize_t CHashTab::GetUsage(ssize_t &iBucketNum,ssize_t &iUsedBucket)
{
	if(!m_pHashHead)
		return -1;

	iBucketNum = m_pHashHead->m_iBucketNum;
	iUsedBucket = m_pHashHead->m_iUsedBucket;
	return 0;
}
//---------------------------------------------------------------------------
CObjQueue::CObjQueue()
{
	m_pObjQueueHead = NULL;
	m_pIdxObjMng = NULL;
	m_iInitType = emInit;
}

ssize_t CObjQueue::CountMemSize()
{
	return sizeof(TObjQueueHead);
}

//绑定内存
ssize_t CObjQueue::AttachMem(char* pMemPtr,const ssize_t MEMSIZE,ssize_t iInitType/*=emInit*/)
{
	if ((MEMSIZE < CountMemSize()) || !pMemPtr || (MEMSIZE<=0))
		return -1;

	m_pObjQueueHead = (TObjQueueHead*)pMemPtr;

	m_iInitType = iInitType;
	if (iInitType == emInit)
	{
		memset(m_pObjQueueHead,-1,sizeof(TObjQueueHead));
		m_pObjQueueHead->m_iTotalCount = 0;
	}
	
	return CountMemSize();
}

ssize_t CObjQueue::AttachIdxObjMng(TIdxObjMng *pIdxObjMng,ssize_t iDSSuffix1/*=-1*/,ssize_t iDSSuffix2/*=-1*/)
{
	if (!m_pObjQueueHead || !pIdxObjMng)
		return -1;
	
	m_pIdxObjMng = pIdxObjMng;

	if (iDSSuffix1 < 0)
		iDSSuffix1 = m_pIdxObjMng->GetOneFreeDS();
	
	if (iDSSuffix1 < 0)	return -2;

	if (iDSSuffix2 < 0)
		iDSSuffix2 = m_pIdxObjMng->GetOneFreeDS();	
	
	if (iDSSuffix2 < 0)	return -3;
	
	if (m_iInitType == emInit)
	{
		m_pObjQueueHead->m_iDSPrev = iDSSuffix1;
		m_pObjQueueHead->m_iDSNext = iDSSuffix2;		
	}
	else
	{
		if (m_pObjQueueHead->m_iDSPrev != iDSSuffix1 ||
			m_pObjQueueHead->m_iDSNext != iDSSuffix2)
		{
			return -4;
		}
	}

	return 0;
}

ssize_t CObjQueue::GetLength()
{
	return m_pObjQueueHead->m_iTotalCount;
}

ssize_t CObjQueue::GetNextItem(ssize_t iItemIdx, ssize_t& iNextItemIdx)
{
	if( !m_pIdxObjMng)
		return -1;
	
	iNextItemIdx = m_pIdxObjMng->GetDsIdx(iItemIdx,m_pObjQueueHead->m_iDSNext);
	return 0;
}

ssize_t CObjQueue::GetPrevItem(ssize_t iItemIdx, ssize_t& iPrevItemIdx)
{
	if( !m_pIdxObjMng)
		return -1;

	iPrevItemIdx = m_pIdxObjMng->GetDsIdx(iItemIdx,m_pObjQueueHead->m_iDSPrev);
	return 0;
}

ssize_t CObjQueue::SetNextItem(ssize_t iItemIdx, ssize_t iNextItemIdx)
{
	if( !m_pIdxObjMng)
		return -1;
	
	m_pIdxObjMng->SetDsIdx(iItemIdx,m_pObjQueueHead->m_iDSNext,iNextItemIdx);
	return 0;
}

ssize_t CObjQueue::SetPrevItem(ssize_t iItemIdx, ssize_t iPrevItemIdx)
{
	if( !m_pIdxObjMng)
		return -1;
	
	m_pIdxObjMng->SetDsIdx(iItemIdx,m_pObjQueueHead->m_iDSPrev,iPrevItemIdx);
	return 0;
}

ssize_t CObjQueue::TakeHeadItem()
{
    ssize_t iTempItem;

    if( !m_pIdxObjMng )
        return -1;

    if( m_pObjQueueHead->m_iHeadItem == -1 )
        return -1;

    iTempItem = m_pObjQueueHead->m_iHeadItem;
    DeleteItem(m_pObjQueueHead->m_iHeadItem);
    return iTempItem;
}

ssize_t CObjQueue::AppendToTail(ssize_t iItemIdx)
{
	TIdxObjMng::TIdx *pTempIdx = NULL;

	if( !m_pIdxObjMng )
		return -1;

	if( !(pTempIdx=m_pIdxObjMng->GetIdx(iItemIdx)) )
		return -2;

	SetNextItem(iItemIdx, -1);
	SetPrevItem(iItemIdx, m_pObjQueueHead->m_iTailItem);
	m_pObjQueueHead->m_iTotalCount++;
	if( m_pObjQueueHead->m_iTailItem == -1 )
	{
		m_pObjQueueHead->m_iHeadItem = m_pObjQueueHead->m_iTailItem = iItemIdx;
		return 0;
	}

	SetNextItem(m_pObjQueueHead->m_iTailItem, iItemIdx);
	m_pObjQueueHead->m_iTailItem = iItemIdx;
	return 0;
}

ssize_t CObjQueue::PushToHead(ssize_t iItemIdx)
{
	TIdxObjMng::TIdx *pTempIdx = NULL;

	if( !m_pIdxObjMng )
		return -1;

	if( !(pTempIdx=m_pIdxObjMng->GetIdx(iItemIdx)) )
		return -2;

	SetPrevItem(iItemIdx, -1);
	SetNextItem(iItemIdx, m_pObjQueueHead->m_iHeadItem);
	m_pObjQueueHead->m_iTotalCount++;

	if( m_pObjQueueHead->m_iHeadItem == -1 )
	{
		m_pObjQueueHead->m_iTailItem = m_pObjQueueHead->m_iHeadItem = iItemIdx;
		return 0;
	}

	SetPrevItem(m_pObjQueueHead->m_iHeadItem, iItemIdx);
	m_pObjQueueHead->m_iHeadItem = iItemIdx;
	return 0;
}

ssize_t CObjQueue::ItemInQueue(ssize_t iItemIdx)
{
	ssize_t iPrevItemIdx;
	ssize_t iNextItemIdx;
	GetPrevItem(iItemIdx,iPrevItemIdx);
	GetNextItem(iItemIdx,iNextItemIdx);

	if (iPrevItemIdx >= 0)
		return 1;
	if (iNextItemIdx >= 0)
		return 1;
	
	return 0;
}

ssize_t CObjQueue::InsertAfter(ssize_t iItemIdx, ssize_t iNewItemIdx)
{
	if(!m_pIdxObjMng->GetIdx(iItemIdx))
		return -1;

	if(!m_pIdxObjMng->GetIdx(iNewItemIdx))
		return -2;
	
	ssize_t iNextItemIdx;
	GetNextItem(iItemIdx,iNextItemIdx);
	SetNextItem(iNewItemIdx,iNextItemIdx);
	SetPrevItem(iNextItemIdx,iNewItemIdx);

	SetPrevItem(iNewItemIdx,iItemIdx);
	SetNextItem(iItemIdx,iNewItemIdx);

	m_pObjQueueHead->m_iTotalCount++;
	return 0;
}

ssize_t CObjQueue::InsertBefore(ssize_t iItemIdx, ssize_t iNewItemIdx)
{
	if(!m_pIdxObjMng->GetIdx(iItemIdx))
		return -1;

	if(!m_pIdxObjMng->GetIdx(iNewItemIdx))
		return -2;
	
	ssize_t iPrevItemIdx;
	GetPrevItem(iItemIdx,iPrevItemIdx);
	SetPrevItem(iNewItemIdx,iPrevItemIdx);
	SetNextItem(iPrevItemIdx,iNewItemIdx);

	SetNextItem(iNewItemIdx,iItemIdx);
	SetPrevItem(iItemIdx,iNewItemIdx);

	m_pObjQueueHead->m_iTotalCount++;
	return 0;
}

ssize_t CObjQueue::GetHeadItem()
{
    return m_pObjQueueHead->m_iHeadItem;
}

ssize_t CObjQueue::GetTailItem()
{
    return m_pObjQueueHead->m_iTailItem;
}

ssize_t CObjQueue::DeleteItem(ssize_t iItemIdx)
{
	TIdxObjMng::TIdx *pTempIdx = NULL;
	ssize_t iNextItemIdx = -1;
	ssize_t iPrevItemIdx = -1;

	if( !m_pIdxObjMng )
		return -1;

	if( !(pTempIdx=m_pIdxObjMng->GetIdx(iItemIdx)) )
		return -2;

	GetPrevItem(iItemIdx, iPrevItemIdx);
	GetNextItem(iItemIdx, iNextItemIdx);

	if( iPrevItemIdx == -1 )
	{
		//前延节点为空，则被删除节点必为首节点
		if( iItemIdx != m_pObjQueueHead->m_iHeadItem )
			return -4;

		m_pObjQueueHead->m_iHeadItem = iNextItemIdx;
	}
	else
	{
		SetNextItem(iPrevItemIdx, iNextItemIdx);
	}

	if( iNextItemIdx == -1 )
	{
		//后续节点为空，则被删除节点必为尾节点
		if( iItemIdx != m_pObjQueueHead->m_iTailItem )
			return -5;

		m_pObjQueueHead->m_iTailItem = iPrevItemIdx;
	}
	else
	{
		SetPrevItem(iNextItemIdx, iPrevItemIdx);
	}

	//清除被删除节点的结构信息
	SetPrevItem(iItemIdx, -1);
	SetNextItem(iItemIdx, -1);
	m_pObjQueueHead->m_iTotalCount--;
	return 0;
}

ssize_t CObjQueue::Print(FILE *fpOut)
{
    ssize_t iNextIdx = m_pObjQueueHead->m_iHeadItem;
    ssize_t iPrevIdx = m_pObjQueueHead->m_iTailItem;

    if( !fpOut || !m_pIdxObjMng )
        return -1;

    fprintf(fpOut, "QUEUE:TOTAL=%lld, HEAD=%lld, TAIL=%lld\nHEAD[DS %lld]->", 
		(long long)m_pObjQueueHead->m_iTotalCount,
		(long long)m_pObjQueueHead->m_iHeadItem,
		(long long)m_pObjQueueHead->m_iTailItem,
		(long long)m_pObjQueueHead->m_iDSNext);

    while( iNextIdx != -1 )
    {
        fprintf(fpOut, "IDX[%lld]->", (long long)iNextIdx);
        GetNextItem(iNextIdx, iNextIdx);
    }
    fprintf(fpOut, "TAIL\n");

    fprintf(fpOut, "TAIL[DS %lld]->",(long long)m_pObjQueueHead->m_iDSPrev);
    while( iPrevIdx != -1 )
    {
        fprintf(fpOut, "IDX[%lld]->",(long long)iPrevIdx);
        GetPrevItem(iPrevIdx, iPrevIdx);
    }
    fprintf(fpOut, "HEAD\n");
    fflush( fpOut );
    return 0;
}

//--------------------------------------------------------------------------
CBuffMng::CBuffMng()
{
	m_pBuffMngHead = NULL;
	m_pBuffItem = NULL;
	m_pIdxObjMng = NULL;
	m_iInitType = emInit;
}

ssize_t CBuffMng::CountMemSize(ssize_t iBuffNum)
{
	return sizeof(TBuffMngHead) + sizeof(TBuffItem)*iBuffNum;
}

//绑定内存
ssize_t CBuffMng::AttachMem(char* pMemPtr,const ssize_t MEMSIZE,ssize_t iBuffNum,ssize_t iInitType/*=emInit*/)
{
	if ((MEMSIZE < CountMemSize(iBuffNum)) || !pMemPtr || (MEMSIZE<=0) ||(iBuffNum<=0))
		return -1;

	m_pBuffMngHead = (TBuffMngHead*)pMemPtr;
	m_pBuffItem = (TBuffItem*)(pMemPtr+sizeof(TBuffMngHead));

	m_iInitType = iInitType;
	if (iInitType == emInit)
	{	
		memset(m_pBuffItem,-1,sizeof(TBuffItem)*iBuffNum);
		
		m_pBuffMngHead->m_iBuffNum = iBuffNum;
		m_pBuffMngHead->m_iFreeBuffNum = iBuffNum;
		m_pBuffMngHead->m_iDSSuffix = -1;
	}
	else
	{
		if (m_pBuffMngHead->m_iBuffNum != iBuffNum)
			return -2;
	}
	
	return CountMemSize(iBuffNum);
}


//绑定对象管理器
ssize_t CBuffMng::AttachIdxObjMng(TIdxObjMng *pIdxObjMng)
{
	if (!m_pBuffMngHead || !pIdxObjMng)
		return -1;

	m_pIdxObjMng = pIdxObjMng;

	ssize_t iDSSuffix = m_pIdxObjMng->GetOneFreeDS();
	if (iDSSuffix < 0)
		return -2;

	if (m_iInitType == emInit)
	{
		m_pBuffMngHead->m_iDSSuffix = iDSSuffix;	
	}
	else
	{
		if (m_pBuffMngHead->m_iDSSuffix != iDSSuffix)
			return -3;
	}
	
	return 0;
}

ssize_t CBuffMng::SetBufferSpace(ssize_t iBuffSuffix,ssize_t iBuffSize)
{
	if (!m_pBuffMngHead||iBuffSuffix<0 || iBuffSuffix>=m_pBuffMngHead->m_iBuffNum || !m_pIdxObjMng|| iBuffSize<=0)
	{
		return -1;
	}

	const ssize_t BLOCKSIZE = m_pIdxObjMng->GetObjSize();

	//空间不足
	ssize_t iObjFree = m_pIdxObjMng->GetObjNum() - m_pIdxObjMng->GetUsedCount();
	ssize_t iFreeBytes = iObjFree*BLOCKSIZE;
	if(iFreeBytes < iBuffSize-m_pBuffItem[iBuffSuffix].m_iBuffSize)
		return -2;
	
	if(m_pBuffItem[iBuffSuffix].m_iBuffIdx >= 0)
	{
		FreeBuffer(iBuffSuffix);
	}

	ssize_t iObjCnt = iBuffSize/BLOCKSIZE;
    if((iBuffSize%BLOCKSIZE) != 0) 
    	iObjCnt++;	

	//第一个
	ssize_t iFirstObjIdx = m_pIdxObjMng->CreateObject();
	if (iFirstObjIdx < 0)
	{
		printf("ERR:CBuffMng::%s Create IdxObjMng Object failed![%s:%d]\n",
				__FUNCTION__,__FILE__,__LINE__);
		return -3;
	}
	m_pBuffItem[iBuffSuffix].m_iBuffIdx = iFirstObjIdx;
	m_pBuffMngHead->m_iFreeBuffNum--;
	
	ssize_t iLastObjIdx = iFirstObjIdx;
	
	for(ssize_t i=0; i<iObjCnt-1; i++)
	{
		ssize_t iObjNewIdx = m_pIdxObjMng->CreateObject();
		if (iObjNewIdx < 0)
		{
			printf("ERR:CBuffMng::%s Create IdxObjMng Object failed![%s:%d]\n",
				__FUNCTION__,__FILE__,__LINE__);
			FreeBuffer(iBuffSuffix);
			return -4;
		}	
		m_pIdxObjMng->SetDsIdx(iLastObjIdx,m_pBuffMngHead->m_iDSSuffix,iObjNewIdx);
		iLastObjIdx = iObjNewIdx;	
	}

	m_pBuffItem[iBuffSuffix].m_iBuffSize = iBuffSize;

#ifdef _BUFFMNG_APPEND_SKIP
	m_pBuffItem[iBuffSuffix].m_iBuffOffset = 0;
	m_pBuffItem[iBuffSuffix].m_iLastBuffIdx = iLastObjIdx;
#endif

	return 0;
}

ssize_t CBuffMng::SetDataToSpace(ssize_t iBuffSuffix,char* pBuffer, ssize_t iBuffSize)
{
	if (!pBuffer || !m_pBuffMngHead||iBuffSuffix<0 || iBuffSuffix>=m_pBuffMngHead->m_iBuffNum || !m_pIdxObjMng|| iBuffSize<0)
	{
		return -1;
	}
	
	if(iBuffSize != m_pBuffItem[iBuffSuffix].m_iBuffSize)
		return -2;

	if(m_pBuffItem[iBuffSuffix].m_iBuffIdx < 0)
		return -3;
	
	const ssize_t BLOCKSIZE = m_pIdxObjMng->GetObjSize();
	
	//后续
	ssize_t iObjIdx = m_pBuffItem[iBuffSuffix].m_iBuffIdx;
	ssize_t iCopiedLen = 0;
	while(iCopiedLen < iBuffSize)
	{
		if(iObjIdx < 0)
			return -4;
		
		ssize_t iLeftLen = iBuffSize-iCopiedLen;
		ssize_t iCopyLen = iLeftLen > BLOCKSIZE ? BLOCKSIZE : iLeftLen;

		m_pIdxObjMng->SetAttachObj(iObjIdx,0,pBuffer+iCopiedLen,iCopyLen);	
		iCopiedLen += iCopyLen;

		iObjIdx = m_pIdxObjMng->GetDsIdx(iObjIdx,m_pBuffMngHead->m_iDSSuffix);
	}

	return 0;
}

#if 0
ssize_t CBuffMng::AppendBufferSpace(ssize_t iBuffSuffix,ssize_t iBuffSize)
{
	if (!m_pBuffMngHead||iBuffSuffix<0 || iBuffSuffix>=m_pBuffMngHead->m_iBuffNum || !m_pIdxObjMng|| iBuffSize<=0)
	{
		return -1;
	}

	const ssize_t BLOCKSIZE = m_pIdxObjMng->GetObjSize();

	if(m_pBuffItem[iBuffSuffix].m_iBuffIdx >= 0)
	{
		FreeBuffer(iBuffSuffix);
	}

	ssize_t iTotalSize = m_pBuffItem[iBuffSuffix].m_iBuffSize + iBuffSize;
	ssize_t iTotalObjCnt = iTotalSize/BLOCKSIZE;
    if((iTotalSize%BLOCKSIZE) != 0) 
    	iTotalObjCnt++;	

	//第一个
	int iCurrCnt = 1;
	ssize_t iObjIdx = m_pBuffItem[iBuffSuffix].m_iBuffIdx;
	ssize_t iLastObjIdx = iObjIdx;
	while(iObjIdx >= 0)
	{
		iObjIdx = m_pIdxObjMng->GetDsIdx(iObjIdx,m_pBuffMngHead->m_iDSSuffix);
		
		if(iObjIdx >= 0)
		{
			iLastObjIdx = iObjIdx;
			iCurrCnt++;
		}	
	}

	ssize_t iNewStartIdx = iLastIdx;
	ssize_t iMissCnt = iTotalObjCnt - iCurrCnt;
	for(ssize_t i = 0; i<iMissCnt; i++)
	{
		ssize_t iObjNewIdx = m_pIdxObjMng->CreateObject();
		if (iObjNewIdx < 0)
		{
			printf("ERR:CBuffMng::%s Create IdxObjMng Object failed![%s:%d]\n",
				__FUNCTION__,__FILE__,__LINE__);
			FreeObjList(iNewStartIdx);
			return -3;
		}
		m_pIdxObjMng->SetDsIdx(iLastObjIdx,m_pBuffMngHead->m_iDSSuffix,iObjNewIdx);
		iLastObjIdx = iObjNewIdx;	
	}

	m_pBuffItem[iBuffSuffix].m_iBuffSize+ = iBuffSize;

#ifdef _BUFFMNG_APPEND_SKIP
	m_pBuffItem[iBuffSuffix].m_iLastBuffIdx = iLastObjIdx;
#endif

	return 0;
}
ssize_t CBuffMng::AppendDataToSpace(ssize_t iBuffSuffix,char* pBuffer, ssize_t iBuffSize)
{
	if (!pBuffer || !m_pBuffMngHead||iBuffSuffix<0 || iBuffSuffix>=m_pBuffMngHead->m_iBuffNum || !m_pIdxObjMng|| iBuffSize<0)
	{
		return -1;
	}
	
	if(iBuffSize != m_pBuffItem[iBuffSuffix].m_iBuffSize)
		return -2;

	if(m_pBuffItem[iBuffSuffix].m_iBuffIdx < 0)
		return -3;
	
	const ssize_t BLOCKSIZE = m_pIdxObjMng->GetObjSize();
	
	//后续
	ssize_t iObjIdx = m_pBuffItem[iBuffSuffix].m_iBuffIdx;
	ssize_t iCopiedLen = 0;
	while(iCopiedLen < iBuffSize)
	{
		if(iObjIdx < 0)
			return -4;
		
		ssize_t iLeftLen = iBuffSize-iCopiedLen;
		ssize_t iCopyLen = iLeftLen > BLOCKSIZE ? BLOCKSIZE : iLeftLen;

		m_pIdxObjMng->SetAttachObj(iObjIdx,0,pBuffer+iCopiedLen,iCopyLen);	
		iCopiedLen += iCopyLen;

		iObjIdx = m_pIdxObjMng->GetDsIdx(iObjIdx,m_pBuffMngHead->m_iDSSuffix);
	}

	return 0;
}
#endif
ssize_t CBuffMng::SetBuffer(ssize_t iBuffSuffix,char* pBuffer, ssize_t iBuffSize)
{
	if (!m_pBuffMngHead||iBuffSuffix<0 || iBuffSuffix>=m_pBuffMngHead->m_iBuffNum || !m_pIdxObjMng|| iBuffSize<0)
	{
		return -1;
	}
	if(iBuffSize == 0)
		return 0;

	const ssize_t BLOCKSIZE = m_pIdxObjMng->GetObjSize();

	if(m_pBuffItem[iBuffSuffix].m_iBuffIdx >= 0)
	{
		FreeBuffer(iBuffSuffix);
	}

	//第一个
	ssize_t iFirstObjIdx = m_pIdxObjMng->CreateObject();
	if (iFirstObjIdx < 0)
	{
		printf("ERR:CBuffMng::%s Create IdxObjMng Object failed![%s:%d]\n",
				__FUNCTION__,__FILE__,__LINE__);
		return -2;
	}
	m_pBuffItem[iBuffSuffix].m_iBuffIdx = iFirstObjIdx;	
	m_pBuffMngHead->m_iFreeBuffNum--;
	
	ssize_t iLastObjIdx = iFirstObjIdx;
	
	ssize_t iCopyLen = iBuffSize > BLOCKSIZE ? BLOCKSIZE : iBuffSize;

	if(pBuffer)
		m_pIdxObjMng->SetAttachObj(iFirstObjIdx,0,pBuffer,iCopyLen);
	
	m_pBuffItem[iBuffSuffix].m_iBuffSize = iCopyLen;
	
	//后续
	while(m_pBuffItem[iBuffSuffix].m_iBuffSize < iBuffSize)
	{
		ssize_t iObjNewIdx = m_pIdxObjMng->CreateObject();
		if (iObjNewIdx < 0)
		{
			printf("ERR:CBuffMng::%s Create IdxObjMng Object failed![%s:%d]\n",
				__FUNCTION__,__FILE__,__LINE__);
			FreeBuffer(iBuffSuffix);
			return -3;
		}	
		m_pIdxObjMng->SetDsIdx(iLastObjIdx,m_pBuffMngHead->m_iDSSuffix,iObjNewIdx);
		iLastObjIdx = iObjNewIdx;
		
		ssize_t iCopiedLen = m_pBuffItem[iBuffSuffix].m_iBuffSize;
		ssize_t iLeftLen = iBuffSize-iCopiedLen;
		iCopyLen = iLeftLen > BLOCKSIZE ? BLOCKSIZE : iLeftLen;

		if(pBuffer)
			m_pIdxObjMng->SetAttachObj(iObjNewIdx,0,pBuffer+iCopiedLen,iCopyLen);
		
		m_pBuffItem[iBuffSuffix].m_iBuffSize += iCopyLen;
	}


#ifdef _BUFFMNG_APPEND_SKIP
	m_pBuffItem[iBuffSuffix].m_iBuffOffset = 0;
	m_pBuffItem[iBuffSuffix].m_iLastBuffIdx = iLastObjIdx;
#endif

	return 0;
}

#ifdef _BUFFMNG_APPEND_SKIP

/*
ssize_t CBuffMng::AppendBufferSpace(ssize_t iBuffSuffix,ssize_t iBuffSize)
{
	if (!m_pBuffMngHead||iBuffSuffix<0 || iBuffSuffix>=m_pBuffMngHead->m_iBuffNum || !m_pIdxObjMng|| iBuffSize<=0)
	{
		return -1;
	}

	const ssize_t BLOCKSIZE = m_pIdxObjMng->GetObjSize();

	//空间不足
	ssize_t iObjFree = m_pIdxObjMng->GetObjNum() - m_pIdxObjMng->GetUsedCount();
	ssize_t iFreeBytes = iObjFree*BLOCKSIZE;
	if(iFreeBytes < iBuffSize)
		return -2;

	ssize_t iObjCnt = (iBuffSize+m_pBuffItem[iBuffSuffix].m_iBuffSize)/BLOCKSIZE;
    if(((iBuffSize+m_pBuffItem[iBuffSuffix].m_iBuffSize)%BLOCKSIZE) != 0) 
    	iObjCnt++;	

	ssize_t iCurrObjIdx;
	ssize_t iLastObjIdx;
	if(m_pBuffItem[iBuffSuffix].m_iBuffIdx >= 0)
	{
		iCurrObjIdx = m_pBuffItem[iBuffSuffix].m_iBuffIdx;
		iLastObjIdx = iCurrObjIdx;
		while(iCurrObjIdx >= 0)
		{
			iLastObjIdx = iCurrObjIdx;
			iObjCnt--;
			
			iCurrObjIdx = m_pIdxObjMng->GetDsIdx(iCurrObjIdx,m_pBuffMngHead->m_iDSSuffix);
		}

		if(iObjCnt <= 0)
			return 0;	

		//第一次
		ssize_t iFirstObjIdx = m_pIdxObjMng->CreateObject();
		if (iFirstObjIdx < 0)
		{
			printf("ERR:CBuffMng::%s Create IdxObjMng Object failed![%s:%d]\n",
					__FUNCTION__,__FILE__,__LINE__);
			return -3;
		}
		m_pIdxObjMng->SetDsIdx(iLastObjIdx,m_pBuffMngHead->m_iDSSuffix,iFirstObjIdx);
		iLastObjIdx = iFirstObjIdx;	
	}
	else
	{
		//第一个
		ssize_t iFirstObjIdx = m_pIdxObjMng->CreateObject();
		if (iFirstObjIdx < 0)
		{
			printf("ERR:CBuffMng::%s Create IdxObjMng Object failed![%s:%d]\n",
					__FUNCTION__,__FILE__,__LINE__);
			return -3;
		}
		m_pBuffItem[iBuffSuffix].m_iBuffIdx = iFirstObjIdx;
		iLastObjIdx = iFirstObjIdx;	

		m_pBuffMngHead->m_iFreeBuffNum--;
	}
	
	for(int i=0; i<iObjCnt-1; i++)
	{
		ssize_t iObjNewIdx = m_pIdxObjMng->CreateObject();
		if (iObjNewIdx < 0)
		{
			printf("ERR:CBuffMng::%s Create IdxObjMng Object failed![%s:%d]\n",
				__FUNCTION__,__FILE__,__LINE__);
			FreeBuffer(iBuffSuffix);
			return -4;
		}	
		m_pIdxObjMng->SetDsIdx(iLastObjIdx,m_pBuffMngHead->m_iDSSuffix,iObjNewIdx);
		iLastObjIdx = iObjNewIdx;	
	}

	m_pBuffItem[iBuffSuffix].m_iBuffSize += iBuffSize;
	m_pBuffItem[iBuffSuffix].m_iBuffOffset = 0;
	m_pBuffItem[iBuffSuffix].m_iLastBuffIdx = iLastObjIdx;
	return 0;
}
*/
ssize_t CBuffMng::AppendBuffer(ssize_t iBuffSuffix,char* pBuffer, ssize_t iBuffSize)
{
	if (!m_pBuffMngHead||iBuffSuffix<0 || iBuffSuffix>=m_pBuffMngHead->m_iBuffNum ||!m_pIdxObjMng|| iBuffSize<0)
	{
		return -1;
	}
	if(iBuffSize == 0)
		return 0;

	const ssize_t BLOCKSIZE = m_pIdxObjMng->GetObjSize();
	
	//新分配
	if (m_pBuffItem[iBuffSuffix].m_iBuffIdx < 0)
	{
		//第一个
		ssize_t iFirstObjIdx = m_pIdxObjMng->CreateObject();
		if (iFirstObjIdx < 0)
		{
			printf("ERR:CBuffMng::%s Create IdxObjMng Object failed![%s:%d]\n",
					__FUNCTION__,__FILE__,__LINE__);
			return -2;
		}
		m_pBuffItem[iBuffSuffix].m_iBuffIdx = iFirstObjIdx;
		m_pBuffItem[iBuffSuffix].m_iLastBuffIdx = iFirstObjIdx;		
		m_pBuffItem[iBuffSuffix].m_iBuffOffset = 0;
		
		m_pBuffMngHead->m_iFreeBuffNum--;
		
		ssize_t iCopyLen = iBuffSize > BLOCKSIZE ? BLOCKSIZE : iBuffSize;
		ssize_t iLeftLen = iBuffSize-iCopyLen;
		if(pBuffer)
			m_pIdxObjMng->SetAttachObj(iFirstObjIdx,0,pBuffer,iCopyLen);
		
		m_pBuffItem[iBuffSuffix].m_iBuffSize = iCopyLen;
		
		//后续
		while(m_pBuffItem[iBuffSuffix].m_iBuffSize < iBuffSize)
		{
			ssize_t iObjNewIdx = m_pIdxObjMng->CreateObject();
			if (iObjNewIdx < 0)
			{
				printf("ERR:CBuffMng::%s Create IdxObjMng Object failed![%s:%d]\n",
					__FUNCTION__,__FILE__,__LINE__);
				FreeBuffer(iBuffSuffix);
				return -3;
			}	
			m_pIdxObjMng->SetDsIdx(m_pBuffItem[iBuffSuffix].m_iLastBuffIdx,m_pBuffMngHead->m_iDSSuffix,iObjNewIdx);
			m_pBuffItem[iBuffSuffix].m_iLastBuffIdx = iObjNewIdx;
			
			ssize_t iCopiedLen = m_pBuffItem[iBuffSuffix].m_iBuffSize;
			iLeftLen = iBuffSize-iCopiedLen;
			iCopyLen = iLeftLen > BLOCKSIZE ? BLOCKSIZE : iLeftLen;

			if(pBuffer)
				m_pIdxObjMng->SetAttachObj(iObjNewIdx,0,pBuffer+iCopiedLen,iCopyLen);
			
			m_pBuffItem[iBuffSuffix].m_iBuffSize += iCopyLen;
		}		
	}
	else
	{
		ssize_t iLastObjIdxBak = m_pBuffItem[iBuffSuffix].m_iLastBuffIdx;
		ssize_t iBlockDataLen = (m_pBuffItem[iBuffSuffix].m_iBuffSize+m_pBuffItem[iBuffSuffix].m_iBuffOffset)%BLOCKSIZE;
		ssize_t iBlockDataLeft = BLOCKSIZE - iBlockDataLen;

		ssize_t iLeftLen = iBuffSize;
		//最后块拷贝
		if (iBlockDataLen > 0)
		{
			ssize_t iCopyLen = iLeftLen > iBlockDataLeft ? iBlockDataLeft : iLeftLen;

			if(pBuffer)
				m_pIdxObjMng->SetAttachObj(m_pBuffItem[iBuffSuffix].m_iLastBuffIdx,iBlockDataLen,pBuffer,iCopyLen);
			iLeftLen -= iCopyLen;		
		}

		while(iLeftLen > 0)
		{
			ssize_t iObjNewIdx = m_pIdxObjMng->CreateObject();
			if (iObjNewIdx < 0)
			{
				printf("ERR:CBuffMng::%s Create IdxObjMng Object failed![%s:%d]\n",
					__FUNCTION__,__FILE__,__LINE__);
				ssize_t iLastObjIdx = m_pIdxObjMng->GetDsIdx(iLastObjIdxBak,m_pBuffMngHead->m_iDSSuffix);
				FreeObjList(iLastObjIdx);
				m_pIdxObjMng->SetDsIdx(iLastObjIdxBak,m_pBuffMngHead->m_iDSSuffix,-1);
				return -4;
			}
			m_pIdxObjMng->SetDsIdx(m_pBuffItem[iBuffSuffix].m_iLastBuffIdx,m_pBuffMngHead->m_iDSSuffix,iObjNewIdx);
			m_pBuffItem[iBuffSuffix].m_iLastBuffIdx = iObjNewIdx;
			
			ssize_t iCopiedLen = iBuffSize-iLeftLen;
			ssize_t iCopyLen = iLeftLen > BLOCKSIZE ? BLOCKSIZE : iLeftLen;

			if(pBuffer)
				m_pIdxObjMng->SetAttachObj(iObjNewIdx,0,pBuffer+iCopiedLen,iCopyLen);
			
			iLeftLen -= iCopyLen;
		}

		m_pBuffItem[iBuffSuffix].m_iBuffSize += iBuffSize;
	}
	return 0;
}
//越过并释放数据
ssize_t CBuffMng::Skip(ssize_t iBuffSuffix, ssize_t iSkipLen)
{
	if (!m_pBuffMngHead ||iBuffSuffix<0 || iBuffSuffix>=m_pBuffMngHead->m_iBuffNum || iSkipLen<=0)
		return -1;

	if (m_pBuffItem[iBuffSuffix].m_iBuffIdx < 0)
		return -2;

	const ssize_t BLOCKSIZE = m_pIdxObjMng->GetObjSize();
	ssize_t iSkipBlockNum = (ssize_t)((iSkipLen+m_pBuffItem[iBuffSuffix].m_iBuffOffset)/BLOCKSIZE);
	ssize_t iSkipLenInBlock = (iSkipLen+m_pBuffItem[iBuffSuffix].m_iBuffOffset)%BLOCKSIZE;

	//越过块
	for (ssize_t i=0; i<iSkipBlockNum; i++)
	{
		if (m_pBuffItem[iBuffSuffix].m_iBuffIdx < 0)
		{
			break;
		}
		ssize_t iNextFreeIdx = m_pIdxObjMng->GetDsIdx(m_pBuffItem[iBuffSuffix].m_iBuffIdx,m_pBuffMngHead->m_iDSSuffix);
		m_pIdxObjMng->DestroyObject(m_pBuffItem[iBuffSuffix].m_iBuffIdx);
		m_pBuffItem[iBuffSuffix].m_iBuffIdx = iNextFreeIdx;	
	}

	//回收
	if (m_pBuffItem[iBuffSuffix].m_iBuffIdx < 0)
	{		
		m_pBuffItem[iBuffSuffix].m_iLastBuffIdx = -1;
		m_pBuffItem[iBuffSuffix].m_iBuffSize = 0;
		m_pBuffItem[iBuffSuffix].m_iBuffOffset = 0;	

		m_pBuffMngHead->m_iFreeBuffNum++;	
		return 0;
	}
	
	//越过块内字节
	m_pBuffItem[iBuffSuffix].m_iBuffOffset = iSkipLenInBlock;
	m_pBuffItem[iBuffSuffix].m_iBuffSize -= iSkipLen;
	return 0;
}
//直接将数据发送到socket，同时清除已发送数据
ssize_t CBuffMng::SendBufferToSocket(ssize_t iSocketFD,ssize_t iBuffSuffix)
{
	if (!m_pBuffMngHead ||iSocketFD<0 || iBuffSuffix<0 || iBuffSuffix>=m_pBuffMngHead->m_iBuffNum)
		return -1;

	const ssize_t BLOCKSIZE = m_pIdxObjMng->GetObjSize();
	ssize_t iCurrIdx = m_pBuffItem[iBuffSuffix].m_iBuffIdx;
	if (iCurrIdx < 0)
		return 0;

	ssize_t iNextIdx = -1;
	ssize_t iBytesSend = 0;
	ssize_t iSendedLen = 0;	

	//第一块
	ssize_t iSendLen = m_pBuffItem[iBuffSuffix].m_iBuffSize > (BLOCKSIZE-m_pBuffItem[iBuffSuffix].m_iBuffOffset) ?
					(BLOCKSIZE-m_pBuffItem[iBuffSuffix].m_iBuffOffset) : m_pBuffItem[iBuffSuffix].m_iBuffSize;
	if(iSendLen>0)
	{
		iBytesSend = send(iSocketFD,m_pIdxObjMng->GetAttachObj(iCurrIdx)+m_pBuffItem[iBuffSuffix].m_iBuffOffset,iSendLen,0);
		if (iBytesSend<=0)
		{
			return iBytesSend;
		}
		else if (iBytesSend<iSendLen)
		{
			m_pBuffItem[iBuffSuffix].m_iBuffOffset += iBytesSend;
			m_pBuffItem[iBuffSuffix].m_iBuffSize -= iBytesSend;
			return iBytesSend;
		}
		iSendedLen = iBytesSend;

		m_pBuffItem[iBuffSuffix].m_iBuffSize -= iSendedLen;
		iNextIdx = m_pIdxObjMng->GetDsIdx(iCurrIdx,m_pBuffMngHead->m_iDSSuffix);
		m_pIdxObjMng->DestroyObject(iCurrIdx);
		m_pBuffItem[iBuffSuffix].m_iBuffIdx = iNextIdx;	
		m_pBuffItem[iBuffSuffix].m_iBuffOffset = 0;
	}
	else
	{
		iNextIdx = m_pIdxObjMng->GetDsIdx(iCurrIdx,m_pBuffMngHead->m_iDSSuffix);
		m_pIdxObjMng->DestroyObject(iCurrIdx);
		m_pBuffItem[iBuffSuffix].m_iBuffIdx = iNextIdx;	
	}

	//后续
	iCurrIdx = m_pBuffItem[iBuffSuffix].m_iBuffIdx;
	while((m_pBuffItem[iBuffSuffix].m_iBuffSize > 0) && (iCurrIdx >= 0))
	{
		iSendLen = m_pBuffItem[iBuffSuffix].m_iBuffSize > BLOCKSIZE ? BLOCKSIZE : m_pBuffItem[iBuffSuffix].m_iBuffSize;
		iBytesSend = send(iSocketFD,m_pIdxObjMng->GetAttachObj(iCurrIdx),iSendLen,0);
		if (iBytesSend<=0)
		{
			break;
		}
		else if (iBytesSend<iSendLen)
		{
			m_pBuffItem[iBuffSuffix].m_iBuffOffset = iBytesSend;
			m_pBuffItem[iBuffSuffix].m_iBuffSize -= iBytesSend;
			iSendedLen += iBytesSend;
			break;
		}
		
		iNextIdx = m_pIdxObjMng->GetDsIdx(iCurrIdx,m_pBuffMngHead->m_iDSSuffix);
		m_pIdxObjMng->DestroyObject(iCurrIdx);
		m_pBuffItem[iBuffSuffix].m_iBuffIdx = iNextIdx;
		
		iCurrIdx = m_pBuffItem[iBuffSuffix].m_iBuffIdx;
		
		m_pBuffItem[iBuffSuffix].m_iBuffSize -= iSendLen;
		iSendedLen += iSendLen;
	}

	//回收
	if (m_pBuffItem[iBuffSuffix].m_iBuffIdx < 0)
	{
		m_pBuffItem[iBuffSuffix].m_iLastBuffIdx = -1;
		m_pBuffItem[iBuffSuffix].m_iBuffSize = 0;
		m_pBuffItem[iBuffSuffix].m_iBuffOffset = 0;	

		m_pBuffMngHead->m_iFreeBuffNum++;
	}
	return iSendedLen;
}
#endif

char* CBuffMng::GetFirstBlockPtr(ssize_t iBuffSuffix)
{
	if (!m_pBuffMngHead ||iBuffSuffix<0 || iBuffSuffix>=m_pBuffMngHead->m_iBuffNum)
	{
		return NULL;
	}
	
	ssize_t iCurrIdx = m_pBuffItem[iBuffSuffix].m_iBuffIdx;
	if (iCurrIdx < 0)
		return NULL;

	return m_pIdxObjMng->GetAttachObj(iCurrIdx);
}

//获取数据,返回长度,受限于BUFFSIZE
ssize_t CBuffMng::GetBuffer(ssize_t iBuffSuffix,char* pBuffer,const ssize_t BUFFSIZE)
{
	if (!m_pBuffMngHead ||iBuffSuffix<0 || iBuffSuffix>=m_pBuffMngHead->m_iBuffNum || !pBuffer || BUFFSIZE<=0)
	{
		//返回0长度
		return 0;
	}

	const ssize_t BLOCKSIZE = m_pIdxObjMng->GetObjSize();
	ssize_t iCurrIdx = m_pBuffItem[iBuffSuffix].m_iBuffIdx;
	if (iCurrIdx < 0)
		return 0;

	//第一块
#ifdef _BUFFMNG_APPEND_SKIP
	ssize_t iBuffOffset = m_pBuffItem[iBuffSuffix].m_iBuffOffset;
#else
	ssize_t iBuffOffset = 0;	
#endif

	ssize_t iCopyLen = m_pBuffItem[iBuffSuffix].m_iBuffSize > (BLOCKSIZE-iBuffOffset) ?
					(BLOCKSIZE-iBuffOffset) : m_pBuffItem[iBuffSuffix].m_iBuffSize;	

	//大于给定缓冲区
	if(BUFFSIZE <= iCopyLen)
	{
		m_pIdxObjMng->CopyAttachObj(iCurrIdx,iBuffOffset,pBuffer,BUFFSIZE);
		return BUFFSIZE;
	}
	m_pIdxObjMng->CopyAttachObj(iCurrIdx,iBuffOffset,pBuffer,iCopyLen);
	
	ssize_t iCopiedLen = iCopyLen;
	ssize_t iLeftLen = m_pBuffItem[iBuffSuffix].m_iBuffSize - iCopiedLen;
	
	iLeftLen = iLeftLen>(BUFFSIZE-iCopiedLen) ? (BUFFSIZE-iCopiedLen) : iLeftLen;

	//后续
	while(iLeftLen > 0)
	{
		iCurrIdx = m_pIdxObjMng->GetDsIdx(iCurrIdx,m_pBuffMngHead->m_iDSSuffix);
		if (iCurrIdx < 0)	
			break;
		
		iCopyLen = iLeftLen > BLOCKSIZE ? BLOCKSIZE : iLeftLen;
		m_pIdxObjMng->CopyAttachObj(iCurrIdx,0,pBuffer+iCopiedLen,iCopyLen);
		
		iLeftLen -= iCopyLen;
		iCopiedLen += iCopyLen;
	}

	return iCopiedLen;
}

//释放缓冲区
ssize_t CBuffMng::FreeBuffer(ssize_t iBuffSuffix)
{
	if (!m_pBuffMngHead ||iBuffSuffix<0 || iBuffSuffix>=m_pBuffMngHead->m_iBuffNum)
		return -1;

	ssize_t iFreeIdx = m_pBuffItem[iBuffSuffix].m_iBuffIdx;
	if (iFreeIdx < 0)
		return -2;
	
	while (iFreeIdx >= 0)
	{
		ssize_t iNextFreeIdx = m_pIdxObjMng->GetDsIdx(iFreeIdx,m_pBuffMngHead->m_iDSSuffix);
		m_pIdxObjMng->DestroyObject(iFreeIdx);
		iFreeIdx = iNextFreeIdx;
	}

	m_pBuffItem[iBuffSuffix].m_iBuffIdx = -1;
	m_pBuffItem[iBuffSuffix].m_iBuffSize = 0;
	
#ifdef _BUFFMNG_APPEND_SKIP		
	m_pBuffItem[iBuffSuffix].m_iBuffOffset = 0;
	m_pBuffItem[iBuffSuffix].m_iLastBuffIdx = -1;
#endif	
	
	m_pBuffMngHead->m_iFreeBuffNum++;
	return 0;
}

bool CBuffMng::HaveFreeSpace(ssize_t iSize)
{
	if(m_pBuffMngHead->m_iFreeBuffNum <= 0)
		return false;

	ssize_t iObjFree = m_pIdxObjMng->GetObjNum() - m_pIdxObjMng->GetUsedCount();
	ssize_t iFreeBytes = iObjFree*m_pIdxObjMng->GetObjSize();

	return (iFreeBytes >= iSize)?true:false;
}

//获得缓存数据长度
ssize_t CBuffMng::GetBufferSize(ssize_t iBuffSuffix)
{
	if (!m_pBuffMngHead ||iBuffSuffix<0 || iBuffSuffix>=m_pBuffMngHead->m_iBuffNum)
		return 0;
	
	if (m_pBuffItem[iBuffSuffix].m_iBuffIdx < 0)
		return 0;

	return m_pBuffItem[iBuffSuffix].m_iBuffSize;
}

//获取管理块大小
ssize_t CBuffMng::GetBlockSize()
{
	if (!m_pIdxObjMng)
		return 0;
	
	return m_pIdxObjMng->GetObjSize();
}

//总空间使用率
ssize_t CBuffMng::GetBufferUsage(ssize_t &iBufferUsed,ssize_t &iBufferCount,ssize_t &iObjUsed,ssize_t &iObjCount)
{
	if (!m_pIdxObjMng)
		return -1;

	//索引使用率
	iBufferUsed = m_pBuffMngHead->m_iBuffNum-m_pBuffMngHead->m_iFreeBuffNum;
	iBufferCount = m_pBuffMngHead->m_iBuffNum;

	//数据空间使用率
	iObjUsed = m_pIdxObjMng->GetUsedCount();
	iObjCount = m_pIdxObjMng->GetObjNum();
	
	return 0;
}

ssize_t CBuffMng::FreeObjList(ssize_t iObjIdx)
{
	if (!m_pIdxObjMng)
		return -1;
	
	const ssize_t BLOCKNUM = m_pIdxObjMng->GetObjNum();
	if (iObjIdx<0 || iObjIdx>=BLOCKNUM)
		return 0;

	ssize_t iCount = 0;
	ssize_t iFreeIdx = iObjIdx;
	while (iFreeIdx >= 0)
	{
		ssize_t iNextFreeIdx = m_pIdxObjMng->GetDsIdx(iFreeIdx,m_pBuffMngHead->m_iDSSuffix);
		m_pIdxObjMng->DestroyObject(iFreeIdx);
		iFreeIdx = iNextFreeIdx;
		iCount++;
	}
	return iCount;
}

ssize_t CBuffMng::Print(FILE *fpOut)
{
	if (!m_pBuffMngHead)
		return -1;
	
	for (ssize_t i=0; i<m_pBuffMngHead->m_iBuffNum; i++)
	{
		if(m_pBuffItem[i].m_iBuffIdx < 0)
		{
			continue;
		}
		fprintf(fpOut,"[BUFF%03lld SIZE %03lld]",(long long)i,(long long)m_pBuffItem[i].m_iBuffSize);

		ssize_t iCurrIdx = m_pBuffItem[i].m_iBuffIdx;
		while(iCurrIdx >= 0)
		{
			fprintf(fpOut,"->|%03lld|",(long long)iCurrIdx);
			iCurrIdx = m_pIdxObjMng->GetDsIdx(iCurrIdx,m_pBuffMngHead->m_iDSSuffix);
		}
		fprintf(fpOut,"\n");
	}

	m_pIdxObjMng->Print(fpOut);
	fprintf(fpOut, "\n");

	return 0;
}

//---------------------------------------------------------------------
CBlockMng::CBlockMng()
{
	m_pBlockMngHead = NULL;
	m_pBlockItem = NULL;
	m_pIdxObjMng = NULL;
	m_iInitType = emInit;
}

ssize_t CBlockMng::CountMemSize(ssize_t iItemNum)
{
	return sizeof(TBlockMngHead) + sizeof(TBlockItem)*iItemNum;
}

//绑定内存
ssize_t CBlockMng::AttachMem(char* pMemPtr,const ssize_t MEMSIZE,ssize_t iItemNum,ssize_t iInitType/*=emInit*/)
{
	if ((MEMSIZE < CountMemSize(iItemNum)) || !pMemPtr || (MEMSIZE<=0) ||(iItemNum<=0))
		return -1;

	m_pBlockMngHead = (TBlockMngHead*)pMemPtr;
	m_pBlockItem = (TBlockItem*)(pMemPtr+sizeof(TBlockMngHead));

	m_iInitType = iInitType;
	if (iInitType == emInit)
	{	
		memset(m_pBlockItem,-1,sizeof(TBlockItem)*iItemNum);
		for (ssize_t i=0; i<iItemNum; i++)
		{
			m_pBlockItem[i].m_iBlockNum = 0;		
		}
		
		m_pBlockMngHead->m_iItemNum = iItemNum;
		m_pBlockMngHead->m_iFreeItemNum = iItemNum;
		m_pBlockMngHead->m_iDSSuffix = -1;
	}
	else
	{
		if (m_pBlockMngHead->m_iItemNum != iItemNum)
			return -2;
	}
	
	return CountMemSize(iItemNum);
}


//绑定对象管理器
ssize_t CBlockMng::AttachIdxObjMng(TIdxObjMng *pIdxObjMng)
{
	if (!m_pBlockMngHead || !pIdxObjMng)
		return -1;

	m_pIdxObjMng = pIdxObjMng;

	ssize_t iDSSuffix = m_pIdxObjMng->GetOneFreeDS();
	if (iDSSuffix < 0)
		return -2;

	if (m_iInitType == emInit)
	{
		m_pBlockMngHead->m_iDSSuffix = iDSSuffix;	
	}
	else
	{
		if (m_pBlockMngHead->m_iDSSuffix != iDSSuffix)
			return -3;
	}
	
	return 0;
}

//写入数据
ssize_t CBlockMng::AppendBlock(ssize_t iItemSuffix,char* pBlockData)
{
	if (!m_pBlockMngHead ||iItemSuffix<0 || iItemSuffix>=m_pBlockMngHead->m_iItemNum ||
			!pBlockData ||!m_pIdxObjMng)
	{
		return -1;
	}
	
#ifdef _BLOCKMNG_APPEND_SKIP	
	ssize_t iLastBlockIdx = m_pBlockItem[iItemSuffix].m_iLastBlockIdx;
#else
	ssize_t iLastBlockIdx = m_pBlockItem[iItemSuffix].m_iBlockIdx;
	while(iLastBlockIdx >= 0)
	{
		ssize_t iNextBlockIdx = m_pIdxObjMng->GetDsIdx(iLastBlockIdx,m_pBlockMngHead->m_iDSSuffix);	
		if(iNextBlockIdx < 0 )
			break;

		iLastBlockIdx = iNextBlockIdx;
	}
	
#endif

	const ssize_t BLOCKSIZE = m_pIdxObjMng->GetObjSize();
	
	//新分配
	if (m_pBlockItem[iItemSuffix].m_iBlockIdx < 0)
	{
		//第一个
		ssize_t iFirstObjIdx = m_pIdxObjMng->CreateObject();
		if (iFirstObjIdx < 0)
		{
			printf("ERR:CBlockMng::%s Create IdxObjMng Object failed![%s:%d]\n",
				__FUNCTION__,__FILE__,__LINE__);
			return -2;
		}
		m_pBlockItem[iItemSuffix].m_iBlockIdx = iFirstObjIdx;
#ifdef _BLOCKMNG_APPEND_SKIP	
		m_pBlockItem[iItemSuffix].m_iLastBlockIdx = iFirstObjIdx;	
#endif
		m_pBlockItem[iItemSuffix].m_iBlockNum = 1;
		memcpy(m_pIdxObjMng->GetAttachObj(iFirstObjIdx),pBlockData,BLOCKSIZE);	
		m_pBlockMngHead->m_iFreeItemNum--;
	}
	else
	{
		ssize_t iObjNewIdx = m_pIdxObjMng->CreateObject();
		if (iObjNewIdx < 0)
		{
			printf("ERR:CBlockMng::%s Create IdxObjMng Object failed![%s:%d]\n",
				__FUNCTION__,__FILE__,__LINE__);
			return -3;
		}
		m_pIdxObjMng->SetDsIdx(iLastBlockIdx,m_pBlockMngHead->m_iDSSuffix,iObjNewIdx);
#ifdef _BLOCKMNG_APPEND_SKIP		
		m_pBlockItem[iItemSuffix].m_iLastBlockIdx = iObjNewIdx;
#endif
		memcpy(m_pIdxObjMng->GetAttachObj(iObjNewIdx),pBlockData,BLOCKSIZE);
		m_pBlockItem[iItemSuffix].m_iBlockNum++;
	}

	return 0;
}

ssize_t CBlockMng::InsertBlock(ssize_t iItemSuffix,char* pBlockData,ssize_t iInsertPos)
{
	if (!m_pBlockMngHead ||iItemSuffix<0 || iItemSuffix>=m_pBlockMngHead->m_iItemNum ||
			!pBlockData ||!m_pIdxObjMng)
	{
		return -1;
	}

	const ssize_t BLOCKSIZE = m_pIdxObjMng->GetObjSize();

	//尾插
	if ((m_pBlockItem[iItemSuffix].m_iBlockIdx < 0) || 
		(iInsertPos > (m_pBlockItem[iItemSuffix].m_iBlockNum-1)))
	{
		return AppendBlock(iItemSuffix,pBlockData);
	}

	//头插
	if (iInsertPos == 0)
	{
		ssize_t iObjNewIdx = m_pIdxObjMng->CreateObject();
		if (iObjNewIdx < 0)
		{
			printf("ERR:CBlockMng::%s Create IdxObjMng Object failed![%s:%d]\n",
				__FUNCTION__,__FILE__,__LINE__);
			return -4;
		}
		memcpy(m_pIdxObjMng->GetAttachObj(iObjNewIdx),pBlockData,BLOCKSIZE);
		m_pIdxObjMng->SetDsIdx(iObjNewIdx,m_pBlockMngHead->m_iDSSuffix,
												m_pBlockItem[iItemSuffix].m_iBlockIdx);
		m_pBlockItem[iItemSuffix].m_iBlockIdx = iObjNewIdx;
		m_pBlockItem[iItemSuffix].m_iBlockNum++;
		return 0;
	}

	//中间插
	ssize_t iCurrBlockIdx = m_pBlockItem[iItemSuffix].m_iBlockIdx;
	for (ssize_t i=0; i<iInsertPos-1; i++)
	{
		iCurrBlockIdx = m_pIdxObjMng->GetDsIdx(iCurrBlockIdx,m_pBlockMngHead->m_iDSSuffix);
		if (iCurrBlockIdx == -1)
		{
			printf("ERR:CBlockMng::%s Link crashed![%s:%d]\n",
				__FUNCTION__,__FILE__,__LINE__);
			return -5;
		}
	}

	ssize_t iObjNewIdx = m_pIdxObjMng->CreateObject();
	if (iObjNewIdx < 0)
	{
		printf("ERR:CBlockMng::%s Create IdxObjMng Object failed![%s:%d]\n",
			__FUNCTION__,__FILE__,__LINE__);
		return -5;
	}
	memcpy(m_pIdxObjMng->GetAttachObj(iObjNewIdx),pBlockData,BLOCKSIZE);

	ssize_t iCurrNextIdx = m_pIdxObjMng->GetDsIdx(iCurrBlockIdx,m_pBlockMngHead->m_iDSSuffix);
	m_pIdxObjMng->SetDsIdx(iObjNewIdx,m_pBlockMngHead->m_iDSSuffix,iCurrNextIdx);
	m_pIdxObjMng->SetDsIdx(iCurrBlockIdx,m_pBlockMngHead->m_iDSSuffix,iObjNewIdx);
	m_pBlockItem[iItemSuffix].m_iBlockNum++;
	return 0;
}

//释放缓冲区
ssize_t CBlockMng::FreeBlock(ssize_t iItemSuffix)
{
	if (!m_pBlockMngHead ||iItemSuffix<0 || iItemSuffix>=m_pBlockMngHead->m_iItemNum)
		return -1;

	if (m_pBlockItem[iItemSuffix].m_iBlockIdx < 0)
		return -2;

	FreeObjList(m_pBlockItem[iItemSuffix].m_iBlockIdx);

	m_pBlockItem[iItemSuffix].m_iBlockIdx = -1;
	m_pBlockItem[iItemSuffix].m_iBlockNum = 0;

#ifdef _BLOCKMNG_APPEND_SKIP
	m_pBlockItem[iItemSuffix].m_iLastBlockIdx = -1;
#endif
	
	m_pBlockMngHead->m_iFreeItemNum++;
	return 0;
}

ssize_t CBlockMng::GetBlockNum(ssize_t iItemSuffix)
{
	if (!m_pBlockMngHead ||iItemSuffix<0 || iItemSuffix>=m_pBlockMngHead->m_iItemNum)
		return 0;
	
	if (m_pBlockItem[iItemSuffix].m_iBlockIdx < 0)
	{
		m_pBlockItem[iItemSuffix].m_iBlockNum = 0;
		return 0;
	}
	return m_pBlockItem[iItemSuffix].m_iBlockNum;
}

char* CBlockMng::GetBlockData(ssize_t iItemSuffix,ssize_t iPos)
{
	if(!m_pBlockMngHead)
		return NULL;
	
	if (iPos<0 ||(m_pBlockItem[iItemSuffix].m_iBlockIdx < 0) || 
		(iPos > (m_pBlockItem[iItemSuffix].m_iBlockNum-1)))
	{
		return NULL;
	}

	ssize_t iCurrBlockIdx = m_pBlockItem[iItemSuffix].m_iBlockIdx;
	for (ssize_t i=0; i<iPos; i++)
	{
		iCurrBlockIdx = m_pIdxObjMng->GetDsIdx(iCurrBlockIdx,m_pBlockMngHead->m_iDSSuffix);
		if (iCurrBlockIdx == -1)
		{
			printf("ERR:CBlockMng::%s Link crashed![%s:%d]\n",__FUNCTION__,__FILE__,__LINE__);
			return NULL;
		}
	}	

	return m_pIdxObjMng->GetAttachObj(iCurrBlockIdx);
}

char* CBlockMng::GetFirstBlockData(ssize_t iItemSuffix)
{
	if(!m_pBlockMngHead)
		return NULL;
	
	if (m_pBlockItem[iItemSuffix].m_iBlockIdx < 0)
		return NULL;

	ssize_t iCurrBlockIdx = m_pBlockItem[iItemSuffix].m_iBlockIdx;
	return m_pIdxObjMng->GetAttachObj(iCurrBlockIdx);
}

//通过指针位置偏移计算索引位置
char* CBlockMng::GetNextBlockData(char* pBlockData)
{
	if(!m_pBlockMngHead)
		return NULL;
	
	if (!pBlockData)
		return NULL;

	char* pBaseMemPtr = m_pIdxObjMng->GetObjBaseMem();
	ssize_t iSize = pBlockData - pBaseMemPtr;
	ssize_t iCurrIdx = iSize/m_pIdxObjMng->GetObjSize();

	if (m_pIdxObjMng->GetAttachObj(iCurrIdx) != pBlockData)
		return NULL;

	//指向下一个
	ssize_t iNextHashIdx = m_pIdxObjMng->GetDsIdx(iCurrIdx,m_pBlockMngHead->m_iDSSuffix);	
	return m_pIdxObjMng->GetAttachObj(iNextHashIdx);
}

ssize_t CBlockMng::GetBlockPos(ssize_t iItemSuffix,char* pBlockData)
{
	if(!m_pBlockMngHead)
		return -1;
	
	if (!pBlockData)
		return -2;
	
	if (iItemSuffix<0 || iItemSuffix>=m_pBlockMngHead->m_iItemNum)
		return -3;
	
	if (m_pBlockItem[iItemSuffix].m_iBlockIdx < 0)
		return -4;
	
	char* pBaseMemPtr = m_pIdxObjMng->GetObjBaseMem();
	ssize_t iSize = pBlockData - pBaseMemPtr;
	ssize_t iCurrIdx = iSize/m_pIdxObjMng->GetObjSize();

	if (m_pIdxObjMng->GetAttachObj(iCurrIdx) != pBlockData)
		return -5;

	ssize_t iPos = 0;
	ssize_t iCurrBlockIdx = m_pBlockItem[iItemSuffix].m_iBlockIdx;
	while((iCurrBlockIdx >= 0) && (iCurrBlockIdx != iCurrIdx))
	{
		iCurrBlockIdx = m_pIdxObjMng->GetDsIdx(iCurrBlockIdx,m_pBlockMngHead->m_iDSSuffix);
		iPos++;
	}	

	if(iCurrBlockIdx == iCurrIdx)
		return iPos;
	
	return -6;
}

//返回选出字节数
ssize_t CBlockMng::SelectBlock(ssize_t iItemSuffix,char* pBuffer,const ssize_t iBUFFSIZE,BLOCK_SELECT_FUNC fBlockSelectFunc/*=NULL*/,void* pSelectArg/*=NULL*/,ssize_t iStartBlockPos/*=0*/,ssize_t iBlockNum/*=-1*/)
{
	if (!m_pBlockMngHead ||iItemSuffix<0 || iItemSuffix>=m_pBlockMngHead->m_iItemNum)
		return 0;
	
	if (m_pBlockItem[iItemSuffix].m_iBlockIdx < 0)
		return -2;

	const ssize_t BLOCKSIZE = m_pIdxObjMng->GetObjSize();

	ssize_t iCurrBlockIdx = m_pBlockItem[iItemSuffix].m_iBlockIdx;
	for (ssize_t i=0; i<iStartBlockPos; i++)
	{
		iCurrBlockIdx = m_pIdxObjMng->GetDsIdx(iCurrBlockIdx,m_pBlockMngHead->m_iDSSuffix);
		if (iCurrBlockIdx == -1)
		{
			return -3;
		}
	}

	if (iBlockNum < 0)
	{
		iBlockNum = m_pBlockItem[iItemSuffix].m_iBlockNum;
	}
	
	ssize_t iOutBufferSize = 0;
	for (ssize_t i=0; i<iBlockNum; i++)
	{
		char *pCurrBlock = m_pIdxObjMng->GetAttachObj(iCurrBlockIdx);
		if (!pCurrBlock)
		{
			return iOutBufferSize;
		}
		
		if(!fBlockSelectFunc ||(fBlockSelectFunc(pCurrBlock,pSelectArg) ==0))
		{
			if (iOutBufferSize+BLOCKSIZE > iBUFFSIZE)
			{
				return -4;
			}	
			
			memcpy(pBuffer+iOutBufferSize,pCurrBlock,BLOCKSIZE);
			iOutBufferSize+=BLOCKSIZE;
		}
		iCurrBlockIdx = m_pIdxObjMng->GetDsIdx(iCurrBlockIdx,m_pBlockMngHead->m_iDSSuffix);	
	}

	return iOutBufferSize;
}

//返回pos位置
ssize_t CBlockMng::FindBlock(ssize_t iItemSuffix,BLOCK_FIND_EQU_FUNC fFindEquFunc,void* pFindArg/*=NULL*/)
{
	if (!m_pBlockMngHead ||iItemSuffix<0 || iItemSuffix>=m_pBlockMngHead->m_iItemNum ||!m_pIdxObjMng)
		return -1;

	if (m_pBlockItem[iItemSuffix].m_iBlockIdx < 0)
		return -2;

	ssize_t iPos = 0;
	ssize_t iCurrBlockIdx = m_pBlockItem[iItemSuffix].m_iBlockIdx;
	while(iCurrBlockIdx >= 0)
	{
		char *pCurrBlock = m_pIdxObjMng->GetAttachObj(iCurrBlockIdx);
		if(fFindEquFunc(pCurrBlock,pFindArg) ==0)
		{
			return iPos;
		}
		iCurrBlockIdx = m_pIdxObjMng->GetDsIdx(iCurrBlockIdx,m_pBlockMngHead->m_iDSSuffix);
		iPos++;
	}

	return -3;
}

//条件删除,返回删除的块数
ssize_t CBlockMng::DeleteBlock(ssize_t iItemSuffix,BLOCK_DEL_EQU_FUNC fDelEquFunc,void* pDeleteArg/*=NULL*/)
{
	if (!m_pBlockMngHead ||iItemSuffix<0 || iItemSuffix>=m_pBlockMngHead->m_iItemNum ||!m_pIdxObjMng)
		return -1;

	if (m_pBlockItem[iItemSuffix].m_iBlockIdx < 0)
		return -2;

	ssize_t iDelCount = 0;

	//第二块开始
	ssize_t iPos = 1;
	ssize_t iCurrBlockIdx = m_pBlockItem[iItemSuffix].m_iBlockIdx;
	while(1)
	{
		ssize_t iCheckIdx = m_pIdxObjMng->GetDsIdx(iCurrBlockIdx,m_pBlockMngHead->m_iDSSuffix);
		if (iCheckIdx < 0)
		{
			break;
		}
		
		char *pCheckBlock = m_pIdxObjMng->GetAttachObj(iCheckIdx);
		if(fDelEquFunc(pCheckBlock,pDeleteArg) ==0)
		{
			ssize_t iNextIdx = m_pIdxObjMng->GetDsIdx(iCheckIdx,m_pBlockMngHead->m_iDSSuffix);
			//最后一个了
			if (iNextIdx < 0)
			{
#ifdef _BLOCKMNG_APPEND_SKIP			
				m_pBlockItem[iItemSuffix].m_iLastBlockIdx = iCurrBlockIdx;
#endif
			}
			m_pIdxObjMng->SetDsIdx(iCurrBlockIdx,m_pBlockMngHead->m_iDSSuffix,iNextIdx);	
			m_pIdxObjMng->DestroyObject(iCheckIdx);
			m_pBlockItem[iItemSuffix].m_iBlockNum--;
			iDelCount++;
			continue;
		}
		iCurrBlockIdx = m_pIdxObjMng->GetDsIdx(iCurrBlockIdx,m_pBlockMngHead->m_iDSSuffix);
		iPos++;
	}

	//第一个块
	iCurrBlockIdx = m_pBlockItem[iItemSuffix].m_iBlockIdx;
	if(fDelEquFunc(m_pIdxObjMng->GetAttachObj(iCurrBlockIdx),pDeleteArg) ==0)
	{
		ssize_t iNextIdx = m_pIdxObjMng->GetDsIdx(iCurrBlockIdx,m_pBlockMngHead->m_iDSSuffix);
		//最后一个了
		if (iNextIdx < 0)
		{
#ifdef _BLOCKMNG_APPEND_SKIP		
			m_pBlockItem[iItemSuffix].m_iLastBlockIdx = -1;
#endif
		}		
		m_pBlockItem[iItemSuffix].m_iBlockIdx = iNextIdx;
		m_pIdxObjMng->DestroyObject(iCurrBlockIdx);
		m_pBlockItem[iItemSuffix].m_iBlockNum--;
		iDelCount++;
	}
	return iDelCount;
}

ssize_t CBlockMng::DeleteBlock(ssize_t iItemSuffix,ssize_t iPos)
{
	if (!m_pBlockMngHead ||iPos<0 || iItemSuffix<0 ||
		iItemSuffix>=m_pBlockMngHead->m_iItemNum ||!m_pIdxObjMng)
	{
		return -1;
	}

	//无块
	if ((m_pBlockItem[iItemSuffix].m_iBlockIdx < 0) || 
		(iPos > (m_pBlockItem[iItemSuffix].m_iBlockNum-1)))
	{
		return -2;
	}	

	//头删
	if (iPos == 0)
	{
		if (m_pBlockItem[iItemSuffix].m_iBlockNum == 1)
		{
			return FreeBlock(iItemSuffix);
		}
		
		ssize_t iNextIdx = m_pIdxObjMng->GetDsIdx(m_pBlockItem[iItemSuffix].m_iBlockIdx,m_pBlockMngHead->m_iDSSuffix);	
		m_pIdxObjMng->DestroyObject(m_pBlockItem[iItemSuffix].m_iBlockIdx);
		m_pBlockItem[iItemSuffix].m_iBlockIdx = iNextIdx;
		m_pBlockItem[iItemSuffix].m_iBlockNum--;
		return 0;
	}
	
	//尾删
	if (iPos == (m_pBlockItem[iItemSuffix].m_iBlockNum-1))
		return TrimTail(iItemSuffix,m_pBlockItem[iItemSuffix].m_iBlockNum-1);

	//中删
	ssize_t iCurrBlockIdx = m_pBlockItem[iItemSuffix].m_iBlockIdx;
	for (ssize_t i=0; i<iPos-1; i++)
	{
		iCurrBlockIdx = m_pIdxObjMng->GetDsIdx(iCurrBlockIdx,m_pBlockMngHead->m_iDSSuffix);
		if (iCurrBlockIdx < 0)
		{
			printf("ERR:CBlockMng::%s Link crashed![%s:%d]\n",__FUNCTION__,__FILE__,__LINE__);
			return -3;
		}
	}

	ssize_t iDeleteIdx = m_pIdxObjMng->GetDsIdx(iCurrBlockIdx,m_pBlockMngHead->m_iDSSuffix);
	if (iDeleteIdx < 0)
		return -4;
	
	ssize_t iNextIdx = m_pIdxObjMng->GetDsIdx(iDeleteIdx,m_pBlockMngHead->m_iDSSuffix);
	m_pIdxObjMng->SetDsIdx(iCurrBlockIdx,m_pBlockMngHead->m_iDSSuffix,iNextIdx);
	m_pIdxObjMng->DestroyObject(iDeleteIdx);
	m_pBlockItem[iItemSuffix].m_iBlockNum--;
	return 0;
}

//保留iBlockNum个节点,之后的都删除
ssize_t CBlockMng::TrimTail(ssize_t iItemSuffix,ssize_t iBlockNum)
{
	if (!m_pBlockMngHead ||iItemSuffix<0 || 
				iItemSuffix>=m_pBlockMngHead->m_iItemNum ||!m_pIdxObjMng)
		return -1;
	
	if (m_pBlockItem[iItemSuffix].m_iBlockIdx < 0)
		return -2;

	//全删
	if (iBlockNum <= 0)
		return FreeBlock(iItemSuffix);

	//不删
	if (iBlockNum >= m_pBlockItem[iItemSuffix].m_iBlockNum)
		return 0;

	//删
	ssize_t iCurrBlockIdx = m_pBlockItem[iItemSuffix].m_iBlockIdx;
	for (ssize_t i=0; i<iBlockNum-1; i++)
	{
		iCurrBlockIdx = m_pIdxObjMng->GetDsIdx(iCurrBlockIdx,m_pBlockMngHead->m_iDSSuffix);
		if (iCurrBlockIdx < 0)
		{
			printf("ERR:CBlockMng::%s Link crashed![%s:%d]\n",__FUNCTION__,__FILE__,__LINE__);
			return -3;
		}
	}	

	ssize_t iNextIdx = m_pIdxObjMng->GetDsIdx(iCurrBlockIdx,m_pBlockMngHead->m_iDSSuffix);
	if (iNextIdx < 0)
	{
		printf("ERR:CBlockMng::%s Link crashed![%s:%d]\n",__FUNCTION__,__FILE__,__LINE__);
		return -4;
	}
	
	ssize_t iFreeNum = FreeObjList(iNextIdx);
	m_pIdxObjMng->SetDsIdx(iCurrBlockIdx,m_pBlockMngHead->m_iDSSuffix,-1);
#ifdef _BLOCKMNG_APPEND_SKIP	
	m_pBlockItem[iItemSuffix].m_iLastBlockIdx = iCurrBlockIdx;
#endif
	m_pBlockItem[iItemSuffix].m_iBlockNum -= iFreeNum;
	return 0;
}

//获取管理块大小
ssize_t CBlockMng::GetBlockObjSize()
{
	if (!m_pIdxObjMng)
		return 0;
	
	return m_pIdxObjMng->GetObjSize();
}

//总空间使用率
ssize_t CBlockMng::GetUsage(ssize_t &iItemUsed,ssize_t &iItemCount,ssize_t &iBlockUsed,ssize_t &iBlockCount)
{
	if (!m_pIdxObjMng)
		return -1;

	//索引使用率
	iItemUsed = m_pBlockMngHead->m_iItemNum-m_pBlockMngHead->m_iFreeItemNum;
	iItemCount = m_pBlockMngHead->m_iItemNum;

	//数据空间使用率
	iBlockUsed = m_pIdxObjMng->GetUsedCount();
	iBlockCount = m_pIdxObjMng->GetObjNum();
	
	return 0;
}

ssize_t CBlockMng::FreeObjList(ssize_t iObjIdx)
{
	if (!m_pIdxObjMng)
		return -1;
	
	const ssize_t BLOCKNUM = m_pIdxObjMng->GetObjNum();
	if (iObjIdx<0 || iObjIdx>=BLOCKNUM)
	{
		return 0;
	}

	ssize_t iCount = 0;
	ssize_t iFreeIdx = iObjIdx;
	while (iFreeIdx >= 0)
	{
		ssize_t iNextFreeIdx = m_pIdxObjMng->GetDsIdx(iFreeIdx,m_pBlockMngHead->m_iDSSuffix);
		m_pIdxObjMng->DestroyObject(iFreeIdx);
		iFreeIdx = iNextFreeIdx;
		iCount++;
	}
	return iCount;
}

ssize_t CBlockMng::Print(FILE *fpOut)
{
	if (!m_pBlockMngHead)
		return -1;
	
	for (ssize_t i=0; i<m_pBlockMngHead->m_iItemNum; i++)
	{
		if(m_pBlockItem[i].m_iBlockIdx < 0)
		{
			continue;
		}
		fprintf(fpOut,"<%03lld>|%03lld(%03lld)|",
			(long long)i,(long long)m_pBlockItem[i].m_iBlockIdx,(long long)m_pBlockItem[i].m_iBlockNum);

		ssize_t iCurrIdx = m_pBlockItem[i].m_iBlockIdx;
		while(iCurrIdx >= 0)
		{
			fprintf(fpOut,"->>|%03lld|",(long long)iCurrIdx);
			iCurrIdx = m_pIdxObjMng->GetDsIdx(iCurrIdx,m_pBlockMngHead->m_iDSSuffix);
		}
		fprintf(fpOut,"\n");
	}

	return 0;
}
//---------------------------------------------------------------------

CTree::CTree()
{
	m_pTreeRoot = NULL;
	m_pIdxObjMng = NULL;
	m_iInitType = emInit;
	m_piNodeIdxArray = NULL;
}

CTree::~CTree()
{
	if (m_piNodeIdxArray)
		delete []m_piNodeIdxArray;
}

ssize_t CTree::CountMemSize()
{
	return (ssize_t)sizeof(TTreeRoot);
}

ssize_t CTree::AttachMem(char* pMemPtr,const ssize_t MEMSIZE,ssize_t iInitType/*=emInit*/)
{
	if ((MEMSIZE < CountMemSize()) || !pMemPtr || (MEMSIZE<=0))
	{
		return -1;
	}

	m_pTreeRoot = (TTreeRoot*)pMemPtr;

	m_iInitType = iInitType;
	if (iInitType == emInit)
	{
		memset(m_pTreeRoot,-1,sizeof(TTreeRoot));
	}
	
	return CountMemSize();	
}

//绑定对象管理器
ssize_t CTree::AttachIdxObjMng(TIdxObjMng *pIdxObjMng)
{
	if (!m_pTreeRoot ||!pIdxObjMng)
	{
		return -1;
	}

	m_pIdxObjMng = pIdxObjMng;

	ssize_t iDSSuffix1 = m_pIdxObjMng->GetOneFreeDS();
	if (iDSSuffix1 < 0)	return -2;
	ssize_t iDSSuffix2 = m_pIdxObjMng->GetOneFreeDS();
	if (iDSSuffix2 < 0)	return -3;
	ssize_t iDSSuffix3 = m_pIdxObjMng->GetOneFreeDS();
	if (iDSSuffix3 < 0)	return -4;
	ssize_t iDSSuffix4 = m_pIdxObjMng->GetOneFreeDS();
	if (iDSSuffix4 < 0)	return -5;	
	
	if (m_iInitType == emInit)
	{
		m_pTreeRoot->m_iRootIdx = -1;
		
		m_pTreeRoot->m_iChildDS = iDSSuffix1;
		m_pTreeRoot->m_iPrevBrotherDS = iDSSuffix2;
		m_pTreeRoot->m_iNextBrotherDS = iDSSuffix3;
		m_pTreeRoot->m_iParentDS = iDSSuffix4;
	}
	else
	{
		if((m_pTreeRoot->m_iChildDS != iDSSuffix1) ||
			(m_pTreeRoot->m_iPrevBrotherDS != iDSSuffix2) ||
			(m_pTreeRoot->m_iNextBrotherDS != iDSSuffix3) ||
			(m_pTreeRoot->m_iParentDS != iDSSuffix3))
		{
			return -6;
		}
	}

	m_piNodeIdxArray = new ssize_t[pIdxObjMng->GetObjNum()];
	return 0;
}

ssize_t CTree::BuildRoot()
{
	if (!m_pTreeRoot ||(m_pTreeRoot->m_iRootIdx>=0))
	{
		return -1;
	}

	m_pTreeRoot->m_iRootIdx = BuildNode();
	return m_pTreeRoot->m_iRootIdx;
}

ssize_t CTree::GetRoot()
{
	return m_pTreeRoot->m_iRootIdx;
}

ssize_t CTree::BuildNode()
{
	ssize_t iNodeObjIdx = m_pIdxObjMng->CreateObject();
	if (iNodeObjIdx < 0)
	{
		printf("ERR:CTree::%s Create IdxObjMng Object failed![%s:%d]\n",
			__FUNCTION__,__FILE__,__LINE__);
		return -1;
	}
	
	//指针初始化
	m_pIdxObjMng->SetDsIdx(iNodeObjIdx,m_pTreeRoot->m_iChildDS,-1);
	m_pIdxObjMng->SetDsIdx(iNodeObjIdx,m_pTreeRoot->m_iPrevBrotherDS,-1);
	m_pIdxObjMng->SetDsIdx(iNodeObjIdx,m_pTreeRoot->m_iNextBrotherDS,-1);
	m_pIdxObjMng->SetDsIdx(iNodeObjIdx,m_pTreeRoot->m_iParentDS,-1);
	return iNodeObjIdx;
}

char* CTree::GetNode(ssize_t iNodeObjIdx)
{
	if (iNodeObjIdx<0 || iNodeObjIdx>=(ssize_t)m_pIdxObjMng->GetObjNum())
	{
		return NULL;
	}
	return m_pIdxObjMng->GetAttachObj(iNodeObjIdx);
}

ssize_t CTree::InsertChild(ssize_t iNodeObjIdx,ssize_t iChildObjIdx, ssize_t iOrderPos/*=0*/)
{
	if (iNodeObjIdx<0 || iNodeObjIdx>=(ssize_t)m_pIdxObjMng->GetObjNum() ||
		iChildObjIdx<0 || iChildObjIdx>=(ssize_t)m_pIdxObjMng->GetObjNum())
	{
		return -1;
	}

	//没有孩子或插入第一个孩子位置
	ssize_t iCurrChildIdx = m_pIdxObjMng->GetDsIdx(iNodeObjIdx,m_pTreeRoot->m_iChildDS);
	if ((iCurrChildIdx < 0) ||(0 == iOrderPos))
	{
		m_pIdxObjMng->SetDsIdx(iChildObjIdx,m_pTreeRoot->m_iParentDS,iNodeObjIdx);	
		m_pIdxObjMng->SetDsIdx(iChildObjIdx,m_pTreeRoot->m_iPrevBrotherDS,-1);
		m_pIdxObjMng->SetDsIdx(iChildObjIdx,m_pTreeRoot->m_iNextBrotherDS,iCurrChildIdx);
		m_pIdxObjMng->SetDsIdx(iNodeObjIdx,m_pTreeRoot->m_iChildDS,iChildObjIdx);

		m_pIdxObjMng->SetDsIdx(iCurrChildIdx,m_pTreeRoot->m_iPrevBrotherDS,iChildObjIdx);
		return 0;
	}

	//插入到最后一个孩子位置
	if (iOrderPos < 0)
	{
		iOrderPos = m_pIdxObjMng->GetObjNum();
	}

	//找到插入位置的前一个位置
	for(ssize_t i=0; i<(iOrderPos-1); i++)
	{
		ssize_t iTmpIdx = m_pIdxObjMng->GetDsIdx(iCurrChildIdx,m_pTreeRoot->m_iNextBrotherDS);
		if (iTmpIdx<0)
		{
			break;
		}	
		iCurrChildIdx = iTmpIdx;
	}	
		
	//挂入,child的孩子保留不动
	m_pIdxObjMng->SetDsIdx(iChildObjIdx,m_pTreeRoot->m_iParentDS,iNodeObjIdx);	
	m_pIdxObjMng->SetDsIdx(iChildObjIdx,m_pTreeRoot->m_iPrevBrotherDS,iCurrChildIdx);	
	ssize_t iCurrBrother = m_pIdxObjMng->GetDsIdx(iCurrChildIdx,m_pTreeRoot->m_iNextBrotherDS);
	m_pIdxObjMng->SetDsIdx(iChildObjIdx,m_pTreeRoot->m_iNextBrotherDS,iCurrBrother);
	if (iCurrBrother >= 0)
		m_pIdxObjMng->SetDsIdx(iCurrBrother,m_pTreeRoot->m_iPrevBrotherDS,iChildObjIdx);
	
	m_pIdxObjMng->SetDsIdx(iCurrChildIdx,m_pTreeRoot->m_iNextBrotherDS,iChildObjIdx);

	return 0;	
}

//将以iNodeObjIdx为根的树摘下
ssize_t CTree::TakeTree(ssize_t iNodeObjIdx)
{
	if (iNodeObjIdx<0 || iNodeObjIdx>=(ssize_t)m_pIdxObjMng->GetObjNum())
	{ 
		return -1;
	}

	//修复兄弟链
	ssize_t iPrevBrotherIdx = m_pIdxObjMng->GetDsIdx(iNodeObjIdx,m_pTreeRoot->m_iPrevBrotherDS);
	ssize_t iNextBrotherIdx = m_pIdxObjMng->GetDsIdx(iNodeObjIdx,m_pTreeRoot->m_iNextBrotherDS);
	if (iPrevBrotherIdx >= 0)
		m_pIdxObjMng->SetDsIdx(iPrevBrotherIdx,m_pTreeRoot->m_iNextBrotherDS,iNextBrotherIdx);
	if (iNextBrotherIdx >= 0)
		m_pIdxObjMng->SetDsIdx(iNextBrotherIdx,m_pTreeRoot->m_iPrevBrotherDS,iPrevBrotherIdx);

	m_pIdxObjMng->SetDsIdx(iNodeObjIdx,m_pTreeRoot->m_iPrevBrotherDS,-1);
	m_pIdxObjMng->SetDsIdx(iNodeObjIdx,m_pTreeRoot->m_iNextBrotherDS,-1);
	
	//修复父母链
	ssize_t iParentIdx = m_pIdxObjMng->GetDsIdx(iNodeObjIdx,m_pTreeRoot->m_iParentDS);
	if (iParentIdx >= 0)
	{
		ssize_t iFirstChildIdx = m_pIdxObjMng->GetDsIdx(iParentIdx,m_pTreeRoot->m_iChildDS);
		if (iFirstChildIdx == iNodeObjIdx)
		{
			m_pIdxObjMng->SetDsIdx(iParentIdx,m_pTreeRoot->m_iChildDS,iNextBrotherIdx);
		}
	}
	m_pIdxObjMng->SetDsIdx(iNodeObjIdx,m_pTreeRoot->m_iParentDS,-1);

	//根节点修改
	if (iNodeObjIdx == m_pTreeRoot->m_iRootIdx)
	{
		m_pTreeRoot->m_iRootIdx = -1;
	}	
	return iNodeObjIdx;	
}

ssize_t CTree::RemoveTree(ssize_t iNodeObjIdx/*=-1*/)
{
	if (iNodeObjIdx<0 || iNodeObjIdx>=(ssize_t)m_pIdxObjMng->GetObjNum())
	{
		m_pTreeRoot->m_iRootIdx = -1;
		m_pIdxObjMng->FormatIdx();  
		return 0;
	}

	ssize_t iChildIdx = m_pIdxObjMng->GetDsIdx(iNodeObjIdx,m_pTreeRoot->m_iChildDS);
	if (iChildIdx < 0)
	{
		//修复兄弟链
		ssize_t iPrevBrotherIdx = m_pIdxObjMng->GetDsIdx(iNodeObjIdx,m_pTreeRoot->m_iPrevBrotherDS);
		ssize_t iNextBrotherIdx = m_pIdxObjMng->GetDsIdx(iNodeObjIdx,m_pTreeRoot->m_iNextBrotherDS);
		if (iPrevBrotherIdx >= 0)
			m_pIdxObjMng->SetDsIdx(iPrevBrotherIdx,m_pTreeRoot->m_iNextBrotherDS,iNextBrotherIdx);
		if (iNextBrotherIdx >= 0)
			m_pIdxObjMng->SetDsIdx(iNextBrotherIdx,m_pTreeRoot->m_iPrevBrotherDS,iPrevBrotherIdx);

		//修复父母链
		ssize_t iParentIdx = m_pIdxObjMng->GetDsIdx(iNodeObjIdx,m_pTreeRoot->m_iParentDS);
		if (iParentIdx >= 0)
		{
			ssize_t iFirstChildIdx = m_pIdxObjMng->GetDsIdx(iParentIdx,m_pTreeRoot->m_iChildDS);
			if (iFirstChildIdx == iNodeObjIdx)
			{
				m_pIdxObjMng->SetDsIdx(iParentIdx,m_pTreeRoot->m_iChildDS,iNextBrotherIdx);
			}
		}
		//回收自己
		m_pIdxObjMng->DestroyObject(iNodeObjIdx);

		//根节点修改
		if (iNodeObjIdx == m_pTreeRoot->m_iRootIdx)
		{
			m_pTreeRoot->m_iRootIdx = -1;
		}	
		return 0;
	}
	else
	{
		//回收所有孩子
		while(1)
		{
			ssize_t iNextBrotherIdx = m_pIdxObjMng->GetDsIdx(iChildIdx,m_pTreeRoot->m_iNextBrotherDS);
			RemoveTree(iChildIdx);
			iChildIdx = iNextBrotherIdx;	
			if (iChildIdx < 0)
			{
				break;
			}
		}
		
		//回收自己
		RemoveTree(iNodeObjIdx);
	}

	return 0;
}

ssize_t CTree::LeafNum(ssize_t iNodeObjIdx/*=-1*/)
{
	if (iNodeObjIdx<0 || iNodeObjIdx>=(ssize_t)m_pIdxObjMng->GetObjNum())
	{
		iNodeObjIdx = m_pTreeRoot->m_iRootIdx;
	}
	
	ssize_t iChildIdx = m_pIdxObjMng->GetDsIdx(iNodeObjIdx,m_pTreeRoot->m_iChildDS);
	if (iChildIdx < 0)
	{
		return 1;
	}

	ssize_t iChildLeafNum = 0;
	while(iChildIdx >= 0)
	{
		ssize_t iTmpIdx = m_pIdxObjMng->GetDsIdx(iChildIdx,m_pTreeRoot->m_iNextBrotherDS);
		iChildLeafNum += LeafNum(iChildIdx);
		iChildIdx = iTmpIdx;
	}	
	
	return iChildLeafNum;
}

ssize_t CTree::IsLeaf(ssize_t iNodeObjIdx)
{
	if (iNodeObjIdx<0 || iNodeObjIdx>=(ssize_t)m_pIdxObjMng->GetObjNum())
	{
		return 0;
	}
	ssize_t iChildIdx = m_pIdxObjMng->GetDsIdx(iNodeObjIdx,m_pTreeRoot->m_iChildDS);
	if (iChildIdx < 0)
	{
		return 1;
	}
	return 0;
}

ssize_t CTree::Height(ssize_t iRootNodeIdx/*=-1*/)
{
	if (!m_pTreeRoot || (m_pTreeRoot->m_iRootIdx<0))
	{
		return 0;
	}

	if (iRootNodeIdx < 0)
	{
		return Height(m_pTreeRoot->m_iRootIdx);   		
	}

	//叶子高度0
	ssize_t iChildIdx = m_pIdxObjMng->GetDsIdx(iRootNodeIdx,m_pTreeRoot->m_iChildDS);
	if (iChildIdx < 0)
	{
		return 0;
	}

	//取孩子中的最大高度
	ssize_t iMaxChildH = 0;
	while(iChildIdx >= 0)
	{
		ssize_t iTmpIdx = m_pIdxObjMng->GetDsIdx(iChildIdx,m_pTreeRoot->m_iNextBrotherDS);
		ssize_t iTmpH = Height(iChildIdx);
		if (iTmpH > iMaxChildH)
		{
			iMaxChildH = iTmpH;
		}
		iChildIdx = iTmpIdx;
	}

	return iMaxChildH+1;	
}

void CTree::PrintTree(ssize_t iNodeObjIdx/*=-1*/)
{
	if (m_pTreeRoot->m_iRootIdx < 0)
	{
		return ;
	}
	
	if (iNodeObjIdx<0 || iNodeObjIdx>=(ssize_t)m_pIdxObjMng->GetObjNum())
	{
		iNodeObjIdx = m_pTreeRoot->m_iRootIdx;
	}

	printf("Height:%lld\n",(long long)Height());
	RecursionPrint(iNodeObjIdx,0);
	return;	
}

void CTree::RecursionPrint(ssize_t iNodeObjIdx,ssize_t iLevel)
{
	if (iNodeObjIdx<0 || iNodeObjIdx>=(ssize_t)m_pIdxObjMng->GetObjNum())
	{
		return;
	}

	//打印自己
	printf("|");  
	for (ssize_t i=0; i<iLevel; i++)
	{
		printf("  |");	
	}
	printf("--%lld\n",(long long)iNodeObjIdx);

	//打印孩子
	ssize_t iCurrIdx = m_pIdxObjMng->GetDsIdx(iNodeObjIdx,m_pTreeRoot->m_iChildDS);
	while (iCurrIdx >= 0)
	{
		ssize_t iBrotherIdx = m_pIdxObjMng->GetDsIdx(iCurrIdx,m_pTreeRoot->m_iNextBrotherDS);
		
		RecursionPrint(iCurrIdx,iLevel+1);	
		iCurrIdx = iBrotherIdx;		
	}

	return;	
}

ssize_t* CTree::GetChild(ssize_t iNodeObjIdx,ssize_t &iChildNum)
{
	iChildNum = 0;

	ssize_t iChildIdx = m_pIdxObjMng->GetDsIdx(iNodeObjIdx,m_pTreeRoot->m_iChildDS);
	while(iChildIdx >= 0)
	{
		m_piNodeIdxArray[iChildNum++] = iChildIdx;
		iChildIdx = m_pIdxObjMng->GetDsIdx(iChildIdx,m_pTreeRoot->m_iNextBrotherDS);
	}
	return m_piNodeIdxArray;
}
	
ssize_t* CTree::GetDeepSequeue(ssize_t iNodeObjIdx,ssize_t &iSeqObjNum)
{
	iSeqObjNum = 0;
	GetDeepSequeue(iNodeObjIdx,m_piNodeIdxArray,iSeqObjNum);
	return m_piNodeIdxArray;
}

ssize_t CTree::GetDeepSequeue(ssize_t iNodeObjIdx,ssize_t* piSeqObj,ssize_t &iSeqObjNum)
{
	if (!piSeqObj || iNodeObjIdx<0 || iNodeObjIdx>=(ssize_t)m_pIdxObjMng->GetObjNum())
	{
		return 0;
	}

	//访问自己	
	piSeqObj[iSeqObjNum++] = iNodeObjIdx;

	//访问孩子
	ssize_t iCurrIdx = m_pIdxObjMng->GetDsIdx(iNodeObjIdx,m_pTreeRoot->m_iChildDS);
	while (iCurrIdx >= 0)
	{
		ssize_t iBrotherIdx = m_pIdxObjMng->GetDsIdx(iCurrIdx,m_pTreeRoot->m_iNextBrotherDS);
		GetDeepSequeue(iCurrIdx,piSeqObj,iSeqObjNum);
		iCurrIdx = iBrotherIdx;		
	}

	return 0;
}

ssize_t* CTree::GetExpandSequeue(ssize_t iNodeObjIdx,ssize_t &iSeqObjNum)
{
	iSeqObjNum = 0;
	GetExpandSequeue(iNodeObjIdx,m_piNodeIdxArray,iSeqObjNum);
	return m_piNodeIdxArray;
}

ssize_t CTree::GetExpandSequeue(ssize_t iNodeObjIdx,ssize_t* piSeqObj,ssize_t &iSeqObjNum)
{
	if (!piSeqObj || iNodeObjIdx<0 || iNodeObjIdx>=(ssize_t)m_pIdxObjMng->GetObjNum())
	{
		return 0;
	}
	
	iSeqObjNum = 0;
	
	//访问自己	
	piSeqObj[iSeqObjNum++] = iNodeObjIdx;

	for ( ssize_t i=0; i<iSeqObjNum; i++)
	{
		//取一个，压入其孩子
		ssize_t iCurrIdx = m_pIdxObjMng->GetDsIdx(piSeqObj[i],m_pTreeRoot->m_iChildDS);
		while(iCurrIdx >= 0)
		{
			ssize_t iBrotherIdx = m_pIdxObjMng->GetDsIdx(iCurrIdx,m_pTreeRoot->m_iNextBrotherDS);
			piSeqObj[iSeqObjNum++] = iCurrIdx;
			iCurrIdx = iBrotherIdx;
		}	
	}
	
	return 0;
}

/*
int main()
{
	char *pmem = new char[1024*1024*5];
	TIdxObjMng stTIdxObjMng;
	int ret = stTIdxObjMng.AttachMem(pmem, 1024*1024*5, 0, 0,emInit,0);

	int idx1 = stTIdxObjMng.CreateObject();
	int idx2 = stTIdxObjMng.CreateObject();

	TIdxObjMng::TIdx *pidx1 = stTIdxObjMng.GetIdx(idx1);
	TIdxObjMng::TIdx *pidx2 = stTIdxObjMng.GetIdx(idx2);
	stTIdxObjMng.Print(stdout);
}
*/

