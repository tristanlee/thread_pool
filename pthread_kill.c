
/******************************* pthread_kill.c *******************************/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <signal.h>
#include <pthread.h>
#include <errno.h>

void *func1()/*1 Seconds out */
{
    sleep(1);
    printf(" Threads 1(ID:0x%x) Exit .\n",(unsigned int)pthread_self());
    pthread_exit((void *)0);
}

void *func2()/*5 Seconds out */
{
    sleep(5);
    printf(" Threads 2(ID:0x%x) Exit .\n",(unsigned int)pthread_self());
    pthread_exit((void *)0);
}

void test_pthread(pthread_t tid) /*pthread_kill The return value of : Success (0)  Thread does not exist (ESRCH)  Signals are not legitimate (EINVAL)*/
{
    int pthread_kill_err;
    pthread_kill_err = pthread_kill(tid,0);

    if(pthread_kill_err == ESRCH)
        printf("ID For the 0x%x Thread that does not exist or has exited .\n",(unsigned int)tid);
    else if(pthread_kill_err == EINVAL)
        printf(" Sends a signal of illegal .\n");
    else
        printf("ID For the 0x%x The thread is still alive .\n",(unsigned int)tid);
}

int main()
{
    int ret;
    pthread_t tid1,tid2;

    pthread_create(&tid1,NULL,func1,NULL);
    pthread_create(&tid2,NULL,func2,NULL);

    sleep(3);/* Create two processes after 3 seconds , Test whether they are still alive */

    test_pthread(tid1);/* Test ID tid1 Presence of threads */
    test_pthread(tid2);/* Test ID tid2 Presence of threads */

    exit(0);
}
