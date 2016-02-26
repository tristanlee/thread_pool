#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <sys/types.h>
#include <pthread.h>
#include <signal.h>


pthread_cond_t thread_cond;
pthread_mutex_t thread_lock;

static void *work_thread(void *arg)
{
	sleep(10);
	printf("work_thread() exit!\n");

	return 0;
}

int main(void)
{
	int err;
	pthread_t tId;

	pthread_cond_init(&thread_cond, NULL);
	pthread_mutex_init(&thread_lock, NULL);

	err = pthread_create(&tId, NULL, work_thread, NULL);
	if (0 != err) {
		perror("create work thread failed.\n");
		return -1;
	}
		
	sleep(5);
	//printf("kill thread id: %p\n", (void*)tId);
	//pthread_kill(tId, SIGINT);
	printf("pthread_cancel() begin\n");
	pthread_cancel(tId);
	printf("pthread_cancel() end\n");

	printf("pthread_join() begin\n");
	pthread_join(tId,NULL);
	printf("pthread_join() end\n");

	sleep(7);

	printf("main thread exit!\n");
	return 0;
}

