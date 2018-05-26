# SMP processing in ctOS


## Design implications of multiprocessor support

In this section, we summarize some of the major design implications which a support of symmetric multiprocessing in the operating system kernel has. Details about support for SMP in specific modules can be found in the HTML documentation for these modules and in the comments within the module source code.

### Locking

The most obvious impact of SMP support on a kernel is the way how global data structures are protected against concurrent access. On a uniprocessor system, a thread which is currently being executed can easily protect itself from being preempted by blocking interrupts on the CPU temporarily. As there is only one CPU, this also implies that no other thread of execution can concurrently access any data while preemption is suppressed. Therefore turning off interrupts does not only disable preemption, but at the same time also serves as protection against concurrent access. In fact, it might even be a valid strategy to disable interrupts altogether while executing code in the kernel (with the exception of putting a task to sleep of course) so that effectively, no protection against concurrent access is necessary any more.

However, on a multiprocessor system, there is a difference between turning off preemption - which guarantees that a thread of execution continues without being interrupted - and protecting against concurrent access, as obviously, the same thread of execution can run concurrently on other CPUs as well. Therefore more advanced mechanisms are required. Typical ways to handle concurrent access are spinlocks, semaphores and condition variables. As using locks creates additional overhead, it is also reasonable to avoid locking of not absolutely needed and use lock-free algorithms based on atomic access as far as possible.

Different locking strategies are possible. One strategy is usually called **coarse grained locking**. With this strategy, there would be only a small number of locks each of which protects several kernel data structures or modules. In fact, some kernels - like early Linux SMP kernels - even use one lock only, commonly referred to as "big kernel lock" which is acquired when the kernel space is entered and only released again upon returning to user space. This approach is simple, but does of course imply that only one CPU can be in kernel space at a time, so access to the kernel is serialized.

A different strategy would be to use **fine grained locking**, i.e. to protect each data structure or even each part of a data structure by a separate lock. This approach tries to avoid unnecessary serialization, but has the disadvantage that the code becomes more complex and deadlocks are more likely. In addition, as acquiring and releasing a lock adds some overhead, this might be even slower than a coarse grained approach in some cases.

In general, ctOS tries to use a comparatively small number of locks within each module. Similar to the data structures, each lock is owned by a module and only acquired and released by code in this module. In most modules, the concept of monitors is used to organize the usage of locks, trying to avoid deadlocks. If in doubt, a more coarse grained approach is chosen, as the design goal of simplicity is weighted higher than performance.

Even though ctOS has been a uniprocessor operating system during the first few months of development, locking was added from the very beginning to the code. This was done to avoid the huge effort which can result out of the attempt to convert code which has been developed for a pure single-threaded environment into multi-CPU safe code. In fact, is has turned out that the locking strategy in many cases has a huge impact on the structure of the functions and the data, so that adding locking at a later point in time would have meant to redesign a large part of the module from scratch.


### Scheduling

When designing an SMP operating system, some decisions need to be taken about how tasks are scheduled to the different CPUs. Again, several approaches are possible. 

There could be one global run queue in which tasks ready to be executed are placed. Each CPU which has completed a piece of work would then scan this queue for available tasks, remove a task from the queue and run it. With this approach, tasks would be distributed to the next available CPU, keeping each CPU at an equal load.

Alternatively, there could be one dedicated run queue per CPU. At some point in time, maybe at task creation, a decision would be made on which queue a task is placed. Each CPU would then work with its dedicated task queue only. Consequently, a task would always be executed by the same CPU.

Both approaches have advantages and disadvantages.

-   having a global queue of runnables is most likely to create an equal load on each CPU. However, this queue needs to be locked every time a scheduler executes on any of the CPUs in the system, thus creating a potential performance bottleneck
-   with one global queue, no guarantees can be made towards application programs and the operating system that a task which has been executing on CPU X at some point in time will execute on the same CPU at same later point in time. **Migration**, i.e. moving the task to a different CPU, can occur at any point in time when the task is preempted. This again has some consequences. First, data structures specific to a CPU need to be protected by additional measures to make sure that they remain accessible and valid across a migration. Second, caches and the TLB need to be refilled each time a task switches CPU.

For ctOS, a combination of a global queue and a per-CPU queue has been chosen. There is one ready queue per CPU. When a task is RUNNABLE, it is placed on the run queue of the CPU which - at this point in time - has the smallest number of runnables. The task remains in this queue if it is preempted, i.e. it is added again to the same queue when it is interrupted because its quantum has been used up. If, however, a task blocks itself - for instance by using a down operation on a semaphore - it is removed from the run queue and added again if the event it is waiting for occurs. Thus tasks can be migrated only when they deliberately decide to go to sleep, not as a result of preemption. This gives tasks some control over the validity of CPU specific data structures. In addition, it implies that cache refills due to a task migration only occur at points in time when a task decides to wait for an external event like I/O. Usually, the time which passes until the task wakes up again is a multiple of the performance penalty associated with a cache refill, thus reducing the impact of the cache migration on the overall performance.

Another point which needs to be considered is the behaviour of the scheduler if a task with a high priority is added to the ready queue. Suppose for instance that a task goes to sleep, waiting for keyboard input. If now the user presses a key, an interrupt is issued. As part of the interrupt processing, the keyboard driver will place the task on the run queue again, using a higher priority to make sure that the task has a chance to process the user input as soon as possible. When the interrupt handler for the keyboard interrupt completes, a task switch will therefore take place and the task which is supposed to process the user input gains control of the CPU again.

