#include <stdio.h>
#include <unistd.h>
#include <linux/sched.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <semaphore.h>
#include <errno.h>
#include <time.h>
#include <sys/syscall.h>

#define SCHED_POLICY_MYCFS 6
#define SYSCALLNUM 378

int fib(int x)
{
	if(x <= 1) return 1;
	return fib(x-1) + fib(x-2);
}

struct sched_param MYPARAM;
struct timeval INIT_TVAL;


void *run_session(void *x) {
	struct timeval tval, tinit;
	int id = *((int *)x);
	int i;
	int rc;
	int omg = 10;
	if(id == 1){omg = 99;}
	rc = syscall(SYSCALLNUM, 0, omg);
	printf("Thread %d rc from syscall: %d\n", id, rc);
	struct timespec its;
	{
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &its);
	}
	//gettimeofday(&tinit, NULL);
	for(i=0;i<500;++i) {
		if(i % 50 == 0) {
			struct timespec ts;
			double d;
			clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
			d = (ts.tv_sec - its.tv_sec) + 1e-9*(ts.tv_nsec - its.tv_nsec);
			printf("Thread %d iter %d %.6lf\n", id, i, d);
			usleep(1);
		}
		fib(30);
	}
	{
		struct timespec ts;
		double d;
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
		d = (ts.tv_sec - its.tv_sec) + 1e-9*(ts.tv_nsec - its.tv_nsec);
		printf("Thread %d TT %.6lf\n", id, d);
		
	}
	return NULL;
}
#define NUM_THREADS 2
pthread_t tids[NUM_THREADS];
int myids[NUM_THREADS];

int main() {
	int rc;int i;
	gettimeofday(&INIT_TVAL, NULL);
	rc = sched_getscheduler(0);
	printf("Init sched: %d\n", rc);
	rc = sched_setscheduler(0, SCHED_POLICY_MYCFS, &MYPARAM);
	printf("Rc from set: %d\n", rc);
	if(rc == -1) {
		printf("Errno %d: %s\n", errno, strerror(errno));
		return -1;
	}
	rc = syscall(SYSCALLNUM, 0, 10);
	printf("Rc from syscall: %d\n", rc);
	if(rc == -1) {
		printf("Errno %d: %s\n", errno, strerror(errno));
		return -1;
	}
	//int idd = 0;
	//run_session(&idd);
#if 1
	/* make threads */
	for(i=0;i<NUM_THREADS;++i) {
		myids[i] = i;
		pthread_create(&tids[i], NULL, run_session, (void *)(&myids[i]));
	}
	for(i=0;i<NUM_THREADS;++i) {
	}
	for(i=0;i<NUM_THREADS;++i) {
		pthread_join(tids[i], NULL);
	}
#endif
	printf("ALL DONE\n");
	return 0;
}
