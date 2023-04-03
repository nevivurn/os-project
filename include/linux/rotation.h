#ifndef __ROT_LOCK_H__
#define __ROT_LOCK_H__

#include <linux/sched.h>

#define ROT_READ  0
#define ROT_WRITE 1

void exit_rotlock(struct task_struct *tsk);

#endif
