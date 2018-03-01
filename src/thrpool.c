/*************************************************************************
	> File Name: thrpool.c
	> Author: lroyd
	> Mail: htzhangxmu@163.com
	> Created Time: 
*************************************************************************/
#include "thrpool.h"


/************************************************************************
* 			Logout Setting
************************************************************************/
#ifdef THRPOOL_DBG	
	#define 	TAG_THR			"THRPOOL"
	#if defined(ANDROID) || defined(__ANDROID__)
		/* android */

	#else
		/* linux */
		#include"log_file.h"

		#define 	TRC_THR 			(4)				//TRACE_LEVEL_INFO
		#define 	THR_LOGI(fmt, arg...) 		{if(TRC_THR >= TRACE_LEVEL_INFO){DLOGI(TAG_THR, fmt, ##arg);}}
		#define 	THR_LOGD(fmt, arg...) 		{if(TRC_THR >= TRACE_LEVEL_DEBUG){DLOGD(TAG_THR, fmt, ##arg);}}
		#define 	THR_LOGE(fmt, arg...) 		{if(TRC_THR >= TRACE_LEVEL_ERROR){DLOGE(TAG_THR, fmt, ##arg);}}
	#endif
		/* window */
#else
	#define 	THR_LOGI(fmt, arg...) 
	#define 	THR_LOGD(fmt, arg...)
	#define 	THR_LOGE(fmt, arg...)
#endif
/************************************************************************
* 			Macro Common Definition
* **********************************************************************/
#define MAX_THR_IN_POOL		    (15)					//thread pool maximum thread number
#define MAX_THR_IDL_TIME		(30)					//thread maximum idle time

#define THR_DIE_TASK			((void *)(-3))			//forthcoming thread out of the thread
#define THR_WAIT_TASK			((void *)(-2))			//new thread
#define THR_FREE_TASK			((void *)(-1))			//uninitialized thread
#define THR_IDL_TASK			((void *)(0))			//idle thread

#define TPId 			    	pthread_t
#define TPLock 			    	pthread_mutex_t
#define InitTPLock(lck)			pthread_mutex_init(&(lck), NULL)
#define LockTPLock(lck)			pthread_mutex_lock(&(lck))
#define TryLockTPLock(lck)		pthread_mutex_trylock(&(lck))
#define UnLockTPLock(lck)	    pthread_mutex_unlock(&(lck))

#define TPCond 					pthread_cond_t
#define InitTPCond(cnd)			pthread_cond_init(&(cnd), NULL)
#define WaitTPCond(cnd, lck)	pthread_cond_wait(&(cnd), &(lck))
#define SignalTPCond(cnd)		pthread_cond_signal(&(cnd))
/************************************************************************
* 			Struct Common Definition
* **********************************************************************/
typedef struct msg_hdr
{
    struct msg_hdr *    p_next;
    int					p_type;
    size_t              p_len;
}msg_hdr_t;

typedef struct
{
	int 		msg_iCnt;
    msg_hdr_t	*msg_head;
    msg_hdr_t	*msg_tail;
}msg_queue_t;

typedef void * (*TMyThreadTask)(void * pArg);

typedef struct
{
	int m_iRunFlg;							//thread entity running marking

	unsigned long long m_iLastActive;		//last activity time

	TPId	 				m_tHnd;			//handle
	TPLock					m_tMux;			
	TPCond					m_tCond;
	int 			m_iEvt;

	msg_queue_t				m_iQueue;		//receiving queue
	
	void *(*Task)(void *);					//attaching logical task
	void *m_pArg;							//attachment logic parameters

} LH_Thread;

typedef struct
{
	int 		m_iThrCnt;							//current thread number
	LH_Thread 	m_tThrList[MAX_THR_IN_POOL];		//thread list
	TPLock		m_tMutex;							
	TPId	 	m_tSchldThr;						
} LH_ThreadPool; 
/************************************************************************
* 			Variable Common Definition
* **********************************************************************/
static int g_iThreadPoolInitFlg = 0;
static LH_ThreadPool g_tThrPool;

volatile int g_iThrPoolSchldFlag = 0;

/************************************************************************
* Name: 
* Descriptions:
* Param:		
* Return:
* **********************************************************************/
static void XSleep(int nSec, int nUSec)
{
	struct timeval tv;
	tv.tv_sec = nSec;
	tv.tv_usec = nUSec;
	select(0, NULL, NULL, NULL, &tv);
}