In an SMP system using the scheduling policy outlined above, however, it might happen that the interrupt is processed by CPU A, whereas the task is placed on the ready queue of CPU B. As CPU B might just be processing another task, it might, in the worst case, take until the quantum of this task is elapsed until the task resumes execution. To avoid this, a special IPI is sent to a CPU by the scheduler if a task has been placed on its ready queue which has a higher priority than the task currently being executed on this CPU. This IPI has no special interrupt handler associated with it, but as part of processing it, the scheduler will be invoked - as it would be the case for any other interrupt - and select the higher priority task. This improves the responsibility of the system significantly.

In later versions of ctOS, more advanced SMP scheduling mechanisms will probably introduced. Some approaches to be considered in an updated design would be

-   use more sophisticated load indicators to decide which CPU receives a new task initially
-   support CPU affinity so that an application can pin threads to a CPU
-   take CPU topology into account - if for instance two logical CPUs are in fact two hyper threads on the same CPU core and share a cache, there is little to no overhead associated with a migration, thus scheduling domains containing all logical CPUs on the same core could be used

### Memory management

Several topics have to be considered when it comes to memory handling on an SMP system. First, there is caching. In an SMP system, each CPU typically has its own on-chip cache sitting between the CPU core and the actual shared system memory. Data which is present in this cache is directly manipulated by the CPU. Suppose for instance that CPU A loads a cacheline into its cache and subsequently issues a write to the corresponding memory address. Most common caching strategies will then update the cacheline without also updating the actual value in system memory ("write back cache"). If now another CPU accesses the same memory address, it loads the value from system memory into its own cache. At this point in time, however, this value has not yet been updated and thus both CPUs work with different values.

To solve this problem, most architectures use a cache coherency protocol like MESI or MOESI to make sure that at any point in time, the data in the cache and the data in memory appear to be in sync for all involved CPUs. Thus, caches and main system memory can be seen as a "memory subsystem" by the CPU without the need to distinguish between data in the cache and data in main system memory on a logical level. This is typically achieved by having the CPUs "snoop" the system bus to detect write cycles for memory areas which are currently kept in the cache. Other devices like PCI devices using DMA need to take part in this protocol as well.

ctOS assumes cache coherency which is currently guaranteed by all systems based on Intel MP specification. Note, however, that cache coherency and snooping is an issue with respect to scalability if the number of CPUs in the system is increased.

Another topic which needs to be considered is memory ordering. As writing to and reading from memory is comparatively expensive, even if a cache is used, most CPUs apply some reordering to load and store operations. Intels x86 CPUs, for instance, postpones store operations to some later point in time or implement prefetch to read from memory locations which might be needed by the next few instructions in the pipeline. Even though this is transparent to code executing on the same CPU, it implies that different CPUs will perceive a different order of read and write operations on the system bus. This issue needs to be dealt with by the operating system, see the more detailed section on memory ordering further below in this document.

### Interrupt handling

To handle hardware interrupts in an SMP system, several approaches are possible. The most straightforward approach is to route all interrupts to a designated CPU, say the **bootstrap processor (BSP)**, so that all hardware interrupt handlers execute on this CPU. To spread the interrupt load more evenly across different CPUs (and make the system more fault tolerant), the I/O APIC offers essentially two methods. 

With the first method, interrupts are routed statically to a CPU depending on the interrupt vector. With this approach, all interrupts with vector 32 are processed by one CPU, all interrupts with vector 33 are processed by the next CPU and so forth. This has the advantage that the code of the interrupt handler and the data structures accessed by it can be kept in the cache of this designated CPU which might speed up performance. 

A more advanced way of distributing interrupts is the so-called **lowest priority delivery mode** in which the hardware (i.e. the I/O APIC, the local APICs and the system bus) will distribute an interrupt to the CPU with the currently lowest priority. Here the  priority of a CPU is determined based on the interrupts which it is currently servicing and the content of a dedicated register in the CPUs local APIC, called the task priority register (TPR).

Currently ctOS supports the following interrupt distribution modes which can be selected using the kernel parameter `irq_dlv`.

<table>
<thead>
<tr class="header">
<th>Mode<br />
</th>
<th>Description<br />
</th>
</tr>
</thead>
<tbody>
<tr class="odd">
<td>1<br />
</td>
<td>The APIC is used and programmed for physical delivery mode. All interrupts go to the BSP<br />
</td>
</tr>
<tr class="even">
<td>2<br />
</td>
<td>The APIC is programmed to distribute the interrupts statically across all available CPUs, specifically vector x will be routed to CPU (x mod nr_of_cpus)<br />
</td>
</tr>
<tr class="odd">
<td>3<br />
</td>
<td>The APIC is programmed for lowest priority delivery mode<br />
</td>
</tr>
</tbody>
</table>

The same parameter with the same semantics also applies to devices that use MSI to route interrupts.

## Startup procedure

When, at boot time, control is handed over from the BIOS to the operating system, the kernel starts execution on a designated CPU called the bootstrap processor (BSP). All other CPUs in the system are called **AP** (additional processor) and are supposed to be in the following state (according to the Intel MP Specification):

-   the CPU is halted
-   interrrupts are disabled
-   the local APICs of the APs are reacting only on INIT and STARTUP IPIs


Once the BPS has completed its initialization, including the initialization of the local APIC, Intel recommends to use the following sequence to bring up an additional CPU.

