/*************************************************************************
	> File Name: thrpool.c
	> Author: lroyd
	> Mail: htzhangxmu@163.com
	> Created Time: 
*************************************************************************/
#include <stdarg.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <pthread.h>

#include "queue.h" 
#include "thrpool.h" 

/************************************************************************
* 			Logout Setting
************************************************************************/





/************************************************************************
* 			Macro Common Definition
* **********************************************************************/
typedef enum
{
	THR_MODE_LAZY = 0,	//不启动线程清理
	THR_MODE_BROOM,		//启动线程清理
	THR_MODE_BUTT,
}THR_STEP_MODE;


typedef enum
{
	LH_THREAD_TTPYE_POOL = 0,			//线程池
	LH_THREAD_TTPYE_COR,				//协程
	LH_THREAD_TTPYE_FASION,				//流程
	LH_THREAD_TTPYE_BUTT,
}THR_TTYPE;


#define LH_THREAD_POOL_SIZE_MAX				(10)					//用户最大创建池组数
#define LH_THREAD_ENT_SIZE_MAX				(10)					//每个池最大线程实体数
#define LH_THREAD_ENT_IDL_TIME_MAX			(30)					//thread maximum idle time

#define THR_DIE_TASK			((void *)(-3))			//forthcoming thread out of the thread
#define THR_WAIT_TASK			((void *)(-2))			//new thread
#define THR_FREE_TASK			((void *)(-1))			//没有创建线程，没有任务
#define THR_IDL_TASK			((void *)(0))			//已经创建好的线程，没有任务

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
typedef struct _tagThreadQueue{
	int					m_iTskId;
	int					m_iQEvent;		//消息内部事件
	int					m_iLen;
	char				*pData;
    TAILQ_ENTRY(_tagThreadQueue)  _qThrEntry;    
}T_ThreadQueueMsg;   

#define THREAD_QUEUE_HEAD_SIZE	(sizeof(T_ThreadQueueMsg))	

//TAILQ_HEAD(queue_list, _tagThreadQueue) m_tQueue;

typedef struct
{
	int m_iRunFlg;							//thread entity running marking
	int m_iTaskID;							//子线程实体ID
	unsigned long long m_iLastActive;		//last activity time

	TPId	 				m_tHnd;			//handle
	
	TPLock					m_tMux;			//只在事件消息用到
	TPCond					m_tCond;		//只在事件消息用到
	int						m_iEvent;		//只在事件消息用到
	
TAILQ_HEAD(queue_list, _tagThreadQueue) m_tQueue;
	
	void	*pMaster;						//主池体
	void (*pCleanup)(void *);
	void *(*Task)(void *);					//attaching logical task
	void *m_pArg;							//attachment logic parameters

} LH_Thread;

typedef struct
{
	int			m_iThrPoolId;  
	int			m_iThrNum;
#define THRPOOL_NAME_LEN_MAX	(31)			
	unsigned char m_aucName[THRPOOL_NAME_LEN_MAX + 1];
	int 		m_iActiveThrCnt;					//当前活跃线程数目

	TPLock		m_tMutex;				

	TPId	 	m_tSchldThrClean;				//清理线程ID	
	
#define THRPOOL_STATE_RUNNING		(0)	
#define THRPOOL_STATE_CLOSE			(1)
#define THRPOOL_STATE_WAIT_CLOSE	(2)
	volatile int m_iThrPoolState;			//池体状态
	
	void (*pUserHandle)(int, void *);		//用户回掉
	
	LH_Thread	*pThrList;

} T_ThreadPoolEntity; 

#define THRPOOL_LIST_OFFSET	(sizeof(T_ThreadPoolEntity))



/************************************************************************
* 			Variable Common Definition
* **********************************************************************/
typedef struct _tagMonitQueue{    
    int					m_iCmd;    
	int					m_iVal;
    TAILQ_ENTRY(_tagMonitQueue)  _qEntry;    
}T_MonitQueueMsg;   

TAILQ_HEAD(_, _tagMonitQueue) tQueueHead;	//监管线程队列

unsigned char *g_tUserThrPool[LH_THREAD_POOL_SIZE_MAX] = {0};	//线程池
unsigned char *g_tUserThrCor[1] = {0};		//协程
unsigned char *g_tUserThrFason[1] = {0};	//流协程

volatile int g_iRunning = 0;
static int g_iThrPoolUserIndex = 0;	//g_iThrPoolCreatIndex 自增
TPLock		g_MLock;
TPCond		g_MCond;

#define THRPOOL_STACK_SIZE_MONIT	(20*1024)	//监管线程栈空间
pthread_attr_t g_iMonitAttr;
#define THRPOOL_STACK_SIZE_CLEAN	(20*1024)	//主池清理线程栈空间
pthread_attr_t g_iThrPoolCleanAttr;
#define THRPOOL_STACK_SIZE_USR		(50*1024)	//用户子线程栈空间
pthread_attr_t g_iUsrThrAttr;

static int	g_iEvent = 0;