static unsigned long long CTimerSec(void) 
{
    unsigned long long x=0;

	// use system call to read time clock for other archs
	struct timeval t;
	gettimeofday(&t, 0);
    x = t.tv_sec;

   return x;
   //TODO: add machine instrcutions for different archs
}
/************************************************************************
* Name: 		ThrPoolSchld
* Descriptions:	thread pool task debugger, mainly responsible for cleaning 
*				up idle threads.
* Param:		void *
* Return:		void *
* **********************************************************************/
static void *ThrPoolSchld(void *_pArg) //
{
	LH_ThreadPool *ptThrPool = (LH_ThreadPool *)_pArg;
    int i;
    unsigned long long iTm;
    volatile LH_Thread *ptThr =NULL;	
    
	while ( g_iThrPoolSchldFlag )
	{
		ptThr = ptThrPool->m_tThrList;
		
		XSleep(2,0);
#if 1
		LockTPLock(ptThrPool->m_tMutex);

		iTm = CTimerSec();
		for (i = 0; i < MAX_THR_IN_POOL; i++, ptThr++)
		{
			if (THR_IDL_TASK == ptThr->Task && (iTm >= (ptThr->m_iLastActive + MAX_THR_IDL_TIME)))
			{
				ptThr->Task = THR_DIE_TASK;
				ptThr->m_iRunFlg = 0;
				ptThrPool->m_iThrCnt--;
			}
		}

		UnLockTPLock(ptThrPool->m_tMutex);
#endif
	}

	THR_LOGE("%s ThrPool Dispatch has Exit !!!!",__func__);
	return NULL;
}

/************************************************************************
* Name: 		InitThreadPool(CLI-API)
* Descriptions:	thread pool init
* Param:		int:THREAD_INIT_FLG_MULT/THREAD_INIT_FLG_SING
* Return:		tTHR_STATUS
* **********************************************************************/
tTHR_STATUS InitThreadPool(int _dwFlag)
{
	int i, iRet = THREAD_RET_INVAILED;

	LH_ThreadPool *ptPool = &g_tThrPool;
	TPId tTid;

	ptPool->m_iThrCnt = 0;
	InitTPLock(ptPool->m_tMutex);

	for (i = 0; i < MAX_THR_IN_POOL; i++)
	{
		
		ptPool->m_tThrList[i].Task = THR_FREE_TASK;
		InitTPLock(ptPool->m_tThrList[i].m_tMux);
		InitTPCond(ptPool->m_tThrList[i].m_tCond);

		ptPool->m_tThrList[i].m_iQueue.msg_iCnt = 0;
		ptPool->m_tThrList[i].m_iQueue.msg_head = NULL;
		ptPool->m_tThrList[i].m_iQueue.msg_tail = NULL;

		ptPool->m_tThrList[i].m_iEvt = 0;
	}

	if(_dwFlag == THREAD_INIT_FLG_MULT)
	{
		g_iThrPoolSchldFlag = 1;
		
		if(pthread_create(&tTid, NULL, ThrPoolSchld, ptPool)!=0)
		{
			iRet = THREAD_RET_CREATE_ERR;
			THR_LOGE("Create thread sched error!!!!");
			goto EXIT;
		}
		pthread_detach(tTid);
	}

	g_iThreadPoolInitFlg = 1;

	iRet = THREAD_RET_SUCCESS;
EXIT:
	return iRet;
}

