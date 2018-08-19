/**
 * @file thread_pool.c
 * @version 1.0
 * @author boyce <boyce.ywr@gmail.com>
 * @author Tristan Lee <tristan.lee@qq.com>
 * @brief A Thread Pool implementation on Pthread
 * 
 * 
 * Change Logs:
 * Date			Author		Notes
 * 2011-07-25   boyce
 * 2011-08-04   boyce
 * 2012-05-14   boyce
 * 2016-01-13	Tristan		fix bugs
 * 2016-02-25	Tristan		fix manage thread exit problem
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <sys/time.h>

#include "thread_pool.h"

//#define __DEBUG__

#ifdef __DEBUG__
#define DEBUG(format,...)	printf(format,##__VA_ARGS__)
#else
#define DEBUG(format,...) 
#endif

static int tp_init(TpThreadPool *pTp);
static TpThreadInfo *tp_add_thread(TpThreadPool *pTp, process_job proc_fun, void *job);
static int tp_delete_thread(TpThreadPool *pTp); 
static int tp_get_tp_status(TpThreadPool *pTp); 

static void *tp_work_thread(void *pthread);
static void *tp_manage_thread(void *pthread);
static void afterms(struct timespec *timeout,unsigned long ms);

/**
 * user interface. creat thread pool.
 * para:
 * 	num: min thread number to be created in the pool
 * return:
 * 	thread pool struct instance be created successfully
 */
TpThreadPool *tp_create(unsigned min_num, unsigned max_num) {
	TpThreadPool *pTp;
	pTp = (TpThreadPool*) malloc(sizeof(TpThreadPool));

	memset(pTp, 0, sizeof(TpThreadPool));

	//init member var
	pTp->min_th_num = min_num;
	pTp->max_th_num = max_num;

    tp_init(pTp);
	return pTp;
}

/**
 * member function reality. thread pool init function.
 * para:
 * 	pTp: thread pool struct instance ponter
 * return:
 * 	true: successful; false: failed
 */
static int tp_init(TpThreadPool *pTp) {
	int err;
    unsigned i;
	TpThreadInfo *pThi;

	//init_queue(&pTp->idle_q, NULL);
	pTp->busy_q = ts_queue_create();
	pTp->idle_q = ts_queue_create();
	pTp->busy_threshold = BUSY_THRESHOLD;
	pTp->manage_interval = MANAGE_INTERVAL;

	//create work thread and init work thread info
	for (i = 0; i < pTp->min_th_num; i++) {
        pThi = (TpThreadInfo*) malloc(sizeof(TpThreadInfo));
		pThi->tp_pool = pTp;
		pThi->stop_flag = FALSE;
        pThi->event_sem = (sem_t*)malloc(sizeof(sem_t));
		sem_init(pThi->event_sem, 0, 0);
		pThi->proc_fun = NULL;
		pThi->arg = NULL;
		ts_queue_enq_data(pTp->idle_q, pThi);

		err = pthread_create(&pThi->thread_id, NULL, tp_work_thread, pThi);
		if (0 != err) {
			perror("tp_init: create work thread failed.");
			ts_queue_destroy(pTp->busy_q);
            ts_queue_destroy(pTp->idle_q);
			return -1;
		}
	}

    //create manage thread and init manage thread info
	pThi = (TpThreadInfo*) malloc(sizeof(TpThreadInfo));
	pThi->tp_pool = pTp;
	pThi->stop_flag = FALSE;
    pThi->event_sem = (sem_t*)malloc(sizeof(sem_t));
	sem_init(pThi->event_sem, 0, 0);
	pThi->proc_fun = NULL;
	pThi->arg = NULL;
    
	err = pthread_create(&pThi->thread_id, NULL, tp_manage_thread, pThi);
	if (0 != err) {//clear_queue(&pTp->idle_q);
	    ts_queue_destroy(pTp->busy_q);
		ts_queue_destroy(pTp->idle_q);
		fprintf(stderr, "tp_init: creat manage thread failed\n");
		return 0;
	}
    pTp->manage = pThi;

    #if 0
	//wait for all threads are ready
	while(i++ < pTp->cur_th_num){
		pthread_mutex_lock(&pTp->tp_lock);
		pthread_cond_wait(&pTp->tp_cond, &pTp->tp_lock);
		pthread_mutex_unlock(&pTp->tp_lock);
	}
	DEBUG("All threads are ready now\n");
    #endif
    
	return 0;
}

/**
 * member function reality. thread pool entirely close function.
 * para:
 * 	pTp: thread pool struct instance ponter
 * return:
 */
