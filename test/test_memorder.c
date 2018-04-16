/*
 * This is a test which demonstrates the impact of the store buffer on an x86 CPU. This store buffer
 * makes it possible that stores to memory locations and / or cache are postponed and reads which are
 * at a later point in the execution stream are processed first.
 *
 * More specifically, suppose that you have two processors P0 and P1 which execute the following instructions
 *
 * P0                       P1
 * --------------------------------
 * flag0 = 1                flag1 = 1
 * if (0 == flag1) {        if (0 == flag0) {
 *  // critical section       // critical section
 * }                        }
 *
 * Now the following can happen. P0 executes flag0 = 1. However, the store is not really done, i.e. the cacheline
 * is not modified, but the store is postponed by writing the instruction into some sort of instruction queue called
 * the store buffer. Now P1 enters the corresponding store in its code. Again, the write is postponed and flag0 is read.
 * As this is still zero in cache and main memory, P1 enters the critical section. Now P0 continues, reads flag1, finds
 * that this is zero and continues as well.
 *
 * This access pattern is used for Petersons algorithm which is a lock-free algorithm to protect critical sections. This
 * is the reason why this algorithm fails on x86 CPUs unless memory barriers are built in. This program demonstrates the failure
 *
 * gcc -O0 -o test_memorder test_memorder.c -lpthread
 * ./test_memorder <n>     <--- number of loop iterations, 1000000 is a good value
 *
 * On my Intel Core i7-950 box, I usually have a significant difference when running this with 1000000 iterations. If the
 * same code is executed on a single CPU system, it does not produce any difference.
 *
 * When the mfence operation in lock is added, the problem disappears.
 *
 * Hint: use some sort of system monitor to validate that the threads are actually assigned to different CPUs - they will keep an
 * individual CPU about 100% percent busy and are therefore quite easy to monitor.
 *
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * Reserve space for the shared variables.
 */
static volatile int flag0;
static volatile int flag1;
static volatile int turn;

/*
 * Loop count
 */
int counter = 0;
long long loop_count;

/*
 * Enter the critical section
 */
static void lock(volatile int *my_flag, volatile int *other_flag, int other_id) {
    /*
     * Set interested flag for myself
     */
    *my_flag = 1;      // <---- flag0 resp. flag1 in the comments in the header - might be put into the store buffer first
    /*
     * Yield priority to other
     */
    turn = other_id;
    /*
     * Adding an mfence instruction at this point would
     * actually fix the problem. In fact, it will make sure that
     * the update of *my_flag (and that of turn) are forwarded to the
     * coherency control system before the read in the while loop is executed
     */
    // asm("mfence");
    /*
     * Wait until either other thread is not interested or
     * yields priority to us
     */
    while (*other_flag && turn == other_id)     // <---- flag1 resp. flag0
        ;
}

/*
 * Leave critical section
 */
static void unlock(volatile int *my_flag) {
    *my_flag = 0;
}


/*
 * This is task 0 which runs on CPU P0. It will increase
 * a counter "protected" by the Peterson lock. Once in a while
 * the update will actually not be protected as both CPUs are in the
 * critical section. If this happens, one update is lost. Thus at the end
 * of the day, we expect the total value of the counter to be slightly less
 * than two times the number of iterations
 */
static void* task0(void *arg) {
    long long  i;
    for (i = 0; i < loop_count; i++) {
        lock(&flag0, &flag1, 1);
        counter++;
        unlock(&flag0);
    }
    return (void*) 0;
}

/*
 * Task for CPU P1
 */
static void* task1(void *arg) {
    long long i;
    for (i = 0; i < loop_count; i++) {
        lock(&flag1, &flag0, 0);
        counter++;
        unlock(&flag1);
    }
    return (void*) 0;
}

int main(int argc, char *argv[]) {
    flag0 = 0;
    flag1 = 0;
    pthread_t t0, t1;
    if (argc < 2) {
        printf("Usage: test_memorder <number of iterations>\n");
        exit(1);
    }
    loop_count = atol(argv[1]);
    /*
     * Spawn threads
     */
    if (pthread_create(&t0, NULL, task0, NULL)) {
        printf("Could not create task for P0, giving up\n");
        exit(1);
    }
    if (pthread_create(&t1, NULL,task1, NULL)) {
        printf("Could not create task for P1, giving up\n");
        exit(1);
    }
    /*
     * and wait for them to complete
     */
    if (pthread_join(t0, NULL)) {
        printf("Could not wait for P0, giving up\n");
        exit(1);
    }
    if (pthread_join(t1, NULL)) {
        printf("Could not wait for P1, giving up\n");
        exit(1);
    }
    printf("Results:\n");
    printf("-------------------------\n");
    printf("Value of counter: %d\n", counter);
    printf("Expected value:   %lld\n", loop_count*2);
    printf("Difference:       %lld\n", loop_count*2 - counter);
    return 0;
}
