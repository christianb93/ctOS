# Atomic operations and locks

In this document, we look at a few conceptual issues regarding concurrency, locks and atomic operations. This document is about the concepts and general ideas behind that, the actual implementation in ctOS is described in the document on the process manager.

## Locks

To demonstrate the need for and usage of locks in an operating system kernel, let us look at a simple example. Suppose you are writing a memory manager module and implement a service which checks whether a given physical page in memory is available. To achieve this, you would typically have a global data structure which, say, contains a bit for each page in the system. If the bit is set, the page is in use, if it is clear, the page is available.

Now suppose a process inquires for a free page. The memory manager scans the bitmask and locates a free page. However, before it can mark the page as used, one out of two things happen.

-   an interrupt is invoked, so that the flow of execution is interrupt and code in an interrupt handler is executed
-   a task switch happens and the scheduler selects another task for execution

If the code in the interrupt handler or in the new task also asks the memory manager for memory, it will again check the bit for the page in question and see that the bit is cleared. It will set the bit and hand over the page to the caller. If then the flow of execution in the original process continues, the memory manager - having already read the bit mask - will believe that the page is still available. It will also set the bit and pass the page to the caller. So the physical page has been given to two different processes, which will probably cause a minor desaster.

On multiprocessor systems, even without interrupts or task switch, the memory manager could run concurrently on two CPUs at the same time and the same problem can occur.

To avoid this, a mechanism is needed to protect global data structures - and more general globally used ressources - from concurrent access. This is usually achieved using **locks**. Generally speaking, a lock is a symbolic ressource which a thread of execution (be it a process, an interrupt handler or a task running on another CPU) needs to acquire before executing a certain area of code, but which can only be held by one thread of execution at a time. When a thread of execution cannot acquire the lock, it needs to wait until the lock becomes available, either by going to sleep or by "spinning" around the lock and testing until the lock is available.

## Atomic operations


Suppose you wanted to implement a lock on a multiprocessor machine using the following algorithm. 

You declare a global integer variable *lock* somewhere in memory. When a process wants to acquire the lock, it reads *lock* and tests whether it is zero. If yes, it sets *lock* to 1. 

If another process now tries to acquire the lock, it reads one from *lock*, learns that the lock is not available and enters a wait loop until *lock* is zero again.

This approach has the obvious problems that the process could be interrupted between testing the lock and setting the lock, so that another thread might also test the lock before it can be set by the first process. In this situation, two threads will both set the lock and believe that they are holding the lock exklusively. Even turning off interrupts temporarily does not help, as on an SMP system, we could face a race condition with a thread on a different CPU. So you needed a lock to protect the lock - a chicken-and-egg problem.

For that reason, most platforms offer hardware support to implement locks which is called **atomic operations**. An atomic operation is an operation which accesses a certain ressource - typically memory - for which the hardware will guarantee that while this operation is in progress, no other CPU and no other thread of execution has access to that ressource.

An example is the XCHG instruction on x86 CPUs. When executing this instruction to exchange the content of a register with memory, the CPU will use bus locking to make sure that no other CPU has access to that particular area in memory while the instruction is being executed. As an interrupt will never interrupt the execution of a single instruction, it is also guaranteed that no other thread of execution within the same CPU can interfer with the XCHG.

The x86 additionally offers the LOCK prefix which can be used to make a small set of additional instructions atomic, including BTS (bit test and set), INC and DEC.

Note that "atomic" does not mean that no other operation can be executed in the meantime, it only means that no other operation can modify the same area of memory. So atomic is a relative term.

## Spinlocks

Using atomic operations, we can now start to implement the first types of locks used in ctOS - spinlocks. A **spinlock** is a lock which can at any time only be held by one thread of execution. If the current thread of execution tries to acquire the lock and fails, it enters a loop which is executed until the lock becomes available. No wait is performed, i.e. the current task remains active. 

Using the atomic XCHG operation, the basic algorithm to acquire a spinlock is as follows, assuming that *lock* is a location somewhere in memory which is initialized to zero.

```
REG = 1
WHILE (REG==1)
  XCHG REG, LOCK
END WHILE
// we now have the lock .. do something
// now release lock
LOCK = 0
```

This is surprisingly simple, but works. Let us try to understand why. First assume that when this code is executed, the lock is available, i.e. *lock* is zero. The first XCHG instruction will then set lock to one and move its previous content, i.e. zero, into REG. Therefore the loop will be executed only once and we have successfully acquired the lock.

