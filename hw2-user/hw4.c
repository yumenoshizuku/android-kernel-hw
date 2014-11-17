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

#define SCHED_NORMAL	0
#define SCHED_POLICY_MYCFS	6

inline int sched_setlimit(pid_t pid, int cpulimit) {
    return syscall(378, pid, cpulimit);
}

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
	sched_setlimit(0, (id+1)*10);
	int i;
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
#define NUM_THREADS 3
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
	printf("ALL DONE\n");
	return 0;
}

