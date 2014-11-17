#include <stdio.h>
#include <unistd.h>
#include <linux/sched.h>
#include <pthread.h>
#include <errno.h>

#define SCHED_POLICY_MYCFS 6

int fib(int x)
{
	if(x <= 1) return 1;
	return fib(x-1) + fib(x-2);
}

struct sched_param MYPARAM;

int main() {
	int rc;
	rc = sched_getscheduler(0);
	printf("Init sched: %d\n", rc);
	rc = sched_setscheduler(0, SCHED_POLICY_MYCFS, &MYPARAM);
	printf("Rc from set: %d\n", rc);
	if(rc == -1)
		printf("Errno %d: %s\n", errno, strerror(errno));
	fib(20);
	rc = sched_getscheduler(0);
	printf("Now sched: %d\n", rc);
	return 0;
}

