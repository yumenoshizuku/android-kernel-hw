#include <linux/sched.h>
#include <linux/syscalls.h>
#include "sched.h"

#define MYCFS_GRANULARITY 10000000 /* 10 ms */
#define MYCFS_CPU_PERIOD 100000000LL /* 100 ms */
//#define MYCFS_USE_DEBUG

#ifdef MYCFS_USE_DEBUG
#define MYCFS_DEBUG printk
#else
#define MYCFS_DEBUG(...) ((void)0)
#endif

void init_mycfs_rq(struct mycfs_rq *mycfs_rq, struct rq *parent) {
	mycfs_rq->tasks_timeline = RB_ROOT;
	mycfs_rq->min_vruntime = (u64)(-(1LL << 20));
	mycfs_rq->rq = parent;
#ifndef CONFIG_64BIT
	mycfs_rq->min_vruntime_copy = mycfs_rq->min_vruntime;
#endif
}

int mycfs_scheduler_tick(struct rq *rq)
{
	struct mycfs_rq *cfs_rq = &rq->mycfs;
	int rc = 0;
	u64 now = cfs_rq->rq->clock_task;
	if(now - cfs_rq->curr_period_start > MYCFS_CPU_PERIOD) {
		++cfs_rq->curr_period;
		cfs_rq->curr_period_start = now;
		/* check if we should force resched of process */
		if(cfs_rq->skipped_task)
			rc = 1;
	}
	return rc;
}

static void MYCFS_DUMP_CFSRQ(const struct mycfs_rq *rq) {
	MYCFS_DEBUG("CFSRQ %p {N%lu, L%p, C%p, R%p, V%llu}\n",
	            rq,
		    rq->nr_running,
		    rq->rb_leftmost,
		    rq->curr,
		    rq->rq,
		    rq->min_vruntime);
}

static void MYCFS_DUMP_SE(const struct sched_mycfs_entity *se) {
	MYCFS_DEBUG("SE %p {R%u, V%llu}\n",
	            se,
		    se->on_rq,
		    se->vruntime
		   );
}

/* sched_setlimit(pid, cpulimit) */
SYSCALL_DEFINE2(sched_setlimit, pid_t, pid, int, cpulimit)
{
	struct task_struct *p;
	int retval;

	if(pid < 0)
		return -EINVAL;
	
	if(!(cpulimit >= 0 && cpulimit <= 100))
		return -EINVAL;
	
	retval = -ESRCH;
	rcu_read_lock();
	p = (pid) ? (find_task_by_vpid(pid)) : current;
	if(p) {
		p->mycfs_cpu_limit = cpulimit;
		retval = 0;
	}
	rcu_read_unlock();
	return retval;
}

/* functions for sched_class */
static void enqueue_task_mycfs(struct rq *rq, struct task_struct *p, int flags);
static void dequeue_task_mycfs(struct rq *rq, struct task_struct *p, int flags);
static void yield_task_mycfs(struct rq *rq);
static void check_preempt_curr_mycfs(struct rq *rq, struct task_struct *p, int flags);
static struct task_struct * pick_next_task_mycfs(struct rq *rq);
static void put_prev_task_mycfs(struct rq *rq, struct task_struct *p);
#ifdef CONFIG_SMP
static int select_task_rq_mycfs(struct task_struct *p, int sd_flag, int flags);

static void task_waking_mycfs (struct task_struct *task);
static void rq_online_mycfs(struct rq *rq);
static void rq_offline_mycfs(struct rq *rq);
#endif

static void set_curr_task_mycfs (struct rq *rq);
static void task_tick_mycfs (struct rq *rq, struct task_struct *p, int queued);
static void task_fork_mycfs (struct task_struct *p);

static void switched_from_mycfs (struct rq *this_rq, struct task_struct *task);
static void switched_to_mycfs (struct rq *this_rq, struct task_struct *task);
static void prio_changed_mycfs (struct rq *this_rq, struct task_struct *task,
		     int oldprio);