/************************************************************************
* 			static function list
* **********************************************************************/
static void XSleep(int nSec, int nUSec);
static unsigned long long CTimerSec(void);
static void *thread_pool_auto_clean(void *_pArg);
static void *local_monitor(void *_pArg);
static int add_table_item(int _iTTye, void *_pEnt);
static int delete_table_item(int _iTTye, int _iThrID);
static void *get_table_item(int _iTTye, int _iThrID);
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
* Name: 		thread_pool_auto_clean
* Descriptions:	自动清理用户退出的线程，未开启此项的话，只能调用UnInitThreadPool清理
*				
* Parameter:		void *
* Return:		void *
* **********************************************************************/
static void *thread_pool_auto_clean(void *_pArg) //
{
	T_ThreadPoolEntity *ptThrPool = (T_ThreadPoolEntity *)_pArg;
    int i;
    unsigned long long iTm;
    LH_Thread *ptThr = NULL;	

	while ( ptThrPool->m_iThrPoolState == THRPOOL_STATE_RUNNING )
	{
		ptThr = ptThrPool->pThrList;
		XSleep(2,0);

		LockTPLock(ptThrPool->m_tMutex);

		iTm = CTimerSec();
		for (i = 0; i < ptThrPool->m_iThrNum; i++, ptThr++)
		{
			if (THR_IDL_TASK == ptThr->Task && (iTm >= (ptThr->m_iLastActive + LH_THREAD_ENT_IDL_TIME_MAX)))
			{
				ptThr->Task = THR_DIE_TASK;
				ptThr->m_iRunFlg = 0;
				ptThrPool->m_iActiveThrCnt--;
			}
		}

		UnLockTPLock(ptThrPool->m_tMutex);
	}
	
	//printf("Start the clean up program\r\n");
	
	while(ptThrPool->m_iThrPoolState == THRPOOL_STATE_WAIT_CLOSE)
	{
		/* 清除下属所有线程实体 */
		LockTPLock(ptThrPool->m_tMutex);
		/* 检测线程实体是否都清除完成 */
		int iCnt = 0;
		ptThr = ptThrPool->pThrList;
		for(i = 0; i < ptThrPool->m_iThrNum; i++, ptThr++)
		{
			if(ptThr->Task == THR_FREE_TASK || ptThr->Task == THR_DIE_TASK)
			{
				iCnt++;
			}
			else
			{
				printf("thread sid = %d is live, force cancel...\r\n", ptThr->m_iTaskID);
				/* 发送信号强行退出 */
				pthread_cancel(ptThr->m_tHnd);  //注意不是m_iTaskID
			}
		}
		UnLockTPLock(ptThrPool->m_tMutex);

		if (ptThrPool->m_iThrNum == iCnt)
		{
			/* 确保所有线程都退出了，发送信号给监管者销毁 */
			//printf("Ready to clean up for thread pool id = [%d], name = [%s]\r\n", ptThrPool->m_iThrPoolId, ptThrPool->m_aucName);
			
			T_MonitQueueMsg *pMsg = (T_MonitQueueMsg *)calloc(1, sizeof(T_MonitQueueMsg));    
			pMsg->m_iCmd = 1;    
			pMsg->m_iVal = ptThrPool->m_iThrPoolId;    //需要删除的主池ID
			TAILQ_INSERT_TAIL(&tQueueHead, pMsg, _qEntry);    		
			
			LockTPLock(g_MLock);		
			g_iEvent = 1;
			SignalTPCond(g_MCond);
			UnLockTPLock(g_MLock);	
			
			ptThrPool->m_iThrPoolState = THRPOOL_STATE_CLOSE;		
		}
		else
		{
			/* 还有未退出的线程实体,强行pthread_cancel()线程实体 */
			printf("thread pool id = [%d] have no drop out of sub thread number size [%d], please wait... \r\n",ptThrPool->m_iThrPoolId, (ptThrPool->m_iThrNum - iCnt));
			XSleep(1,0);
		}		
	}

	//printf("%s ThrPool Dispatch has Exit !!!!\r\n",__func__);
	return NULL;
}

/************************************************************************
* Name: 		
* Descriptions:监管线程，管理所有主池
* Parameter:		
* Return:     	
* **********************************************************************/
static void *local_monitor(void *_pArg)
{
	int event = 0;
	T_MonitQueueMsg *pMsg = NULL;
	while(g_iRunning)
	{
		LockTPLock(g_MLock);
		WaitTPCond(g_MCond, g_MLock);
		event = g_iEvent;
		UnLockTPLock(g_MLock);	
		
		for (pMsg = TAILQ_FIRST(&tQueueHead); pMsg; pMsg = TAILQ_NEXT(pMsg, _qEntry)) 
		{    
			//printf("get queue cmd = %d, value = %d\n", pMsg->m_iCmd, pMsg->m_iVal); 
			TAILQ_REMOVE(&tQueueHead, pMsg, _qEntry);  
			/* 目前只有删除 */
			delete_table_item(LH_THREAD_TTPYE_POOL, pMsg->m_iVal);
			free(pMsg);
		}
		
		if (TAILQ_EMPTY(&tQueueHead)) 
		{    
			//printf("the tail queue is empty now.\n");       
		}
	
	}

	g_iRunning = 0;
	
	
}


