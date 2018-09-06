/*************************************************************************
	> File Name: test.c
	> Author: 
	> Mail: 
	> Created Time: 2018年03月01日 星期四 09时47分31秒
 ************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int mid = -1, sid[10];

void CALLBACK(int _id, void *buffer)
{
	printf("call back : id = %d, data = %s \r\n", _id, (char *)buffer);
	//PrintInfoThreadPool();
}

///////////////////////////////////////////////////////////////////////
#define SIMPLE_TEST




///////////////////////////////////////////////////////////////////////
//#define CREATE_POOL_TEST 
///////////////////////////////////////////////////////////////////////
//#define CREAT_THREAD_TEST 

void test(void *_pArg)
{
	while(1)
	{
		sleep(1);
		printf("test %s\r\n", (char *)_pArg);
	}
	
}
///////////////////////////////////////////////////////////////////////
//#define EVENT_TEST	//基础事件测试

void test0(void *_pArg)
{
	printf("..%s\r\n", (char *)_pArg);
	int event = 0;
	while(1)
	{
		if (!ThreadTskWaitForEvent(mid, sid[0], &event, 0))
		{
			printf("0. get event = %d\r\n",event);
		}
		sleep(1);
		printf("this %s\r\n",(char *)_pArg);
	}
}

void test1(void *_pArg)
{
	printf("..%s\r\n", (char *)_pArg);
	int event = 0;
	while(1)
	{
		if (!ThreadTskWaitForEvent(mid, sid[1], &event, 1))
		{
			printf("1. get event = %d\r\n",event);
		}
		//sleep(1);
		//printf("this %s\r\n",(char *)_pArg);
	}
	
}

void test2(void *_pArg)
{
	printf("..%s\r\n", (char *)_pArg);
	int event = 0;
	while(1)
	{
		if (!ThreadTskWaitForEvent(mid, sid[2], &event, 1))
		{
			printf("2. get event = %d\r\n",event);
		}
		//sleep(1);
		//printf("this %s\r\n",(char *)_pArg);
	}
	
}
///////////////////////////////////////////////////////////////////////
//#define QUEUE_TEST //基础队列测试

void qtest0(void *_pArg)
{
	printf("..%s\r\n", (char *)_pArg);
	int event = 0;
	int sid = GetTskSelfIDThreadInPool(mid);
	while(1)
	{
		if (!ThreadTskWaitForEvent(mid, sid, &event, 0))
		{
			printf("0. get event = %d\r\n",event);
			
			if (event = 7) //LH_THREAD_TTPYE_EVT_QUEUE
			{
				int q_evt= 0, read_len = 0;
				char buf[32] = {0};
				while(!ThreadTskGetMsgFromQueue(mid, sid, &q_evt, buf, 64, &read_len))
				{
					/* 多次接收记得清除，否则会有残留数据 */
					printf("user %d, %d, %s\r\n",q_evt, read_len, buf);
					memset(buf, 0, 32);
				}
				
			}

		}
		sleep(1);
		printf("this %s\r\n",(char *)_pArg);
	}
	
}

void qtest1(void *_pArg)
{
	printf("..%s\r\n", (char *)_pArg);
	int event = 0;
	while(1)
	{
		if (!ThreadTskWaitForEvent(mid, sid[1], &event, 1))
		{
			printf("1. get event = %d\r\n",event);
		}
		//sleep(1);
		//printf("this %s\r\n",(char *)_pArg);
	}
	
}

void qtest2(void *_pArg)
{
	printf("..%s\r\n", (char *)_pArg);
	int event = 0;
	while(1)
	{
		if (!ThreadTskWaitForEvent(mid, sid[2], &event, 1))
		{
			printf("2. get event = %d\r\n",event);
		}
	}
	
}

void qclean(void *_pArg)
{
	printf(" user clean ..%s\r\n", (char *)_pArg);
	
}