static unsigned int get_rr_interval_mycfs (struct rq *rq,
				 struct task_struct *task);


const struct sched_class mycfs_sched_class = {
	.next			= &idle_sched_class,
	.enqueue_task		= enqueue_task_mycfs,
	.dequeue_task		= dequeue_task_mycfs,
	.yield_task		= yield_task_mycfs,

	.check_preempt_curr	= check_preempt_curr_mycfs,

	.pick_next_task		= pick_next_task_mycfs,
	.put_prev_task		= put_prev_task_mycfs,

#ifdef CONFIG_SMP
	.select_task_rq		= select_task_rq_mycfs,

	.rq_online		= rq_online_mycfs,
	.rq_offline		= rq_offline_mycfs,

	.task_waking		= task_waking_mycfs,
#endif

	.set_curr_task          = set_curr_task_mycfs,
	.task_tick		= task_tick_mycfs,
	.task_fork		= task_fork_mycfs,

	.prio_changed		= prio_changed_mycfs,
	.switched_from		= switched_from_mycfs,
	.switched_to		= switched_to_mycfs,

	.get_rr_interval	= get_rr_interval_mycfs,
};

static inline struct task_struct *
task_of(struct sched_mycfs_entity *myse)
{
	return container_of(myse, struct task_struct, myse);
}

static inline struct mycfs_rq *
mycfs_rq_of(struct sched_mycfs_entity *myse)
{
	struct task_struct *p = task_of(myse);
	struct rq *rq = task_rq(p);
	return &rq->mycfs;
}

static struct sched_mycfs_entity * 
mycfs_pick_first_entity(struct mycfs_rq *cfs_rq)
{
	struct rb_node *left = cfs_rq->rb_leftmost;
	/* update period maybe */
	{
		u64 now = cfs_rq->rq->clock_task;
		if(now - cfs_rq->curr_period_start > MYCFS_CPU_PERIOD) {
			++cfs_rq->curr_period;
			cfs_rq->curr_period_start = now;
		}
	}
	cfs_rq->skipped_task = 0;
	/* we now evaluate each node and keep going until we find one that isn't expired */
	while(left) {
		struct sched_mycfs_entity *se = rb_entry(left, struct sched_mycfs_entity, run_node);
		struct task_struct *p = task_of(se);
		if(p->mycfs_cpu_limit == 0)
			return se;
		if(se->curr_period_id != cfs_rq->curr_period)
			return se;
		if(se->curr_period_sum_runtime * 100 <= MYCFS_CPU_PERIOD * p->mycfs_cpu_limit)
			return se;
		/* we are forced to skip this task */
		cfs_rq->skipped_task = 1;
		left = rb_next(left);
	}
	return NULL;
}

static inline u64 max_vruntime(u64 min_vruntime, u64 vruntime)
{
	s64 delta = (s64)(vruntime - min_vruntime);
	if (delta > 0)
		min_vruntime = vruntime;

	return min_vruntime;
}

static inline u64 min_vruntime(u64 min_vruntime, u64 vruntime)
{
	s64 delta = (s64)(vruntime - min_vruntime);
	if (delta < 0)
		min_vruntime = vruntime;

	return min_vruntime;
}

static inline int entity_before(struct sched_mycfs_entity *a,
				struct sched_mycfs_entity *b)
{
	return (s64)(a->vruntime - b->vruntime) < 0;
}

static void mycfs_update_min_vruntime(struct mycfs_rq *cfs_rq)
{
	u64 vruntime = cfs_rq->min_vruntime;

	if (cfs_rq->curr)
		vruntime = cfs_rq->curr->vruntime;

	if (cfs_rq->rb_leftmost) {
		struct sched_mycfs_entity *se = rb_entry(cfs_rq->rb_leftmost,
							 struct sched_mycfs_entity,
							 run_node);

		if (!cfs_rq->curr)
			vruntime = se->vruntime;
		else
			vruntime = min_vruntime(vruntime, se->vruntime);
	}

	cfs_rq->min_vruntime = max_vruntime(cfs_rq->min_vruntime, vruntime);
#ifndef CONFIG_64BIT
	smp_wmb();
	cfs_rq->min_vruntime_copy = cfs_rq->min_vruntime;
#endif
}

