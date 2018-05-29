/*
 * test_write.c
 *
 */

#include "unistd.h"
#include "vga.h"
#include "utime.h"

void trap() {

}

int __ctOS_rename(char* old, char* new) {
    return -1;
}


int __ctOS_link(const char *path1, const char *path2) {
    return -1;
}


void win_putchar(win_t* win, u8 c) {
    write(1, &c, 1);
}

int main() {
    write(1, "Hello!\n", 7);
    return 0;
}
