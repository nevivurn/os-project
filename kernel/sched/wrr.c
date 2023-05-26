#include "sched.h"

const struct sched_class wrr_sched_class;

static inline struct task_struct *
wrr_task_of(struct sched_wrr_entity *wrr_se) {
	return container_of(wrr_se, struct task_struct, wrr);
}

/*
 * Global load balancing locks and timestamp.
 */
static DEFINE_RAW_SPINLOCK(wrr_balance_lock);
static unsigned long wrr_global_next_balance;

/*
 * Initialize the wrr_rq with initial empty values.
 */
void __init init_wrr_rq(struct wrr_rq *wrr_rq) {
	INIT_LIST_HEAD(&wrr_rq->head);
	wrr_rq->total_weight = (atomic_t) ATOMIC_INIT(0);
	wrr_rq->wrr_next_balance = jiffies;
}

static void update_curr_wrr(struct rq *rq);

/*
 * Append the incoming task to the WRR list.
 * Called with rq->lock.
 */
static void enqueue_task_wrr(struct rq *rq, struct task_struct *p, int flags) {
	struct sched_wrr_entity *wrr_se = &p->wrr;
	struct wrr_rq *wrr_rq = &rq->wrr;

	INIT_LIST_HEAD(&wrr_se->list);
	list_add_tail(&wrr_se->list, &wrr_rq->head);

	add_nr_running(rq, 1);
	atomic_add(p->wrr.weight, &wrr_rq->total_weight);
}

/*
 * Remove the given task from the WRR list.
 * Called with rq->lock.
 */
static void dequeue_task_wrr(struct rq *rq, struct task_struct *p, int flags) {
	struct sched_wrr_entity *wrr_se = &p->wrr;
	struct wrr_rq *wrr_rq = &rq->wrr;

	update_curr_wrr(rq);
	list_del(&wrr_se->list);

	sub_nr_running(rq, 1);
	atomic_sub(p->wrr.weight,&wrr_rq->total_weight);
}

/*
 * Requeue the current task to the tail of the WRR list.
 * Does not reset timeslice.
 * Called with rq->lock.
 */
static void yield_task_wrr(struct rq *rq) {
	struct task_struct *curr = rq->curr;
	dequeue_task_wrr(rq, curr, 0);
	enqueue_task_wrr(rq, curr, 0);
}

/* Never preempt */
static void check_preempt_curr_wrr(struct rq *rq, struct task_struct *p, int flags) {}

/*
 * Pick next task from the WRR list: simply pick the current list head.
 * Called with rq->lock.
 */
static struct task_struct *
pick_next_task_wrr(struct rq *rq, struct task_struct *prev, struct rq_flags *rf) {
	struct sched_wrr_entity *wrr_se;
	struct wrr_rq *wrr_rq = &rq->wrr;
	struct task_struct *p;

	wrr_se = list_first_entry_or_null(&wrr_rq->head, struct sched_wrr_entity, list);

	if (!wrr_se)
		return NULL;

	put_prev_task(rq, prev);
	p = wrr_task_of(wrr_se);
	p->se.exec_start = rq_clock_task(rq);

	return p;
}

/* Just update current statistics */
static void put_prev_task_wrr(struct rq *rq, struct task_struct *prev) {
	update_curr_wrr(rq);
}

/*
 * Select a CPU to place the incoming task in.
 * Chooses the CPU with the minimum WRR total_weight.
 * Called with no locks.
 *
 * It is OK to traverse and read the total_weights without acquiring any
 * further locks, as we are tolerant of occasional mis-placement of tasks, as
 * long as basic constraints are kept.
 */
static int select_task_rq_wrr(struct task_struct *p, int task_cpu, int sd_flag, int flags) {
	int cpu;
	int min_cpu = task_cpu;
	unsigned int min_weight = atomic_read(&cpu_rq(task_cpu)->wrr.total_weight);
	unsigned int cur_weight;

	// We must acquire RCU read locks here as we need to traverse the CPU list.
	rcu_read_lock();
	for_each_possible_cpu(cpu) {
		if (!cpumask_test_cpu(cpu, &p->cpus_allowed))
			continue;

		cur_weight = atomic_read(&cpu_rq(cpu)->wrr.total_weight);
		if (cur_weight < min_weight) {
			min_cpu = cpu;
			min_weight = cur_weight;
		}
	}
	rcu_read_unlock();

	return min_cpu;
}

static void set_curr_task_wrr(struct rq *rq) {
	rq->curr->se.exec_start = rq_clock_task(rq);
}

/*
 * Update per-rq next_balance timestamps.
 */
static void update_wrr_load_balance(unsigned long next_balance) {
	int cpu;
	for_each_possible_cpu(cpu) {
		struct rq *rq = cpu_rq(cpu);
		rq->wrr.wrr_next_balance = next_balance;
	}
}

