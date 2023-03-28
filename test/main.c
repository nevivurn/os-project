#define _DEFAULT_SOURCE

#include <unistd.h>
#include <stdio.h>

int main(void) {
	long a = syscall(294, 0);
	long b = syscall(295, 0, 360, 0);

	long c = syscall(296, b);

	printf("%ld %ld %ld\n", a, b, c);
}
