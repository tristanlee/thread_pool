
#define __DEBUG__

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "thread_pool.h"
#include "workpool.h"

#define THD_NUM 100 

static pthread_mutex_t lock;
static unsigned exit_cnt;
TpThreadPool *pTp;

void proc_fun(void *arg){
	int i;
	int idx=(int) arg;
	//void *ptr = malloc(20);
	i = 1000000.0 + (int)(9000000.0 * rand() / RAND_MAX);
	fprintf(stderr, "Task Begin: job %d, sleep %d us\n", idx, i);
	usleep(i);
	fprintf(stderr, "Task End:   job %d\n", idx);
	//free(ptr);
	pthread_mutex_lock(&lock);
	exit_cnt++;
	pthread_mutex_unlock(&lock);
	//if(exit_cnt == THD_NUM)
		//tp_exit(pTp);
}

int test1(void)
{
	pTp= tp_create(10, THD_NUM);
	int i;

	exit_cnt = 0;
	pthread_mutex_init(&lock, NULL);
	srand((int)time(0));
	for(i=0; i < THD_NUM; i++){
		tp_process_job(pTp, (process_job)proc_fun, (void*)i);
	}
	//tp_run(pTp);
	//free(pTp);
	//fprintf(stderr, "All jobs done!\n");

	sleep(25);
	printf("timeout, tp_close() %d\n", exit_cnt);
	tp_close(pTp, 1);
	fprintf(stderr, "%d jobs done!\n", exit_cnt);

	return 0;
}

int test2(void)
{
    int i = 0;
    WorkPool *pool = new WorkPool;

    pool->SetManageInterval(5);
    srand((int)time(0));
	for(i=0; i < THD_NUM; i++){
		pool->DoJob(proc_fun, (void*)i);
	}

    sleep(26);
	printf("timeout, delete workpool %d\n", exit_cnt);
	delete pool;
	fprintf(stderr, "%d jobs done!\n", exit_cnt);
    //sleep(25);
    
    return 0;
}

int main(int argc, char **argv)
{
    //test1();
    test2();
    
	return 0;
}

