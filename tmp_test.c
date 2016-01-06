#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

pthread_cond_t thread_cond;
pthread_mutex_t thread_lock;

static void *work_thread(void *arg)
{
	pthread_cond_signal(&thread_cond);
	sleep(10);
	pthread_cond_signal(&thread_cond);
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
	printf("wait condition\n");
	pthread_mutex_lock(&thread_lock);
	pthread_cond_wait(&thread_cond, &thread_lock);
	pthread_mutex_unlock(&thread_lock);
	printf("condition ready\n");

	return 0;
}

