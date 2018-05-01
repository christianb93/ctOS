# Work queues

## Introduction and overview

Obviously, there are some restrictions on the type of processing which can reasonably be done within an interrupt handler. As a rule of thumb, interrupt handlers should be short in the first place. At the very least, the time they take to execute should be predictable, i.e. no waiting for an unspecified amount of time should be involved. In addition, interrupt handlers cannot sleep and work can not easily be handed over from one CPU to another within an interrupt handler.

An example where these restrictions become problematic is networking code. Assume for instance that a ICMP echo request is received by the network card. When the corresponding Ethernet packet is received, the NIC will raise an interrupt. To fully process the request, it needs to be moved through the network stack up to the ICMP layer. The ICMP layer needs to create a response and hand it back to the lower layers of the network stack. However, transmitting the reply back via the network card might involve some waiting. It might for instance be that the network card has been running of out descriptors and needs to wait until some other packets have been sent and transmit descriptors become available. In the worst case, an ARP request might be required to resolve the IP address of the destination of the packet. Doing all this in an interrupt handler would be problematic as it would negatively impact interrupt latency.

Thus a mechanism is needed to postpone some piece of work in an interrupt handler for later processing outside of the interrupt handler, i.e. in a thread context. In ctOS, **work queues** are used for that purpose. Basically, an interrupt handler (or some other part of the kernel) can add an entry to a work queue which contains a pointer to a function to be run and a pointer to an argument. 

For each CPU, a dedicated kernel thread called the **worker thread** is woken up every time a message is added to the queue. This thread scans the queue and, for each entry, calls the associated handler with the provided argument. If the handler returns a special error code meaning "I could not do whatever you told me to do right now, but I would like to try later again", the message is requeued.

A typical use of work queues is the code used to send Ethernet packets to the network card. This code first checks whether any transmit descriptors are available. If all descriptors are in use, it adds a new message to the work queue using itself as handler and the network message to be transmitted as argument. When in this situation a descriptor becomes available again, the card will raise an interrupt. Within the interrupt handler, the worker thread will be woken up. As this is a high priority thread, it will be executed at the earliest possible point in time (either after processing the interrupt if the worker thread is located on the same CPU, or immediately if it is located on a different CPU as waking up a high priority thread will create an IPI for the affected CPU). 

The thread will remove the entry from the work queue and call the handler again. Usually, the descriptor will still be free and the handler will succeed. If again no descriptor is available, the handler will return the above mentioned specific error code and a new entry will be added to the work queue, thus effectively rescheduling the handler.

Data structures and operations
------------------------------

To realize this approach, the following data structures are required.
A **work queue entry** is a structure which holds a pointer to the handler to be executed and the argument (void\*) to be passed to the handler.

```C
typedef struct {
    void* arg;
    int (*handler)(void*);
    u32 expires;
    u32 iteration;
} wq_entry_t;
```

A **work queue** is a ring buffer which can hold WQ\_MAX\_ENTRIES work queue entries.

```C
typedef struct {
    wq_entry_t wq_entries[WQ_MAX_ENTRIES];
    int wq_id;
    u32 head;
    u32 tail;
    spinlock_t queue_lock;
    u32 iteration;
} wq_t;
```

A fixed number of work queues is statically allocated within the kernel BSS section (and therefore contained in the 1-1 mapped memory area and accessible in every memory context). Each work queue is uniquely identified by a work queue ID. 

For each CPU in the system, a worker thread is started. The worker thread on CPU X will only access those work queues for which work queue ID mod \#CPUs == X holds. Thus if a system has four CPUs, the worker thread on CPU 0 will process the work queues 0, 4, 8, ..., the worker thread on CPU 1 will process work queues 1, 5, 9 etc. In this way, lock conflicts are avoided as each queue will only be accessed by a designated CPU. Therefore the worker thread is tied to a specific CPU, i.e. task migration is disabled for these threads.

In addition, there is mutex `wq_sem` per CPU which is used to inform the worker thread on this CPU that a new entry has been added to the work queue or that another event occurred that requires a reprocessing.

Two basic operations are defined for work queues. First, entries can be added to a queue using the function `wq_schedule(int wq_id, int (*handler)(void*), void* arg, int opt)`.  This function will add a new entry to a work queue using the specified handler. 

Additional processing can be requested using the parameter `opt`. If opt == RUN\_NOW, an entry will be added and an *up* operation will be performed on the semaphore `wq_sem` for the CPU on which the worker thread is running to re-run the thread as soon as possible. If `opt` == RUN_LATER, the entry will be added to the queue, but it will be left to the regular triggers received by the timer to wake up the worker thread again.

