# The system call layer


## Overview


The system call layer of ctOS is the part of the kernel which makes kernel services available for programs running in user space. 

A userspace program call invokes a system call by raising the software interrupt 0x80. In the register EAX, a system call number which identifies the system call to be executed needs to be specified. Up to four parameters can be passed to the system call in the registers EBX, ECX, EDX and ESI.Upon completion, the system call will put a return code into the register EAX.

All system call numbers are defined as symbolic constants in the header file lib/syscalls.h
As a system call is an interrupt, the processing of each system call starts in the interrupt manager irq.c. The dispatcher function within the interrupt manager will - based on the interrupt number - identify the interrupt as a system call and will forward the call to the system call dispatcher `syscall_dispatch` defined in syscalls.c.

For each system call, an entry point `entry_XXX` is defined in systemcalls.c. The table `systemcalls` contains a list of all registered system call entry points, indexed by the system call number. This table is used by the dispatcher to locate the entry point corresponding to a system call. 

The entry point function `entry_XXX` then extracts the parameters for the system call from the register contents as stored in the IR context and performs a cast to the actual parameter list. It then invokes the actual system call handler. By convention, the entry point for the system call XXX is called `entry_XXX` whereas the actual handler is called `do_XXX`.

Note that even though most of the system call handlers `do_XXX` require more than one module to do their work, each of these handlers is located in one of the kernel modules which is called the **lead module** for this system call.

Also note that the interrupt manager will enable hardware interrupts immediately before invoking the system call dispatcher and disable them again when the system call dispatcher returns. Thus, by default, __hardware interrupts are enabled__ while the system call kernel code executes.

## Handling error codes


A system call entry point is a function which accepts a pointer to an IR context as argument and returns a signed 32-bit integer value. When the system call entry point `entry_XXX` returns control to the dispatcher, the dispatcher will place this value in the register EAX in the IR context. Thus, when the interrupt returns to userspace, the userspace C library can extract the return value from the EAX register and present it as return value to the program either by placing it in the variable `errno` or by returning it directly to the caller.

There is no common convention on the error codes which are returned by the system call handlers `do_XXX`. Some of these handlers, like `do_read`, are supposed to return a non-negative number upon successful completion and a negative error code if the processing fails. Other system call handlers return 0 upon success and a positive error code if needed.

However, there **is** a convention regarding the return code of the entry points `entry_XXX` which needs to be observed by all entry points. An entry point

-   returns a non-negative number upon success
-   returns a negative error code which is -1 times an error code defined in `errno.h` upon failure

Thus depending on the return code of the system call handler, the entry point function `entry_XXX` needs to convert the return code of the kernel call appropriately. This convention makes it easy for the C library in user space to interpret the return code of a system call. When this return code is negative, it is expected to multiply it with -1 to convert it into a positive number - which then matches an error code in `errno.h` again - and place this number in the variable errno, returning -1 to the caller. When the result of the system call is a non-negative number, this value can be returned to the caller unchanged.

## Currently implemented system calls

