/*************************************************************************
	> File Name: thrpool.h
	> Author: lroyd
	> Mail: htzhangxmu@163.com
	> Created Time: 
 ************************************************************************/
#ifndef _THR_POOL_H_
#define _THR_POOL_H_

#include "common.h" 


/************************************************************************
* 			Macro Definition
* **********************************************************************/
#define LH_THREAD_EVENT_FLAG_NOWAIT				(0)
#define LH_THREAD_EVENT_FLAG_WAITFOREVER		(1)


/*********** ����ģʽ *******************************************/
extern int SStartThread(const char *_pName, void *(_Task)(void *), void *_pArg);


/*********** ����ģʽ *******************************************/

typedef enum
{
	LH_THREAD_TTPYE_EVT_QUEUE = (int)7,			//��������Ϣ�������¼�
	/*
		�û��Զ�����Ϣ�¼�
	*/
	LH_THREAD_TTPYE_EVT_BUTT,
}LH_THREAD_TTYPE_EVENT;	//�߳��¼����ͣ���������¼�


GH_DECL(int) LH_ThrLibraryEnable(void);
GH_DECL(int) LH_ThrLibraryDisable(void);

GH_DECL(int) InitThreadPool(const char *, unsigned char , void (*_pHandle)(int, void *), int *);
GH_DECL(int) UnInitThreadPool(int );
GH_DECL(int) PrintInfoThreadPool(void);


GH_DECL(int) StartThreadInPool(int , void *(*_Task)(void *), void *, void *_pUserClean(void *), int *);
GH_DECL(int) StopForceThreadInPool(int , int );
GH_DECL(int) GetTskSelfIDThreadInPool(int );
GH_DECL(int) PrintInfoThreadInPool(int );

GH_DECL(int) ThreadTskSetEvent(int , int , int );
GH_DECL(int) ThreadTskCleanEvent(int , int , int );
GH_DECL(int) ThreadTskWaitForEvent(int , int , int *, int );

GH_DECL(int) ThreadTskPostToQueue(int , int , int , const char *, int );
GH_DECL(int) ThreadTskGetMsgFromQueue(int , int , int *, char *, int , int *);
GH_DECL(int) ThreadTskCheckMsgQueueInfo(int , int , unsigned char , int *);









#endif


