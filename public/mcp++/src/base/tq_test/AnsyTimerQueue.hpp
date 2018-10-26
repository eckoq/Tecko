/*************************************************************
Copyright (C), 1988-1999
Author:nekeyzhong
Version :1.0
Date: 2008-01
Description: �첽���������,���������ڶ���첽����Ự
***********************************************************/
 
#ifndef _ANSYTIMERQUEUE_HPP
#define _ANSYTIMERQUEUE_HPP

#include "IdxObjMng.hpp"
#include <sys/time.h>
#include <sys/types.h>
#include <assert.h>

// ������Timer�У���ʱ����Timer����delete
class CTimerInfo
{
public:
	CTimerInfo()
	{
		gettimeofday(&m_tCreateTime,NULL);
		m_unTimeOutMs = DEFAULT_ALIVE_TIMEOUT;
	};
	virtual ~CTimerInfo(){};
	
	virtual ssize_t Init(void* pArg1,void* pArg2){return 0;};

	//pArg��Ϣ,unSeq����seq
	virtual ssize_t OnMessage(size_t unSeq,void* pArg=NULL){assert(0);return 0;};
	virtual ssize_t OnMessage(size_t unSeq,void* pArg1,void* pArg2){assert(0);return 0;};	
	
	// ��ʱ,return 0��������,!0���ɾ��
	virtual ssize_t OnExpire(){ return -1;};

	//���ô�ʱʱ������ms
	void SetAliveTimeOutMs(size_t unTimeOutMs)
	{
		m_unTimeOutMs = unTimeOutMs;
	}
public:
	//����ʱ��
	timeval m_tCreateTime;

	//����ʱ�����
	size_t m_unTimeOutMs;

private:
	//��ʱ����
	u_int64_t m_ullDeadTimeMillSecs;
	
	//Ĭ�ϴ��10��
	static const ssize_t DEFAULT_ALIVE_TIMEOUT = 10*1000;

	friend class CAnsyTimerQueue;
};

class CAnsyTimerQueue
{
public:	
	typedef struct
	{
		size_t m_unSeq;
		CTimerInfo* m_pTimerInfo;
	}THashNode;

	CAnsyTimerQueue(ssize_t iTimerNum=100000);
	~CAnsyTimerQueue();

	static ssize_t CountBaseSize(ssize_t iTimerNum);

	ssize_t TimeTick(timeval *ptval=NULL);

	//�ϸ�ʹ��size_t ��seq���ͣ�������ʽ����ת����32λ64λ����ɲ�һ��
	ssize_t Set(size_t unSeq,CTimerInfo* pTimerInfo);
	CTimerInfo* Take(size_t unSeq);
	CTimerInfo* Get(size_t unSeq);
	
	void Print( FILE *fpOut );

	size_t GetCount(){return m_stExpireQueue.GetLength();}

private:
	ssize_t Init(ssize_t iTimerNum);
	
	char* m_pMemPtr;

	timeval m_tNow;
	
	//��ϣ
	TIdxObjMng m_stObjMng;
	CHashTab m_stObjHashTab;
	CObjQueue m_stExpireQueue;
};

#endif

