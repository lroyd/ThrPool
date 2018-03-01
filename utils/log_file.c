/*************************************************************************
	> File Name: log_file.c
	> Author: lroyd
	> Mail: htzhangxmu@163.com
	> Created Time: 
*************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>

#include "log_file.h"

//前景色:30（黑色）、31（红色）、32（绿色）、 33（黄色）、34（蓝色）、35（洋红）、36（青色）、37（白色）


#define MAX_LOG_SIZE (10240)
static char buf_time[20];

static char *getLogTime()
{
    struct timeval time;
    struct tm *tmp;

    /* Get Current time */
    gettimeofday(&time, NULL);
    tmp = localtime(&time.tv_sec);
    sprintf(buf_time, "%02d:%02d:%02d:%03d", tmp->tm_hour, tmp->tm_min, tmp->tm_sec, (int)(time.tv_usec/1000));
    return buf_time;
}

/**************************************************************/
void GLOGI(char *tag, const char* fmt, ...)
{
    va_list argp;
    char tmpLog[MAX_LOG_SIZE];

    memset(tmpLog, 0, sizeof(tmpLog));

    va_start( argp, fmt );

    vsprintf(tmpLog, fmt, argp);

    va_end( argp );

    printf("%s \033[1;32;40m[%s] %s\033[m\n", getLogTime(), tag, tmpLog);
}

void GLOGE(char *tag, const char* fmt, ...)
{
    va_list argp;
    char tmpLog[MAX_LOG_SIZE];

    memset(tmpLog, 0, sizeof(tmpLog));

    va_start( argp, fmt );

    vsprintf(tmpLog, fmt, argp);

    va_end( argp );

    printf("%s \033[1;31;40m[%s] %s \033[m\n", getLogTime(), tag, tmpLog);
}

void GLOGD(char *tag, const char* fmt, ...)
{
    va_list argp;
    char tmpLog[MAX_LOG_SIZE];

    memset(tmpLog, 0, sizeof(tmpLog));

    va_start( argp, fmt );

    vsprintf(tmpLog, fmt, argp);

    va_end( argp );

    printf("%s \033[1;36;40m[%s] %s \033[m\n", getLogTime(), tag, tmpLog);
}

void GLOGW(char *tag, const char* fmt, ...)
{
    va_list argp;
    char tmpLog[MAX_LOG_SIZE];

    memset(tmpLog, 0, sizeof(tmpLog));

    va_start( argp, fmt );

    vsprintf(tmpLog, fmt, argp);

    va_end( argp );

    printf("%s [%s] %s\n", getLogTime(), tag, tmpLog);
}

