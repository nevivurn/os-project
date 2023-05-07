#include "sched.h"

const struct sched_class wrr_sched_class;

static inline struct task_struct *
wrr_task_of(struct sched_wrr_entity *wrr_se) {
	return container_of(wrr_se, struct task_struct, wrr);
}

void __init init_wrr_rq(struct wrr_rq *wrr_rq) {
	INIT_LIST_HEAD(&wrr_rq->head);
}

static void update_curr_wrr(struct rq *rq);

// TODO(wrr): figure out whether rcu even makes sense here
// probably need to implement balance / weight-tracking code first

static void enqueue_task_wrr(struct rq *rq, struct task_struct *p, int flags) {
	// TODO(wrr): deal with flags. we pboably want to support ENQUEUE_HEAD

	struct sched_wrr_entity *wrr_se = &p->wrr;
	struct wrr_rq *wrr_rq = &rq->wrr;

	INIT_LIST_HEAD(&wrr_se->list);
	list_add_tail_rcu(&wrr_se->list, &wrr_rq->head);
}

static void dequeue_task_wrr(struct rq *rq, struct task_struct *p, int flags) {
	struct sched_wrr_entity *wrr_se = &p->wrr;

	update_curr_wrr(rq);
	list_del_rcu(&wrr_se->list);
	// TODO(wrr): boot hangs if we do synchronize_rcu here for some reason
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

	rcu_read_lock();
	wrr_se = list_first_or_null_rcu(&wrr_rq->head, struct sched_wrr_entity, list);
	rcu_read_unlock();

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
	// TODO(wrr): we can try doing balancing here, eg. SD_BALANCE_FORK
	// NOTE: we do not hold locks here, need RCU for reading CPUs and rq's
	return task_cpu;
}

static void set_curr_task_wrr(struct rq *rq) {
	rq->curr->se.exec_start = rq_clock_task(rq);
}

static void task_tick_wrr(struct rq *rq, struct task_struct *p, int queued) {
	update_curr_wrr(rq);

	if (--p->wrr.time_slice)
		return;
	p->wrr.time_slice = WRR_TIMESLICE * WRR_DEFAULT_WEIGHT;

	yield_task_wrr(rq);
	resched_curr(rq);
}

static void switched_to_wrr(struct rq *rq, struct task_struct *p) {
	// TODO(wrr): reset weights? see also switched_from
	// it may not even be necessary, investigate what happens to nice when
	// switching from/to rt
	return;
}

static void prio_changed_wrr(struct rq *rq, struct task_struct *p, int oldprio) {
	// TODO(wrr): we can probably use the existing prio code to implement weights
	return;
};

static void update_curr_wrr(struct rq *rq) {
	// TODO(wrr): investigate which stats we need to keep up-to-date
	return;
}

static unsigned int
get_rr_interval_wrr(struct rq *rq, struct task_struct *p) {
	// TODO(wrr): weight
	return WRR_TIMESLICE * WRR_DEFAULT_WEIGHT;
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
