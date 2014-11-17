/* starvation test
   2 threads run a lot of small fibonnaccis, thus taking the binary semaphore a lot
   n-2 threads run large fibs, which should eventually get the semaphore
   optionally takes one argument: number of threads to run (max 128), default 3
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
	int ntimes;
	int id;
	int done;
}
targs[MAX_THREADS];

sthread_t tdata[MAX_THREADS];

int tfunc(void *pct) {
	struct targ *parg = (struct targ *)(pct);
	int ctr;
	int qq = 0;
	for(ctr = 0; ctr < parg->ntimes; ++ctr)
	{
		printf("Thread %d iter %d trying to get semaphore\n", parg->id, ctr);
		sthread_sem_down(parg->psem);
		printf("Thread %d iter %d got sem, about to fib(%d)\n", parg->id, ctr, parg->arg);
		qq = fib(parg->arg);
		printf("Thread %d iter %d finished fib, releasing\n", parg->id, ctr);
		sthread_sem_up(parg->psem);
	}
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
	num_threads = 3;
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
		targs[i].id = i;
		targs[i].psem = &sema;
		targs[i].done = 0;
		if(i < 2) {
			targs[i].ntimes = 500;
			targs[i].arg = 4;
		} else {
			targs[i].ntimes = 5;
			targs[i].arg = 30;
		}
		printf("About to start thread %d\n", i);
		sthread_create(&tdata[i], &tfunc, &targs[i]);
	}
	for(i=0;i<num_threads;++i){
		while(!targs[i].done){sched_yield();}
	}
	return 0;
}
