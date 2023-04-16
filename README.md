[![Review Assignment Due Date](https://classroom.github.com/assets/deadline-readme-button-8d59dc4de5201274e310e4c54b9627a8934c3b88527886e3b421487c677d23eb.svg)](https://classroom.github.com/a/B0xXPUw_)
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

## Rotation Lock Implementation

In `kernel/rotation.c`, we have implemented a rotation lock, accessed through three syscalls:

- `set_orientation`
- `rotation_lock`
- `rotation_unlock`

The implementation relies on waitqueues and spinlocks for safely reading and
writing to shared state. A single global waitqueue handles putting blocked
threads to sleep, and a single spinlock is used to protect critical sections
from concurrent access.

The shared state is stored in static variables declared near the beginning of the file:

- `device_degree` is the globally shared device degree. Through careful usage
  of memory barriers, it is usually safe to access this value without acquiring
  any locks.
- `w_waiters[360]` is the array storing the number of waiting writer locks for
  each degree value. This value is also accessed without grabbing any locks, as
  this variable does not require mutual exclusion.
- `r_runners[360]` and `w_runners[360]` store the number of running locks of
  each type for each degree value. Access to these values does require mutual
  exclusion, so they are always accessed with locks.
- `rot_list` is a linked list containing the metadata of each running lock. This
  is also always accessed with a lock.

Each syscall is implemented as follows:

- `set_orientation` sets `device_degree` and wakes up all waiting threads on the
  waitqueue. It is safe to do so without a lock, as there is a memory barrier in
  `wake_up_interruptible` and another on the sleeping side.
- `rotation_lock` is more complex:
  - It first increments `w_waiters` if the current request is a writer.
  - It goes to sleep and tries to acquire the rotation lock in a loop.
    - Before acquiring the spinlock, it checks `device_degree` and `w_waiters`
      to see if the current thread shouldn't bother with acquiring the spinlock
      at all.
    - After acquiring the spinlock, it checks both `r_runners` and `w_runners`
      to see if there are any conflicting locks. If not, it increments the
      appropriate values and returns with a success.
    - Before returning from the `try_lock()` function, it releases the spinlock.
  - After a successful `try_lock()`, it re-acquires the spinlock to add the
    metadata entry to the `rot_list`.
- `rotation_unlock` is as follows:
  - It immediately acquires the spinlock, finds the metadata entry for the
    provided lock ID, and decrements the appropriate values in `r_runners` or
    `w_runners`.
