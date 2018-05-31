/*
 * testargs.c
 *
 */
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>

static void vararg_test(char* template, __builtin_va_list args) {
    uint32_t x =  __builtin_va_arg(args,uint32_t);
    printf("Got first argument: %d\n", x);
    uint64_t y = __builtin_va_arg(args,uint64_t);
    printf("Got first argument: %d\n", (uint32_t) y);
}

void stub(char* template, ...) {
    __builtin_va_list args;
    __builtin_va_start(args, template);
    vararg_test(template, args);
}

void stub1(char* path, char* arg0, ...) {
    char* x;
    int count = 0;
    __builtin_va_list ap;
    __builtin_va_start(ap, arg0);
    while (1) {
        x = __builtin_va_arg(ap, char*);
        printf("x = %d\n", (int) x);
        if (x) {
            count++;
        }
        else {
            break;
        }
    }
    __builtin_va_end(ap);
}

int main() {
    int rc;
    char *arguments[] = { "a", "b", NULL };
    printf("Calling stub with argument 105, 106\n");
    stub("105", 105, 106ULL);
    printf("Calling stub1 with argument test, 0, 1\n");
    stub1("test", 0, 1);
    printf("Using execve\n");
    if ((rc = execv("dumpargs", arguments))) {
        printf("execv returned with error code %d, errno = %d\n", rc, errno);
    }
}