static void __enqueue_entity(struct mycfs_rq *cfs_rq, struct sched_mycfs_entity *se)
{
	struct rb_node **link = &cfs_rq->tasks_timeline.rb_node;
	struct rb_node *parent = NULL;
	struct sched_mycfs_entity *entry;
	int leftmost = 1;
	MYCFS_DEBUG("__enqueue %p %p\n", cfs_rq, se);

	/*
	 * Find the right place in the rbtree:
	 */
	while (*link) {
		parent = *link;
		entry = rb_entry(parent, struct sched_mycfs_entity, run_node);
		/*
		 * We dont care about collisions. Nodes with
		 * the same key stay together.
		 */
		if (entity_before(se, entry)) {
			link = &parent->rb_left;
		} else {
			link = &parent->rb_right;
			leftmost = 0;
		}
	}

	/*
	 * Maintain a cache of leftmost tree entries (it is frequently
	 * used):
	 */
	if (leftmost)
		cfs_rq->rb_leftmost = &se->run_node;

	rb_link_node(&se->run_node, parent, link);
	rb_insert_color(&se->run_node, &cfs_rq->tasks_timeline);
}

static void __dequeue_entity(struct mycfs_rq *cfs_rq, struct sched_mycfs_entity *se)
{
	MYCFS_DEBUG("__dequeue %p %p\n", cfs_rq, se);
	if (cfs_rq->rb_leftmost == &se->run_node) {
		struct rb_node *next_node;
		next_node = rb_next(&se->run_node);
		cfs_rq->rb_leftmost = next_node;
	}

	rb_erase(&se->run_node, &cfs_rq->tasks_timeline);
}

static inline int
__update_curr_mycfs(struct mycfs_rq *cfs_rq, struct sched_mycfs_entity *curr,
	      unsigned long delta_exec, u64 now)
{
	struct task_struct *p = task_of(curr);
	curr->vruntime += delta_exec;
	mycfs_update_min_vruntime(cfs_rq);
	/* update periods */
	if(now - cfs_rq->curr_period_start > MYCFS_CPU_PERIOD) {
		/* update ourselves to a new period */
		++cfs_rq->curr_period;
		/* TODO: is this the right way to shift periods? */
		cfs_rq->curr_period_start = now - delta_exec;
	}
	if(curr->curr_period_id != cfs_rq->curr_period) {
		/* this is a new period */
		curr->curr_period_id = cfs_rq->curr_period;
		curr->curr_period_sum_runtime = delta_exec;
	}
	else {
		/* add to current period */
		curr->curr_period_sum_runtime += delta_exec;
	}
	if(p->mycfs_cpu_limit == 0) {
		return 0;
	}
	/* are we at too much usage? */
	if(curr->curr_period_sum_runtime * 100 > MYCFS_CPU_PERIOD * p->mycfs_cpu_limit ) {
		return 1;
	}
	return 0;
}

static int mycfs_update_curr(struct mycfs_rq *cfs_rq)
{
	struct sched_mycfs_entity *curr = cfs_rq->curr;
	u64 now = cfs_rq->rq->clock_task;
	unsigned long delta_exec;
	int rc;

	if(!curr)
		return 0;
	
	delta_exec = (unsigned long)(now - curr->exec_start);
	if(!delta_exec)
		return 0;

	rc = __update_curr_mycfs(cfs_rq, curr, delta_exec, now);
	curr->exec_start = now;

	{
		struct task_struct *curtask = task_of(curr);
		cpuacct_charge(curtask, delta_exec);
		account_group_exec_runtime(curtask, delta_exec);
	}
	return rc;
}

