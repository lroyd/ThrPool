/*************************************************************************
	> File Name: thrpool.h
	> Author: lroyd
	> Mail: htzhangxmu@163.com
	> Created Time: 
 ************************************************************************/
#ifndef _THR_POOL_H_
#define _THR_POOL_H_

#include <stdarg.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <pthread.h>

/************************************************************************
* 			Macro Definition
* **********************************************************************/
#define THRPOOL_DBG	(1)					//Whether to open debug?


/* 初始化线程池标识 : 启动线程清理 ? */
#define		THREAD_INIT_FLG_MULT							(int)(0)		//启动想线程清理
#define		THREAD_INIT_FLG_SING							(int)(-1)		//不启动
/* 事件接收标识 阻塞? */
#define		THREAD_FLG_NOWAIT								(int)(0)
#define		THREAD_FLG_WAITFOREVER							(int)(-1)		//注意，此标识:附着逻辑一定要触发事件才能退出
/* 事件发送标识 单驱动器? */
#define		THREAD_FLG_S_CTR								(int)(0)
#define		THREAD_FLG_M_CTR								(int)(1)

#define		THREAD_RCV_MSG			(int)(0x00000001)		//注意 只有单体事件时才可用



typedef enum
{
	THREAD_RET_CREATE_ERR = -5,			//创建其中的线程失败
	THREAD_RET_MALLOC_ERR,
	THREAD_RET_UNINIT_ERR,	
	THREAD_RET_PARAM_ERR,
	THREAD_RET_INVAILED,		//默认值
	THREAD_RET_SUCCESS
}tTHR_STATUS;




extern tTHR_STATUS InitThreadPool(int flag);
extern tTHR_STATUS UnInitThreadPool(void);

extern tTHR_STATUS StartThreadInPool(void *(*_Task)(void *), void *_pArg);
//extern int StopThreadInPool(int *_pThrId);

extern tTHR_STATUS ThreadTskSetEvent(int _pThrId, int _pEvent, int _pCond, int _pflag);
extern tTHR_STATUS ThreadTskCleanEvent(int _pThrId, int event);
extern tTHR_STATUS ThreadTskWaitForEvent(int _pThrId, int *_pOut_evt, int _pBflag, int _pCflag);

extern tTHR_STATUS ThreadTskPostToQueueMsg(int _pThrId, int _pType, const void* _ptr, size_t _plen);
extern tTHR_STATUS ThreadTskGetMsgFromQueue(int _pThrId, void* _ptr, size_t _plen);




#endif


