/* basic test
   n threads attempt to down a binary semaphore, then calculate slow fibonacci
   optionally takes one argument: number of threads to run (max 128), default 2
*/

#include <sthread.h>
#include <string.h>
#include <stdio.h>
#include <sched.h>

#define MAX_THREADS 128
int fib(int n) {
	if(n <= 1) return 1;
	return fib(n-1) + fib(n-2);
}

struct targ {
	sthread_sem_t *psem;
	int arg;
	int id;
	int done; /* flag as substitute for a needed sthread_wait */
}
targs[MAX_THREADS];

sthread_t tdata[MAX_THREADS];

int tfunc(void *pct) {
	struct targ *parg = (struct targ *)(pct);
	int qq;
	printf("Thread %d trying to get semaphore\n", parg->id);
	sthread_sem_down(parg->psem);
	printf("Thread %d got sem, about to fib(%d)\n", parg->id, parg->arg);
	qq = fib(parg->arg);
	printf("Thread %d finished fib, releasing\n", parg->id);
	sthread_sem_up(parg->psem);
	parg->done = 1;
	return qq;
}

int main(int argc, char **argv)
{
	sthread_sem_t sema;
	int num_threads;
	int i;
	setbuf(stdout, NULL);
	sthread_init();
	sthread_sem_init(&sema, 1);
	num_threads = 2;
	if(argc > 1) {
		if(1 != sscanf(argv[1], "%d", &num_threads)) {
			fprintf(stderr, "Cannot parse arg\n");
			return 1;
		}
		if(!(1 <= num_threads && num_threads <= MAX_THREADS)) {
			fprintf(stderr, "Invalid number of threads\n");
			return 1;
		}
	}
	printf("Will start %d threads\n", num_threads);
	for(i=0;i<num_threads;++i){
		targs[i].psem = &sema;
		targs[i].arg = 24;
		targs[i].id = i;
		targs[i].done = 0;
		printf("About to start thread %d\n", i);
		sthread_create(&tdata[i], &tfunc, &targs[i]);
	}
	for(i=0;i<num_threads;++i){
		while(!targs[i].done){sched_yield();}
	}
	return 0;
}