tTHR_STATUS UnInitThreadPool(void)
{
	int i, iRet = THREAD_RET_INVAILED;
	volatile LH_Thread *ptThr = NULL;
	LH_ThreadPool *ptPool = &g_tThrPool;
	
    g_iThrPoolSchldFlag = 0;

    g_iThreadPoolInitFlg = 0;

	LockTPLock(ptPool->m_tMutex);
	/* Cleaning all thread entities */
	for(i = 0; i < MAX_THR_IN_POOL; i++)
	{	
		if(ptPool->m_tThrList[i].m_iRunFlg)
		{
			ptPool->m_tThrList[i].m_iRunFlg = 0;
		}
	}

	iRet = THREAD_RET_SUCCESS;

	UnLockTPLock(ptPool->m_tMutex);

    return iRet;
}
/************************************************************************
* Name: 		MyThreadTask
* Descriptions:
* Param:		void *
* Return:		void *
* **********************************************************************/
static void *MyThreadTask(void *_pArg)
{
	volatile LH_Thread *ptThr = (LH_Thread *)_pArg;

	while (THR_WAIT_TASK == ptThr->Task)	//New threads have not yet assigned tasks, waiting for...
	{
		XSleep(0,1000);
	}
	while (ptThr->m_iRunFlg)
	{	
		while (THR_IDL_TASK == ptThr->Task)	 //In thread space, wait for assignment task
		{
			XSleep(0,1000);
			if (!ptThr->m_iRunFlg)
			{
				goto EXIT;
			}
		}
		
		if (THR_DIE_TASK == ptThr->Task)
		{
			goto EXIT;
		}
		
		if (THR_FREE_TASK == ptThr->Task || THR_WAIT_TASK == ptThr->Task)
		{
			XSleep(0,1000);
			continue;
		}
		/* Executing thread tasks */ 
		ptThr->Task(ptThr->m_pArg);    
		
		//THR_LOGE("attachment func has exit ~~");
		ptThr->m_iLastActive = CTimerSec();
		//ptThr->m_pArg = NULL;
		ptThr->Task = THR_IDL_TASK;	//After the thread exits, it automatically becomes idle, but it is not destroyed

	}

EXIT:
	THR_LOGE("Thread State Recovery..");
	ptThr->Task = THR_FREE_TASK;
	return NULL;
}
/************************************************************************
* Name: 		MyCreateThread
* Descriptions:
* Param:		
* Return:     	int:id (0 ~ MAX)
* **********************************************************************/
static int MyCreateThread(void *(*_Task)(void *), void *_pArg, LH_ThreadPool *_ptPool)
{
	int iRet = THREAD_RET_INVAILED;
	int i;
	int iThrId = -1;
	int iFreeId = -1;
	volatile LH_Thread *ptThr = NULL;

    TPId tTid;

	LockTPLock(_ptPool->m_tMutex);

	for (i = 0; i < MAX_THR_IN_POOL; i++)
	{	
		if (THR_FREE_TASK == _ptPool->m_tThrList[i].Task)
		{
			iFreeId = i;
		}
		else if (THR_IDL_TASK == _ptPool->m_tThrList[i].Task)
		{
			_ptPool->m_tThrList[i].m_iLastActive = CTimerSec();
			iThrId = i;
			break;
		}
	}

	if (iThrId < 0) //No idle thread
	{
		if (iFreeId < 0) 
		{
			/* The thread pool is full */
#if 0		
			if (pthread_create(_ptThr, NULL, _Task, _pArg) == 0)
			{
				iRet = 0;
				pthread_detach(*_ptThr);
			}
#endif
			THR_LOGE("thr pool is full.");  //Can not be created
			goto EXIT;
		}
		else //Create a new thread and add a thread pool
		{
			ptThr = _ptPool->m_tThrList + iFreeId;
			ptThr->m_iRunFlg = 1;

			ptThr->m_pArg = _pArg;
			ptThr->Task = THR_WAIT_TASK;

			if (pthread_create(&tTid, NULL, MyThreadTask, _ptPool->m_tThrList + iFreeId) != 0)
			{
				_ptPool->m_tThrList[iFreeId].Task = THR_FREE_TASK;
				iRet = THREAD_RET_CREATE_ERR;
				goto EXIT;
			}
			pthread_detach(tTid);
			
			_ptPool->m_tThrList[iFreeId].m_tHnd = tTid;
			_ptPool->m_iThrCnt++;

			iThrId = iFreeId;
			//THR_LOGE("Create new thread in pool = %p",ptThr->Task );
		}
	}
	/* Assignment of tasks for the thread in the pool, the count is diminishing */ 
	ptThr = _ptPool->m_tThrList + iThrId;
	ptThr->m_iLastActive = CTimerSec();
	ptThr->m_pArg = _pArg;
	ptThr->Task = _Task;
	//*_ptThr = -1;
	//THR_LOGE("new task add into pool thread id = %d , = %p",iThrId, ptThr->Task );
	iRet = iThrId;	
EXIT:	
	UnLockTPLock(_ptPool->m_tMutex);
	return iRet;
}


/************************************************************************
* Name: 		StartThreadInPool(CLI-API)
* Descriptions: Thread creation , the creation has not been 
*				destroyed, only the execution entity does not exist.
* Param:		1.void *	2.void *
* Return:		tTHR_STATUS
* **********************************************************************/
tTHR_STATUS StartThreadInPool(void *(*_Task)(void *), void *_pArg)
{
	if (!g_iThreadPoolInitFlg)
	{
		return THREAD_RET_UNINIT_ERR;
	}

	if (NULL == _Task)
	{
		return THREAD_RET_PARAM_ERR;
	}

	//THR_LOGE("task = %p, arg = %p", _Task, _pArg);

	return MyCreateThread(_Task, _pArg, &g_tThrPool);
}