int LH_ThrLibraryEnable(void)
{
	int ret = 0;
	InitTPLock(g_MLock);	
	InitTPCond(g_MCond);
    
	TAILQ_INIT(&tQueueHead);
	
	TPId tTid;
	
	/* 初始化属性 */
	pthread_attr_init(&g_iMonitAttr); 
	pthread_attr_setstacksize(&g_iMonitAttr, THRPOOL_STACK_SIZE_MONIT);
	
	pthread_attr_init(&g_iThrPoolCleanAttr); 
	pthread_attr_setstacksize(&g_iThrPoolCleanAttr, THRPOOL_STACK_SIZE_CLEAN);
	
	pthread_attr_init(&g_iUsrThrAttr); 
	pthread_attr_setstacksize(&g_iUsrThrAttr, THRPOOL_STACK_SIZE_USR);
	
	g_iRunning = 1;
	if(pthread_create(&tTid, &g_iMonitAttr, local_monitor, NULL)!=0)
	{
		printf("清理线程失败,直接退出\r\n");
		
		
		pthread_attr_destroy(&g_iThrPoolCleanAttr); 
		pthread_attr_destroy(&g_iUsrThrAttr); 
		ret = 1;
	}
	pthread_attr_destroy(&g_iMonitAttr); 
	pthread_detach(tTid);	
	
	return ret;
}

int LH_ThrLibraryDisable(void)
{
	g_iRunning = 0;
	
	/* 属性销毁 */
	pthread_attr_destroy(&g_iThrPoolCleanAttr); 
	pthread_attr_destroy(&g_iUsrThrAttr); 	
}

/************************************************************************
* Name: 		
* Descriptions:将新创建的主池加入到数组中，以便管理
* Parameter:		
* Return:     	
* **********************************************************************/
static int add_table_item(int _iTTye, void *_pEnt)
{
	/* 判断类型合法 */
	if (_iTTye >= LH_THREAD_TTPYE_BUTT || _pEnt == NULL )
	{
		printf("参数不合法\r\n");
		return -1;
	}	
	T_ThreadPoolEntity *ptThrPool = (T_ThreadPoolEntity *)_pEnt;
	int i, iRet = -1;
	LockTPLock(g_MLock);
	for(i=0; i< LH_THREAD_POOL_SIZE_MAX;i++)
	{
		if (!g_tUserThrPool[i])
		{
			/* 找到第一个不为空的位置 */
			g_tUserThrPool[i] = _pEnt;

			ptThrPool->m_iThrPoolId = i; 
			
			//ptThrPool->pUserHandle(ptThrPool->m_iThrPoolId, "malloc ok");
			
			iRet = ptThrPool->m_iThrPoolId;
			break;
		}
	}

	UnLockTPLock(g_MLock);
	return iRet;
}

static int delete_table_item(int _iTTye, int _iThrID)
{
	/* 判断类型合法 */
	if (_iTTye >= LH_THREAD_TTPYE_BUTT )
	{
		printf("参数不合法\r\n");
		return -1;
	}	
	int iRet = -1;
	T_ThreadPoolEntity *ptThrPool = NULL;
	LockTPLock(g_MLock);
	if (ptThrPool = (T_ThreadPoolEntity *)(g_tUserThrPool[_iThrID]))
	{
		if (ptThrPool->m_iThrPoolId == _iThrID)
		{
			/* 删除 */
			//printf("free pool ID = %d, %x\r\n",_iThrID, (unsigned int)(long)g_tUserThrPool[_iThrID]);
			g_tUserThrPool[_iThrID] = NULL;	
			
			/* 调用一次主池回掉 ,放在这里结构不好*/
			if (ptThrPool->pUserHandle)
			{
				ptThrPool->pUserHandle(ptThrPool->m_iThrPoolId, "clean up");
			}
			
			free(ptThrPool);		

			iRet = 0;
		}
		else
		{
			printf("error id = %d\r\n", _iThrID);
		}
	}
	else
	{
		printf("not found id = %d\r\n", _iThrID);
	}
	UnLockTPLock(g_MLock);
	
	return iRet;
}


static void *get_table_item(int _iTTye, int _iThrID)
{
	T_ThreadPoolEntity *ptThrPool = NULL;
	LockTPLock(g_MLock);
	if (ptThrPool = (T_ThreadPoolEntity *)g_tUserThrPool[_iThrID])
	{
		if (ptThrPool->m_iThrPoolId != _iThrID && ptThrPool->m_iThrPoolState )
		{
			ptThrPool = NULL;
		}
	}
	UnLockTPLock(g_MLock);
	return ptThrPool;
}