/*
 * Actually perfrom load balancing.
 * Called with wrr_balance_lock.
 * Acquires and releases rq->lock for the two locks.
 */
static void _wrr_load_balance(void) {
	struct rq *min_rq, *max_rq;
	struct sched_wrr_entity *wrr_se, *max_wrr_se = NULL;
	struct task_struct *p;

	int cpu;
	int min_cpu = -1, max_cpu = -1;
	unsigned int min_weight = 0, max_weight = 0;
	unsigned int p_weight;

	// Search for the min/max CPU
	rcu_read_lock();
	for_each_possible_cpu(cpu) {
		struct rq *rq = cpu_rq(cpu);
		struct wrr_rq *wrr = &rq->wrr;
		unsigned int weight = atomic_read(&wrr->total_weight);

		if (min_weight > weight || min_cpu == -1)
			min_cpu = cpu, min_weight = weight;
		if (max_weight < weight || max_cpu == -1)
			max_cpu = cpu, max_weight = weight;
	}
	rcu_read_unlock();
	if (min_cpu == max_cpu)
		return;

	min_rq = cpu_rq(min_cpu);
	max_rq = cpu_rq(max_cpu);

	double_rq_lock(min_rq, max_rq);

	// Search for the max task from max cpu
	list_for_each_entry(wrr_se, &max_rq->wrr.head, list) {
		struct task_struct *p = wrr_task_of(wrr_se);
		unsigned int weight = wrr_se->weight;

		if (task_running(max_rq, p))
			continue;
		if (!cpumask_test_cpu(min_cpu, &p->cpus_allowed))
			continue;
		if (max_weight < weight || min_weight+weight >= max_weight-weight)
			continue;

		if (!max_wrr_se || max_wrr_se->weight < weight)
			max_wrr_se = wrr_se;
	}
	if (!max_wrr_se) {
		double_rq_unlock(min_rq, max_rq);
		return;
	}

	p = wrr_task_of(max_wrr_se);
	p_weight = max_wrr_se->weight;

	// Migrate the task
	deactivate_task(max_rq, p, 0);
	set_task_cpu(p, min_cpu);
	activate_task(min_rq, p, 0);

	double_rq_unlock(min_rq, max_rq);

	printk(KERN_DEBUG "[WRR LOAD BALANCING] jiffies: %Ld\n"
		"[WRR LOAD BALANCING] max_cpu: %d, total weight: %u\n"
		"[WRR LOAD BALANCING] min_cpu: %d, total weight: %u\n"
		"[WRR LOAD BALANCING] migrated task name: %s, task weight: %u\n",
		(long long) jiffies,
		max_cpu, max_weight,
		min_cpu, min_weight,
		p->comm, p_weight);
}

/*
 * Called from scheduler_tick to load balance WRR queues.
 * Called with no locks.
 *
 * Check the per-rq next_balance timestamp first before acquiring the
 * wrr_balance_lock to check the global timestamp. This allows us to avoid
 * wrr_balance_lock contention in the vast majority of scheduler_tick() calls.
 */
void wrr_load_balance(struct rq *rq) {
	struct wrr_rq *wrr_rq = &rq->wrr;

	// It is OK if we race a bit, it will be corrected soon enough
	if (!time_after_eq(jiffies, wrr_rq->wrr_next_balance))
		return;

	raw_spin_lock(&wrr_balance_lock);

	if (!time_after_eq(jiffies, wrr_global_next_balance)) {
		raw_spin_unlock(&wrr_balance_lock);
		return;
	}

	wrr_global_next_balance = jiffies + WRR_BALANCE;
	update_wrr_load_balance(wrr_global_next_balance);
	_wrr_load_balance();

	raw_spin_unlock(&wrr_balance_lock);
}

/*
 * Tick down the current running task and yield if it has exhausted its
 * time_slice.
 * Called with rq->lock.
 */
static void task_tick_wrr(struct rq *rq, struct task_struct *p, int queued) {
	update_curr_wrr(rq);

	if (--p->wrr.time_slice)
		return;
	p->wrr.time_slice = WRR_TIMESLICE * p->wrr.weight;

	yield_task_wrr(rq);
	resched_curr(rq);
}

/*
 * Just reset the task's weight.
 */
static void switched_to_wrr(struct rq *rq, struct task_struct *p) {
	p->wrr.weight = WRR_DEFAULT_WEIGHT;
}

static void prio_changed_wrr(struct rq *rq, struct task_struct *p, int oldprio) {}

/*
 * Update current process' runtime statistics.
 */
static void update_curr_wrr(struct rq *rq) {
	struct task_struct *curr = rq->curr;

	u64 now, delta_exec;

	if (curr->sched_class != &wrr_sched_class)
		return;

	now = rq_clock_task(rq);
	delta_exec = now - curr->se.exec_start;
	if (unlikely((s64)delta_exec <= 0))
		return;

	schedstat_set(curr->se.statistics.exec_max,
		      max(curr->se.statistics.exec_max, delta_exec));

	curr->se.sum_exec_runtime += delta_exec;
	account_group_exec_runtime(curr, delta_exec);

	curr->se.exec_start = now;
	cgroup_account_cputime(curr, delta_exec);
}

