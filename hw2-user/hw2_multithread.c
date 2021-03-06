#include <stdio.h>
#include <sys/utsname.h>
#include <pthread.h>
#include <stdlib.h>
#include "lock.h"
#define DIVISOR 500000000

void* thread_fn(void *arg) {
	int id = (int) arg;
	int ret;
	int i;
	for(i = 0; i < 5; i++) {
	if((ret = netlock_acquire(NET_LOCK_R))!=0)
    	printf("Readlock not acquired for thread %d.\n", id);
    else {
    	printf("Readlock acquired for thread %d.\n", id);
    sleep(rand() / DIVISOR);
    if((ret = netlock_release())!=0)
    	printf("Readlock not released for thread %d.\n", id);
    else
    	printf("Readlock released for thread %d.\n", id);
    	}
    if((ret = netlock_acquire(NET_LOCK_E))!=0)
    	printf("Writelock not acquired for thread %d.\n", id);
    else {
    	printf("Writelock acquired for thread %d.\n", id);
    sleep(rand() / DIVISOR);
    if((ret = netlock_release())!=0)
    	printf("Writelock not released for thread %d.\n", id);
    else
    	printf("Writelock released for thread %d.\n", id);
    	}
    }
    return NULL;
}


int main() {
    pthread_t t1, t2, t3;
    pthread_create(&t1, NULL, thread_fn, (void *)1);
    pthread_create(&t2, NULL, thread_fn, (void *)2);
    pthread_create(&t3, NULL, thread_fn, (void *)3);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    pthread_join(t3, NULL);
    return 0;
}