/* functions for sched_class */
static void enqueue_task_mycfs(struct rq *rq, struct task_struct *p, int flags)
{
	struct sched_mycfs_entity *se = &p->myse;
	struct mycfs_rq *mycfs_rq = mycfs_rq_of(se);
	MYCFS_DEBUG("enqueue_task_mycfs %p %p %d\n", rq, p, flags);
	MYCFS_DEBUG("EQBEFORE\n");
	MYCFS_DUMP_SE(se);
	if(se->on_rq)
		return;

	if (!(flags & ENQUEUE_WAKEUP) || (flags & ENQUEUE_WAKING))
		se->vruntime += mycfs_rq->min_vruntime;
	
	/* we shouldn't be hitting the limit here */
	mycfs_update_curr(mycfs_rq);
	if (flags & ENQUEUE_WAKEUP) {
		u64 vruntime = mycfs_rq->min_vruntime;
		vruntime = max_vruntime(se->vruntime, vruntime);
		se->vruntime = vruntime;
	}
	if (se != mycfs_rq->curr)
		__enqueue_entity(mycfs_rq, se);
	se->on_rq = 1;
	inc_nr_running(rq);
	++mycfs_rq->nr_running;
	MYCFS_DEBUG("EQAFTER\n");
	MYCFS_DUMP_SE(se);
	MYCFS_DEBUG("enqueue_task_mycfs: Now with %lu\n", mycfs_rq->nr_running);
}

static void dequeue_task_mycfs(struct rq *rq, struct task_struct *p, int flags)
{
	struct sched_mycfs_entity *se = &p->myse;
	struct mycfs_rq *cfs_rq = mycfs_rq_of(se);
	MYCFS_DEBUG("dequeue_task_mycfs %p %p %d\n", rq, p, flags);
	MYCFS_DEBUG("DQBEFORE\n");
	MYCFS_DUMP_CFSRQ(cfs_rq);
	MYCFS_DUMP_SE(se);

	/* we're dequeuing, doesn't matter if we hit limit */
	mycfs_update_curr(cfs_rq);

	if (se != cfs_rq->curr)
		__dequeue_entity(cfs_rq, se);
	se->on_rq = 0;
	--cfs_rq->nr_running;
	dec_nr_running(rq);

	if (!(flags & DEQUEUE_SLEEP))
		se->vruntime -= cfs_rq->min_vruntime;

	mycfs_update_min_vruntime(cfs_rq);
	MYCFS_DEBUG("DQAFTER\n");
	MYCFS_DUMP_CFSRQ(cfs_rq);
	MYCFS_DUMP_SE(se);
}

static void yield_task_mycfs(struct rq *rq)
{
	struct mycfs_rq *mycfs_rq = &rq->mycfs;
	MYCFS_DEBUG("yield_task_mycfs\n");

	if(rq->nr_running == 1)
		return;
	update_rq_clock(rq);
	/* TODO: what to do if exceed limit here? */
	mycfs_update_curr(mycfs_rq);
	rq->skip_clock_update = 1;
}

static int
wakeup_preempt_entity(struct sched_mycfs_entity *curr, struct sched_mycfs_entity *se)
{
	s64 gran, vdiff = curr->vruntime - se->vruntime;

	if (vdiff <= 0)
		return -1;

	gran = sysctl_sched_wakeup_granularity;//wakeup_gran(curr, se);
	if (vdiff > gran)
		return 1;

	return 0;
}

static void check_preempt_curr_mycfs(struct rq *rq, struct task_struct *p, int flags)
{
	struct task_struct *curr = rq->curr;
	struct sched_mycfs_entity *se = &curr->myse, *pse = &p->myse;
	int need_resched;
	MYCFS_DEBUG("check_preempt_curr_mycfs %p %p %d\n", rq, p, flags);

	if (unlikely(se == pse))
		return;

	need_resched = mycfs_update_curr(mycfs_rq_of(se));
	if (!need_resched && wakeup_preempt_entity(se, pse) != 1) {
		return;
	}
	resched_task(curr);
}