#if 0
/************************************************************************
* Name: 		StopThreadInPool
* Descriptions: Only the outer layer of logical detachment
* Param:		
* Return:
* **********************************************************************/
int StopThreadInPool(int *_pThrId)
{
	int iRet = -1;

	LockTPLock(g_tThrPool.m_tMutex);
	volatile LH_Thread *ptThr = &g_tThrPool.m_tThrList[*_pThrId];
	
	/* Do not delete the entity, only delete the attachment logic */	
	dbgmsg("thread id = %d not exist \r\n", *_pThrId);
EXIT:
	UnLockTPLock(g_tThrPool.m_tMutex);
	return iRet;
}
#endif

/************************************************************************
* Name: 		ThreadTskSetEvent
* Descriptions: 
* Param:		1.int:thread id     2.int:
* Return:
* **********************************************************************/
tTHR_STATUS ThreadTskSetEvent(int _pThrId, int _pEvent, int _pCond, int _pflag)
{
	int iRet = THREAD_RET_INVAILED, EVT_MSK = 0x000000FF;
	
	LH_Thread *ptThr = &g_tThrPool.m_tThrList[_pThrId];
	if(ptThr->m_iRunFlg)
	{
		LockTPLock(ptThr->m_tMux);

		if(THREAD_FLG_S_CTR == _pflag)
		{
			ptThr->m_iEvt |= _pEvent;
			//THR_LOGD("set : event = %#010x, ptThr->m_iEvt = %#010x",_pEvent,ptThr->m_iEvt);  
		}
		else
		{
			/* 多驱动器 最多4个 (1字节事件,最后一个字节有效)*/
			ptThr->m_iEvt |= _pEvent & (EVT_MSK << (_pCond * 8));
		}

	    SignalTPCond(ptThr->m_tCond);
	    UnLockTPLock(ptThr->m_tMux);
	    iRet = THREAD_RET_SUCCESS;
	}

	return iRet;
}

int ThreadTskCleanEvent(int _pThrId, int event)
{
	int iRet = THREAD_RET_INVAILED;
	LH_Thread *ptThr = &g_tThrPool.m_tThrList[_pThrId];
	if(ptThr->m_iRunFlg)
	{
		LockTPLock(ptThr->m_tMux);
		
	    ptThr->m_iEvt &= ~event;

	    SignalTPCond(ptThr->m_tCond);
	    UnLockTPLock(ptThr->m_tMux);
	    iRet = THREAD_RET_SUCCESS;
	}

	return iRet;	
}
/* 返回 0-4个字节位置 FF FF FF FF,每次返回一个字节的事件，然后清除 其他的事件依然保留 */
tTHR_STATUS ThreadTskWaitForEvent(int _pThrId, int *_pOut_evt, int _pBflag, int _pCflag)
{
	int i, iRet = THREAD_RET_INVAILED, EVT_MSK = 0x000000FF;
	
	LH_Thread *ptThr = &g_tThrPool.m_tThrList[_pThrId];
	
	if(ptThr->m_iRunFlg)
	{
		if(_pBflag == THREAD_FLG_NOWAIT)
		{
	        if(!TryLockTPLock(ptThr->m_tMux))
	        {
	        	/* 获得锁 */
	            if( ptThr->m_iEvt == 0 )
	            {
	            	/* 无事件,需要解锁 */
	            	UnLockTPLock(ptThr->m_tMux);	
	                goto EXIT;
	            }
	            /* 有事件跳出,取出事件 */
	            
	        }
	        else
	        {
	            goto EXIT;
	        }			
		}
		else if(_pBflag == THREAD_FLG_WAITFOREVER)
		{
			LockTPLock(ptThr->m_tMux);
			while (ptThr->m_iEvt == 0)
			{
				/* 完全没有事件 */
				WaitTPCond(ptThr->m_tCond, ptThr->m_tMux);
			}

		}
		else
		{
			iRet = THREAD_RET_PARAM_ERR;
			goto EXIT;
		}

		if(THREAD_FLG_S_CTR == _pCflag)
		{
			/* 单驱动器 直接取出整个 */
			*_pOut_evt = ptThr->m_iEvt;
			//THR_LOGE("gett : ptThr->m_iEvt = %#010x, _pOut_evt = %#010x",ptThr->m_iEvt, *_pOut_evt);  
			ptThr->m_iEvt = 0;
			iRet = 0;
			
		}
		else
		{
			for(i=0; i < (sizeof(ptThr->m_iEvt)); i++, EVT_MSK<<=8)
			{
				if( ptThr->m_iEvt & EVT_MSK )
				{
					int tmp;
					tmp = (ptThr->m_iEvt & EVT_MSK);
					ptThr->m_iEvt &= ~tmp;
					*_pOut_evt = (tmp>>(i*8))&(0x000000FF);
					iRet = i;
					break;
				}
			}
		}
		
		UnLockTPLock(ptThr->m_tMux);	
		
		//iRet = THREAD_RET_SUCCESS;
	}
	
EXIT:
	//THR_LOGD("gett : iRet = %d, ptThr->m_iEvt = %#010x, _pOut_evt = %#010x",iRet,ptThr->m_iEvt, *_pOut_evt);  
	return iRet;
}

