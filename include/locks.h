/*
 * locks.h
 */

#ifndef _LOCKS_H_
#define _LOCKS_H_

#include "ktypes.h"

/*
 * Structure describing a spinlock
 * Note that this is just a u32
 * but we use a typedef for future extensions
 */

typedef u32 spinlock_t;

/*
 * Event control block.
 */
typedef struct _ecb_t {
    u32 waiting_task;                           // ID of waiting task
    struct __ecb_timer_t {
        u32 timeout_value;                      // value of timeout in ticks
        u32 timeout;                            // timeout occured
        u32 is_active;                          // timer is active
        int cpuid;                              // CPU on which ECB is queued
        struct __ecb_timer_t* next;
        struct __ecb_timer_t* prev;
    } timer;
    struct _ecb_t* next;
    struct _ecb_t* prev;
} ecb_t;

/*
 * Convert forth and back between a pointer to the timer field and a pointer to the ECB
 */
#define ECB2TIMER(ecb)  ((struct __ecb_timer_t*)(&((ecb)->timer)))
#define TIMER2ECB(ecb_timer) ((ecb_t*)(((void*)(ecb_timer)) - offsetof(ecb_t, timer)))

/*
 * This structure describes a semaphore
 */
typedef struct  _semaphore_t {
    u32 value;                // value of the semaphore
    spinlock_t lock;          // spinlock to make operations on semaphore atomic
    ecb_t* queue_head;        // head of event control block queue
    ecb_t* queue_tail;        // tail of queue
    u32 timeout_val;          // timeout value (kernel ticks)
    u32 timeout;              // set to 1 if sem_down completed due to a timeout
    u32 timed;                // is this a timed semaphore?
    int cpuid;                // if this was a timed semaphore, this is the CPU on which the timer runs
    struct _semaphore_t* next;
    struct _semaphore_t* prev;
} semaphore_t;

/*
 * This structure describes a condition variable
 */
typedef struct _cond_t {
    spinlock_t lock;          // lock to protect queue
    ecb_t* queue_head;        // head of event control block queue
    ecb_t* queue_tail;        // tail of queue
    u32 timeout_val;          // timeout value (kernel ticks)
    u32 timeout;              // set to 1 if wakeup performed due to a timeout
    u32 timed;                // is this a timed condition variable?
    int cpuid;                // if this was a timed condition variable, this is the CPU on which the timer runs
    struct _cond_t* next;
    struct _cond_t* prev;
} cond_t;



typedef struct {
    u32 readers;
    semaphore_t read_count_mutex;
    semaphore_t wrt_mutex;
} rw_lock_t;

#define rw_lock_get_write_lock(rw_lock) do { __rw_lock_get_write_lock(rw_lock, __FILE__, __LINE__); } while(0)
#define rw_lock_get_read_lock(rw_lock) do { __rw_lock_get_read_lock(rw_lock, __FILE__, __LINE__); } while(0)
#define sem_down(sem) do { __sem_down(sem, __FILE__, __LINE__); } while (0)
#define sem_down_intr(sem) (__sem_down_intr(sem, __FILE__, __LINE__))
#define sem_down_timed(sem, timeout) (__sem_down_timed(sem, __FILE__, __LINE__, timeout))


void spinlock_init(spinlock_t* lock);
void spinlock_get(spinlock_t* lock, u32* flags);
void spinlock_release(spinlock_t* lock, u32* flags);
void sem_init(semaphore_t* sem, u32 value);
void __sem_down(semaphore_t* sem, char* file, int line);
int sem_down_nowait(semaphore_t* sem);
int __sem_down_intr(semaphore_t* sem, char* file, int line);
int __sem_down_timed(semaphore_t* sem, char* file, int line, u32 timeout);
void sem_up(semaphore_t* sem);
void sem_timeout(semaphore_t* sem);
void mutex_up(semaphore_t* mutex);
void rw_lock_init(rw_lock_t* rw_lock);
void __rw_lock_get_read_lock(rw_lock_t* rw_lock, char*, int);
void rw_lock_release_read_lock(rw_lock_t* rw_lock);
void __rw_lock_get_write_lock(rw_lock_t* rw_lock, char*, int);
void rw_lock_release_write_lock(rw_lock_t* rw_lock);
void cond_init(cond_t* cond);
int cond_wait_intr(cond_t* cond, spinlock_t* lock, u32* eflags);
int cond_wait_intr_timed(cond_t* cond, spinlock_t* lock, u32* lock_eflags, unsigned int timeout);
void cond_broadcast(cond_t* cond);
void atomic_store(u32* address, u32 value);
u32 atomic_load(u32* address);
#endif /* _LOCKS_H_ */