static inline void mycfs_set_next_entity(struct mycfs_rq *cfs_rq, struct sched_mycfs_entity *se)
{
	if(se->on_rq)
		__dequeue_entity(cfs_rq, se);
	
	cfs_rq->curr = se;
	se->exec_start = cfs_rq->rq->clock_task;
	MYCFS_DEBUG("mycfs_set_next_entity\n");
	MYCFS_DUMP_SE(se);
}

static struct task_struct * pick_next_task_mycfs(struct rq *rq)
{
	struct mycfs_rq *cfs_rq = &rq->mycfs;
	struct sched_mycfs_entity *se;
//	MYCFS_DEBUG("pick_next_task_mycfs: nr_running %lu\n", mycfs_rq->nr_running);

	if (!cfs_rq->nr_running)
		return NULL;
	

	/* we pick the first entity on the rbtree */
	se = mycfs_pick_first_entity(cfs_rq);
	MYCFS_DEBUG("pick_next_task_mycfs: leftmost %p\n", se);
	if(!se)
		return NULL;

	mycfs_set_next_entity(cfs_rq, se);

	return task_of(se);
}

static void put_prev_task_mycfs(struct rq *rq, struct task_struct *p)
{
	struct sched_mycfs_entity *se = &p->myse;
	struct mycfs_rq *cfs_rq = mycfs_rq_of(se);
	MYCFS_DEBUG("put_prev_task_mycfs %p %p\n", rq, p);

	if(se->on_rq) {
		MYCFS_DEBUG("ppt: about to update_curr\n");
		mycfs_update_curr(cfs_rq);
		MYCFS_DEBUG("ppt: about to __enqueue_entity\n");
		__enqueue_entity(cfs_rq, se);
	}
	cfs_rq->curr = NULL;
}

#ifdef CONFIG_SMP
static int select_task_rq_mycfs(struct task_struct *p, int sd_flag, int flags)
{
	MYCFS_DEBUG("select_task_rq_mycfs\n");
	return task_cpu(p);
}

static void task_waking_mycfs (struct task_struct *task)
{
	struct sched_mycfs_entity *se = &task->myse;
	struct mycfs_rq *cfs_rq = mycfs_rq_of(se);
	u64 min_vruntime;

#ifndef CONFIG_64BIT
	u64 min_vruntime_copy;

	do {
		min_vruntime_copy = cfs_rq->min_vruntime_copy;
		smp_rmb();
		min_vruntime = cfs_rq->min_vruntime;
	} while (min_vruntime != min_vruntime_copy);
#else
	min_vruntime = cfs_rq->min_vruntime;
#endif
	se->vruntime -= min_vruntime;
	MYCFS_DEBUG("did task_waking_mycfs\n");
}

static void rq_online_mycfs(struct rq *rq)
{
	MYCFS_DEBUG("rq_online_mycfs\n");
	/* intentionally blank */
}

static void rq_offline_mycfs(struct rq *rq)
{
	MYCFS_DEBUG("rq_offline_mycfs\n");
	/* intentionally blank */
}
#endif

static void set_curr_task_mycfs (struct rq *rq)
{
	struct sched_mycfs_entity *se = &rq->curr->myse;

	struct mycfs_rq *cfs_rq = mycfs_rq_of(se);
	MYCFS_DEBUG("set_curr_task_mycfs %p\n", rq);
	MYCFS_DEBUG("BEFORE SNE\n");
	MYCFS_DUMP_CFSRQ(cfs_rq);
	MYCFS_DUMP_SE(se);
	mycfs_set_next_entity(cfs_rq, se);
	MYCFS_DEBUG("AFTER SNE\n");
	MYCFS_DUMP_CFSRQ(cfs_rq);
	MYCFS_DUMP_SE(se);
}

