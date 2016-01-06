/*
 * @author: boyce
 * @contact: boyce.ywr#gmail.com (# -> @)
 * @version: 1.02
 * @created: 2011-07-25
 * @modified: 2011-08-04
 * @modified: 2012-05-14
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

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
	pthread_mutex_init(&pTp->tp_lock, NULL);
	pthread_cond_init(&pTp->tp_cond, NULL);
	pthread_mutex_init(&pTp->loop_lock, NULL);
	pthread_cond_init(&pTp->loop_cond, NULL);

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
	int i;
	int err;
	TpThreadInfo *pThi;

	//init_queue(&pTp->idle_q, NULL);
	pTp->busy_q = ts_queue_create();
	pTp->idle_q = ts_queue_create();
	pTp->stop_flag = FALSE;
	pTp->busy_threshold = BUSY_THRESHOLD;
	pTp->manage_interval = MANAGE_INTERVAL;

	//create work thread and init work thread info
	for (i = 0; i < pTp->min_th_num; i++) {
        pThi = (TpThreadInfo*) malloc(sizeof(TpThreadInfo));
		pThi->tp_pool = pTp;
		pThi->is_busy = FALSE;
		pthread_cond_init(&pThi->event_cond, NULL);
		pthread_mutex_init(&pThi->event_lock, NULL);
        pThi->event = FALSE;
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

	//create manage thread
	err = pthread_create(&pTp->manage_thread_id, NULL, tp_manage_thread, pTp);
	if (0 != err) {//clear_queue(&pTp->idle_q);
	    ts_queue_destroy(pTp->busy_q);
		ts_queue_destroy(pTp->idle_q);
		fprintf(stderr, "tp_init: creat manage thread failed\n");
		return 0;
	}

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
 * let the thread pool wait until {@link #tp_exit} is called
 * @params:
 *	pTp: pointer of thread pool
 * @return
 *	none
 */
void tp_run(TpThreadPool *pTp){
	pthread_mutex_lock(&pTp->loop_lock);
	pthread_cond_wait(&pTp->loop_cond, &pTp->loop_lock);
	pthread_mutex_unlock(&pTp->loop_lock);
	tp_close(pTp, TRUE);
}

/**
 * let the thread pool exit, this function will wake up {@link #tp_loop}
 * @params:
 *	pTp: pointer of thread pool
 * @return
 *	none
 */
void tp_exit(TpThreadPool *pTp){
	pthread_cond_signal(&pTp->loop_cond);
}

/**
 * member function reality. thread pool entirely close function.
 * para:
 * 	pTp: thread pool struct instance ponter
 * return:
 */