<table>
<thead>
<tr class="header">
<th>System Call<br />
</th>
<th>Number of parameters<br />
</th>
<th>Lead module<br />
</th>
</tr>
</thead>
<tbody>
<tr class="odd">
<td>fork<br />
</td>
<td>0<br />
</td>
<td>Process and task manager pm.c<br />
</td>
</tr>
<tr class="even">
<td>pthread_create<br />
</td>
<td>4<br />
</td>
<td>Process and task manager pm.c<br />
</td>
</tr>
<tr class="odd">
<td>write<br />
</td>
<td>3<br />
</td>
<td>Generic file system layer fs.c<br />
</td>
</tr>
<tr class="even">
<td>execv<br />
</td>
<td>1<br />
</td>
<td>Process and task manager pm.c<br />
</td>
</tr>
<tr class="odd">
<td>read<br />
</td>
<td>3<br />
</td>
<td>Generic file system layer fs.c<br />
</td>
</tr>
<tr class="even">
<td>exit<br />
</td>
<td>0<br />
</td>
<td>Process and task manager pm<br />
</td>
</tr>
<tr class="odd">
<td>open<br />
</td>
<td>1<br />
</td>
<td>Generic file system layer fs.c</td>
</tr>
<tr class="even">
<td>readdir<br />
</td>
<td>2<br />
</td>
<td>Generic file system layer fs.c<br />
</td>
</tr>
<tr class="odd">
<td>close<br />
</td>
<td>1<br />
</td>
<td>Generic file system layer fs.c<br />
</td>
</tr>
<tr class="even">
<td>unlink<br />
</td>
<td>1<br />
</td>
<td>Generic file system layer fs.c<br />
</td>
</tr>
<tr class="odd">
<td>sbrk<br />
</td>
<td>1<br />
</td>
<td>Memory manager mm.c<br />
</td>
</tr>
<tr class="even">
<td>lseek<br />
</td>
<td>3<br />
</td>
<td>File system fs.c<br />
</td>
</tr>
<tr class="odd">
<td>sleep<br />
</td>
<td>1<br />
</td>
<td>Timer timer.c<br />
</td>
</tr>
<tr class="even">
<td>waitpid<br />
</td>
<td>4<br />
</td>
<td>Process and task manager pm.c<br />
</td>
</tr>
<tr class="odd">
<td>kill<br />
</td>
<td>2<br />
</td>
<td>Process and task manager pm.c</td>
</tr>
<tr class="even">
<td>sigaction<br />
</td>
<td>3<br />
</td>
<td>Process and task manager pm.c</td>
</tr>
<tr class="odd">
<td>sigreturn<br />
</td>
<td>2<br />
</td>
<td>Process and task manager pm.c</td>
</tr>
<tr class="even">
<td>sigwait<br />
</td>
<td>2<br />
</td>
<td>Process and task manager pm.c</td>
</tr>
<tr class="odd">
<td>quit<br />
</td>
<td>0<br />
</td>
<td>Process and task manager pm.c</td>
</tr>
<tr class="even">
<td>pause<br />
</td>
<td>0<br />
</td>
<td>Process and task manager pm.c</td>
</tr>
<tr class="odd">
<td>sigprocmask<br />
</td>
<td>3<br />
</td>
<td>Process and task manager pm.c</td>
</tr>
<tr class="even">
<td>getpid<br />
</td>
<td>0<br />
</td>
<td>Process and task manager pm.c</td>
</tr>
<tr class="odd">
<td>sigpending<br />
</td>
<td>1<br />
</td>
<td>Process and task manager pm.c</td>
</tr>
<tr class="even">
<td>chdir<br />
</td>
<td>1<br />
</td>
<td>File system fs.c<br />
</td>
</tr>
<tr class="odd">
<td>fcntl<br />
</td>
<td>3<br />
</td>
<td>File system fs.c</td>
</tr>
<tr class="even">
<td>stat<br />
</td>
<td>2<br />
</td>
<td>File system fs.c</td>
</tr>
<tr class="odd">
<td>seteuid<br />
</td>
<td>1<br />
</td>
<td>File system fs.c</td>
</tr>
<tr class="even">
<td>geteuid<br />
</td>
<td>0<br />
</td>
<td>File system fs.c</td>
</tr>
<tr class="odd">
<td>setuid<br />
</td>
<td>1<br />
</td>
<td>File system fs.c</td>
</tr>
<tr class="even">
<td>getuid<br />
</td>
<td>0<br />
</td>
<td>File system fs.c</td>
</tr>
<tr class="odd">
<td>getegid<br />
</td>
<td>0<br />
</td>
<td>File system fs.c</td>
</tr>
<tr class="even">
<td>dup<br />
</td>
<td>1<br />
</td>
<td>File system fs.c</td>
</tr>
<tr class="odd">
<td>isatty<br />
</td>
<td>1<br />
</td>
<td>File system fs.c</td>
</tr>
<tr class="even">
<td>getppid<br />
</td>
<td>0<br />
</td>
<td>File system fs.c</td>
</tr>
<tr class="odd">
<td>umask<br />
</td>
<td>1<br />
</td>
<td>File system fs.c</td>
</tr>
<tr class="even">
<td>pipe<br />
</td>
<td>2<br />
</td>
<td>File system fs.c</td>
</tr>
<tr class="odd">
<td>getpgrp<br />
</td>
<td>0<br />
</td>
<td>File system fs.c</td>
</tr>
<tr class="even">
<td>setpgid<br />
</td>
<td>2<br />
</td>
<td>File system fs.c</td>
</tr>
<tr class="odd">
<td>ioctl<br />
</td>
<td>depending on type<br />
</td>
<td>depending on type, routed by system call layer<br />
</td>
</tr>
<tr class="even">
<td>getgid<br />
</td>
<td>0<br />
</td>
<td>File system fs.c</td>
</tr>
<tr class="odd">
<td>dup2<br />
</td>
<td>2<br />
</td>
<td>File system fs.c</td>
</tr>
<tr class="even">
<td>fstat<br />
</td>
<td>2<br />
</td>
<td>File system fs.c</td>
</tr>
<tr class="odd">
<td>times<br />
</td>
<td>1<br />
</td>
<td>Process and task manager pm.c<br />
</td>
</tr>
<tr class="even">
<td>getcwd<br />
</td>
<td>0<br />
</td>
<td>File system fs.c</td>
</tr>
<tr class="odd">
<td>tcgetattr<br />
</td>
<td>2<br />
</td>
<td>File system fs.c</td>
</tr>
<tr class="even">
<td>time<br />
</td>
<td>1<br />
</td>
<td>Timer timer.c<br />
</td>
</tr>
<tr class="odd">
<td>tcsetattr<br />
</td>
<td>3<br />
</td>
<td>File system fs.c</td>
</tr>
<tr class="even">
<td>socket<br />
</td>
<td>3<br />
</td>
<td>File system fs.c</td>
</tr>
<tr class="odd">
<td>connect<br />
</td>
<td>3<br />
</td>
<td>File system fs.c<br />
</td>
</tr>
<tr class="even">
<td>send<br />
</td>
<td>3<br />
</td>
<td>File system fs.c<br />
</td>
</tr>
<tr class="odd">
<td>recv<br />
</td>
<td>3<br />
</td>
<td>File system fs.c</td>
</tr>
<tr class="even">
<td>listen<br />
</td>
<td>2<br />
</td>
<td>File system fs.c<br />
</td>
</tr>
<tr class="odd">
<td>bind<br />
</td>
<td>3<br />
</td>
<td>File system fs.c<br />
</td>
</tr>
<tr class="even">
<td>accept<br />
</td>
<td>3<br />
</td>
<td>File system fs.c<br />
</td>
</tr>
<tr class="odd">
<td>select<br />
</td>
<td>4<br />
</td>
<td>File system fs.c<br />
</td>
</tr>
<tr class="even">
<td>alarm<br />
</td>
<td>1<br />
</td>
<td>Timer timer.c<br />
</td>
</tr>
<tr class="odd">
<td>sendto<br />
</td>
<td>6<br />
</td>
<td>File system fs.c<br />
</td>
</tr>
<tr class="even">
<td>recvfrom<br />
</td>
<td>6<br />
</td>
<td>File system fs.c</td>
</tr>
<tr class="odd">
<td>setsockopt<br />
</td>
<td>5<br />
</td>
<td>File system fs.c</td>
</tr>
<tr class="even">
<td>utime<br />
</td>
<td>2<br />
</td>
<td>File system fs.c<br />
</td>
</tr>
<tr class="odd">
<td>chmod<br />
</td>
<td>2<br />
</td>
<td>File system fs.c<br />
</td>
</tr>
<tr class="even">
<td>getsockaddr<br />
</td>
<td>4<br />
</td>
<td>File system fs.c</td>
</tr>
<tr class="odd">
<td>mkdir<br />
</td>
<td>2<br />
</td>
<td>File system fs.c</td>
</tr>
<tr class="even">
<td>sigsuspend<br />
</td>
<td>2<br />
</td>
<td>Process and task manager pm.c</td>
</tr>
<tr class="odd">
<td>rename<br />
</td>
<td>2<br />
</td>
<td>File system fs.c</td>
</tr>
<tr class="even">
<td>setsid<br />
</td>
<td>0<br />
</td>
<td>Process and task manager pm.c</td>
</tr>
<tr class="odd">
<td>getsid<br />
</td>
<td>1<br />
</td>
<td>Process and task manager pm.c</td>
</tr>
</tbody>
</table>

