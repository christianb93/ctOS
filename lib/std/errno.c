/*
 * errno.c
 */


/*
 * This is were we store the value of errno
 */
static int __errno = 0;


/*
 * Return a pointer to __errno
 */
int* __errno_location() {
    return &__errno;
}
