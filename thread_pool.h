
#ifndef __THREAD_POOL_H
#define __THREAD_POOL_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <pthread.h>
#include <semaphore.h>
#include "tsqueue.h"

#ifndef BOOL
#define BOOL int
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#define BUSY_THRESHOLD 0.5	//(busy thread)/(all thread threshold)
#define MANAGE_INTERVAL 20	//tp manage thread sleep interval, every MANAGE_INTERVAL seconds, manager thread will try to recover idle threads as BUSY_THRESHOLD

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tp_thread_info_s TpThreadInfo;
typedef struct tp_thread_pool_s TpThreadPool;

typedef void (*process_job)(void *arg);

//thread info
struct tp_thread_info_s {
	pthread_t thread_id; //thread id num
	BOOL stop_flag; //whether stop the thread
	sem_t *event_sem;    
	process_job proc_fun;
	void *arg;
	TpThreadPool *tp_pool;
};

//main thread pool struct
struct tp_thread_pool_s {
	unsigned min_th_num; //min thread number in the pool
	unsigned max_th_num; //max thread number in the pool	
    TSQueue *busy_q; //busy queue
	TSQueue *idle_q; //idle queue

    TpThreadInfo *manage;
	float busy_threshold; //
	unsigned manage_interval; //
};

TpThreadPool *tp_create(unsigned min_num, unsigned max_num);
void tp_close(TpThreadPool *pTp, BOOL wait);
int tp_process_job(TpThreadPool *pTp, process_job proc_fun, void *arg);

float tp_get_busy_threshold(TpThreadPool *pTp);
int tp_set_busy_threshold(TpThreadPool *pTp, float bt);
unsigned tp_get_manage_interval(TpThreadPool *pTp);
int tp_set_manage_interval(TpThreadPool *pTp, unsigned mi); //mi - manager interval time, in second

#ifdef __cplusplus
}
#endif


#endif
