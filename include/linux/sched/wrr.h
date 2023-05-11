#ifndef _LINUX_SCHED_WRR_H
#define _LINUX_SCHED_WRR_H

#include <linux/sched.h>

#define WRR_TIMESLICE (10 * HZ / 1000)
#define WRR_DEFAULT_WEIGHT 10
#define WRR_BALANCE (2 * HZ)

#endif
