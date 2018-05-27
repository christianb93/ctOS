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

int main() {
    int rc;
    char *arguments[] = { "a", "b", NULL };
    printf("Calling stub with argument 105, 106\n");
    stub("105", 105, 106ULL);
    printf("Using execve\n");
    if ((rc = execv("dumpargs", arguments))) {
        printf("execv returned with error code %d, errno = %d\n", rc, errno);
    }
}
