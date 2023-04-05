device_degree = 0

write_waiters = [360]
read_runners = [360]
write_runners = [360]

entry_list = list of Entry

def set_orientatation(degree):
    # atomically set device_degree and wake_up

def rotation_lock(lo, hi, type):
    # validate arguments

    # if READ, atomically decrement write_waiters

    ok = True
    while True:
        prepare_for_wait()

        if try_lock():
            break

        if signal_pending():
            # atomically decrement write_waiters
            finish_wait()
            return -ERESTARTSYS

        schedule()
    finish_wait()

    id = add_list_entry()

    if id == FAIL:
        # kmalloc failed, revert and wake_up
        return -ENOMEM

    return id

def try_lock(lo, hi, type):
    # 1. check if in range
    # 2. if we are READ, check if there are any write_waiters in range

    spin_lock()

    # 3. check if in range again

    # 4. check if any writers in range
    # 5. if we are WRITE, check if any readers in range

    # 6. success! increment read_runners or write_runners

    spin_unlock()

    return True

def add_list_entry(lo, hi, type):
    spin_lock()
    id = add entry to entry_list
    spin_unlock()
    return id

def rotation_unlock(id):
    spin_lock()
    entry = find entry by id
    # decrement read_runners and write_runners accordingly
    spin_unlock()

    wake_up()

def exit_rotlock():
    spin_lock()

    remove each entry in entry_list if it belongs to current process
    #  decrement read_runners and write_runners accordingly

    spin_unlock()
    wake_up()
