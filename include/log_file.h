/*************************************************************************
	> File Name: log_file.h
	> Author: lroyd
	> Mail: htzhangxmu@163.com
	> Created Time: 
 ************************************************************************/
#ifndef __DEBUG__
#define __DEBUG__

#ifdef __cplusplus
extern "C"{
#endif


// Trace level configuration
typedef enum TRC_LEVEL{
	TRACE_LEVEL_ERROR = 0,			//0 Error condition trace messages [R]
	TRACE_LEVEL_NONE,				//1 No trace messages to be generated
	TRACE_LEVEL_API,				//2 API traces
	TRACE_LEVEL_EVENT,				//3 Debug messages for events
	TRACE_LEVEL_INFO,				//4 Info condition trace messages [G]
	TRACE_LEVEL_DEBUG				//5 Full debug messages [B]
}TRC_LEVEL;


extern void GLOGW(char *tag, const char* fmt, ...);
extern void GLOGI(char *tag, const char* fmt, ...);
extern void GLOGD(char *tag, const char* fmt, ...);
extern void GLOGE(char *tag, const char* fmt, ...);


/* log总开关 */
#if 1
	#ifndef GUN_DBG			
		#define GUN_DBG	
	#endif
#endif

#ifdef GUN_DBG			
	#if defined(ANDROID) || defined(__ANDROID__)
		#include <jni.h>
		#include <android/log.h> 
		#define DLOGW(tag, format, ...)		__android_log_print(ANDROID_LOG_WARN , tag, format, ##__VA_ARGS__)      
		#define DLOGI(tag, format, ...)		__android_log_print(ANDROID_LOG_INFO , tag, format, ##__VA_ARGS__)      
		#define DLOGD(tag, format, ...)		__android_log_print(ANDROID_LOG_DEBUG, tag, format, ##__VA_ARGS__)      
		#define DLOGE(tag, format, ...)		__android_log_print(ANDROID_LOG_ERROR, tag, format, ##__VA_ARGS__)
	#else
		#define DLOGW	GLOGW
		#define DLOGI	GLOGI
		#define DLOGD   GLOGD
		#define DLOGE	GLOGE
	#endif
#else
	#define DLOGW      
	#define DLOGI      
	#define DLOGD      
	#define DLOGE      
#endif





#ifdef __cplusplus
}
#endif

#endif