///////////////////////////////////////////////////////////////////////
int main(void)
{
	LH_ThrLibraryEnable();
	sleep(1);
	int i;
	int aid[10];	//建议全部初始化-1
#ifdef SIMPLE_TEST
/***********************************************************************
*	功能：简易模式创建线程
*	验证：	
*	
************************************************************************/
	LH_SStartThread("simple test", test, "1");
	LH_SStartThread("simple test", test, "2");
	printf("+++++++++++++++ SIMPLE_TEST finish +++++++++++++++\r\n");
#endif	
	
#ifdef CREATE_POOL_TEST	
/***********************************************************************
*	功能：创建10个主池，并释放
*	验证：	1.是否正确创建释放
*			2.回掉是否都有调用
************************************************************************/
	int id = -1;
	

	/* A.顺序 */
	printf("A. \r\n");
	for(i=0;i<10;i++)
	{
		aid[i] = InitThreadPool("pool test", 5, CALLBACK);
		printf("aid[%d] = %d \r\n",i,aid[i]);
	}
	
	PrintInfoThreadPool();
	sleep(1);
	for(i=0;i<10;i++)
	{
		UnInitThreadPool(aid[i]); //注意：清理完aid[i]变量本身的值还存在，需要用户置-1，以免误用
		aid[i] = -1;
	}	
	//sleep(5);
	PrintInfoThreadPool();
	
	/* B.随机 */
	int rol = rand()%10;
	printf("B. cread rand = %d \r\n",rol);
	UnInitThreadPool(aid[rol]);
	
	aid[rol] = InitThreadPool("pool test", 5, CALLBACK);
	printf("aid[%d] = %d \r\n",rol,aid[i]);
	PrintInfoThreadPool();

	UnInitThreadPool(aid[rol]);
	
	UnInitThreadPool(aid[rol]);
	sleep(5);
	PrintInfoThreadPool();
	printf("+++++++++++++++ CREATE_POOL_TEST finish +++++++++++++++\r\n");
#endif

#ifdef CREAT_THREAD_TEST
/***********************************************************************
*	功能：创建1个主池，5个子线程
*	验证：	1.是否正确创建释放
*			2.回掉是否都有调用
************************************************************************/

	mid = InitThreadPool("create test", 6, CALLBACK);

	for(i=0;i<5;i++)
	{
		sid[i] = StartThreadInPool(mid, test, "fuck", qclean);
		printf("sid = [%d]\r\n",sid[i]);
	}
	PrintInfoThreadInPool(mid);
	sleep(3);
	StopForceThreadInPool(mid, sid[2]);
	sleep(5);
	PrintInfoThreadInPool(mid);
	UnInitThreadPool(mid);
	sleep(5);
	printf("+++++++++++++++ CREAT_THREAD_TEST finish +++++++++++++++\r\n");
#endif


#ifdef EVENT_TEST
/***********************************************************************
*	功能：创建1个主池，3个子线程互发事件
*	验证：	1.子线程之间的事件是否能正确接收
************************************************************************/
	mid = InitThreadPool("event test", 6, CALLBACK);	

	sid[0] = StartThreadInPool(mid, test0, "fuck0", NULL);
	printf("sid = [%d]\r\n",sid[0]);

	sid[1] = StartThreadInPool(mid, test1, "fuck1", NULL);
	printf("sid = [%d]\r\n",sid[1]);

	sid[2] = StartThreadInPool(mid, test2, "fuck2", NULL);
	printf("sid = [%d]\r\n",sid[2]);	

	for (i=0;i<10;i++)
	{
		ThreadTskSetEvent(mid, sid[i%3], i);
		
		sleep(1);
	}
	
	UnInitThreadPool(mid);
	sleep(5);
	printf("+++++++++++++++ EVENT_TEST finish +++++++++++++++\r\n");
#endif

#ifdef QUEUE_TEST
/***********************************************************************
*	功能：创建1个主池，3个子线程互发消息
*	验证：	1.子线程之间的消息是否能正确接收
*	注意事项：消息附属于事件里面，会占用一个事件LH_THREAD_TTPYE_EVT_QUEUE
************************************************************************/
	mid = InitThreadPool("queue test", 3, NULL);

	sid[0] = StartThreadInPool(mid, qtest0, "fuck0", qclean);
	printf("sid = [%d]\r\n",sid[0]);

	sid[1] = StartThreadInPool(mid, qtest1, "fuck1", NULL);
	printf("sid = [%d]\r\n",sid[1]);

	sid[2] = StartThreadInPool(mid, qtest2, "fuck2", NULL);
	printf("sid = [%d]\r\n",sid[2]);

	ThreadTskPostToQueue(mid, sid[0], 18, "queue msg1", 10);

	ThreadTskPostToQueue(mid, sid[0], 4, "lalalalal", 5);
	
	PrintInfoThreadInPool(mid);
	sleep(5);
	UnInitThreadPool(mid);
	sleep(5);
	printf("+++++++++++++++ QUEUE_TEST finish +++++++++++++++\r\n");
#endif


	while(1)
	{
		
		sleep(1);
	}
	
    return 0;
}
