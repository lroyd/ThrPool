#ifndef _COMM_TYPE_H_
#define _COMM_TYPE_H_

#include <stdarg.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <assert.h>
#include <pthread.h>


/* Error code  70001 - */
#define GH_ERRNO_START			20000
#define GH_ERRNO_SPACE_SIZE		50000
#define GH_ERRNO_START_STATUS	(GH_ERRNO_START + GH_ERRNO_SPACE_SIZE)

#define GH_EUNKNOWN				(GH_ERRNO_START_STATUS + 1)	// 70001 Unknown error has been reported
#define GH_EPENDING				(GH_ERRNO_START_STATUS + 2)	// 70002 The operation is pending and will be completed later.
#define GH_ETOOMANYCONN			(GH_ERRNO_START_STATUS + 3)	// 70003 Too many connecting sockets.
#define GH_EINVAL	    		(GH_ERRNO_START_STATUS + 4)	// 70004 Invalid argument.
#define GH_ENAMETOOLONG	    	(GH_ERRNO_START_STATUS + 5)	// 70005 Name too long (eg. hostname too long).
#define GH_ENOTFOUND	    	(GH_ERRNO_START_STATUS + 6)	// 70006 Not found.
#define GH_ENOMEM	    		(GH_ERRNO_START_STATUS + 7)	// 70007 Not enough memory.
#define GH_EBUG             	(GH_ERRNO_START_STATUS + 8)	// 70008 Bug detected!
#define GH_ETIMEDOUT        	(GH_ERRNO_START_STATUS + 9)	// 70009 Operation timed out.
#define GH_ETOOMANY         	(GH_ERRNO_START_STATUS + 10)// 70010 Too many objects.
#define GH_EBUSY            	(GH_ERRNO_START_STATUS + 11)// 70011 Object is busy.
#define GH_ENOTSUP	    		(GH_ERRNO_START_STATUS + 12)// 70012 The specified option is not supported.
#define GH_EINVALIDOP	    	(GH_ERRNO_START_STATUS + 13)// 70013 Invalid operation.
#define GH_ECANCELLED	    	(GH_ERRNO_START_STATUS + 14)// 70014 Operation is cancelled.
#define GH_EEXISTS          	(GH_ERRNO_START_STATUS + 15)// 70015 Object already exists.
#define GH_EEOF		    		(GH_ERRNO_START_STATUS + 16)// 70016 End of file.
#define GH_ETOOBIG	    		(GH_ERRNO_START_STATUS + 17)// 70017 Size is too big.
#define GH_ERESOLVE	    		(GH_ERRNO_START_STATUS + 18)// 70018 
#define GH_ETOOSMALL	    	(GH_ERRNO_START_STATUS + 19)// 70019 Size is too small.
#define GH_EIGNORED	    		(GH_ERRNO_START_STATUS + 20)// 70020 Ignored
#define GH_EIPV6NOTSUP	    	(GH_ERRNO_START_STATUS + 21)// 70021 IPv6 is not supported
#define GH_EAFNOTSUP	    	(GH_ERRNO_START_STATUS + 22)// 70022 Unsupported address family
#define GH_EGONE	    		(GH_ERRNO_START_STATUS + 23)// 70023 Object no longer exists
#define GH_ESOCKETSTOP	    	(GH_ERRNO_START_STATUS + 24)// 70024 Socket is stopped
#define GH_EFULL            	(GH_ERRNO_START_STATUS + 25)// 70025 Object is full.


#define GH_DEF(type)		    type

#if defined(__cplusplus)
	#define GH_DECL(type)	    type
#else
	#define GH_DECL(type)	    extern type
#endif


#ifndef GHSUA_CONSOLE_LOG
	enum 
	{
		LOG_FATAL = 0,
		LOG_ERROR,
		LOG_WARNING,	
		LOG_DEBUG,
		LOG_INFO,
		LOG_TRACE,
		LOG_MAX,
	};
	#define LOGOUT(level, format, arg...)	if(level < LOG_TRACE){printf(format, ##arg);printf("\r\n");}
#else
	#include "log_file.h"
	#define LOGOUT			syslog_wrapper
#endif


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


#define	_ASSERT(expr)	assert(expr)

#define _ERROR_RETURN(expr,retval)    \
	do { \
	if (!(expr)) { return retval; } \
	} while (0)




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



#endif