void tp_close(TpThreadPool *pTp, BOOL wait) {
    TpThreadInfo *pThi;
    pthread_t thread_id;
    
	//close manage thread first
    DEBUG("close manage thread\n");
    thread_id = pTp->manage->thread_id; //:NOTE: get thread_id before post event
    pTp->manage->stop_flag = TRUE;
    sem_post(pTp->manage->event_sem);
	pthread_join(thread_id, NULL);

    DEBUG("total number of threads: %d\n", ts_queue_count(pTp->busy_q)+ts_queue_count(pTp->idle_q));
	if (wait) {
        while (!ts_queue_is_empty(pTp->busy_q)) {
            pThi = (TpThreadInfo *)ts_queue_deq_data(pTp->busy_q);
            thread_id = pThi->thread_id; //:NOTE: get thread_id before post event
            pThi->stop_flag = TRUE;
            sem_post(pThi->event_sem);

            DEBUG("join thread 0x%08x\n", (unsigned)thread_id);
			if(0 != pthread_join(thread_id, NULL)){
				perror("pthread_join");
			}
			//DEBUG("join a thread success.\n");
        }

        while (!ts_queue_is_empty(pTp->idle_q)) {
            pThi = (TpThreadInfo *)ts_queue_deq_data(pTp->idle_q);
            thread_id = pThi->thread_id; //:NOTE: get thread_id before post event
            pThi->stop_flag = TRUE;
            sem_post(pThi->event_sem);

            DEBUG("join thread 0x%08x\n", (unsigned)thread_id);
			if(0 != pthread_join(thread_id, NULL)){
				perror("pthread_join");
			}
			//DEBUG("join a thread success.\n");
        }

        DEBUG("join all thread success.\n");
	} else {
		//close work thread
		while (!ts_queue_is_empty(pTp->busy_q)) {
            pThi = (TpThreadInfo *)ts_queue_deq_data(pTp->busy_q);
            pThi->stop_flag = TRUE;
            sem_post(pThi->event_sem);
        }
        
        while (!ts_queue_is_empty(pTp->idle_q)) {
            pThi = (TpThreadInfo *)ts_queue_deq_data(pTp->idle_q);
            pThi->stop_flag = TRUE;
            sem_post(pThi->event_sem);
        }
	}

	//clear_queue(&pTp->idle_q);
	ts_queue_destroy(pTp->busy_q);
	ts_queue_destroy(pTp->idle_q);
    free(pTp);
}

/**
 * member function reality. main interface opened.
 * after getting own worker and job, user may use the function to process the task.
 * para:
 * 	pTp: thread pool struct instance ponter
 *	worker: user task reality.
 *	job: user task para
 * return:
 */
int tp_process_job(TpThreadPool *pTp, process_job proc_fun, void *arg) {
	TpThreadInfo *pThi ;

    if (!pTp || !proc_fun) return -1;
    
	pThi = (TpThreadInfo *) ts_queue_deq_data(pTp->idle_q);
	if(pThi){
		DEBUG("Fetch a thread from pool.\n");
		pThi->proc_fun = proc_fun;
		pThi->arg = arg;
        ts_queue_enq_data(pTp->busy_q, pThi);
        
		//let the thread to deal with this job
		DEBUG("wake up thread %u\n", (unsigned)pThi->thread_id);
        sem_post(pThi->event_sem);
	}
	else{
		//if all current thread are busy, new thread is created here
		if(!(pThi = tp_add_thread(pTp, proc_fun, arg))){
			DEBUG("The thread pool is full, no more thread available.\n");
			return -1;
		}
		/* should I wait? */
		//pthread_mutex_lock(&pTp->tp_lock);
		//pthread_cond_wait(&pTp->tp_cond, &pTp->tp_lock);
		//pthread_mutex_unlock(&pTp->tp_lock);
		
		DEBUG("No more idle thread, create a new thread\n");
	}
	return 0;
}

/**
 * member function reality. add new thread into the pool and run immediately.
 * para:
 * 	pTp: thread pool struct instance ponter
 * 	proc_fun:
 * 	job:
 * return:
 * 	pointer of TpThreadInfo
 */
static TpThreadInfo *tp_add_thread(TpThreadPool *pTp, process_job proc_fun, void *arg) {
	int err;
	TpThreadInfo *pThi;

	if (pTp->max_th_num <= ts_queue_count(pTp->busy_q)+ts_queue_count(pTp->idle_q)){
		return NULL;
	}
    
	//malloc new thread info struct
	pThi = (TpThreadInfo*) malloc(sizeof(TpThreadInfo));

	//init status, queue into busy_q, only new process job will call this function
	pThi->tp_pool = pTp;
	pThi->stop_flag = FALSE;
	pThi->event_sem = (sem_t*)malloc(sizeof(sem_t));
	sem_init(pThi->event_sem, 0, 0);
	pThi->proc_fun = proc_fun;
	pThi->arg = arg;
    ts_queue_enq_data(pTp->busy_q, pThi);

	err = pthread_create(&pThi->thread_id, NULL, tp_work_thread, pThi);
	if (0 != err) {
		perror("tp_add_thread: pthread_create");
        ts_queue_rm_data(pTp->busy_q, pThi);
        sem_destroy(pThi->event_sem);
        free(pThi->event_sem);
		free(pThi);
		return NULL;
	}

    sem_post(pThi->event_sem);
	return pThi;
}

/**
 * member function reality. delete idle thread in the pool.
 * only delete last idle thread in the pool.
 * para:
 * 	pTp: thread pool struct instance ponter
 * return:
 * 	true: successful; false: failed
 */
