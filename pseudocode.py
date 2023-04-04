device_degree = 0

write_waiters = [360]
read_runners = [360]
write_runners = [360]

entry_list = list of Entry

def set_orientatation(degree):
    # validate arguments

    atomic device_degree = degree
    wake_up()

def rotation_lock(lo, hi, type):
    # validate arguments

    # atomically set write_waiters
    if type == WRITE:
        atomic increment write_waiters[lo..hi]

    # main wait loop
    ok = True
    while True:
        prepare_for_wait()

        if try_lock(lo, hi, type):
            break

        if signal_pending():
            ok = False
            break

        schedule()
    finish_wait()

    if not ok:
        # we were interrupted, rollback
        if type == WRITE:
            atomic decrement write_waiters[lo..hi]
            wake_up() # in case someone went to sleep based on write_waiters
        return -ERESTARTSYS

    id = add_list_entry(lo, hi, type)
    if id < 0: # kmalloc probably failed, revert effects of try_lock()
        spin_lock()
        decrement read_runners and write_runners accordingly
        spin_unlock()

        wake_up() # in case someone went to sleep based on $type_runners
        return -ENOMEM

    return id

def try_lock(lo, hi, type):
    # check if in range
    degree = atomic device_degree
    if degree not between lo, hi:
        return False

    # if we are a read lock, check if there are any in-range waiting write locks
    if type == READ and atomic write_waiters[degree]:
        return False

    spin_lock()

    # check if still in range
    degree = atomic device_degree
    if degree not between lo, hi:
        return False

    # check if any write lock is set in range
    if any write_runners[lo..hi] > 0:
        spin_unlock()
        return False

    # check if any read lock is set in range
    if type == WRITE:
        if any read_runners[lo..hi] > 0:
            spin_unlock()
            return False

    increment read_runners and write_runners accordingly

    spin_unlock()

def add_list_entry(lo, hi, type):
    entry = Entry(lo, hi, type, current_pid)

    spin_lock()
    id = add entry with unique id to entry_list
    spin_unlock()

    return id

def rotation_unlock(id):
    spin_lock()

    entry = find_entry_by_id(entry_list)
    # check pid, etc.

    decrement read_runners and write_runners accordingly

    spin_unlock()

    wake_up()

def exit_rotlock():
    spin_lock()

    remove each entry in entry_list if it belongs to current process
    also decrement read_runners and write_runners accordingly

    spin_unlock()
    wake_up()