## Validating system call parameters


Some system calls accept parameters from an application which are in fact pointers to buffer areas which are supposed to point to a suitable area in user space and to which the kernel will write data or from which data will be read. Obviously, care must be taken to make sure that these buffers are located within read- and/or writeable memory areas in user space. For this purpose, the memory manager offers a validation function `mm_validate_buffer` which receives the location of a buffer, its length in bytes and a parameter specifying whether this area is supposed to be readable (0) or writable (1) as well.

Note however that kernel threads can also execute system calls and will most likely use buffers which are located in kernel space, thus this validation would fail. Therefore the validation is skipped if the system call is invoked from within a kernel level thread. For this purpose, the previous execution level is passed to the system call dispatcher by the interrupt handler and forwarded to the respective system call entry point. Thus an entry point which receives a buffer needs to perform the following steps-

```
IF previous_execution_level == EXECUTION_LEVEL_USER THEN
  if mm_validate_buffer(buffer, buffer_len, read_write) THEN
    return -EFAULT;
  END IF
END IF
```

To simplify this, a macro VALIDATE(buffer, buffer_len, read_write) is defined in systemcalls.c which contains the respective code and needs to be added once for every buffer to the entry point.

## Adding system calls

To implement a new system call, the following steps are therefore necessary:

-   define the lead module for the system call
-   within the lead module, define a public function `do_XXX`
-   in systemcalls.c, define an entry point `entry_XXX` using the macro SYSENTRY 
-   add the entry point to the table`systemcalls` in systemcalls.c
-   add a corresponding constant `__SYSNO_XXX`` to the header file lib/syscalls.h 
-   implement a userspace function which uses the function `syscall` defined in unistd.h to invoke the new system call


