#include "sched.h"

const struct sched_class wrr_sched_class;

static inline struct task_struct *
wrr_task_of(struct sched_wrr_entity *wrr_se) {
	return container_of(wrr_se, struct task_struct, wrr);
}

void __init init_wrr_rq(struct wrr_rq *wrr_rq, bool balancer) {
	INIT_LIST_HEAD(&wrr_rq->head);
	wrr_rq->total_weight = (atomic_t) ATOMIC_INIT(0);
	//if (balancer)
		wrr_rq->next_balance = WRR_BALANCE;
}

static void update_curr_wrr(struct rq *rq);

static void enqueue_task_wrr(struct rq *rq, struct task_struct *p, int flags) {
	// TODO(wrr): deal with flags. we probably want to support ENQUEUE_HEAD

	struct sched_wrr_entity *wrr_se = &p->wrr;
	struct wrr_rq *wrr_rq = &rq->wrr;

	INIT_LIST_HEAD(&wrr_se->list);
	list_add_tail(&wrr_se->list, &wrr_rq->head);

	add_nr_running(rq, 1);
	atomic_add(p->wrr.weight, &wrr_rq->total_weight);
}

static void dequeue_task_wrr(struct rq *rq, struct task_struct *p, int flags) {
	struct sched_wrr_entity *wrr_se = &p->wrr;
	struct wrr_rq *wrr_rq = &rq->wrr;

	update_curr_wrr(rq);
	list_del(&wrr_se->list);

	sub_nr_running(rq, 1);
	atomic_sub(p->wrr.weight,&wrr_rq->total_weight);
}

static void yield_task_wrr(struct rq *rq) {
	struct task_struct *curr = rq->curr;
	dequeue_task_wrr(rq, curr, 0);
	enqueue_task_wrr(rq, curr, 0);
}

// never preempt
static void check_preempt_curr_wrr(struct rq *rq, struct task_struct *p, int flags) {
	return;
}

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

static void put_prev_task_wrr(struct rq *rq, struct task_struct *prev) {
	update_curr_wrr(rq);
}

static int select_task_rq_wrr(struct task_struct *p, int task_cpu, int sd_flag, int flags) {
	int cpu;
	int min_cpu = task_cpu;
	unsigned int min_weight = atomic_read(&cpu_rq(task_cpu)->wrr.total_weight);
	unsigned int cur_weight;

	// TODO(wrr): do we actually need rcu_read_lock here?
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

static struct callback_head wrr_balance_callback;

static void wrr_balance(struct rq *this_rq) {
	printk(KERN_INFO "would balance %d\n", this_rq->cpu);
}

static void task_tick_wrr(struct rq *rq, struct task_struct *p, int queued) {
	struct wrr_rq *wrr_rq = &rq->wrr;

	update_curr_wrr(rq);

	if (wrr_rq->next_balance && !--wrr_rq->next_balance) {
		wrr_rq->next_balance = WRR_BALANCE;
		queue_balance_callback(rq, &wrr_balance_callback, wrr_balance);
	}


	if (--p->wrr.time_slice)
		return;
	p->wrr.time_slice = WRR_TIMESLICE * p->wrr.weight;

	yield_task_wrr(rq);
	resched_curr(rq);
}

static void switched_to_wrr(struct rq *rq, struct task_struct *p) {
	p->wrr.weight = WRR_DEFAULT_WEIGHT;
}

static void prio_changed_wrr(struct rq *rq, struct task_struct *p, int oldprio) {
	// TODO(wrr): we can probably use the existing prio code to implement weights
	return;
};

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

	return;
}

static unsigned int
get_rr_interval_wrr(struct rq *rq, struct task_struct *p) {
	return WRR_TIMESLICE * p->wrr.weight;
}

const struct sched_class wrr_sched_class = {
	.next = &fair_sched_class,	// TODO(wrr): change to idle?

	.enqueue_task		= enqueue_task_wrr,
	.dequeue_task		= dequeue_task_wrr,
	.yield_task		= yield_task_wrr,
	/* .yield_to_task */	// optional, probably optimization when a task yields to its own thread group.
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

	/* .switched_from */	// optional, maybe we can reset prio/weight here?
	.switched_to		= switched_to_wrr,
	.prio_changed		= prio_changed_wrr,

	.get_rr_interval	= get_rr_interval_wrr,	// optional, but nice to have probably

	.update_curr		= update_curr_wrr,

#ifdef CONFIG_FAIR_GROUP_SCHED
	/* .task_change_group */			// optional
#endif
};

#ifdef CONFIG_SCHED_DEBUG
void print_wrr_stats(struct seq_file *m, int cpu)
{
	print_wrr_rq(m, cpu, &cpu_rq(cpu)->wrr);
}
#endif /* CONFIG_SCHED_DEBUG */