/************************************************************************
* Name: 		InitThreadPool(CLI-API)
* Descriptions:	私有池初始化(没有可重入，多次调用等于多次创建)[同步]
* Parameter:	
*				1.const char *:主池名字
*				2.unsigned char：主池包含的子线程个数
*				3.void (*_pHandle)(int, void *)：(目前只用在子线程清理上，也可以当钩子函数使用)
* Return:		-1：失败/0+：主池ID
* **********************************************************************/
int InitThreadPool(const char *_pName, unsigned char _ucThrNum, void (*_pHandle)(int, void *))
{
	int iRet = -1,i;
	T_ThreadPoolEntity *ptThrPool = NULL;
	TPId tTid;
	/* 1.判断参数合法性 */
	if (strlen(_pName) > THRPOOL_NAME_LEN_MAX || _ucThrNum > LH_THREAD_ENT_SIZE_MAX )
	{
		printf("参数不合法\r\n");
		goto EXIT;
	}
	
	/* 2.初始化私有池 */
	ptThrPool = malloc(sizeof(T_ThreadPoolEntity)+ sizeof(LH_Thread) * _ucThrNum);
	
	//printf("malloc address %x \r\n",ptThrPool);
	
	ptThrPool->pThrList = (LH_Thread *)(ptThrPool + THRPOOL_LIST_OFFSET);
	ptThrPool->m_iActiveThrCnt = 0;
	InitTPLock(ptThrPool->m_tMutex);	
	
	ptThrPool->m_iThrNum	= _ucThrNum;
	ptThrPool->pUserHandle	= _pHandle;
	
	memcpy(&ptThrPool->m_aucName[0], _pName, strlen(_pName));
	ptThrPool->m_aucName[strlen(_pName)] = 0;
	
	LH_Thread *pThread = ptThrPool->pThrList;
	/* 3.初始化池体单线程 */
	for (i = 0; i < _ucThrNum; i++, pThread++)
	{
		pThread->m_iTaskID	= -1;
		pThread->m_iRunFlg	= 0;
		pThread->m_iLastActive = 0;
		pThread->Task = THR_FREE_TASK;

		InitTPLock(pThread->m_tMux);
		InitTPCond(pThread->m_tCond);

		TAILQ_INIT(&pThread->m_tQueue);
		
		pThread->m_iEvent	= 0;
		pThread->pMaster	= NULL;
		pThread->pCleanup	= NULL;
		pThread->m_pArg		= NULL;
	}
	
	/* 4.初始化池体清理模式 （默认开启）*/
	//if(THR_MODE_BROOM == _emMode)
	{
		/* 使用自清理，栈空间20k */
		ptThrPool->m_iThrPoolState = THRPOOL_STATE_RUNNING;
		if(pthread_create(&tTid, &g_iThrPoolCleanAttr, thread_pool_auto_clean, ptThrPool)!=0)
		{
			printf("清理线程失败,直接退出\r\n");
			
			free(ptThrPool);
			ptThrPool = NULL;
			goto EXIT;
		}
		
		pthread_detach(tTid);
		ptThrPool->m_tSchldThrClean = tTid;
	}
	
	/* 将创建的主池加入监管线程 */
	iRet = add_table_item(LH_THREAD_TTPYE_POOL, (void *)ptThrPool);
	if (iRet < 0)
	{
		/* 加入失败 */
		iRet = -1;
	}
EXIT:
	return iRet;
}
/************************************************************************
* Name: 		UnInitThreadPool
* Descriptions:	此函数返回不能说明成功销毁 [异步释放，有点慢]，此函数会延时强行清理子线程
* Parameter:	
*				1.int：要销毁的主池ID
* Return:		
* **********************************************************************/
void UnInitThreadPool(int _iThrPoolID)
{
	T_ThreadPoolEntity *ptThrPool = (T_ThreadPoolEntity *)get_table_item(LH_THREAD_TTPYE_POOL, _iThrPoolID);
	
	if (ptThrPool)
	{
		/* 1.先让子线程实体关闭 */
		int i;
		LH_Thread *ptThr = NULL;
		ptThr = ptThrPool->pThrList;
		for(i = 0; i < ptThrPool->m_iThrNum; i++, ptThr++)
		{	
			if(ptThr->m_iRunFlg)
			{
				ptThr->m_iRunFlg = 0;
			}
		}
		
		/* 2.再关闭清理线程 */
		ptThrPool->m_iThrPoolState = THRPOOL_STATE_WAIT_CLOSE;
	}
	else
	{
		printf("unknown ID\r\n");
	}
}

/************************************************************************
* Name: 		
* Descriptions: 打印当前活着的主池ID
* Parameter:		
* Return:     	
* **********************************************************************/
void PrintInfoThreadPool(void)
{
	int i;
	printf("=====================================================================\r\n");
	for(i=0; i< LH_THREAD_POOL_SIZE_MAX;i++)
	{
		printf("User Table[%d]:\r\n",i);
		if (g_tUserThrPool[i])
		{
			T_ThreadPoolEntity *pEnt = (T_ThreadPoolEntity *)g_tUserThrPool[i];
			printf("	name:%s ,id:%d ,num:%d ,active:%d \r\n",pEnt->m_aucName, pEnt->m_iThrPoolId, pEnt->m_iThrNum, pEnt->m_iActiveThrCnt);
		}		
	}
	printf("=====================================================================\r\n");
}

