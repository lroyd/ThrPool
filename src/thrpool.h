/*************************************************************************
	> File Name: thrpool.h
	> Author: lroyd
	> Mail: htzhangxmu@163.com
	> Created Time: 
 ************************************************************************/
#ifndef _THR_POOL_H_
#define _THR_POOL_H_


/************************************************************************
* 			Macro Definition
* **********************************************************************/
#define LH_THREAD_EVENT_FLAG_NOWAIT				(0)
#define LH_THREAD_EVENT_FLAG_WAITFOREVER		(1)


/*********** 简易模式 *******************************************/
extern int LH_SStartThread(const char *_pName, void *(_Task)(void *), void *_pArg);


/*********** 功能模式 *******************************************/

typedef enum
{
	LH_THREAD_TTPYE_EVT_QUEUE = (int)7,			//队列有消息待处理事件
	/*
		用户自定义消息事件
	*/
	LH_THREAD_TTPYE_EVT_BUTT,
}LH_THREAD_TTYPE_EVENT;	//线程事件类型，区别队列事件


extern int LH_ThrLibraryEnable(void);
extern int LH_ThrLibraryDisable(void);

extern int InitThreadPool(const char *_pName, unsigned char _ucThrNum, void (*_pHandle)(int, void *));
extern void UnInitThreadPool(int _iThrPoolID);
extern void PrintInfoThreadPool(void);


extern int StartThreadInPool(int _iThrPoolID, void *(*_Task)(void *), void *_pArg, void *_pUserClean(void *));
extern int StopForceThreadInPool(int _iThrPoolID, int _iThrID);
extern int GetTskSelfIDThreadInPool(int _iThrPoolID);
extern int PrintInfoThreadInPool(int _iThrPoolID);

extern int ThreadTskSetEvent(int _iThrPoolID, int _iThrID, int _pSetEvent);
extern int ThreadTskCleanEvent(int _iThrPoolID, int _iThrID, int _pCleanEvent);
extern int ThreadTskWaitForEvent(int _iThrPoolID, int _iThrID, int *_pOut_evt, int _pCflag);

extern int ThreadTskPostToQueue(int _iThrPoolID, int _iThrID, int _pQEvent, const char *_pData, int _iLen);
extern int ThreadTskGetMsgFromQueue(int _iThrPoolID, int _iThrID, int *_pQEvent, char *_pData, int _iLen, int *_iReadLen);
extern int ThreadTskCheckMsgQueueInfo(int _iThrPoolID, int _iThrID, unsigned char _fIsPrint);









#endif