The second basic operation on a work queue is to trigger processing of the queue using `wq_trigger(int wq_id)`. Essentially, this will wake up the worker thread for the provided queue. It can for instance be used by interrupt handlers to inform a work queue that queued work which is waiting for a certain condition to become true can be re-executed. 

It should be noted that work queues are subject to a few potential errors. First, it might happen that a queue is full when a message needs to be added. As the whole point of work queues is that the code adding an entry to a work queue should not wait, it is not an option to wait until an entry becomes available again in this case. Thus the message needs to be discarded (it would of course be an option to allocate additional memory, but even this memory would have to be capped somehow, this would therefore not solve the problem but just make it less likely), and interrupt handlers placing messages on the queue somehow need to deal with this issue. Second, an interrupt handler placing a message on a work queue does not have a way to learn about the outcome of the actual execution, making error handling more complicated.

Messages in a work queues can time out. When processing a message, the worker thread checks the current local time against the local time when the message plus a constant (this sum is saved in the field `expires` of the message when the message is added to the queue). If the message has expired, the handler is still called, but is informed using a timeout flag. Messages which have timed out are not requeued, even if the handler returns EAGAIN.

To make sure that in one iteration of the worker thread, no message is processed twice, an iteration ID is maintained within the queue structure which is incremented each time the worker thread wakes up. If a message is processed but added to the queue again because the handler returned EAGAIN, the iteration field in the message will be filled with the iteration number in the queue structure. The current iteration will be stopped as soon as a message is encountered for which the field iteration is equal to the queue iteration field.

The following pseudo-code summarizes the operation of the work queue related functions.

__wq\_schedule (queue\_id, handler, arg, opt)__:

```
get lock on work queue
message = queue->wq_entries + queue->tail
fill message->expires with local time (i.e. number of clock ticks) plus TIMEOUT
message->iteration = 0
message->handler = handler
message->arg = arg
release lock on queue
IF options == RUN_NOW THEN
  cpuid = queue_id % number of CPUs
  do up operation on mutex for CPU
DONE
```

__wq\_trigger (wq\_id)__:

```
cpuid = queue_id % number of CPUs
do up operation on mutex wq_sem[cpuid]
```


__Worker thread__:

```
get cpuid
WHILE TRUE DO
  FOREACH queue associated to CPU cpuid DO
    queue->iteration++
    WHILE TRUE DO
      get lock on queue
      IF (queue->tail != queue_head) THEN    // still an entry in the queue
        get entry from queue->head % QUEUE_SIZE
        queue->head++
        release lock on queue
        IF (entry->expired > current local TIME) THEN
           IF (entry->iteration = queue->iteration) THEN 
             break  // stop processing of queue
           END IF
           Call handler for entry
           IF (return code of handler == EAGAIN) THEN
             get lock on queue
             IF (queue->tail - queue->head != QUEUE_SIZE) THEN
               store element again at position queue->tail % QUEUE_SIZE
               element->iteration = queue->iteration
               queue->tail++
             END IF
             release lock on queue
           END IF
        ELSE
          call handler with second argument (timeout) = 1
        END IF
      ELSE
        break
      END IF
    DONE    // done with queue
  END FOR   // all queues processed
  do a down operation on mutex for CPU
DONE
```




## Guidelines for writing queuable code


Note that handlers which are called via a work queue need to conform to some coding guidelines.

-   the handler needs to have the signature `int (*handler)(void* arg, int timeout)`. Here `arg` is the argument supplied and `timeout` is set to 1 if the queue entry has timed out
-   do not sleep within a work queue handler. Instead return with EAGAIN if the piece of work cannot be completed due to a temporary condition. In fact, sleeping within a handler would block the entire worker thread and stop processing of the queue. Also do not call functions which in turn might sleep, like file system operations
-   try to keep the entire handler as short as possible
-   if a piece of work has been scheduled to wait for a specific condition, the handler still needs to be prepared to be called if this condition is not fulfilled. Thus the handler needs to recheck the condition carefully. The reason for this restriction is that if several entries are queued and the worker thread is woken up because the condition the entries have been waiting for is fulfilled, the worker thread will try to process as many queue entries as possible in one iteration, not only one. Thus at a point in time when a specific entry is processed, the condition might again not be fulfilled
-   if a handler has been successfully processed, it needs to free the memory pointed to by the argument. This also applies if the handler has been called with the timeout flag set