/************************************************************************
* Name: 		clean_all_queue
* Descriptions: 强行释放线程下的所有队列
* Parameter:		
* Return:     	
* **********************************************************************/
static void clean_all_queue(LH_Thread *_ptThr)  
{
	T_ThreadQueueMsg *pMsg = NULL;
	for (pMsg = TAILQ_FIRST(&_ptThr->m_tQueue); pMsg; pMsg = TAILQ_NEXT(pMsg, _qThrEntry)) 
	{    
		TAILQ_REMOVE(&_ptThr->m_tQueue, pMsg, _qThrEntry);  
		free(pMsg);
	}	
}

/************************************************************************
* Name: 		except_abort
* Descriptions: 异常终止
* Parameter:		
* Return:     	
* **********************************************************************/
static void except_abort(void *_pArg)  
{
	LH_Thread *ptThr = (LH_Thread *)_pArg;
    printf(".......except_abort thread id = %d\n",ptThr->m_iTaskID); 
	/* 1.需要强行释放锁 */
	UnLockTPLock(ptThr->m_tMux);
	/* 2.需要强行释放下属队列 */
	clean_all_queue(ptThr);

	/* 3.异常终止需要将THR_FREE_TASK */
	ptThr->m_iRunFlg = 0;
	ptThr->Task = THR_FREE_TASK;
	
	/* 4.调用子线程清理函数 */
	if (ptThr->pCleanup)
	{
		ptThr->pCleanup(ptThr->m_pArg);
	}
}  

/************************************************************************
* Name: 		inter_thread_task
* Descriptions:
* Parameter:		
* Return:     	
* **********************************************************************/
static void *inter_thread_task(void *_pArg)
{
	LH_Thread *ptThr = (LH_Thread *)_pArg;
	
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
	pthread_cleanup_push(except_abort, _pArg);
	
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
		
		//printf("attachment func has exit ~~");
		ptThr->m_iLastActive = CTimerSec();	//用户的逻辑退出后记录时间等待清理
		//ptThr->m_pArg = NULL;
		ptThr->Task = THR_IDL_TASK;	//After the thread exits, it automatically becomes idle, but it is not destroyed

	}

EXIT:
	printf("Thread State Recovery.. %d \r\n",ptThr->m_iTaskID);
	ptThr->Task = THR_FREE_TASK;  //准备释放线程所以是THR_FREE_TASK 而不是 THR_IDL_TASK
	/* 释放下属队列 */
	clean_all_queue(ptThr);

	/* 调用子线程清理函数 */
	if (ptThr->pCleanup)
	{
		ptThr->pCleanup(ptThr->m_pArg);
	}
#if 0	
	/* 主池回掉 */
	if (((T_ThreadPoolEntity *)(ptThr->pMaster))->pUserHandle)
	{
		((T_ThreadPoolEntity *)(ptThr->pMaster))->pUserHandle(ptThr->m_iTaskID,"line..");
	}
#endif
	pthread_cleanup_pop(0);
	return NULL;
}

/************************************************************************
* Name: 		inter_create_thread
* Descriptions:
* Parameter:		
* Return:     	
* **********************************************************************/
static int inter_create_thread(void *(*_Task)(void *), void *_pArg, void *_pUserClean(void *), T_ThreadPoolEntity *_ptPool)
{
	int iRet = -1;
	int i;
	int iThrId = -1;
	int iFreeId = -1;
	LH_Thread *ptThr = NULL;

    TPId tTid;

	LockTPLock(_ptPool->m_tMutex);
	
	ptThr = _ptPool->pThrList;
	for (i = 0; i < _ptPool->m_iThrNum; i++, ptThr++)
	{	
		if (THR_FREE_TASK == ptThr->Task)
		{
			iFreeId = i;
			break;
		}
		else if (THR_IDL_TASK == ptThr->Task)
		{
			ptThr->m_iLastActive = CTimerSec();
			iThrId = i;
			break;
		}
	}

	if (iThrId < 0) //No idle thread
	{
		if (iFreeId < 0) 
		{
			/* The thread pool is full 目前线程池满的话，不能创建 */
#if 0		
			if (pthread_create(_ptThr, &g_iUsrThrAttr, _Task, _pArg) == 0)
			{
				iRet = 0;
				pthread_detach(*_ptThr);
			}
#endif
			printf("thr pool is full.");  //Can not be created
			goto EXIT;
		}
		else //Create a new thread and add a thread pool
		{
			ptThr = _ptPool->pThrList + iFreeId;
			ptThr->m_iRunFlg = 1;

			ptThr->m_pArg = _pArg;
			ptThr->Task = THR_WAIT_TASK;  //刚创建完，还没有附着逻辑，所以需要线程等待

			if (pthread_create(&tTid, &g_iUsrThrAttr, inter_thread_task, ptThr) != 0)
			{
				ptThr->Task = THR_FREE_TASK;
				goto EXIT;
			}
			pthread_detach(tTid);
			
			ptThr->m_tHnd = tTid;
			//printf("***************  pthread id = %d, %d\r\n", tTid, ptThr->m_tHnd);
			_ptPool->m_iActiveThrCnt++;

			iThrId = iFreeId;
			//printf("Create new thread in pool = %p",ptThr->Task );
		}
	}
	/* Assignment of tasks for the thread in the pool, the count is diminishing */ 
	ptThr = _ptPool->pThrList + iThrId;
	ptThr->m_iTaskID = iThrId;
	ptThr->m_iLastActive = CTimerSec(); //激活时间
	ptThr->m_pArg = _pArg;
	ptThr->Task = _Task;		//由THR_WAIT_TASK -》_Task 可运行
	ptThr->pMaster	= _ptPool;
	ptThr->pCleanup	= _pUserClean;
	//*_ptThr = -1;
	//printf("new task add into pool thread id = %d , = %p",iThrId, ptThr->Task );
	iRet = iThrId;	
EXIT:	
	UnLockTPLock(_ptPool->m_tMutex);
	return iRet;
}

