[![Review Assignment Due Date](https://classroom.github.com/assets/deadline-readme-button-24ddc0f5d75046c5622901739e7c5dd533143b0c8e959d652212380cedb1ea36.svg)](https://classroom.github.com/a/_-wGDd5L)
Linux kernel
============

There are several guides for kernel developers and users. These guides can
be rendered in a number of formats, like HTML and PDF. Please read
Documentation/admin-guide/README.rst first.

In order to build the documentation, use ``make htmldocs`` or
``make pdfdocs``.  The formatted documentation can also be read online at:

    https://www.kernel.org/doc/html/latest/

There are various text files in the Documentation/ subdirectory,
several of them using the Restructured Text markup notation.
See Documentation/00-INDEX for a list of what is contained in each file.

Please read the Documentation/process/changes.rst file, as it contains the
requirements for building and running the kernel, and information about
the problems which may result by upgrading your kernel.


## WRR Scheduler

We implemented a weighted round-robin scheduler in `kernel/sched/wrr.c` to
replace the existing CFS scheduler. To facilitate this scheduler, we maintain
the following data in the WRR RQ structure:

```c
// kernel/sched/sched.h
struct wrr_rq {
	struct list_head	head;
	atomic_t		total_weight;
	unsigned long		wrr_next_balance;
};
```

The `head` field is a linked list that holds the runnable tasks in a FIFO order.
We also track the total weight of all runnable tasks in the runqueue in
`total_weight`. The `wrr_next_balance` field is an optimization that avoids lock
contention during load balancing, as explained below.

With this data structure, the implementation of the WRR scheduler becomes
straightforward. We simply enqueue, dequeue, and requeue tasks into the WRR list
as needed, using the existing kernel list API. The core scheduler logic in
`kernel/sched/core.c` handles most of the necessary lock acquisition for us.

### Load Balancing

For load balancing, we perform a global load balancing operation every two
seconds, triggered from `scheduler_tick()`. As the load balancing logic is
called concurrently from each CPU, it must be synchronized with glbal spinlock.

Since acquiring a global spinlock on every call to `scheduler_tick()` just to
check a timestamp would be very costly, we perform a minor optimization. We also
hold the next_balance timestamp on every runqueue, which can be compared with
`jiffies` without acquiring any locks.

Once it is determined that the load balancing logic should be performed, we
iterate over every CPU search and transfer the max weight task from the max
total-weight runqueue to the min total-weight runqueue.