If, however, the lock has already been acquired by some other thread of execution, *lock* is already one when we enter the loop. So the first XCHG will not actually change *lock*, but put its previous content - one in this case - into REG. Therefore the loop will be executed again, yielding the same result. So the loop will continue to be executed until some other thread of execution updates the lock to zero. 

If that happens, the next execution of XCHG will move zero into REG and set lock back to one, so that our thread has acquired the lock successfully and continues execution.

This straightforward implementation works but has a few disadvantages. Due to the loop being constantently executed, the CPU will almost always be holding a lock on the memory bus and send data forth and back between memory and CPU, which will significantly slow down all other running processes. It can even make it harder for the thread currently holding the lock to complete and return the lock. To avoid this, a second loop could be implemented in which the process stays until a read from lock has indicated a changed value.

```
REG = 1
WHILE (REG==1)
  XCHG REG, LOCK
  IF (REG==1)
    WHILE (1==LOCK)
      // do nothing
    END WHILE
  END IF
END WHILE
// we now have the lock .. do something
// now release lock
LOCK = 0
```

The second problem which is not yet addressed by our code is the interaction with interrupts. Suppose this method of getting a spinlock is used by a process and an interrupt handler and suppose that the process has successfully acquired the lock. Now the interrupt handler kicks in. As the lock is not available, it starts to spin around the lock and never returns. Then the main process is blocked, as the CPU is busy within the interrupt handler, and the entire system is locked up. 

To avoid this sort of deadlock, we can disable interrupts on the local processor before we  acquire the lock and enable it again when giving the lock back. Note that it is good enough to disable interrupts on the local CPU, as an interrupt on another CPU which also tries to get the lock will simply wait for the local CPU to give the lock back and no deadlock occurs. So on a x86 CPU which realizes blocking of interrupts with the IF bit in the EFLAGS register, our code looks like

```
REG = 1
SAVE EFLAGS
CLI
WHILE (REG==1)
  XCHG REG, LOCK
  IF (REG==1)
    WHILE (1==LOCK)
      // do nothing
    END WHILE
  END IF
END WHILE
// we now have the lock .. do something
// now release lock and turn on interrupts again
LOCK = 0
RESTORE EFLAGS
```

## The producer-consumer pattern and circular buffers

A design pattern which occurs quite often in an operating system context and which has become a popular example to demonstrate some of the pitfalls of multiprogramming and some classical solution is the **producer-consumer pattern**. 

In this pattern, one process - called the producer - produces tokens which have to be processed by some other process which is called the consumer. In real life examples, the producer could be a process which generates I/O requests and the consumer could be the part of a device driver which sends I/O requests to a hard disk controller and handles the interaction with the controller until a request is processed.

A straightforward approach to implement this pattern (which, however, leads to serious problems in a multithreaded environment, as we will see in an instant) is to use a bounded request queue of size QUEUE_SIZE. The producer adds items to the queue, and the consumer removes processed items from the queue. If the queue is full, the producer needs to sleep before additional items can be added. Likewise, the consumer needs to sleep when the queue is empty. So a naive implementation would be as follows.

__Producer__

```
WHILE(1)
  WHILE (queue full)
    SLEEP()
  END WHILE
  queue_add_token(token)
END WHILE
```

__Consumer__

```
WHILE (1)
  WHILE (queue empty)
    SLEEP()
  END WHILE
  queue_remove_token(token)
  wakeup()
END WHILE
``` 

In this solution, the producer simply waits until a slot becomes available in the queue and adds new items to the queue as needed. The consumer sits in an endless loop, periodically checking for unprocessed items in the queue. As soon as an item is found, it is removed from the queue and processed. As soon as the consumer has removed an item, it wakes up the producer which might have gone to sleep as the queue is full.

This simple approach has two problems in a multithreaded environment.

-   the queue is a global data structure and as such needs to be protected by some sort of lock, for instance by a spin lock as described above
-   even if this is done, the algorithm above still bears the risk of a deadlock due to the so-called lost wakeup call issue

What do we mean by the lost wakeup issue? Suppose that at a given point in time, the queue is empty and the producer adds new requests to the queue until it is full. After adding the last request, it realizes that the queue is full. If, at this very moment and before the producer can go to sleep, the producer is interrupted and the consumer starts processing, it will find the new items, remove them from the queue and call wakeup() to notify the producer. 

However, at this point in time, the producer is not yet sleeping, so the wakeup calls do nothing. If the consumer has emptied the entire queue, it goes to sleep. Now the producer is scheduled again and resumes execution at the point where it was interrupted, i.e. it goes to sleep. Now producer and consumer are both sleeping forever - a classical deadlock has occured.

