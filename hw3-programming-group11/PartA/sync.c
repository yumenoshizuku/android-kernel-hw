/*
 * sync.c
 *
 * Synchronization routines for SThread
 */

#define _REENTRANT

#include "sync.h"

#include <stdlib.h>
#include <sched.h>
#include <assert.h>
#include "sthread.h"


/* we model the queue as a circular double linked list
   of queueitem, where the next process to be woken is at the head
   empty queue are represented as a NULL head
   we put new sleepers on the tail of the queue, for fifo behavior
   => A <=> B <=> C <=
      *
   after push D:
   => D <=> A <=> B <=> C <=
            *
   after pop:
   => D <=> B <=> C <=
            *
   The queueitems are allocated on the stack of sthread_sem_down
   of the sleeping threads, thus no need for mallocs and frees.
   (thanks to the Linux kernel's sempahore.c for the idea!)
*/

typedef struct sthread_sem_queueitem {
	struct sthread_sem_queueitem *next;
	struct sthread_sem_queueitem *prev;
	sthread_t item;
} sem_qitem;

static int sq_push(sthread_sem_t *sem, sem_qitem *p);
static int sq_pop(sthread_sem_t *sem, sem_qitem **pitem);


/* push item onto the end of the queue */
static int sq_push(sthread_sem_t *sem, sem_qitem *p) {
	if(sem == NULL || p == NULL)
		return -1;
	if(sem->head == NULL) {
		sem->head = p;
	}
	else {
		//push to tail
		p->next = sem->head;
		p->prev = sem->head->prev;
		p->prev->next = p;
		p->next->prev = p;
	}
	return 0;
}

/* attempt to pop an item from sem, placing it in *pitem */
static int sq_pop(sthread_sem_t *sem, sem_qitem **pitem) {
	if(sem == NULL){return -1;}
	if(pitem == NULL){return -1;}
	sem_qitem *p = sem->head;
	/* list empty? */
	if(p == NULL) {
		return -1;
	}
	*pitem = p;
	/* pop this element from the list */
	if(p->next == p) {
		/* we have exhausted whole list */
		sem->head = NULL;
	}
	else {
		/* remove p from circular list, move head */
		p->prev->next = p->next;
		p->next->prev = p->prev;
		sem->head = p->next;
	}
	return 0;
}

int sthread_sem_init(sthread_sem_t *sem, int count) {
	if(count < 0) {
		return -1;
	}
	sem->lock = 0;
	sem->sem_count = count;
	sem->head = NULL;
	return 0;
}

int sthread_sem_destroy(sthread_sem_t *sem)
{
	/* we don't actually allocate anything, so no need to do anything */
        return 0;
}


int sthread_sem_down(sthread_sem_t *sem) {
	if(sem == NULL) {return -1;}
	while(test_and_set(&sem->lock)){sched_yield();}
	if(sem->sem_count > 0) {
		sem->sem_count -= 1;
		sem->lock = 0;
		return 0;
	}
	else {
		/* put ourself onto the queue */
		sem_qitem qitem;
		int rc;
		qitem.prev = qitem.next = &qitem;
		qitem.item = sthread_self();
		rc = sq_push(sem, &qitem);
		assert(!rc && "Unexpected failure of sq_push");
		/* 
		   There is a race condition here where wakeup is called on this
		   process before this process even suspends.

		   In this case sthread is ok with the simple 
		   "release lock, then sleep" code because of the use of
		   read/write on pipes to simulate sleep/suspend, 
		   but this wouldn't work in general.
		*/
		sem->lock = 0;
		sthread_suspend();
		return 0;
	}
}

int sthread_sem_try_down(sthread_sem_t *sem) {
	int ret;
	if(sem == NULL) {return -1;}
	while(test_and_set(&sem->lock)){sched_yield();}
	if(sem->sem_count > 0) {
		sem->sem_count -= 1;
		ret = 0;
	}
	else {
		ret = -1;
	}
	sem->lock = 0;
	return ret;
}

int sthread_sem_up(sthread_sem_t *sem) {
	if(sem == NULL) {return -1;}
	while(test_and_set(&sem->lock)){sched_yield();}
	//is there something on the queue?
	if(sem->head != NULL) {
		sem_qitem *item;
		int rc = sq_pop(sem, &item);
		assert(!rc && "Queue should have had an item");
		sem->lock = 0;
		sthread_wake(item->item);
		return 0;
	}
	else {
		sem->sem_count += 1;
		sem->lock = 0;
		return 0;
	}
}