/************************************************************************
* Name: 
* Descriptions: _pType 目前没有使用
* Param:		
* Return:
* **********************************************************************/
tTHR_STATUS ThreadTskPostToQueueMsg(int _pThrId, int _pType, const void* _ptr, size_t _plen)
{
	int iRet = THREAD_RET_INVAILED;
	msg_hdr_t * p_msg_hdr = NULL;
	
	LH_Thread *ptThr = &g_tThrPool.m_tThrList[_pThrId];
	if(ptThr->m_iRunFlg)
	{
		LockTPLock(ptThr->m_tMux);
	
	    p_msg_hdr = (msg_hdr_t*)malloc(sizeof(msg_hdr_t) + _plen);

	    if(p_msg_hdr == NULL)
	    {
	        iRet = THREAD_RET_MALLOC_ERR;
	        goto EXIT;
	    }
	    memset(p_msg_hdr, 0, sizeof(msg_hdr_t) + _plen);

	    p_msg_hdr->p_next 	= NULL;
	    p_msg_hdr->p_type 	= _pType;
	    p_msg_hdr->p_len 	= _plen;
	    memcpy(p_msg_hdr + 1, _ptr, _plen);

	    if(ptThr->m_iQueue.msg_head == NULL)
	    {
	    	ptThr->m_iQueue.msg_head = p_msg_hdr;
	        ptThr->m_iQueue.msg_tail = p_msg_hdr;
	    }
	    else
	    {
	        ptThr->m_iQueue.msg_tail->p_next = p_msg_hdr;
	        ptThr->m_iQueue.msg_tail = p_msg_hdr;
	    }

		ptThr->m_iQueue.msg_iCnt++;

		ptThr->m_iEvt |= THREAD_RCV_MSG;
	    SignalTPCond(ptThr->m_tCond);
		UnLockTPLock(ptThr->m_tMux);	
		
		iRet = THREAD_RET_SUCCESS;

	}
EXIT:
	return iRet;
}

/************************************************************************
* Name: 
* Descriptions: 没有判断取出的数据类型
* Param:		
* Return:		返回邮箱的里面的数据个数
* **********************************************************************/
tTHR_STATUS ThreadTskGetMsgFromQueue(int _pThrId, void* _ptr, size_t _plen)
{
	int iRet = THREAD_RET_INVAILED;
	msg_hdr_t * p_msg_hdr = NULL;
	
	LH_Thread *ptThr = &g_tThrPool.m_tThrList[_pThrId];
	if(ptThr->m_iRunFlg)
	{	
		LockTPLock(ptThr->m_tMux);

	    p_msg_hdr = ptThr->m_iQueue.msg_head;
	    ptThr->m_iQueue.msg_head = ptThr->m_iQueue.msg_head->p_next;


	    size_t len_min = (_plen < p_msg_hdr->p_len) ? _plen : p_msg_hdr->p_len;

		ptThr->m_iQueue.msg_iCnt--;
		iRet = ptThr->m_iQueue.msg_iCnt;
		UnLockTPLock(ptThr->m_tMux);	

		
		memcpy(_ptr, p_msg_hdr + 1, len_min);

		if(p_msg_hdr)
		{
			free(p_msg_hdr);
		}

		iRet = THREAD_RET_SUCCESS;
	}

	return iRet;
}