To avoid these issues, several solutions exist. One solution for the lost wakeup issue uses semaphores for synchronisation at which we will look further below. Semaphores can be used to eliminate the lost wakeup issue, but additional locks or binary semaphores are still needed to protect the data structure itself. To avoid the need for these additional locks, a circular buffer can be used.

A **circular buffer** is a buffer with a limited number of elements (BUFFER_SIZE) in which access to element x + BUFFER_SIZE equates access to element x, i.e. the elements in the buffer form a ring. 

To manage the elements in the buffer, two integer variables called *tail* and *head* are used, which indicate the current tail and head of the buffer. When the producer adds elements to the buffer, it increases *tail*. When the consumer removes items from the queue, it increases *head*. Therefore each of the control variables it only written by one of the two threads and no race conditions on these variables can occur. 

The pseudo-code for the circular buffer implemenation then looks as follows.

__Producer:__
```
WHILE(1)
  WHILE (tail - head == BUFFER_SIZE)  // buffer is full, do busy wait
  END WHILE
  buffer[tail % BUFFER_SIZE]=token
  tail++
END WHILE
```

__Consumer:__

```
WHILE(1)
  WHILE (tail == head) // buffer is empty, do busy wait
  END WHILE
  token=buffer[head % BUFFER\_SIZE\]
  head++
END WHILE 
```

Note that the producer must only increment *tail* after the token has been placed in the buffer. If *tail* were incremented first, it might happen that the consumer gains the CPU after tail has been incremented, but before the token has been placed in the buffer, and tries to process the still empty slot in the buffer.

Also note that in this solution, consumer and producer do not go to sleep, but "spin" around the tail and head variables. This avoids the problem of lost wakeups, but is of course a waste of CPU time. We will look at semaphores as a proper way to avoid this issue in the next section.

This algorithm makes the implicit assumption that the pure reading or writing of an integer variable is an atomic operation. This is true on most architectures (for instance x86) as long as the size of the integer variable is the size of the data bus.

Of course *head* and *tail*, being integer variables, will wrap at some point. This is not a problem as long as the BUFFER\_SIZE is smaller than the limit of head and tail at which wrapping occurs.

Alternatively, pointers could be used to indicate the current head and tail which point directly to the buffer slots in memory. This, however, poses the problem that then the condition head == tail means a full buffer and an empty buffer at the same time and hence the pointers alone are not sufficient to distinguish these two cases. One possible approach to solve this issue would be to always leave one slot between head and tail empty, but this comes at the price of wasted memory.

To conclude this section, it is worth mentioning that this simple implemenation is not sufficient if there is more than one producer or more than one consumer. If this is the case, additional locks are needed to avoid that, for instance, two producer threads put a token into the same location in the buffer at the same time.

## Semaphores

As already mentioned, **semaphores** are a way to solve the problem of lost wakeup calls. A semaphore is an object on which two operations can be performed and which has an integer value. 

The first operation is called *down*. When *down* is executed and the current value of the semaphore is positive, the value is decremented and the function returns. If, however, the value is already zero when *down* is called, the current thread is put to sleep and added to a queue of threads waiting for the semaphore.

The second operation which can be performed on a semaphore is called *up*. When this function is called, it increases the integer value of the semaphore by one. If the value was zero before, it will in addition wake up the next thread in the queue. This thread will then resume execution and is able to complete its *down* operation. It is essential that both operations, up and down, are atomic and cannot be interrupted.

To see how this solves the lost wakeup issue, let us look at the example of a print spool which is managed using semaphores. The print queue has a number of available slots which is reduced by one each time a request is sent to the printer. If there is no free slot, the thread which wishes to send output to the printer needs to wait. Each time a slot has been processed, all threads waiting for slots are woken up. The naive implementation without semaphores would be as follows.

```
IF (0==free_slots) 
  sleep();
do_print();
```

Again there is the obvious race condition that if the number of free slots is zero, the thread might be pre-empted before it can go to sleep and receive the wakeup request before it calls sleep (there are other race conditions if more than one thread wishes to print at the same time which we ignore for the time being). Using semaphores, the code would look as follows.

```       
down(free_slots);do_print();
```

The printer would then call *up* whenever a print job has been completed. As down is atomic, this implementation is not subject to the lost wakeup issue.