int tp_delete_thread(TpThreadPool *pTp) {
    TpThreadInfo *pThi;
    pthread_t thread_id;

	//current thread num can't < min thread num
	if (ts_queue_count(pTp->busy_q)+ts_queue_count(pTp->idle_q) <= pTp->min_th_num)
		return -1;
	//all threads are busy
	pThi = (TpThreadInfo *) ts_queue_deq_data(pTp->idle_q);
	if(!pThi)
		return -1;
	
    DEBUG("Delete idle thread 0x%08x\n", (unsigned)pThi->thread_id);
    //close the idle thread
    thread_id = pThi->thread_id; //:NOTE: get thread_id before post event
    pThi->stop_flag = TRUE;
    sem_post(pThi->event_sem);
    pthread_join(thread_id, NULL);

	return 0;
}

/**
 * internal interface. real work thread.
 * @params:
 * 	arg: args for this method
 * @return:
 *	none
 */
static void *tp_work_thread(void *arg) {
	TpThreadInfo *pThi = (TpThreadInfo *) arg;
	TpThreadPool *pTp = pThi->tp_pool;

#if 0
	//wake up waiting thread, notify it I am ready
	pthread_cond_signal(&pTp->tp_cond);
#endif

    while (1) {
		//wait event for processing real job.
        sem_wait(pThi->event_sem);

        //process
		if(pThi->proc_fun){
			DEBUG("thread 0x%08x is running\n", (unsigned)pThi->thread_id);
			pThi->proc_fun(pThi->arg);
			//thread state should be set idle after work
			pThi->proc_fun = NULL;
            //pThi->arg = NULL;
		}

        //stop
		if(pThi->stop_flag){
			DEBUG("thread 0x%08x stop\n", (unsigned)pThi->thread_id);
			break;
		}

        //work thread is idle now, we must check stop_flag before
        //accessing pTp in case of pTp already freed by tp_close()
		if (ts_queue_rm_data(pTp->busy_q, pThi) != NULL) {
		    ts_queue_enq_data(pTp->idle_q, pThi);
		}            
	}

    DEBUG("thread 0x%08x exit\n", (unsigned)pThi->thread_id);
    sem_destroy(pThi->event_sem);
    free(pThi->event_sem);
    free(pThi);
    return NULL;
}

/**
 * member function reality. get current thread pool status:idle, normal, busy, .etc.
 * para:
 * 	pTp: thread pool struct instance ponter
 * return:
 * 	0: idle; 1: normal or busy(don't process)
 */
int tp_get_tp_status(TpThreadPool *pTp) {
    float busy_rate = 0.0;
    unsigned busy_nr, idle_nr;

    //get busy thread number
    busy_nr = ts_queue_count(pTp->busy_q);
    idle_nr = ts_queue_count(pTp->idle_q);
    busy_rate = busy_nr;
    busy_rate = busy_rate / (busy_nr+idle_nr);

    DEBUG("Thread pool status, total num: %d, busy num: %d, idle num: %d\n", \
          (busy_nr+idle_nr), busy_nr, idle_nr);
    if (busy_rate < pTp->busy_threshold)
        return 0;//idle status
    else
        return 1;//busy or normal status
}

/**
 * internal interface. manage thread pool to delete idle thread.
 * para:
 * 	pthread: thread struct ponter
 * return:
 */
static void *tp_manage_thread(void *arg) {
	TpThreadInfo *pThi = (TpThreadInfo *) arg;
	TpThreadPool *pTp = pThi->tp_pool;

    while (1) {
        struct timespec abs_timeout;
    	afterms(&abs_timeout, pTp->manage_interval*1000);
        sem_timedwait(pThi->event_sem, &abs_timeout);
    
		if(pThi->stop_flag){
			break;
		}

        if (tp_get_tp_status(pTp) == 0) {
            tp_delete_thread(pTp);
		}
    }

    DEBUG("manage thread 0x%08x exit\n", (unsigned)pThi->thread_id);
    sem_destroy(pThi->event_sem);
    free(pThi->event_sem);
    free(pThi);
	return NULL;
}

float tp_get_busy_threshold(TpThreadPool *pTp){
	return pTp->busy_threshold;
}

int tp_set_busy_threshold(TpThreadPool *pTp, float bt){
	if(bt <= 1.0 && bt > 0.) {
		pTp->busy_threshold = bt;
        return 0;
	}

    return -1;
}

unsigned tp_get_manage_interval(TpThreadPool *pTp){
	return pTp->manage_interval;
}

int tp_set_manage_interval(TpThreadPool *pTp, unsigned mi){
	pTp->manage_interval = mi;
    return 0;
}

static void afterms(struct timespec *timeout,unsigned long ms)
{
	struct timeval tt;
	gettimeofday(&tt,NULL);

	timeout->tv_sec = tt.tv_sec+ms/1000;
	timeout->tv_nsec = tt.tv_usec*1000 + (ms%1000) * 1000 *1000;  //这里可能造成纳秒>1000 000 000
	timeout->tv_sec += timeout->tv_nsec/(1000 * 1000 *1000);
	timeout->tv_nsec %= (1000 * 1000 *1000);
}

