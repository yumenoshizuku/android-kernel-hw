#define DIVISOR 500000000
#define BUFFER_SIZE 5
#define RUNTIME 5
#define PNUM 6
#define CNUM 7

#define _REENTRANT
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include "sthread.h"

/* semaphores for full, empty and mutex */
sthread_sem_t full, empty, mutex;

/* storage buffer and counter */
int buffer[BUFFER_SIZE];
int counter;

sthread_t pThr;		//producer threads
sthread_t cThr; 	//consumer threads

int producer(void *arg);
int consumer(void *arg);
int insert_item(int item);
int remove_item(int *item);

// producer routine
int producer(void *arg) {
	int item;
	while(1) {
		// sleep for a random period of time
		sleep(rand() / DIVISOR);
		// let item be a random number
		item = rand();
		// acquire empty lock & mutex lock
		sthread_sem_down(&empty);
		sthread_sem_down(&mutex);
		
		if(!insert_item(item))
			printf("producer %ld produced %d,\t%d in buffer\n", (long int) arg, item, counter);
		else
			printf("Failed to produce\n");
			
		// release lock and add to full
		sthread_sem_up(&mutex);
		sthread_sem_up(&full);
	}
	return 0;
}

// consumer routine
int consumer(void *arg) {
	int item;
	while(1) {
		sleep(rand() / DIVISOR);

		// acquire full lock & mutex lock
		sthread_sem_down(&full);
		sthread_sem_down(&mutex);
		if(!remove_item(&item))
			printf("consumer %ld consumed %d,\t%d in buffer\n", (long int) arg, item, counter);
		else
			printf("Failed to consume\n");
		// release lock and add to empty
		sthread_sem_up(&mutex);
		sthread_sem_up(&empty);
	}
	return 0;
}

int insert_item(int item) {
	if(counter < BUFFER_SIZE) {
		buffer[counter] = item;
		counter += 1;
		return 0;
	}
	else
		return -1;
}

int remove_item(int *item) {
	if(counter > 0) {
		*item = buffer[(counter-1)];
		counter -= 1;
		return 0;
	}
	else
      return -1;
}

int main(int argc, char *argv[]) {

	sthread_init();
	// mutex lock
	sthread_sem_init(&mutex, 1);
	// create full semaphore start with 0
	sthread_sem_init(&full, 0);
	// create empty semaphore to BUFFER_SIZE
	sthread_sem_init(&empty, BUFFER_SIZE);
	counter = 0;

	long int i;
	// create producer threads
	for(i = 0; i < PNUM; i++) {
		sthread_create(&pThr,producer,(void *)i);
	}
	// create consumer threads
	for(i = 0; i < CNUM; i++) {
		sthread_create(&cThr,consumer,(void *)i);
	}
	sleep(RUNTIME);
	return 0;
}
