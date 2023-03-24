#include <linux/syscalls.h>
#include <linux/errno.h>
#include <linux/sched/task.h>
#include <linux/sched.h>
#include <linux/pinfo.h>
#include <linux/slab.h>

static void to_pinfo(struct pinfo *p, struct task_struct *task) {
	p->state = READ_ONCE(task->state);
	p->pid = task->pid;
	p->uid = from_kuid(&init_user_ns, task_uid(task));

	task_lock(task);
	strncpy(p->comm, task->comm, TASK_COMM_LEN);
	task_unlock(task);
}

SYSCALL_DEFINE2(ptree, struct pinfo __user *, buf, size_t, len)
{
	struct task_struct *leader, *parent, *child;

	struct pinfo *kbuf;
	size_t count;
	unsigned int depth;

	int res;

	if (buf == NULL || len <= 0)
		return -EINVAL;

	kbuf = kcalloc(len, sizeof *kbuf, GFP_KERNEL);
	if (kbuf == NULL)
		return -ENOMEM;

	read_lock(&tasklist_lock);

	parent = &init_task;
	leader = parent->group_leader;

	to_pinfo(&kbuf[0], parent);
	kbuf[0].depth = 0;

	count = 1;
	depth = 1;
	if (count >= len)
		goto out;

down:
	for_each_thread(leader, parent) {
		list_for_each_entry(child, &parent->children, sibling) {
			to_pinfo(&kbuf[count], child);
			kbuf[count].depth = depth;
			if (++count >= len)
				goto out;

			leader = child;
			depth++;
			goto down;
	up:
			depth--;
		}
	}

	if (leader != &init_task) {
		child = leader;
		parent = child->real_parent;
		leader = parent->group_leader;
		goto up;
	}

out:
	read_unlock(&tasklist_lock);

	res = copy_to_user(buf, kbuf, sizeof *kbuf * count);
	kfree(kbuf);
	if (res)
		return -EFAULT;

	return count;
}
