/*
 * exec.c
 *
 */

#define __NR_execve      11

#define _syscall3(name,a,b, c) \
{ \
__asm__ volatile ("int $0x80" \
    : "=a" (__res) \
    : "a" (__NR_##name),"b" (a),"c" (b), "d" (c)); \
}

/*
 * Execute a program
 */
int __ctOS_execve(char* path, char* argv[], char* envp[]) {
    int __res;
    _syscall3(execve, path, argv, envp);
    return __res;
}