1.  initialize the BIOS shutdown status to 0x0a. The shutdown status is the byte at offset 0xf in the CMOS. This value instructs the CPU to perform a JMP to the address stored at 40:67 within the BDA
2.  write the address of the startup code into the BDA at address 40:67, i.e. 0x467
3.  send an INIT IPI to the AP
4.  wait for 10 msecs
5.  send a STARTUP IPI to the AP
6.  wait for at most 200 microseconds
7.  send another STARTUP IPI to the AP

In fact, as we will see below, ctOS uses a slightly modified algorithm which tries to avoid the second STARTUP IPI as is turns out not to be necessary in most cases.

This is a generic algorithm which is supposed to work with all types of CPUs. At the first glance, it might appear that this algorithm will make the CPUs execute the bootstrap code twice. This is however not the case, as newer processors (starting with Intel Xeon) will examine their BSP flag (stored in the IA32\_APIC\_BASE\_MSR) and only start executing code when receiving the INIT IPI if they are the BSP. Otherwise, they wait for the startup IPIs (see section 8.4 of the Intel System Programming Guide). As the BIOS has already executed the MP startup procedure once when the OS takes control, the INIT IPI will not have any effect on newer CPUs instead of placing them in a "wait-for-SIPI" state.
From the Intel docs:
"The MP protocol is executed only after a power-up or RESET. If the MP protocol has completed and a BSP is chosen, subsequent INITs (either to a specific processor or system wide) do not cause the MP protocol to be repeated. Instead, each logical processor examines its BSP flag (in the IA32\_APIC\_BASE MSR) to determine whether it should execute the BIOS boot-strap code (if it is the BSP) or enter a wait-for-SIPI state (if it is an AP)."
However, older CPUs (namely 468DX) already start execution when they receive the INIT IPI and ignore the STARTUP IPIs.

To send an IPI, the ICR (interrupt command register) in the local APIC needs to be used. This is a 64 bit register. Writing to the low dword of this register causes the IPI to be sent, so the upper dword needs to be set up first.

<table>
<thead>
<tr class="header">
<th>Bits<br />
</th>
<th>Description<br />
</th>
</tr>
</thead>
<tbody>
<tr class="odd">
<td>56 - 63<br />
</td>
<td>Specifies the CPU respectively local APIC to which the IPI should be sent. The meaning of these bits depends on the destination mode, for &quot;physical destination mode&quot;, this is the local APIC ID of the target<br />
</td>
</tr>
<tr class="even">
<td>32 - 55<br />
</td>
<td>Reserved<br />
</td>
</tr>
<tr class="odd">
<td>20 - 31<br />
</td>
<td>Reserved<br />
</td>
</tr>
<tr class="even">
<td>18 - 19<br />
</td>
<td>Shorthand for destination. 00 = no shorthand, use destination in bits 56 - 63, 01 = self , 10 = all, 11 = all excluding self<br />
</td>
</tr>
<tr class="odd">
<td>16 - 17<br />
</td>
<td>Reserved<br />
</td>
</tr>
<tr class="even">
<td>15<br />
</td>
<td>Trigger mode (only meaningful if level is deassert, always 0 on newer CPUs)<br />
</td>
</tr>
<tr class="odd">
<td>14<br />
</td>
<td>Level (1 = assert, 0 = deassert) - newer CPUs only support 1<br />
</td>
</tr>
<tr class="even">
<td>13<br />
</td>
<td>Reserved<br />
</td>
</tr>
<tr class="odd">
<td>12<br />
</td>
<td>Delivery status ( 0 = idle, 1 = send pending). This field is read only and set to 0 by the HW once an IPI has been sent successfully<br />
</td>
</tr>
<tr class="even">
<td>11<br />
</td>
<td>Destination mode ( 0 = physical, 1 = logical)<br />
</td>
</tr>
<tr class="odd">
<td>8 - 10<br />
</td>
<td>Delivery mode:<br />
0 = fixed<br />
1 = lowest priority<br />
10 = SMI<br />
11 = reserved<br />
100 = NMI<br />
101 = INIT<br />
110 = STARTUP<br />
111 = reserved<br />
</td>
</tr>
<tr class="even">
<td>0 - 7<br />
</td>
<td>Vector number. For an STARTUP IPI, this number is the page at which execution starts, i.e. the CPU will branch to vector * 0x1000<br />
</td>
</tr>
</tbody>
</table>

 
Thus to send an IPI, the following steps are necessary:

-   load the upper dword of the ICR with the number of the target APIC ID, shifted to the left by 24 bits (assuming physical delivery and no shorthand is used)
-   assemble the lower dword of the ICR
-   write the lower dword to the ICR
-   spin around bit 12 of the ICR until it is cleared again

Let us execute a series of tests to see how this works in practice. In the first test, we are going to send an INIT IPI to the first AP and then halt the bootstrap CPU. The so-called "trampoline code" which is executed on the AP will write to the video memory and send a byte to the Bochs debugging port 0xe9 and then also halt the CPU. Before starting this sequence, we write 0xa into the CMOS shutdown status field and the address of our trampoline code to the restart vector at address 0x467.

On QEMU, the result is as expected. As the AP has already executed the boot sequence once, it is not executing any code as a result of the INIT IPI, but will enter the "wait-for-SIPI" state. Consequently, nothing is displayed on the screen. The same result is obtained on real hardware.

On Bochs, however, the result is different. Both the video output and the debugging port indicate that the CPU has started to execute the bootstrap code. The output of Bochs is as follows (Bochs 2.4.6 has been used for this and all following tests):