/************************************************************************
* Name: 		StartThreadInPool
* Descriptions: 建议用户使用时将自己业务里的所有变量都挂在_pArg，以便退出时free
* Parameter:	
*				1.int：主池ID
*				2.void *(*_Task)(void *)：子线程函数
*				3.void *：子线程运行参数
*				4.void *_pUserClean(void *)：子线程清理函数
* Return:	-1：失败/0+ 子线程id
* **********************************************************************/
int StartThreadInPool(int _iThrPoolID, void *(*_Task)(void *), void *_pArg, void *_pUserClean(void *))
{
	T_ThreadPoolEntity *ptThrPool = (T_ThreadPoolEntity *)get_table_item(LH_THREAD_TTPYE_POOL, _iThrPoolID);
	int iRet = -1;
	//printf("0000000\r\n");
	if (ptThrPool)
	{
		if (_Task)
		{
			//printf("111111111\r\n");
			iRet = inter_create_thread(_Task, _pArg, _pUserClean, ptThrPool);
		}
		else
		{
			printf("task can not NULL\r\n");
		}
	}
	else
	{
		printf("unknown pool ID\r\n");
	}	
EXIT:	
	return iRet;
}

/************************************************************************
* Name: 		StopForceThreadInPool
* Descriptions:	清除指定主池中的一个子线程实体
* Parameter:		
*				1.int：主池ID
*				2.int：子线程ID
* Return:	
* **********************************************************************/
int StopForceThreadInPool(int _iThrPoolID, int _iThrID)
{
	T_ThreadPoolEntity *ptThrPool = (T_ThreadPoolEntity *)get_table_item(LH_THREAD_TTPYE_POOL, _iThrPoolID);
	int iRet = -1;
	if (ptThrPool)
	{
		LH_Thread *ptThr = NULL;
		ptThr = ptThrPool->pThrList + _iThrID;
		pthread_cancel(ptThr->m_tHnd);
		iRet = 0;
	}
	else
	{
		printf("unknown pool ID\r\n");
	}	

	return iRet;
}


/************************************************************************
* Name: 	GetTskSelfIDThreadInPool
* Descriptions:此函数有点多余但兼容通用
* Parameter:	
* Return:	-1不存在 0+存在
* **********************************************************************/
int GetTskSelfIDThreadInPool(int _iThrPoolID)
{
	T_ThreadPoolEntity *ptThrPool = (T_ThreadPoolEntity *)get_table_item(LH_THREAD_TTPYE_POOL, _iThrPoolID);
	int iRet = -1, i;
	if (ptThrPool)
	{
		LH_Thread *ptThr = NULL;
		TPId tTid = pthread_self();
		ptThr = ptThrPool->pThrList;
		for (i = 0; i < ptThrPool->m_iThrNum; i++, ptThr++)
		{
			if (ptThr->m_tHnd == tTid)
			{
				iRet = ptThr->m_iTaskID;
			}
		}			
	}
	else
	{
		printf("非法ID\r\n");
	}	
	
	return iRet;
}
/************************************************************************
* Name: 		
* Descriptions:打印指定主池中的所有子线程
* Parameter:		
* Return:	
* **********************************************************************/
int PrintInfoThreadInPool(int _iThrPoolID)
{
	T_ThreadPoolEntity *ptThrPool = (T_ThreadPoolEntity *)get_table_item(LH_THREAD_TTPYE_POOL, _iThrPoolID);
	int iRet = -1, i;
	if (ptThrPool)
	{
		printf("thread pool ID = [%d], name = [%s], thread number = [%d], thread active = [%d]\r\n", 
		ptThrPool->m_iThrPoolId, ptThrPool->m_aucName, ptThrPool->m_iThrNum, ptThrPool->m_iActiveThrCnt);
		
		LH_Thread *ptThr = NULL;
		ptThr = ptThrPool->pThrList;
		for (i = 0; i < ptThrPool->m_iThrNum; i++, ptThr++)
		{
			if (ptThr->m_iRunFlg)
			{
				/* 需要的信息再加 */
				printf("%d. task id = %d, task address = %x\r\n", i + 1, ptThr->m_iTaskID, ptThr->Task);
			}
		}	
		iRet = 0;
	}
	else
	{
		printf("unknown pool ID\r\n");
	}	
	
	return iRet;	
}