void tp_close(TpThreadPool *pTp, BOOL wait) {
	unsigned i;
    TpThreadInfo *pThi;
    
	pTp->stop_flag = TRUE;

	//close manage thread first
	kill((pid_t)pTp->manage_thread_id, SIGKILL);

	if (wait) {
        DEBUG("Total number of threads: %u\n", ts_queue_count(pTp->busy_q)+ts_queue_count(pTp->idle_q));
        while (!ts_queue_is_empty(pTp->busy_q)) {
            pThi = ts_queue_deq_data(pTp->busy_q);
            pthread_mutex_lock(&pThi->event_lock);
            pThi->event = TRUE;
			pthread_cond_signal(&pThi->event_cond);
            pthread_mutex_unlock(&pThi->event_lock);

            DEBUG("join thread 0x%08x\n", (unsigned)pThi->thread_id);
			if(0 != pthread_join(pThi->thread_id, NULL)){
				perror("pthread_join");
			}
			//DEBUG("join a thread success.\n");
			pthread_mutex_destroy(&pThi->event_lock);
			pthread_cond_destroy(&pThi->event_cond);
            free(pThi);
        }

        while (!ts_queue_is_empty(pTp->idle_q)) {
            pThi = ts_queue_deq_data(pTp->idle_q);
            pthread_mutex_lock(&pThi->event_lock);
            pThi->event = TRUE;
			pthread_cond_signal(&pThi->event_cond);
            pthread_mutex_unlock(&pThi->event_lock);

            DEBUG("join thread 0x%08x\n", (unsigned)pThi->thread_id);
			if(0 != pthread_join(pThi->thread_id, NULL)){
				perror("pthread_join");
			}
			//DEBUG("join a thread success.\n");
			pthread_mutex_destroy(&pThi->event_lock);
			pthread_cond_destroy(&pThi->event_cond);
            free(pThi);
        }

        DEBUG("join all thread success.\n");
	} else {
		//close work thread
		while (!ts_queue_is_empty(pTp->busy_q)) {
            pThi = ts_queue_deq_data(pTp->busy_q);
            kill((pid_t)pThi->thread_id, SIGKILL);
			pthread_mutex_destroy(&pThi->event_lock);
			pthread_cond_destroy(&pThi->event_cond);
            free(pThi);
        }
        
        while (!ts_queue_is_empty(pTp->idle_q)) {
            pThi = ts_queue_deq_data(pTp->idle_q);
            kill((pid_t)pThi->thread_id, SIGKILL);
			pthread_mutex_destroy(&pThi->event_lock);
			pthread_cond_destroy(&pThi->event_cond);
            free(pThi);
        }
	}

	pthread_mutex_destroy(&pTp->tp_lock);
	pthread_cond_destroy(&pTp->tp_cond);
	pthread_mutex_destroy(&pTp->loop_lock);
	pthread_cond_destroy(&pTp->loop_cond);

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

    if (!pTp || pTp->stop_flag) return -1;
    
	pThi = (TpThreadInfo *) ts_queue_deq_data(pTp->idle_q);
	if(pThi){
		DEBUG("Fetch a thread from pool.\n");
		pThi->is_busy = TRUE;
		pThi->proc_fun = proc_fun;
		pThi->arg = arg;
        ts_queue_enq_data(pTp->busy_q, pThi);
        
		//let the thread to deal with this job
		DEBUG("wake up thread %u\n", (unsigned)pThi->thread_id);
        pthread_mutex_lock(&pThi->event_lock);
        pThi->event = TRUE;
		pthread_cond_signal(&pThi->event_cond);
        pthread_mutex_unlock(&pThi->event_lock);
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
	TpThreadInfo *new_thread;

	if (pTp->max_th_num <= ts_queue_count(pTp->busy_q)+ts_queue_count(pTp->idle_q)){
		return NULL;
	}
    
	//malloc new thread info struct
	new_thread = (TpThreadInfo*) malloc(sizeof(TpThreadInfo));

	new_thread->tp_pool = pTp;
	//init new thread's cond & mutex
	pthread_cond_init(&new_thread->event_cond, NULL);
	pthread_mutex_init(&new_thread->event_lock, NULL);

	//init status is busy, only new process job will call this function
	new_thread->is_busy = TRUE;
	new_thread->event = TRUE;
	new_thread->proc_fun = proc_fun;
	new_thread->arg = arg;
    ts_queue_enq_data(pTp->busy_q, new_thread);

	err = pthread_create(&new_thread->thread_id, NULL, tp_work_thread, new_thread);
	if (0 != err) {
		perror("tp_add_thread: pthread_create");
        ts_queue_rm_data(pTp->busy_q, new_thread);
        pthread_mutex_destroy(&new_thread->event_lock);
	    pthread_cond_destroy(&new_thread->event_cond);
		free(new_thread);
		return NULL;
	}
	return new_thread;
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
	unsigned idx;
	TpThreadInfo *pThi;
	TpThreadInfo tT;

	//current thread num can't < min thread num
	if (ts_queue_count(pTp->busy_q)+ts_queue_count(pTp->idle_q) <= pTp->min_th_num)
		return -1;
	//all threads are busy
	pThi = (TpThreadInfo *) ts_queue_deq_data(pTp->idle_q);
	if(!pThi)
		return -1;
	
    DEBUG("Delete idle thread 0x%08x\n", (unsigned)pThi->thread_id);
    //kill the idle thread and free info struct
    kill((pid_t)pThi->thread_id, SIGKILL);
    pthread_mutex_destroy(&pThi->event_lock);
    pthread_cond_destroy(&pThi->event_cond);
    free(pThi);

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
	TpThreadInfo *pTinfo = (TpThreadInfo *) arg;
	TpThreadPool *pTp = pTinfo->tp_pool;

#if 0
	//wake up waiting thread, notify it I am ready
	pthread_cond_signal(&pTp->tp_cond);
#endif

    while (!(pTp->stop_flag)) {
		//wait cond for processing real job.
        DEBUG("thread 0x%08x is waiting for event\n", (unsigned)pTinfo->thread_id);
		pthread_mutex_lock(&pTinfo->event_lock);
		while (!pTinfo->event) pthread_cond_wait(&pTinfo->event_cond, &pTinfo->event_lock);
        //reset event flag
        pTinfo->event = FALSE;
		pthread_mutex_unlock(&pTinfo->event_lock);
        DEBUG("thread 0x%08x get event\n", (unsigned)pTinfo->thread_id);

		if(pTp->stop_flag){
			DEBUG("thread 0x%08x stop\n", (unsigned)pTinfo->thread_id);
			break;
		}
                
		//process
		if(pTinfo->proc_fun){
			DEBUG("thread 0x%08x is running\n", (unsigned)pTinfo->thread_id);
			pTinfo->proc_fun(pTinfo->arg);
			//thread state shoulde be set idle after work
			pTinfo->is_busy = FALSE;
			pTinfo->proc_fun = NULL;
            //pTinfo->arg = NULL;
			//I am idle now
			if (ts_queue_rm_data(pTp->busy_q, pTinfo) != NULL) {
			    ts_queue_enq_data(pTp->idle_q, pTinfo);
			}
		}
	}
    DEBUG("thread 0x%08x exit\n", (unsigned)pTinfo->thread_id);
}

/**
 * member function reality. get current thread pool status:idle, normal, busy, .etc.
 * para:
 * 	pTp: thread pool struct instance ponter
 * return:
 * 	0: idle; 1: normal or busy(don't process)
 */
int tp_get_tp_status(TpThreadPool *pTp) {
	float busy_num = 0.0;
    unsigned total_num = 0;
    
	//get busy thread number
	busy_num = ts_queue_count(pTp->busy_q);
    total_num = ts_queue_count(pTp->busy_q)+ts_queue_count(pTp->idle_q);
    busy_num = busy_num / total_num;

	DEBUG("Thread pool status, total num: %u, busy num: %u, idle num: %u\n", \
        total_num, (unsigned)busy_num, ts_queue_count(pTp->idle_q));
	if(busy_num < pTp->busy_threshold)
		return 0;//idle status
	else
		return 1;//busy or normal status	
}

/**
 * internal interface. manage thread pool to delete idle thread.
 * para:
 * 	pthread: thread pool struct ponter
 * return:
 */
static void *tp_manage_thread(void *arg) {
	TpThreadPool *pTp = (TpThreadPool*) arg;//main thread pool struct instance

	//1?
	sleep(pTp->manage_interval);

	do {
		if (tp_get_tp_status(pTp) == 0) {
            tp_delete_thread(pTp);
		}//end for if

		//1?
		sleep(pTp->manage_interval);
	} while (!pTp->stop_flag);
	return NULL;
}

float tp_get_busy_threshold(TpThreadPool *pTp){
	return pTp->busy_threshold;
}

int tp_set_busy_threshold(TpThreadPool *pTp, float bt){
	if(bt <= 1.0 && bt > 0.)
		pTp->busy_threshold = bt;
}

unsigned tp_get_manage_interval(TpThreadPool *pTp){
	return pTp->manage_interval;
}

int tp_set_manage_interval(TpThreadPool *pTp, unsigned mi){
	pTp->manage_interval = mi;
}