```
00439540839i\[APIC1\] Deliver INIT IPI
00439540839i\[CPU1 \] cpu software reset
00439540839i\[APIC1\] allocate APIC id=1 (MMIO enabled) to 0x00000000fee00000
00439540839i\[CPU1 \] CPU\[1\] is an application processor. Halting until IPI.
00439540839i\[CPU1 \] CPUID\[0x00000000\]: 00000003 756e6547 6c65746e 49656e69
00439540839i\[CPU1 \] CPUID\[0x00000001\]: 00000f23 01000800 00002000 07cbfbff
00439540839i\[CPU1 \] CPUID\[0x00000002\]: 00410601 00000000 00000000 00000000
00439540839i\[CPU1 \] CPUID\[0x00000003\]: 00000000 00000000 00000000 00000000
00439540839i\[CPU1 \] CPUID\[0x00000004\]: 00000000 00000000 00000000 00000000
00439540839i\[CPU1 \] CPUID\[0x00000007\]: 00000000 00000000 00000000 00000000
00439540839i\[CPU1 \] CPUID\[0x80000000\]: 80000008 00000000 00000000 00000000
00439540839i\[CPU1 \] CPUID\[0x80000001\]: 00000000 00000000 00000001 2a100800
00439540839i\[CPU1 \] CPUID\[0x80000002\]: 48434f42 50432053 20402055 20333331
00439540839i\[CPU1 \] CPUID\[0x80000003\]: 007a484d 00000000 00000000 00000000
00439540839i\[CPU1 \] CPUID\[0x80000004\]: 00000000 00000000 00000000 00000000
00439540839i\[CPU1 \] CPUID\[0x80000006\]: 00000000 42004200 02008140 00000000
00439540839i\[CPU1 \] CPUID\[0x80000007\]: 00000000 00000000 00000000 00000000
00439540839i\[CPU1 \] CPUID\[0x80000008\]: 00003028 00000000 00000000 00000000
00439540869i\[CPU0 \] WARNING: HLT instruction with IF=0!
A00439540874i\[CPU1 \] WARNING: HLT instruction with IF=0!
```

