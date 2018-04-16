/*
 * locks.c
 *
 * Functions to manage spinlocks. Note that while spinlocks are contained in
 * this module, semaphores are part of the process manager as they may change the
 * status of a task
 */

#include "locks.h"
#include "util.h"
#include "smp.h"
#include "pm.h"
#include "debug.h"


/*
 * Initialize a spin lock
 */
void spinlock_init(spinlock_t* lock) {
    *((u32*)lock)=0;
}


/*
 * Acquire a spin lock
 * This function acquires a spin lock. When the spinlock could
 * be acquired, it turns off interrupts for the local CPU
 * to avoid race conditions
 * @lock - the spinlock to apply for, make sure that you have
 * called spinlock_init on the lock before
 * @flags - used to store the value of the EFLAGS register before
 * turning off interrupts
 */
void spinlock_get(spinlock_t* lock, u32* flags) {
    volatile u32* lock_int = (u32*) lock;
    u32 reg = 1;
    save_eflags(flags);
    cli();
    while (reg) {
        reg = xchg(reg, (u32*) lock_int);
        if (1 == reg) {
            while (1 == (*lock_int)) {
                /*
                 * Spin around the lock until it is free. Note that we keep
                 * interrupts disabled here - this might change in future versions. In
                 * each case, we need to make sure that we do not simply turn on interrupts
                 * again but use the stored EFLAGS value as we would otherwise run into
                 * a problem with nested spinlocks.
                 * We also issue a pause statement to empty the pipeline - needed for CPUs
                 * supporting HT
                 */
                asm("pause");
            }
        }
    }
}

/*
 * Release a spin lock again. We also enable interrupts
 * again, so you need to make sure that the second parameter
 * is the location where we stored the flags when doing
 * spinlock_get
 * @lock - the spinlock to release
 * @flags - location of saved EFLAGS register
 */
void spinlock_release(spinlock_t* lock, u32* flags) {
    *((u32*)lock)=0;
    /*
     * Put a memory barrier here to make sure that
     * all changes within the critical section are
     * visible globally
     */
    smp_mb();
    restore_eflags(flags);
}

/*
 * Initialize a read-write lock
 * Parameters:
 * @rw_lock - the read-write-lock to use
 */
void rw_lock_init(rw_lock_t* rw_lock) {
    sem_init(&rw_lock->read_count_mutex, 1);
    sem_init(&rw_lock->wrt_mutex, 1);
    rw_lock->readers = 0;
}

/*
 * This function gets a read lock
 * Parameter:
 * @rw_lock - the read write lock to use
 */
void __rw_lock_get_read_lock(rw_lock_t* rw_lock, char* file, int line) {
    debug_lock_wait((u32) rw_lock, 1, 0, file, line);
    sem_down(&rw_lock->read_count_mutex);
    rw_lock->readers++;
    if (1 == rw_lock->readers) {
        sem_down(&rw_lock->wrt_mutex);
    }
    mutex_up(&rw_lock->read_count_mutex);
    debug_lock_acquired((u32) rw_lock, 0);
}


/*
 * This function releases a read lock
 * Parameter:
 * @rw_lock - the read write lock to use
 */
void rw_lock_release_read_lock(rw_lock_t* rw_lock) {
    sem_down(&rw_lock->read_count_mutex);
    rw_lock->readers--;
    if (0 == rw_lock->readers) {
        mutex_up(&rw_lock->wrt_mutex);
    }
    mutex_up(&rw_lock->read_count_mutex);
    debug_lock_released((u32) rw_lock, 0);
}

/*
 * Acquire a write lock
 * Parameters:
 * @rw_lock - the lock to be acquired
 */
void __rw_lock_get_write_lock(rw_lock_t* rw_lock, char* file, int line) {
    debug_lock_wait((u32) rw_lock, 1, 1, file, line);
    sem_down(&rw_lock->wrt_mutex);
    debug_lock_acquired((u32) rw_lock, 1);
}


/*
 * Release a write lock
 * Parameters:
 * @rw_lock - the lock to be acquired
 */
void rw_lock_release_write_lock(rw_lock_t* rw_lock) {
    mutex_up(&rw_lock->wrt_mutex);
    debug_lock_released((u32) rw_lock, 1);
}

/*
 * Atomically store a 32 bit value in memory
 */
void atomic_store(u32* address, u32 value) {
    /*
     * Make sure that the address is dword aligned
     *
     */
    KASSERT(0 == ((u32) address % sizeof(u32)));
    /*
     * Store is atomic on x86 at dword boundaries. However, we need to add a memory barrier
     * to make sure that atomic operations imply memory barriers as foreseen by our
     * memory model
     */
    *address = value;
    smp_mb();
}

/*
 * Atomically read a 32 bit value from memory
 */
u32 atomic_load(u32* address) {
    /*
     * Make sure that the address is dword aligned
     *
     */
    KASSERT(0 == ((u32) address % sizeof(u32)));
    /*
     * Store is atomic on x86 at dword boundaries. However, we need to add a memory barrier
     * to make sure that atomic operations imply memory barriers as foreseen by our
     * memory model
     */
    smp_mb();
    return *address;
}
