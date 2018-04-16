/*
 * wq.h
 *
 */

#ifndef _WQ_H_
#define _WQ_H_


#include "ktypes.h"
#include "locks.h"


/*
 * A work queue entry
 */
typedef struct {
  void* arg;
  int (*handler)(void*, int);
  u32 expires;
  u32 iteration;
} wq_entry_t;

/*
 * Number of entries per work queue
 */
#define WQ_MAX_ENTRIES 8912

/*
 * A work queue
 */
typedef struct {
  wq_entry_t wq_entries[WQ_MAX_ENTRIES];
  int wq_id;
  u32 head;
  u32 tail;
  spinlock_t queue_lock;
  u32 iteration;
} wq_t;

/*
 * Number of work queues supported
 */
#define WQ_COUNT 4

/*
 * Used work queues
 */
#define NET_IF_QUEUE_ID 3                 // used by net_if.c
#define IP_TX_QUEUE_ID 2                  // used by ip.c

/*
 * Timeout in ticks - time out after 5 seconds
 */
#define WQ_TIMEOUT 500

/*
 * After how many ticks do we retrigger the queue?
 */
#define WQ_TICKS 5

/*
 * Options
 */
#define WQ_RUN_NOW 0
#define WQ_RUN_LATER 1

void wq_init();
void wq_trigger(int wq_id);
int wq_schedule(int wq_id, int (*handler)(void*, int), void* arg, int opt);
void wq_do_tick(int cpuid);

#endif /* _WQ_H_ */