/************************************************************************
* Name: 		
* Descriptions:
* Parameter:		
* Return:	
* **********************************************************************/
static int check_pool_available(int _iThrPoolID, int _iThrID, LH_Thread **_ptThr)
{
	T_ThreadPoolEntity *ptThrPool = (T_ThreadPoolEntity *)get_table_item(LH_THREAD_TTPYE_POOL, _iThrPoolID);
	int iRet = -1;
	if (ptThrPool)
	{
		LH_Thread *ptThr = NULL;
		ptThr = ptThrPool->pThrList + _iThrID;
		*_ptThr = ptThr;

		iRet = 0;
	}
	else
	{
		printf("非法ID\r\n");
	}		
	return 0;
}
/************************************************************************
* Name: 		
* Descriptions:	给指定子线程设置事件
* Parameter:		
*				1.int：主池ID
*				2.int：子线程ID
*				3.int：设置的事件
* Return:     	
* **********************************************************************/
int ThreadTskSetEvent(int _iThrPoolID, int _iThrID, int _pSetEvent)
{
	int iRet = -1;
	LH_Thread *ptThr = NULL;

	if (!check_pool_available(_iThrPoolID, _iThrID, &ptThr))
	{
		if(ptThr->m_iRunFlg)
		{
			LockTPLock(ptThr->m_tMux);
			ptThr->m_iEvent |= _pSetEvent;
			SignalTPCond(ptThr->m_tCond);
			UnLockTPLock(ptThr->m_tMux);
			iRet = 0;
		}			
	}
			
	return iRet;
}

int ThreadTskCleanEvent(int _iThrPoolID, int _iThrID, int _pCleanEvent)
{
	int iRet = -1;
	LH_Thread *ptThr = NULL;
	if (!check_pool_available(_iThrPoolID, _iThrID, &ptThr))
	{
		if(ptThr->m_iRunFlg)
		{
			LockTPLock(ptThr->m_tMux);
			ptThr->m_iEvent &= ~_pCleanEvent;
			SignalTPCond(ptThr->m_tCond);
			UnLockTPLock(ptThr->m_tMux);
			iRet = 0;
		}
	}		
	return iRet;	
}

/* LH_THREAD_EVENT_FLAG_NOWAIT：非阻塞 */
int ThreadTskWaitForEvent(int _iThrPoolID, int _iThrID, int *_pOut_evt, int _pCflag)
{
	int iRet = -1;
	LH_Thread *ptThr = NULL;
	
	if (!check_pool_available(_iThrPoolID, _iThrID, &ptThr))
	{
		if(ptThr->m_iRunFlg)
		{
			if(_pCflag == LH_THREAD_EVENT_FLAG_NOWAIT) 
			{
				if(!TryLockTPLock(ptThr->m_tMux))
				{
					/* 获得锁 */
					if( ptThr->m_iEvent == 0 )
					{
						/* 无事件,需要解锁 */
						UnLockTPLock(ptThr->m_tMux);	
						goto EXIT;
					}
				}
				else
				{
					goto EXIT;
				}			
			}
			else //if(_pCflag == LH_THREAD_EVENT_FLAG_WAITFOREVER) 
			{
				LockTPLock(ptThr->m_tMux);
				while (ptThr->m_iEvent == 0)
				{
					/* 完全没有事件 */
					WaitTPCond(ptThr->m_tCond, ptThr->m_tMux);
				}

			}
			
			/* 有事件跳出,取出事件 */
			*_pOut_evt = ptThr->m_iEvent;
			//THR_LOGE("gett : ptThr->m_iEvent = %#010x, _pOut_evt = %#010x",ptThr->m_iEvent, *_pOut_evt);  
			ptThr->m_iEvent = 0;
			iRet = 0;		

			UnLockTPLock(ptThr->m_tMux);	
		}		
	}
EXIT:
	//THR_LOGD("gett : iRet = %d, ptThr->m_iEvent = %#010x, _pOut_evt = %#010x",iRet,ptThr->m_iEvent, *_pOut_evt);  
	return iRet;
}