static unsigned int
get_rr_interval_wrr(struct rq *rq, struct task_struct *p) {
	return WRR_TIMESLICE * p->wrr.weight;
}

const struct sched_class wrr_sched_class = {
	.next = &fair_sched_class,

	.enqueue_task		= enqueue_task_wrr,
	.dequeue_task		= dequeue_task_wrr,
	.yield_task		= yield_task_wrr,
	/* .yield_to_task */	// optional, just an optimization when a task yields to its own thread group.
	.check_preempt_curr	= check_preempt_curr_wrr,

	.pick_next_task		= pick_next_task_wrr,
	.put_prev_task		= put_prev_task_wrr,

#ifdef CONFIG_SMP
	.select_task_rq		= select_task_rq_wrr,
	/* .migrate_task_rq */	// optional

	/* .task_woken */	// optional

	.set_cpus_allowed	= set_cpus_allowed_common,

	/* .rq_online */	// optional
	/* .rq_offline */	// optional
#endif

	.set_curr_task		= set_curr_task_wrr,
	.task_tick		= task_tick_wrr,
	/* .task_fork */	// optional
	/* .task_dead */	// optional

	/* .switched_from */	// optional
	.switched_to		= switched_to_wrr,
	.prio_changed		= prio_changed_wrr,

	.get_rr_interval	= get_rr_interval_wrr,	// optional, but nice to have

	.update_curr		= update_curr_wrr,

#ifdef CONFIG_FAIR_GROUP_SCHED
	/* .task_change_group */	// optional
#endif
};

/*
 * Set the task's weight.
 */
static long sched_setweight(struct task_struct *p, unsigned int weight) {
	struct rq *rq;
	struct rq_flags rf;
	const struct cred *cred = current_cred(), *pcred = __task_cred(p);
	int retval;

	if (!capable(CAP_SYS_NICE)
		&& !(uid_eq(cred->euid, pcred->euid) && (p->wrr.weight >= weight))
		&& !(uid_eq(cred->euid, pcred->uid) && (p->wrr.weight >= weight)))
		return -EPERM;

	// We need to synchronize with rq->wrr, and make sure it's not suddenly
	// moving around while we update total_weight.
	rq = task_rq_lock(p, &rf);
	retval = -EINVAL;
	if (fair_policy(p->policy)) {
		if (task_on_rq_queued(p))
			atomic_sub(p->wrr.weight, &rq->wrr.total_weight);
		p->wrr.weight = weight;
		if (task_on_rq_queued(p))
			atomic_add(p->wrr.weight, &rq->wrr.total_weight);
		retval = 0;
	}
	task_rq_unlock(rq, p, &rf);

	return retval;
}

/*
 * Get the task's weight.
 */
static long sched_getweight(struct task_struct *p) {
	struct rq *rq;
	struct rq_flags rf;
	int retval;

	// While it is possible to be less strict with acquiring locks here, it
	// doesn't hurt to do so. It's not lie sched_getweight is being called
	// every scheduler tick or anything.
	rq = task_rq_lock(p, &rf);
	retval = -EINVAL;
	if (p->sched_class == &wrr_sched_class)
		retval = p->wrr.weight;
	task_rq_unlock(rq, p, &rf);

	return retval;
}

/*
 * sched_setweight syscall definition.
 * Adquires RCU read lock as it needs to search the task list.
 */
SYSCALL_DEFINE2(sched_setweight, pid_t, pid, unsigned int, weight) {
	struct task_struct *p;
	int retval;

	if (pid < 0)
		return -EINVAL;
	if (weight < 1 || weight > 20)
		return -EINVAL;

	rcu_read_lock();
	retval = -ESRCH;
	p = pid ? find_task_by_vpid(pid) : current;
	if (p)
		retval = sched_setweight(p, weight);
	rcu_read_unlock();

	return retval;
}

/*
 * sched_getweight syscall definition.
 * Adquires RCU read lock as it needs to search the task list.
 */
SYSCALL_DEFINE1(sched_getweight, pid_t, pid) {
	struct task_struct *p;
	int retval;

	if (pid < 0)
		return -EINVAL;

	rcu_read_lock();
	retval = -ESRCH;
	p = pid ? find_task_by_vpid(pid) : current;
	if (p)
		retval = sched_getweight(p);
	rcu_read_unlock();

	return retval;
}

#ifdef CONFIG_SCHED_DEBUG
void print_wrr_stats(struct seq_file *m, int cpu)
{
	print_wrr_rq(m, cpu, &cpu_rq(cpu)->wrr);
}
#endif /* CONFIG_SCHED_DEBUG */
