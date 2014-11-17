/*
 * sync.h
 */

#ifndef _STHREAD_SYNC_H_
#define _STHREAD_SYNC_H_

/*
 * Semaphore structure
 */
struct sthread_sem_queueitem;

struct sthread_sem_struct {
	volatile unsigned long lock;
	int sem_count;
	struct sthread_sem_queueitem *head;
};

typedef struct sthread_sem_struct sthread_sem_t;

int sthread_sem_init(sthread_sem_t *sem, int count);

/* Note: it is undefined behavior to call sthread_sem_destroy on a semaphore that has any sleeping threads */
int sthread_sem_destroy(sthread_sem_t *sem);
int sthread_sem_down(sthread_sem_t *sem);
int sthread_sem_try_down(sthread_sem_t *sem);
int sthread_sem_up(sthread_sem_t *sem);

#endif