/************************************************************************
* Name:
* Descriptions:
* Parameter:		_pQEvent: 队列事件
* Return:     	
* **********************************************************************/
int ThreadTskPostToQueue(int _iThrPoolID, int _iThrID, int _pQEvent, const char *_pData, int _iLen)
{
	int iRet = -1;
	LH_Thread *ptThr = NULL;
	if (!check_pool_available(_iThrPoolID, _iThrID, &ptThr))
	{
		if(ptThr->m_iRunFlg)
		{
			LockTPLock(ptThr->m_tMux);
			
			T_ThreadQueueMsg *pMsg = (T_ThreadQueueMsg *)calloc(1, THREAD_QUEUE_HEAD_SIZE + _iLen);
			if (!pMsg)
			{
				/* 分配失败 */
				printf("calloc error \r\n");
				goto EXIT; 
			}
			pMsg->m_iTskId	= _iThrID;    
			pMsg->m_iQEvent	= _pQEvent;
			pMsg->m_iLen	= _iLen;
			pMsg->pData		= (char *)(pMsg + THREAD_QUEUE_HEAD_SIZE);
			memcpy(pMsg->pData, _pData, _iLen);

			TAILQ_INSERT_TAIL(&ptThr->m_tQueue, pMsg, _qThrEntry);  			

			ptThr->m_iEvent |= LH_THREAD_TTPYE_EVT_QUEUE;
			SignalTPCond(ptThr->m_tCond);
			UnLockTPLock(ptThr->m_tMux);
			iRet = 0;
		}
	}

EXIT:
	return iRet;
}

/************************************************************************
* Name:
* Descriptions:
* Parameter:		
*				1.int：主池ID
*				2.int：子线程ID
*				3.int *：获得消息事件
*				4.char*:数据存放的buf
*				5.int：想要读取的字节数
*				6.int *：实际读取的字节数
* Return:     	-1：队列空，0：队列有数据
* **********************************************************************/
int ThreadTskGetMsgFromQueue(int _iThrPoolID, int _iThrID, int *_pQEvent, char *_pData, int _iLen, int *_iReadLen)
{
	int iRet = -1;
	LH_Thread *ptThr = NULL;

	if (!check_pool_available(_iThrPoolID, _iThrID, &ptThr))
	{
		if(ptThr->m_iRunFlg)
		{
			LockTPLock(ptThr->m_tMux);	
			
			T_ThreadQueueMsg *pMsg = NULL;
			if (pMsg = TAILQ_FIRST(&ptThr->m_tQueue))
			{
				printf("get queue msg task id = %d, event = %d, len = %d\n", pMsg->m_iTskId, pMsg->m_iQEvent, pMsg->m_iLen); 
				TAILQ_REMOVE(&ptThr->m_tQueue, pMsg, _qThrEntry);
				if (pMsg->m_iTskId == _iThrID)
				{
					if (_pQEvent)
					{
						*_pQEvent = pMsg->m_iQEvent;
					}
					int iLen = (_iLen < pMsg->m_iLen)?_iLen:pMsg->m_iLen;
					if (_iReadLen)
					{
						*_iReadLen = iLen;
					}					
					memcpy(_pData, pMsg->pData, iLen);
					iRet = 0;
				}
				
				free(pMsg);   //如果 pMsg->m_iTskId ！= _iThrID 直接释放
				pMsg = NULL;
			}

			UnLockTPLock(ptThr->m_tMux);
		}
	}

	return iRet;
}
/* 返回当前队列有多少消息 ,是否打印消息*/
int ThreadTskCheckMsgQueueInfo(int _iThrPoolID, int _iThrID, unsigned char _fIsPrint)
{
	int iRet = -1,i = 0;
	LH_Thread *ptThr = NULL;

	if (!check_pool_available(_iThrPoolID, _iThrID, &ptThr))
	{
		if(ptThr->m_iRunFlg)
		{
			LockTPLock(ptThr->m_tMux);	
			
			T_ThreadQueueMsg *pMsg = NULL;
			for (pMsg = TAILQ_FIRST(&ptThr->m_tQueue); pMsg; pMsg = TAILQ_NEXT(pMsg, _qThrEntry)) 
			{    
				i++;
				if (_fIsPrint)
				{
					/* 打印消息信息 */
					printf(" %d.dst task id = %d, event = %d, len = %d\n", i,pMsg->m_iTskId, pMsg->m_iQEvent, pMsg->m_iLen); 
				}
			}
			iRet = i;
			UnLockTPLock(ptThr->m_tMux);
		}
	}

	return iRet;
}

/************************************************************************
* Name:
* Descriptions:直接创建线程(简易模式)
* Parameter:		
* Return:     	
* **********************************************************************/
int LH_SStartThread(const char *_pName, void *(_Task)(void *), void *_pArg)
{
	int iRet;
	TPId tTid;
	pthread_attr_t iThrAttr;

	pthread_attr_init(&iThrAttr);

	iRet = pthread_attr_setstacksize(&iThrAttr, THRPOOL_STACK_SIZE_USR); // 20K
	if(0 != iRet) 
	{
		iRet = pthread_attr_destroy(&iThrAttr);
		printf ("pthread_attr_setstacksize error\r\n");
		return -1;
	}

	iRet = pthread_create(&tTid, &iThrAttr, _Task, _pArg);
	if(0 != iRet) 
	{
		iRet = pthread_attr_destroy(&iThrAttr);
		printf("pthread_create error\r\n");
		return -1;
	}
	pthread_attr_destroy(&iThrAttr);
	pthread_detach(tTid);	
	printf("thread id %lx, thread name %s \r\n", tTid, _pName);
	return tTid;
}






