/*
 * sleep.c
 *
 */

/*
 * Mock implementation of sleep - currently this does not do anything
 */
unsigned int __ctOS_sleep(unsigned int seconds) {
    return 0;
}

unsigned int __ctOS_alarm(unsigned int seconds) {
    return 0;
}
