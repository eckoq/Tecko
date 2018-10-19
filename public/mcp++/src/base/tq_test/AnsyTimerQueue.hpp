/*************************************************************
Copyright (C), 1988-1999
Author:nekeyzhong
Version :1.0
Date: 2008-01
Description: 异步事务管理器,用来管理众多的异步事务会话
***********************************************************/
 
#ifndef _ANSYTIMERQUEUE_HPP
#define _ANSYTIMERQUEUE_HPP

#include "IdxObjMng.hpp"
#include <sys/time.h>
#include <sys/types.h>
#include <assert.h>

// 保存在Timer中，超时后由Timer负责delete
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

	//pArg消息,unSeq自身seq
	virtual ssize_t OnMessage(size_t unSeq,void* pArg=NULL){assert(0);return 0;};
	virtual ssize_t OnMessage(size_t unSeq,void* pArg1,void* pArg2){assert(0);return 0;};	
	
	// 超时,return 0继续放入,!0外层删除
	virtual ssize_t OnExpire(){ return -1;};

	//设置存活超时时间属性ms
	void SetAliveTimeOutMs(size_t unTimeOutMs)
	{
		m_unTimeOutMs = unTimeOutMs;
	}
public:
	//创建时间
	timeval m_tCreateTime;

	//过期时间参数
	size_t m_unTimeOutMs;

private:
	//超时控制
	u_int64_t m_ullDeadTimeMillSecs;
	
	//默认存活10秒
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

	//严格使用size_t 的seq类型，避免隐式类型转换在32位64位下造成不一致
	ssize_t Set(size_t unSeq,CTimerInfo* pTimerInfo);
	CTimerInfo* Take(size_t unSeq);
	CTimerInfo* Get(size_t unSeq);
	
	void Print( FILE *fpOut );

	size_t GetCount(){return m_stExpireQueue.GetLength();}

private:
	ssize_t Init(ssize_t iTimerNum);
	
	char* m_pMemPtr;

	timeval m_tNow;
	
	//哈希
	TIdxObjMng m_stObjMng;
	CHashTab m_stObjHashTab;
	CObjQueue m_stExpireQueue;
};

#endif