So we see that even though the AP (CPU \#1) has identified itself correctly as an AP, it still starts to execute code (as indicated by the "A" which has been written to the debugging port which appears in the last line). If we hit the power button to force Bochs to generate a register dump, we also see that CPU \#1 is in real mode and the value of its instruction pointer proves that it has executed our code.


In the second test scenario, we use the same trampoline code. However, this time, we send an INIT IPI, wait for approximately 10 ms and then send a STARTUP IPI to the AP. We then halt the BSP. Again the result is as expected on QEMU. The STARTUP IPI will start execution of our trampoline code on the AP and we are able to see the debugging output it generates. On Bochs, however, we get a message indicating that the STARTUP IPI was received by the AP, but it seems to be disregarded because the CPU is already executing and not in the "wait-for-SIPI" state. Here is the Bochs debugging output.

```
00439542905i\[APIC1\] Deliver INIT IPI
00439542905i\[CPU1 \] cpu software reset
00439542905i\[APIC1\] allocate APIC id=1 (MMIO enabled) to 0x00000000fee00000
00439542905i\[CPU1 \] CPU\[1\] is an application processor. Halting until IPI.
00439542905i\[CPU1 \] CPUID\[0x00000000\]: 00000003 756e6547 6c65746e 49656e69
00439542905i\[CPU1 \] CPUID\[0x00000001\]: 00000f23 01000800 00002000 07cbfbff
00439542905i\[CPU1 \] CPUID\[0x00000002\]: 00410601 00000000 00000000 00000000
00439542905i\[CPU1 \] CPUID\[0x00000003\]: 00000000 00000000 00000000 00000000
00439542905i\[CPU1 \] CPUID\[0x00000004\]: 00000000 00000000 00000000 00000000
00439542905i\[CPU1 \] CPUID\[0x00000007\]: 00000000 00000000 00000000 00000000
00439542905i\[CPU1 \] CPUID\[0x80000000\]: 80000008 00000000 00000000 00000000
00439542905i\[CPU1 \] CPUID\[0x80000001\]: 00000000 00000000 00000001 2a100800
00439542905i\[CPU1 \] CPUID\[0x80000002\]: 48434f42 50432053 20402055 20333331
00439542905i\[CPU1 \] CPUID\[0x80000003\]: 007a484d 00000000 00000000 00000000
00439542905i\[CPU1 \] CPUID\[0x80000004\]: 00000000 00000000 00000000 00000000
00439542905i\[CPU1 \] CPUID\[0x80000006\]: 00000000 42004200 02008140 00000000
00439542905i\[CPU1 \] CPUID\[0x80000007\]: 00000000 00000000 00000000 00000000
00439542905i\[CPU1 \] CPUID\[0x80000008\]: 00003028 00000000 00000000 00000000
A00439542940i\[CPU1 \] WARNING: HLT instruction with IF=0!
00439721700i\[APIC1\] Deliver Start Up IPI
00439721700i\[CPU1 \] CPU 1 started up by APIC, but was not halted at the time
00439721730i\[CPU0 \] WARNING: HLT instruction with IF=0!
```

Interestingly enough, the behaviour of Bochs changes if we do NOT write 0xa into the CMOS shutdown status field. In this case, the system behaves as expected. When the INIT IPI is received, no code is executed yet, but the trampoline code gets executed upon receiving the STARTUP IPI. Here is the Bochs output in this case.

```
00439540941i\[APIC1\] Deliver INIT IPI
00439540941i\[CPU1 \] cpu software reset
00439540941i\[APIC1\] allocate APIC id=1 (MMIO enabled) to 0x00000000fee00000
00439540941i\[CPU1 \] CPU\[1\] is an application processor. Halting until IPI.
00439540941i\[CPU1 \] CPUID\[0x00000000\]: 00000003 756e6547 6c65746e 49656e69
00439540941i\[CPU1 \] CPUID\[0x00000001\]: 00000f23 01000800 00002000 07cbfbff
00439540941i\[CPU1 \] CPUID\[0x00000002\]: 00410601 00000000 00000000 00000000
00439540941i\[CPU1 \] CPUID\[0x00000003\]: 00000000 00000000 00000000 00000000
00439540941i\[CPU1 \] CPUID\[0x00000004\]: 00000000 00000000 00000000 00000000
00439540941i\[CPU1 \] CPUID\[0x00000007\]: 00000000 00000000 00000000 00000000
00439540941i\[CPU1 \] CPUID\[0x80000000\]: 80000008 00000000 00000000 00000000
00439540941i\[CPU1 \] CPUID\[0x80000001\]: 00000000 00000000 00000001 2a100800
00439540941i\[CPU1 \] CPUID\[0x80000002\]: 48434f42 50432053 20402055 20333331
00439540941i\[CPU1 \] CPUID\[0x80000003\]: 007a484d 00000000 00000000 00000000
00439540941i\[CPU1 \] CPUID\[0x80000004\]: 00000000 00000000 00000000 00000000
00439540941i\[CPU1 \] CPUID\[0x80000006\]: 00000000 42004200 02008140 00000000
00439540941i\[CPU1 \] CPUID\[0x80000007\]: 00000000 00000000 00000000 00000000
00439540941i\[CPU1 \] CPUID\[0x80000008\]: 00003028 00000000 00000000 00000000
00439719811i\[APIC1\] Deliver Start Up IPI
00439719811i\[CPU1 \] CPU 1 started up at 8000:00000000 by APIC
A00439719816i\[CPU1 \] WARNING: HLT instruction with IF=0!
00439719841i\[CPU0 \] WARNING: HLT instruction with IF=0!
```

The following table summarizes the results of the tests executed so far.

<table>
<thead>
<tr class="header">
<th>BSP<br />
</th>
<th>AP<br />
</th>
<th>Result on Bochs<br />
</th>
<th>Result on QEMU<br />
</th>
<th>Result on real hardware (Core i7 / X58 chipset)<br />
</th>
</tr>
</thead>
<tbody>
<tr class="odd">
<td>Send INIT IPI to AP, then turn off interrupts and halt<br />
</td>
<td>Write byte to video memory and Bochs debugging port, then halt<br />
</td>
<td>Unexpected: AP starts execution of code, even though no STARTUP IPI has been received<br />
</td>
<td>As expected: no code is executed by AP<br />
</td>
<td>As expected: no code is executed by AP<br />
</td>
</tr>
<tr class="even">
<td>Send INIT IPI to AP. Wait for 10ms, then send STARTUP IPI to AP. Turn off interrupts and halt<br />
</td>
<td>Write byte to video memory and Bochs debugging port, then halt</td>
<td>Unexpected: AP start after INIT IPI already, STARTUP IPI is ignored<br />
</td>
<td>As expected: output of AP appears<br />
</td>
<td>As expected: output of AP appears</td>
</tr>
<tr class="odd">
<td>Do NOT write 0xa to CMOS shutdown status byte. Send INIT IPI to AP. Wait for 10ms, then send STARTUP IPI to AP. Turn off interrupts and halt</td>
<td>Write byte to video memory and Bochs debugging port, then halt</td>
<td>Bochs does not start execution of code upon receiving the INIT IPI. Instead, it behaves as expected and only starts upon receiving the STARTUP IPI<br />
</td>
<td>As expected: output of AP appears</td>
<td>As expected: output of AP appears</td>
</tr>
</tbody>
</table>

To deal with this behaviour of Bochs, the first idea was to use a startup sequence which does not send a STARTUP IPI to Bochs if the AP comes up after the INIT IPI already. However, it turned out that this approach does not work. Instead, the trampoline code is executed up to the point where the CPU has entered protected mode and the stack segment is loaded with its new value, but then the CPU seems to hang in this state indefinitely. The second attempt was to to simply ignore the unexpected behaviour of Bochs, i.e. we send a STARTUP IPI to Bochs even though the trampoline code is already being executed. Thus our startup sequence looks as follows.

```
Write address of trampoline code to BIOS reboot vector at 0x467
Write 0xa to BIOS shutdown status at offset 0xF in RTC CMOS
Send INIT IPI to AP
Wait for 10 ms
Send STARTUP IPI to AP
Wait for 10 ms
Check startup counter at 0x10000
If trampoline code is not yet running THEN
  Send second STARTUP IPI to AP
  Wait for 10 ms
  Check counter at 0x10000 again
  IF trampoline code is not yet running THEN
    give up
  END IF
END IF
```

However, this is again not working on Bochs. Instead, it leads to the error message

```
00439731613i\[APIC1\] Deliver Start Up IPI
00439731613p\[CPU1 \] >>PANIC<< load_seg_reg(): invalid segment register passed!
00439731613i\[CPU0 \] CPU is in protected mode (active)
```

The reason for this behaviour is that at the point in time when the STARTUP IPI is received, Bochs has already entered protected mode. However, when the STARTUP IPI is processed, the code segment is loaded with the value derived from the IPI vector. As in protected mode, this value is invalid and does not correspond to a valid GDT entry, the above message is displayed and Bochs panics. Given the test results above, an obvious version would be not to write 0xa to the CMOS shutdown status. This, however, would imply that even theoretically, the startup sequence would not work on an 486DX. We therefore use the local APIC version to check whether we are running on an 486DX with an external local APIC. If this is the case, we write 0xa to the CMOS shutdown status field and do not send any INIT IPIs. If the APIC is on-chip, we leave the CMOS shutdown status alone and use the sequence as above. Thus our final startup code looks as follows.

```
Get local APIC version
IF local APIC version & 0xF0 == 0x10 THEN
  apic_type = on-chip
ELSE IF local APIC version & 0xF0 = 0x0 THEN
  apic_type = 468DX
ELSE
  raise error - unknown APIC type
IF apic_type == 486DX THEN
  Write address of trampoline code to BIOS reboot vector at 0x467
  Write 0xa to BIOS shutdown status at offset 0xF in RTC CMOS
END IF
Send INIT IPI to AP
Wait for 10 ms
IF (apic_type == on-chip) THEN
  Send STARTUP IPI to AP
  Wait for 10 ms
  Check startup counter at 0x10000
  IF trampoline code is not yet running THEN
    Send second INIT IPI to AP
    Wait for 10 ms
    Check counter at 0x10000 again
    If trampoline code is not yet running THEN
      give up
    END IF
  END IF
END IF
```

This sequence now works on QEMU, Bochs and real hardware. I have not been able to test this on an old 486DX as I do not have access to an older machine. For the 486DX, issuing an INIT IPI does in fact involve sending two IPI messages, the first one being an ordinary INIT IPI message, whereas the second one is a "de-assert" message.

## Memory ordering


Starting with the Pentium 4, the CPU does not necessarily execute write and read operations to and from system memory in the order in which they appear in the instruction stream. Instead, the CPU is free to reorder the execution in order to optimize performance. 

It might for instance decide to perform a read access to an uncached address before forwarding any buffered writes to the cache or the system bus. In fact, the x86 CPU has a component called "store buffer". This is essentially a queue of store instructions which is sitting between the CPU and the cache and the main memory. When a store instruction is processed, the CPU might decide to put this instruction into this queue. This makes it possible that load instructions which come after a store instruction are actually executed BEFORE the store takes place. If, however, a read to the same memory location is issued on the same CPU, this read is served from the store buffer without any actual memory access ("store buffer forwarding"). Other CPUs have similar mechanisms which might generate even less intuitive situations.

Even though a CPU will guarantee that this is hidden from the point of view of the CPU on which the code executes, this is a problem if more than one CPU accesses data in main memory or in the cache, as it implies that different CPUs might have a different view on the order of load and store operations. We will look at a few examples below which illustrate nicely that this is not a theoretical issue only but in fact will render some "naive" implementation patterns for synchronization unusable on many SMP systems.

Different models for memory ordering have been used to handle this sort of problems. Some systems use a memory model which is called **weak consistency** and implies that loads and stores can be arbitrarily reordered by the CPU. However, special instructions called **memory barriers** can be used to achieve some sort of ordering at critical points in the code.

The other extreme is called **sequential consistency**. In this model, all reads and writes are always executed in the same order in which they appear in the instruction stream. Further, all memory accessing instructions in an SMP system are executed in some sequential order. The operations of the individual CPUs appear in this sequence in the same order in which they appear in the instruction stream.

This model corresponds to a physical system in which only one CPU can access memory at a time over some sort of system bus. Thus if for instance two CPUs execute a series of operations A1, B1, C1,... and A2, B2, C2 , where Ax
is a "store" or "load" operation (x being the CPU), then the total order in which these instructions operate on system memory could for instance be A1, A2, B2, C1, C2.

Most CPUs have a memory model somewhere in between called **relaxed consistency**. These models allow a few types of re-ordering and disallows others. Let us look at each of these types now and see what these rules mean in practice.

### Loads may be reordered with older stores

This type of reordering basically means that a store is put into a "store buffer" and "newer" load operations (i.e. load operations which appear later in the instruction stream) are executed before the store operation has been completed. Thus if the instruction stream looks like this

..... S....L......

the load L migh occur before the store S.

To see what this means in practice, consider the following example, which is in fact a part of [Dekkers algorithm](http://en.wikipedia.org/wiki/Dekkers_algorithm) for mutual exclusion and example 8.3 in the Intel documentation.

<table>
<tbody>
<tr class="odd">
<td>CPU 1<br />
</td>
<td>CPU 2<br />
</td>
</tr>
<tr class="even">
<td>flag1 = 1<br />
if (0 == flag2) {<br />
   // enter critical section<br />
}<br />
</td>
<td>flag2 = 1<br />
if (0 == flag1) {<br />
  // enter critical section<br />
}<br />
</td>
</tr>
</tbody>
</table>

In this example, both CPUs perform a load and a store for different locations. If loads can be reordered with stores, it is possible that both threads enter the critical section.

### Stores may be reordered with stores

If this rule applies, stores can be reordered with other store operations. Thus if a CPU writes to memory locations A and B in this order, another CPU reading from these locations might see the change in B before being aware of the change in A. This might for instance happen if the stores are written to different memory modules and the first store has not yet been completed when the second store is executed. Thus if the instruction stream contains two stores S1 and S2

....S1.....S2....

they might actually be executed in the order S2, S1. Again let us look at an example which is basically example 8.1 in the Intel documentation.

<table>
<tbody>
<tr class="odd">
<td>CPU 1<br />
</td>
<td>CPU 2<br />
</td>
</tr>
<tr class="even">
<td>x = 1<br />
valid = 1<br />
</td>
<td>r1 = valid<br />
r2 = x<br />
if (1 == r1)<br />
{<br />
   // data in r2 valid, do something with it<br />
}<br />
</td>
</tr>
</tbody>
</table>

Here CPU 1 markes the data at memory location x as valid by setting the memory location valid to one. However, if these write operations are reordered, then it might happen that the update of valid becomes effective first and CPU 2 reads the unchanged value of x. Thus CPU 2 would believe that the data in r2 is valid and proceed with it, even though r2 still has the old value of x.

### Loads may be reordered with loads

If the instruction streams contains two loads

....L1.....L2.....

this rule says that in fact L2 can take place before L1. The same example as used above can be used to illustrate the impact of a load-load reordering. In fact, suppose that the writes of CPU 1 are executed in the order written, but CPU 2 first executes the read of x and than the read of valid. Then the read of x might be before CPU 1 executes its code, but the read of valid if afterwards, so that r1 = 1. Again CPU 2 would assume that the data in r2 is valid and continue to work with it.

### Stores may be reordered with older loads

If an instruction stream contains a store and an older load, i.e. looks like

...L....S......

this rule implies that in fact the load can be executed before the store could take place. Again let us look at an example. Suppose that we come up with some algorithm where CPU 2 sets a flag to indicate that is has read some data
and so CPU 1 is free to overwrite it. A part of this algorithm could be as follows.

<table>
<tbody>
<tr class="odd">
<td>CPU 1<br />
</td>
<td>CPU 2<br />
</td>
</tr>
<tr class="even">
<td>if (1 == have_read) {<br />
  data = 1<br />
}<br />
</td>
<td>r1 = data<br />
have_read = 1<br />
<br />
</td>
</tr>
</tbody>
</table>

Thus CPU 1 executes a load followed by a store, and similarly CPU 2 executes a load followed by a store. If now CPU 2 performs store and load out-of-order, i.e. if it first performs the store of `have_read` and then the load, it might happen that CPU 1 sees the value `have_read` = 1 and updates data to data = 1.

### Memory barriers

In order to enforce memory ordering which is stronger than the rules outlined above, most CPUs offer special operations which can be inserted into the instruction stream to enforce a particular ordering of things. These operations are called **memory barriers**. Usually, three different types of barriers are distinguished.

A **write memory barrier** is an instruction which makes sure that with respect to other components of the system, all stores which are located before the memory barrier in the instruction stream appear to be executed before all stores located after the memory barrier. Thus if you have an instruction stream like

...W1...W2....WBARRIER...W3...W4...

W1 and W2 could still be reordered and W3 and W4 could still be reordered, but W1 and W2 both appear to happen before W3 and W4 for all other components of the system (they cannot "jump across") the barrier. A write memory barrier does not have any impact on loads.

Similarly, a **read memory barrier** enforces a partial ordering on loads, i.e. all loads which are located earlier in the instruction stream than the read memory barrier appear to have executed before any load after the memory barrier. Finally, a **general memory barrier** is a combined write and read memory barrier.

As an example, let us consider once more Dekker's algorithm. The problem described above that this algorithm is facing on SMP system can be fixed by adding memory barriers into the code as follows.

<table>
<tbody>
<tr class="odd">
<td>CPU 1<br />
</td>
<td>CPU 2<br />
</td>
</tr>
<tr class="even">
<td>flag1 = 1<br />
&lt;&lt;general memory barrier&gt;&gt;<br />
if (0 == flag2) {<br />
   // enter critical section<br />
}<br />
</td>
<td>flag2 = 1<br />
&lt;&lt;general memory barrier&gt;&gt;<br />
if (0 == flag1) {<br />
  // enter critical section<br />
}<br />
</td>
</tr>
</tbody>
</table>

 
Here the memory barrier makes sure that no reordering occurs at the critical points. Note that a write memory barrier or read memory barrier is not sufficient here, as we need to make sure that the read and write operations on the flag variable are not reversed, whereas a read or write memory barrier would not prevent that from happening as it avoids reordering of reads with other reads resp. writes with other writes.


### The x86 memory model and memory barriers on x86

The x86 CPU has a memory model which could be described as "write ordered with store-buffer forwarding". To understand this, let us look a little bit into the microarchitecture of newer Intel CPUs. 

In addition to caches and pipelines, these CPUs facilitate a component called "store buffer". Essentially, this is a buffer in which the CPU temporarily places store instructions to execute them at a later point in time. When a read is performed from a memory location and an instruction which is still in the store buffer changes this memory location, the read can be satisfied from the store buffer, i.e. the CPU will look at the instruction in the store buffer, see what value this instruction is supposed to write and use this value to service the load instruction. This allows a CPU to see the results of a store even though the store itself has not yet been forwarded to the memory sub system and is therefore not yet visible to other components of the system, including other CPUs.

Note that the store buffer is located between the CPU and the cache. As x86 guarantees cache coherency, the combination of main memory and all caches can be considered as one unit from the point of view of the CPU when it comes to memory ordering.

Exceptions from this rule exist for fast-string operations and non-temporal move instructions. However, these instructions are not used by ctOS and are therefore not relevant for these considerations. Also note that the memory model described above only applies to memory areas set up with caching mode write-back. This is the case for all ordinary memory, but not necessarily for memory areas like the VGA video memory.

Another feature of the x86 memory model is the fact that "atomic operations act as memory barrier". More precisely, the memory model prevents locked instructions from being reordered with loads and stores that execute earlier
or later. Thus the usage of locked instructions effectively acts as a memory barrier. This is extremely useful in practice, as most access to shared data which might be vulnerable to memory ordering issues is included in critical sections
which use spinlocks anyways. As spinlocks use locked instructions like XCHG, this means that loads and stores can be reordered within the critical sections, but not escape out a critical section, i.e. be moved past the enclosing spinlock operations.

Thus read and write memory barriers are effectively no-ops on x86. To realize a general memory barrier, a locked operation which does nothing can be used. Alternatively, for systems supporting the SSE2 instruction set, mfence realizes a general memory barrier.

### The memory model used in ctOS

To make porting of multi-threaded code to another platform feasible, an abstract memory model has been defined for ctOS. Obviously, this model is leaning towards the x86 memory model, as this is the only platform which is currently supported by ctOS (and also the only platform to which the author has access for testing). The following rules define the memory model of ctOS.

1.  No assumptions are made about reordering of stores with stores, stores with loads, loads with loads and loads with stores
2.  Thus all code which is not agnostic to memory ordering needs to use memory barrier instructions to protect against undesired side effects
3.  Memory barriers are defined as macros in smp.h
4.  Furthermore, it is assumed that the operations of acquiring and releasing a spinlock (`spinlock_get` and `spinlock_release`) act as general memory barrier. Therefore loads and stores may be reordered within a critical section protected by spinlocks if no memory barriers are used, but entering and leaving the critical section by acquiring and releasing the spinlock acts as a memory barrier.
5.  Atomic operations are also supposed to act as general memory barriers (on x86, this comes "for free" as they are implemented using the lock prefix as well)
6.  It is also assumed that the system firmware will set up all memory used for memory mapped I/O with a memory model which avoids memory reordering. Thus, on x86 for instance, we assume that the BIOS will set up all memory areas used for memory mapped I/O as uncachable.

## The local APIC timer

The local APIC timer is a timing facility built into each local APIC. In can be used to generate interrupts in a one-shot mode or in a periodic mode. The local APIC timer is controller by four registers within the local APIC.

<table>
<thead>
<tr class="header">
<th>Register<br />
</th>
<th>Offset within local APIC address space<br />
</th>
<th>Description<br />
</th>
</tr>
</thead>
<tbody>
<tr class="odd">
<td>Divide configuration register<br />
</td>
<td>0x3e0<br />
</td>
<td>The rate at which the current count register is decremented is the bus clock divided by the value in this field. Only bits 0, 1 and 3 of this register are used. Bit 2 is supposed to be zero, all other bits are reserved. The value n from bits 0, 1 and 3 taken as binary values leads to division factor 1^n. So to set the division factor to 128 = 2^5 which is the largest possible values, bits 0, 1 and 3 need to form the binary number 110b. Thus the register needs to be set to 1010b = 12<br />
</td>
</tr>
<tr class="even">
<td>Initial count register<br />
</td>
<td>0x380<br />
</td>
<td>This is the initial count at which a countdown begins. The countdown starts if the value within the initial count register is copied to the current count register. In periodic mode, the current count register is automatically reloaded from the initial count register each time it reaches zero<br />
</td>
</tr>
<tr class="odd">
<td>Current count register<br />
</td>
<td>0x390<br />
</td>
<td>This register contains the current counter and is incremented at a rate determined by the value of the divide configuration register. If this register reaches zero, an interrupt is raised<br />
</td>
</tr>
<tr class="even">
<td>Timer LVT<br />
</td>
<td>0x320<br />
</td>
<td>Bits 17-18: determine mode (00b = one shot mode, 01b = periodic mode)<br />
Bit 16: mask (0 = enable interrupt, 1 = disable interrupt)<br />
Bit 12: delivery status (0 = idle, 1 = send pending)<br />
Bits 8-10: delivery mode, 000 = fixed<br />
Bits 0-7: interrupt vector<br />
</td>
</tr>
</tbody>
</table>

Thus the frequency at which the local APIC timer fires depends on the CPU bus clock (this is not the internal CPU clock, for instance for my Core i7-950, the bus clock is 133 Mhz). As the CPU bus clock is unknown at compile time, we first have to calibrate the local APIC timer before we can actually use it.

For this purpose, we follow these steps (this only works if the BSP has already set up the PIC and maintains the ticks field for CPU 0) while interrupts are still disabled on the local CPU.

-   set the divide configuration register to some reasonable initial value (for instance 128)
-   write to the timer LVT to set up the timer in one-shot mode, but with the mask bit set so that no interrupt will actually be received
-   read from the global kernel variable ticks for CPU 0
-   set the initial count register to 0xFFFFFFFF
-   set the current count register to 0xFFFFFFFF
-   loop until the ticks variable has been increased by N
-   read the value of the current count register
-   subtract this value from 0xFFFFFFFF to see how many ticks the local APIC timer has generated - call this x

Thus if we now set the initial counter register to x / N and reprogram the local APIC to periodic mode, we have the same frequency for the local APIC interrupt as we have for the global PIT interrupt. Note that obviously the precision of this algorithm depends a lot on the chosen value for N. The bigger we chose N, the better the synchronisation between the two interrupt sources will be, however, if N is to big, the boot time will increase.

## References

-   Intel System Programming Guide (325384), May 2011
-   [Dekkers algorithm](http://en.wikipedia.org/wiki/Dekkers_algorithm) on Wikipedia
-   [Memory barriers on PowerPC](http://ridiculousfish.com/blog/posts/barrier.html)


