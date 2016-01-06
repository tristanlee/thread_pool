/*
 * =====================================================================================
 *
 *       Filename:  tsqueue.c
 *
 *    Description:  it's a thread safe queue
 *
 *        Version:  1.0
 *        Created:  05/08/2012 09:53:42 AM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  boyce
 *   Organization:  gw
 *
 * =====================================================================================
 */
 #include <stdlib.h>
 #include <pthread.h>
 #include "tsqueue.h"


static void ts_queue_init(TSQueue *cq);
static TSQItem *ts_queue_item_new(TSQueue *cq);
static void ts_queue_item_free(TSQueue *cq, TSQItem *item);

static TSQItem *ts_queue_head(TSQueue *cq);
static TSQItem *ts_queue_tail(TSQueue *cq);
static TSQItem *ts_queue_peek(TSQueue *cq);

static TSQItem *ts_queue_deq(TSQueue *cq);
static void ts_queue_enq(TSQueue *cq, TSQItem *item);

TSQueue *ts_queue_create(){
	TSQueue *cq = (TSQueue *) malloc(sizeof(TSQueue));
	ts_queue_init(cq);
	return cq;
}

void ts_queue_destroy(TSQueue *cq){
	if(!cq)
		return;

    while (!ts_queue_is_empty(cq)) ts_queue_deq_data(cq);
	pthread_mutex_destroy(&cq->lock);
	free(cq);
}

void *ts_queue_deq_data(TSQueue *cq){
	void *data;
	TSQItem *item;
	if(!cq)
		return NULL;
	item = ts_queue_deq(cq);
	if(!item){
		//
		return NULL;
	}
	data = item->data;
	free(item);
	return data;
}

int ts_queue_enq_data(TSQueue *cq, void *data){
	TSQItem *item;
	if(!cq || !data)
		return -1;

    item = (TSQItem *) malloc(sizeof(TSQItem));
	if(!item){
		//perror("ts_queue_push_data");
		return -1;
	}
	item->data = data;
	ts_queue_enq(cq, item);
	return 0;
}

void *ts_queue_rm_data(TSQueue *cq, void *data){
    TSQItem **p;
    TSQItem *item = NULL;
    TSQItem *prev = NULL;

	if(!cq || !data)
		return NULL;
    
    pthread_mutex_lock(&cq->lock);
    p = &cq->head;
    while (*p) {
        if ((*p)->data == data) {
            //data found
            item = *p;
            *p = item->next; //remove item from queue
            item->next = NULL;
            if (item == cq->tail) cq->tail = prev;
            cq->count--;
            break;                
        }
        prev = *p;
        p = &(*p)->next;
    }
    pthread_mutex_unlock(&cq->lock);
    
    if (item) {
        free(item);
        return data;
    }

    return NULL;
}

unsigned ts_queue_count(TSQueue *cq){
    unsigned count = 0;

    pthread_mutex_lock(&cq->lock);
    count = cq->count;
	pthread_mutex_unlock(&cq->lock);
    
	return count;
}

BOOL ts_queue_is_empty(TSQueue *cq){
	return ts_queue_count(cq)? FALSE : TRUE;
}

static void ts_queue_init(TSQueue *cq){
	if(!cq)
		return;
	pthread_mutex_init(&cq->lock, NULL);
	cq->head = NULL;
	cq->tail = NULL;
	cq->count = 0;
}

static TSQItem *ts_queue_head(TSQueue *cq){
	TSQItem *item;
	if(!cq)
		return NULL;
	return cq->head;
}

static TSQItem *ts_queue_tail(TSQueue *cq){
	TSQItem *item;
	if(!cq)
		return NULL;
	return cq->tail;
}

static TSQItem *ts_queue_peek(TSQueue *cq){
	return ts_queue_head(cq);
}

static TSQItem *ts_queue_deq(TSQueue *cq){
	TSQItem *item;
	if(!cq)
		return NULL;

	pthread_mutex_lock(&cq->lock);
	item = cq->head;
	if(NULL != item){
		cq->head = item->next;
		if(NULL == cq->head)
			cq->tail = NULL;
		cq->count--;
	}
	pthread_mutex_unlock(&cq->lock);

	return item;
}

static void ts_queue_enq(TSQueue *cq, TSQItem *item) {
	if(!cq || !item)
		return;
	item->next = NULL;
	pthread_mutex_lock(&cq->lock);
	if (NULL == cq->tail)
		cq->head = item;
	else
		cq->tail->next = item;
	cq->tail = item;
	cq->count++;
	pthread_mutex_unlock(&cq->lock);
}