static void mycfs_check_preempt_tick(struct mycfs_rq *cfs_rq, struct sched_mycfs_entity *curr)
{
	unsigned long ideal_runtime, delta_exec;
	struct sched_mycfs_entity *se;
	u64 now = cfs_rq->rq->clock_task;
	s64 delta;

	ideal_runtime = MYCFS_GRANULARITY;

	delta_exec = (unsigned long)(now - curr->exec_start);
	if (delta_exec > ideal_runtime) {
		resched_task(cfs_rq->rq->curr);
		return;
	}
	/*
	 * Ensure that a task that missed wakeup preemption by a
	 * narrow margin doesn't have to wait for a full slice.
	 * This also mitigates buddy induced latencies under load.
	 */
	if (delta_exec < MYCFS_GRANULARITY)
		return;

	se = mycfs_pick_first_entity(cfs_rq);
	delta = curr->vruntime - se->vruntime;

	if (delta < 0)
		return;

	if (delta > ideal_runtime)
		resched_task(cfs_rq->rq->curr);
}

static void task_tick_mycfs (struct rq *rq, struct task_struct *curr, int queued)
{
	struct sched_mycfs_entity *se = &curr->myse;
	struct mycfs_rq *cfs_rq = mycfs_rq_of(se);
	int need_resched;
	MYCFS_DEBUG("task_tick_mycfs\n");
	need_resched = mycfs_update_curr(cfs_rq);
	if(need_resched) {
		resched_task(curr);
		return;
	}
	if(cfs_rq->nr_running > 1)
	{
		mycfs_check_preempt_tick(cfs_rq, se);
	}
}

static void task_fork_mycfs (struct task_struct *p)
{
	struct mycfs_rq *cfs_rq;
	struct sched_mycfs_entity *se = &p->myse, *curr;
	int this_cpu = smp_processor_id();
	struct rq *rq = this_rq();
	unsigned long flags;
	int need_resched;
	MYCFS_DEBUG("task_fork_mycfs\n");

	raw_spin_lock_irqsave(&rq->lock, flags);

	update_rq_clock(rq);

	cfs_rq = &rq->mycfs;
	curr = cfs_rq->curr;

	if (unlikely(task_cpu(p) != this_cpu)) {
		rcu_read_lock();
		__set_task_cpu(p, this_cpu);
		rcu_read_unlock();
	}

	need_resched = mycfs_update_curr(cfs_rq);

	if (curr)
		se->vruntime = curr->vruntime;

	{
		u64 vruntime = cfs_rq->min_vruntime;
		vruntime = max_vruntime(se->vruntime, vruntime);
		se->vruntime = vruntime;
	}

	if (need_resched && curr && entity_before(curr, se)) {
		/*
		 * Upon rescheduling, sched_class::put_prev_task() will place
		 * 'current' within the tree based on its new key value.
		 */
		swap(curr->vruntime, se->vruntime);
		resched_task(rq->curr);
	}

	se->vruntime -= cfs_rq->min_vruntime;
	raw_spin_unlock_irqrestore(&rq->lock, flags);
	MYCFS_DEBUG("done task_fork_mycfs\n");
}

static void switched_from_mycfs (struct rq *this_rq, struct task_struct *p)
{
	struct sched_mycfs_entity *se = &p->myse;
	struct mycfs_rq *cfs_rq = mycfs_rq_of(se);
	MYCFS_DEBUG("switched_from_mycfs\n");
	if (!se->on_rq && p->state != TASK_RUNNING) {
		{
			u64 vruntime = cfs_rq->min_vruntime;
			vruntime = max_vruntime(se->vruntime, vruntime);
			se->vruntime = vruntime;
		}
		se->vruntime -= cfs_rq->min_vruntime;
	}
}

static void switched_to_mycfs (struct rq *rq, struct task_struct *task)
{
	MYCFS_DEBUG("switched_to_mycfs\n");
	task->mycfs_cpu_limit = 0;
	if (rq->curr == task)
		resched_task(rq->curr);
}

static void prio_changed_mycfs (struct rq *this_rq, struct task_struct *task,
		     int oldprio)
{
	MYCFS_DEBUG("prio_changed_mycfs\n");
	/* intentionally empty */
}

static unsigned int get_rr_interval_mycfs (struct rq *rq,
				 struct task_struct *task) {
	MYCFS_DEBUG("get_rr_interval_mycfs\n");
	return NS_TO_JIFFIES(MYCFS_GRANULARITY);
}
