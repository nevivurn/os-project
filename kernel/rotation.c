#include <linux/atomic.h>
#include <linux/kernel.h>
#include <linux/rotation.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/syscalls.h>
#include <linux/wait.h>
#include <linux/list.h>

static DECLARE_WAIT_QUEUE_HEAD(rot_wq);

// These are accessed without any locks; be careful!
static int device_degree;
static atomic_t w_waiters[360] = {[0 ... 359] = ATOMIC_INIT(0)};

struct rotlock_struct {
	struct list_head list;

	long id;
	int lo, hi, type;

	pid_t pid;
};

static DEFINE_SPINLOCK(rot_spin);
// All these must be accessed with spin_lock(&rot_spin)
static int r_runners[360]; // TODO: not sure if these need {READ,WRITE}_ONCE.
static int w_runners[360];
static LIST_HEAD(rot_list);
static long last_lock_id;

// For loop that handles hi<lo wraparound.
#define FOR_WRAP_RANGE(i, lo, hi, cmd) \
	{ \
		if (lo < hi) \
			for (i = lo; i <= hi; i++) cmd; \
		else { \
			for (i = 0; i <= lo; i++) cmd; \
			for (i = hi; i <= 359; i++) cmd; \
		} \
	}

// Try to allocate a unique lock id
long allocate_lock_id(void) {
	struct rotlock_struct *cur;
	long lid = last_lock_id;

retry:
	if (lid == LONG_MAX)
		lid = 1;
	else
		lid++;

	list_for_each_entry(cur, &rot_list, list)
		if (cur->id == lid)
			goto retry;

	last_lock_id = lid;
	return lid;
}

// Add a new rot_list entry, grabbing rot_list briefly to add the entry.
// Can fail with -ENOMEM if kmalloc fails.
long add_rot_list_entry(int lo, int hi, int type) {
	struct rotlock_struct *entry;
	long lid;
	pid_t current_pid;

	entry = kmalloc(sizeof *entry, GFP_KERNEL);
	if (!entry)
		return -ENOMEM;

	current_pid = task_pid_nr(current);

	entry->lo = lo;
	entry->hi = hi;
	entry->type = type;
	entry->pid = current_pid;
	INIT_LIST_HEAD(&entry->list);

	spin_lock(&rot_spin);

	lid = allocate_lock_id();
	entry->id = lid;
	list_add(&entry->list, &rot_list);

	spin_unlock(&rot_spin);

	return lid;
}

SYSCALL_DEFINE1(set_orientation, int, degree) {
	if (degree < 0 || degree >= 360)
		return -EINVAL;

	// All we need is a guarantee that any threads woken up by wake_up can
	// observe this write, no need for locks (probably).
	WRITE_ONCE(device_degree, degree);
	wake_up_interruptible(&rot_wq);

	return 0;
}

// Try to grab the given lock, returns whether it succeeded.
int try_lock(int lo, int hi, int type) {
	int ret, degree;
	int i;

	// Because try_lock is called after prepare_to_wait, we are guaranteed
	// to observe any changes made in set_orientation, as long as we
	// actually read it.  However, the degree can change between this check
	// and actually acquiring rot_spin, so we must check again afterwards.
	degree = READ_ONCE(device_degree);
	if (lo <= hi && (degree < lo || degree > hi))
		return 0;
	if (hi < lo && (degree < lo && degree > hi))
		return 0;

	// False positives (detecting waiting write locks when there aren't
	// any) can lead to deadlocks, but false negatives are fine, as long as
	// it eventually settles.
	if (type == ROT_READ && atomic_read(&w_waiters[degree]))
		return 0;

	ret = 1;
	spin_lock(&rot_spin);

	// Check again
	degree = READ_ONCE(device_degree);
	if (lo <= hi && (degree < lo || degree > hi)) {
		ret = 0;
		goto exit;
	}
	if (hi < lo && (degree < lo && degree > hi)) {
		ret = 0;
		goto exit;
	}

	FOR_WRAP_RANGE(i, lo, hi, ret &= !w_runners[i]);
	if (ret && type == ROT_WRITE)
		FOR_WRAP_RANGE(i, lo, hi, ret &= !r_runners[i]);

	if (ret) {
		if (type == ROT_READ)
			FOR_WRAP_RANGE(i, lo, hi, r_runners[i]++);
		if (type == ROT_WRITE)
			FOR_WRAP_RANGE(i, lo, hi, w_runners[i]++);
	}

exit:
	spin_unlock(&rot_spin);
	return ret;
}

