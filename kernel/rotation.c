#include <linux/syscalls.h>

SYSCALL_DEFINE1(set_orientation, int, degree) {
	return 0;
}

SYSCALL_DEFINE3(rotation_lock, int, lo, int, hi, int, type) {
	return 0;
}

SYSCALL_DEFINE1(rotation_unlock, long, id) {
	return 0;
}