A semaphore that can only have the values 0 and 1 is called a **binary semaphore** or a **mutex**. A mutex effectively acts as a lock that avoids busy waits, where the down operation is the equivalent of getting the lock and the up operation is the equivalent of releasing the lock.

## Read/write locks

Now let us consider the following problem. Suppose you want to synchronize access to a ressource (say a file on disk) which can be accessed by threads reading from it and by threads writing to it. We want to make sure that a writer thread can only access the file if no other thread is reading or writing, whereas several reader threads can read from the file at the same time.  Here is an algorithm which realizes this pattern using semaphores. It uses an integer variable *readers* which contains the number of readers currently active, a mutex *read_count_mutex* to protect this variable and a mutex *wrt_mutex* which any process which wants to write to the file needs to acquire.

__Reader:__
down(read_count_mutex);
readers++;
IF readers == 1 THEN
  down(wrt_mutex);
END IF
up(read_count_mutex);
<<do actual read operation>>
down(read_count_mutex);
readers--;
IF (readers == 0) THEN
  up(wrt_mutex);
END IF
up(read_count_mutex);

__Writer:__
```
down(wrt_mutex);
<<do actual write operation>>
up(wrt_mutex);
```

This algorithm is simple, but has the problem that a thread waiting for write access can die of starvation. In fact, suppose that one thread is reading, i.e. *wrt_mutex* = 0, when a writer thread tries to enter the critical region. The writer thread will then stall, waiting for the mutex *wrt_mutex* to become available. 

Now a second reader thread might start to read and increase *readers*. When the first reading thread completes, it will not wake up the writer as there is still one reading thread. Again another reading thread might enter and increase readers again and so on, so that the writing thread will never be woken up.  Several approaches exist in the literature on this subject to circumvent this, all of which increase the complexity considerably and lead to additional problems as no reader could suffer from starvation. For this release of ctOS, we restrict ourselves to the simple algorithm described above.

## Condition variables


Roughly speaking, a semaphore is a way to block a thread of execution until a certain condition, namely that the value of the semaphore is positive, holds true. However, there are sometimes situations in which a more complex condition needs to be waited for which cannot easily be expressed in terms of an integer variable.

Suppose that, for instance, we have a condition P(x,y) which depends on two integer variables x and y which might be concurrently updated by more than one thread and want to suspend a thread until P(x,y) is true. A first approach could be as follows.

```
WHILE 1 DO
  IF P(X,Y) THEN
    react upon condition
    BREAK
  END IF
  Put current task to sleep
END WHILE
```

We would then require every thread that modifies x and y to wake up our task so that it can recheck the condition. Quite obviously, this approach is too simple in realistic cases where the evaluation of P(x,y) is not an atomic operation. In these cases, we will have to protect x and y by a lock L. So our algorithm now might look as follows.

```
WHILE 1 DO
  Get lock L
  IF P(X,Y) THEN
    react upon condition
    Release lock L
    BREAK
  END IF
  Release lock L
  Put current task to sleep
END WHILE
```

Unfortunately, this algorithm contains a race condition. Suppose that the condition we are waiting for does not hold true at some specific point in time when we execute the loop and we get to the point where we release the lock. Now it might happen that another task manipulates x or y after we have released the lock, but before we have completed the sleep operation. Then our task would go to sleep even though the condition is fulfilled. To solve this, we need an operation which **atomically** releases the lock L and puts the current task to sleep in one step.

This is exactly what **condition variables** are good for. A condition variable is - despite its name - not a variable but a queue of tasks associated with a lock L which implements the following operations:

-   *wait* - this operation atomically releases the lock L (which the calling task is supposed to hold), adds the current task to the queue and blocks it. When the task resumes execution, it acquires L again
-   *signal* - this operation  will wake up the process at the head of the queue and remove it from the queue
-   *broadcast* - this operation will wake up all processes in the queue and empty the queue

Conceptually, one can think of the lock L as implementing a monitor which protects the variables used to evaluate the condition, i.e. x and y in the example above. Using a condition variable C, our example can be rewritten as follows.

```
WHILE 1 DO
  Get lock L
  IF P(X,Y) THEN
    react upon condition
    Release lock L
    BREAK
  END IF
  wait(C,L)
END WHILE
```

Another thread changing x and / or y would then have to use the following pattern

```
Get lock L
Change x and y
broadcast(C)
Release lock L
```

Note that in contrast to semaphores, condition variables do not "remember" broadcasts. So if a task executes a broadcast or signal operation on a condition variable at a point in time when no task is executing wait, this signal will be lost. The next task executing wait will still block if the condition is not fulfilled.