SYSCALL_DEFINE3(rotation_lock, int, lo, int, hi, int, type) {
	long ret = 0;
	int i;
	DEFINE_WAIT(wait);

	if (lo < 0 || lo >= 360 || hi < 0 || hi >= 360)
		return -EINVAL;
	if (type != ROT_READ && type != ROT_WRITE)
		return -EINVAL;

	// It's probably very hard to actually guarantee ordering on concurrent
	// read-write waiters. As long as the increments happen atomically, it
	// will eventually settle.
	if (type == ROT_WRITE)
		FOR_WRAP_RANGE(i, lo, hi, atomic_inc(&w_waiters[i]));

	while (1) {
		prepare_to_wait(&rot_wq, &wait, TASK_INTERRUPTIBLE);

		if (try_lock(lo, hi, type))
			break;

		if (signal_pending(current)) {
			ret = -ERESTARTSYS;
			break;
		}

		schedule();
	}
	finish_wait(&rot_wq, &wait);

	if (ret)
		goto exit;

	ret = add_rot_list_entry(lo, hi, type);
	// Unlikely, but can fail with -ENOMEM.
	if (ret < 0) {
		spin_lock(&rot_spin);
		if (type == ROT_READ)
			FOR_WRAP_RANGE(i, lo, hi, r_runners[i]--);
		if (type == ROT_WRITE)
			FOR_WRAP_RANGE(i, lo, hi, w_runners[i]--);
		spin_unlock(&rot_spin);

		// It's possible that some other lock failed because of us
		// between us succeeding at try_lock and failing to
		// add_rot_list_entry.  In the case of write locks, the wake_up
		// is handled near the exit label, so we only need to handle
		// the read lock case here.
		if (type == ROT_READ)
			wake_up_interruptible(&rot_wq);
	}

exit:
	if (type == ROT_WRITE) {
		FOR_WRAP_RANGE(i, lo, hi, atomic_dec(&w_waiters[i]));
		// Without this, we may rarely have non-advancing read locks.
		// Specifically, a read lock failure due to waiting writers can
		// race with a write lock failure due to -ERESTARTSYS or
		// -ENOMEM. In the case of successful write locks, it doesn't
		// matter, as the read lock would have failed anyway.
		if (ret < 0)
			wake_up_interruptible(&rot_wq);
	}

	return ret;
}

SYSCALL_DEFINE1(rotation_unlock, long, lid) {
	long ret = 0;

	struct rotlock_struct *cur, *tmp;
	pid_t current_pid;
	int i;

	current_pid = task_pid_nr(current);

	spin_lock(&rot_spin);

	list_for_each_entry_safe(cur, tmp, &rot_list, list)
		if (cur->id == lid)
			goto found;

	// No matching lock found
	ret = -EINVAL;
	goto error;

found:
	// Lock not owned by process(group)
	if (cur->pid != current_pid) {
		ret = -EPERM;
		goto error;
	}

	if (cur->type == ROT_READ)
		FOR_WRAP_RANGE(i, cur->lo, cur->hi, r_runners[i]--);
	if (cur->type == ROT_WRITE)
		FOR_WRAP_RANGE(i, cur->lo, cur->hi, w_runners[i]--);

	list_del(&cur->list);
	spin_unlock(&rot_spin);

	wake_up_interruptible(&rot_wq);
	kfree(cur);
	return 0;

error:
	spin_unlock(&rot_spin);
	return ret;
}

void exit_rotlock(struct task_struct *tsk) {
	struct rotlock_struct *cur, *tmp;

	pid_t current_pid;
	int i;
	int woke = 0;

	current_pid = task_pid_nr(tsk);

	if (!thread_group_leader(tsk))
		return;

	spin_lock(&rot_spin);

	list_for_each_entry_safe(cur, tmp, &rot_list, list) {
		if (cur->pid != current_pid)
			continue;
		woke = 1;

		if (cur->type == ROT_READ)
			FOR_WRAP_RANGE(i, cur->lo, cur->hi, r_runners[i]--);
		if (cur->type == ROT_WRITE)
			FOR_WRAP_RANGE(i, cur->lo, cur->hi, w_runners[i]--);

		list_del(&cur->list);
		kfree(cur);
	}

	spin_unlock(&rot_spin);

	if (woke)
		wake_up_interruptible(&rot_wq);

	return;
}
