/*
 * main.c
 *
 * This is the main entry point for the kernel at boot-up. The function run will be called by
 * the bootloader. It needs to invoke all necessary initialization routines. Then it
 * puts process 0 into an idle loop, forks of process 1 and runs /bin/init
 *
 * Depending on the kernel parameter do_test and the defines in tests.h, several assembly
 * tests are executed during initialization. These tests are run via various hooks in tests.c:
 *
 * 1) do_pre_init_tests() is called during the boot process of the BSP immediately before the INIT
 *    process is generated
 * 2) do_post_init_tests() is called during the boot process of the BPS immediately before /bin/init
 *    is started and runs as process 1
 * 3) do_smp_tests_boot_ap() is called by the AP immediately after it has been brought up into
 *    protected mode - this happens for the first AP only
 * 4) do_smp_tests_boot_bsp() is run on the BSP immediately after the startup IPI sequence has been run
 *    for the first AP. Together with do_smp_tests_boot_bsp() it can be used to run tests which require full control
 *    of the CPU as interrupts are disabled on the AP at this time
 * 5) do_pre_init_tests_ap() is run on the first AP immediately before entering the idle loop, i.e. at a point in time when the
 *    CPU has fully set up
 *
 * The following diagram shows the overall timeline of the initialization process
 *
 *                           ---------                                                     ---------
 *                           -  BSP  -                                                     -  AP   -
 *                           ---------                                                     ---------
 *
 * void run()  ---->   parse kernel command line
 *                               |
 *                       Init memory manager         <--- mm_init()
 *                               |
 *                         Init video driver         <--- vga_init()
 *                               |
 *                          Init console             <--- cons_init()
 *                               |
 *                            Load TSS               <--- load_tss()
 *                               |
 *                        Init process manager       <--- pm_init()
 *                               |
 *                          Init scheduler           <--- sched_init()
 *                               |
 *                          Enable paging            <--- enable_paging()
 *                               |
 *                          Init kernel heap         <--- mm_init_heap()
 *                               |
 *                           Set up IRQ              <--- irq_init()
 *                           manager and
 *                           IRQ routing
 *                               |
 *                          Init keyboard            <--- kbd_init()
 *                               |
 *                            set up timer           <--- timer_init()
 *                               |
 *                            Initialize             <--- dm_init()
 *                             device
 *                             driver
 *                               |
 *                             Set up                <--- fs_init()
 *                              file
 *                             systems
 *                               |
 *                      Migrate stack to kernel
 *                          stack for task 0
 *                               |
 *                             enable
 *                            interrupts
 *                               |             smp_start_aps()
 *                             start APs  ------------------------------------------->   start
 *                               |                                                      smp_ap_main()
 *                               |                                                         |
 *                            do_smp_tests_boot_bsp()  <--------------->           do_smp_tests_boot_ap()
 *                               |                                                         |
 *                            smp_run_main()           <--------------->             wait until BSP calls
 *                               |                                                      smp_run_main()
 *                               |                                                         |
 *                               |                                                       create               <--- pm_create_idle_task()
 *                               |                                                        idle
 *                               |                                                        task
 *                               |                                                         |
 *                               |                                                    add idle task           <-- sched_add_idle_task()
 *                               |                                                     to scheduler
 *                               |                                                         |
 *                               |                                                  switch to thread          <-- mm_reserve_task_stack()
 *                               |                                                   specific stack
 *                               |                                                         |
 *                               |                                                    set up local            <-- apic_init_ap()
 *                               |                                                       APIC
 *                               |                                                         |
 *                               |                                                     enable IRQs
 *                               |                                                         |
 *                               |                                                         |
 *                               |                                                         |
 *                               |                                                       start                <-- smp_idle_loop()
 *                         wait for AP to -------------------------------------------->  idle
 *                           reach idle                                                  loop
 *                             loop                                                        |
 *                               |                                                         |
 *                            rebalance                                                    |
 *                              IRQs                                                       |
 *                               |                                                         |
 *                            do pre-init            <-- do_pre_init_tests()          do_pre_init_tests_ap()
 *                             tests
 *                               |
 *                               |
 *                            fork off
 *                             proc 1
 *                               |
 *                      ------------------
 *                      |                |
 *             proc 0   |                |   proc 1 (go_idle)
 *                      |                |
 *                      |                |
 *                init work queues       |
 *                      |                |
 *              init system monitor      |
 *                      |                |
 *                init network stack     |
 *                      |                |
 *                    enter         mount root fs
 *                    idle               |
 *                    loop         open /dev/tty
 *                                       |
 *                                   do post-init
 *                                     tests
 *                                       |
 *                                      run
 *                                     /bin/init
 */

#include "debug.h"
#include "vga.h"
#include "console.h"
#include "util.h"
#include "pic.h"
#include "mm.h"
#include "keyboard.h"
#include "pm.h"
#include "lib/unistd.h"
#include "sched.h"
#include "lib/stdlib.h"
#include "lib/string.h"
#include "params.h"
#include "dm.h"
#include "fs.h"
#include "pit.h"
#include "blockcache.h"
#include "pata.h"
#include "drivers.h"
#include "ahci.h"
#include "lib/os/oscalls.h"
#include "lib/os/syscalls.h"
#include "smp.h"
#include "timer.h"
#include "cpu.h"
#include "sysmon.h"
#include "arp.h"
#include "net.h"
#include "wq.h"


static int __errno = 0;
int* __errno_location() {
    return &__errno;
}


/*
 * These defines can be used to turn specific tests on and off
 */
#include "tests.h"

static char* __module = "BOOT  ";


/*
 * This is the idle task
 */
static void idle() {
    while (1) {
        sched_yield();
        asm("hlt");
    }
}


/*
 * This function forks off a new task which does the reminder of the initialization
 * and invokes the executable /bin/sh. The current process (process 0) is put into
 * an idle loop.
 *
 * Depending on the content of tests.h and the kernel parameter do_test,
 * additional tests are performed
 */
static void go_idle() {
    int rc;
    sysmon_init();
    wq_init();
    net_init();
    rc = __ctOS_fork();
    if (rc) {
        idle();
    };
    dev_t root;
    MSG("Process INIT (1) started\n");
    root = (dev_t) params_get_int("root");
    MSG("Now mounting root device (%h,%h)\n", MAJOR(root), MINOR(root));
    KASSERT(0 == do_mount("/", root, "ext2"));
    /*
     * Open /dev/tty. This will also attach /dev/tty as controlling terminal
     * to the INIT process which has been set up as session leader of session 1
     * by the fork system call
     */
    KASSERT(0 == do_open("/dev/tty", 0, 0));
    KASSERT(1 == do_open("/dev/tty", 0, 0));
    KASSERT(2 == do_open("/dev/tty", 0, 0));
    do_post_init_tests();
    /*
     * Load init program
     */
    MSG("Starting /bin/init\n");
    KASSERT(0 == do_exec("/bin/init", 0, 0, 0));
    KASSERT(0);
}

/*
 * This is the main entry point to the kernel, invoked by the
 * bootstrap code in start.S
 * Parameter:
 * @magic - the magic value of EAX
 * @multiboot_ptr - physical address of the GRUB2 multiboot information structure
 */
void run(u32 magic, u32 multiboot_ptr) {
    /*
     * Parse multiboot information
     */
    multiboot_init(multiboot_ptr, magic);
    /*
     * Parse kernel command line
     */
    params_parse();
    cpu_init();
    /*
     * Init video driver in text mode and the console driver
     */
    vga_init(0);
    cons_init();
    MSG("Multiboot flags %x\n", ((mb1_info_block_t*) (multiboot_ptr))->flags);
    /*
     * Init memory manager. We do this before we set up the VGA graphics
     * adapter in graphics mode as the VGA code might return to real mode
     * and potentially overwrites data stored by GRUB there as part of the multiblock
     * information structure which is required by the memory manager
     */
    mm_init();
    /*
     * Now switch to graphics mode
     */
    vga_init(1);
    cons_init();
    /*
     * Load TSS. We have not yet done this in the initial startup code
     * in start.S as I have experienced issues with the busy flag being
     * set if the TSS is already loaded while we are switching forth and
     * back between real mode and protected mode
     */
    load_tss();
    MSG("Setting up process manager and scheduler\n");
    pm_init();
    sched_init();
    MSG("Turning on paging\n");
    enable_paging();
    /*
     * Inform video driver that we have turned on paging so
     * that subsequent access to the video linear frame buffer
     * needs to go via virtual memory
     */
    vga_enable_paging();
    mm_init_heap();
    irq_init();
    MSG("Initializing keyboard\n");
    kbd_init();
    timer_init();
    MSG("Initializing device driver\n");
    dm_init();
    KASSERT(0 == mm_validate());
    MSG("Setting up file system\n");
    fs_init(DEVICE_NONE);
    /*
     * Note that after this point, the following is no longer allowed
     * for code in run:
     * - access multiboot_ptr
     * - access any local variables of run
     * There is also a reason why we use MM_VIRTUAL_TOS-3
     * instead of MM_VIRTUAL_TOS: the latter is not word aligned
     * In fact, when calling a function taking a parameter from
     * main, GCC will create code which does a long move to (%esp)
     * so we need at least 3 bytes on the stack above %esp
     */
    asm("mov %0, %%esp" : : "i" (MM_VIRTUAL_TOS-3));
    sti();
    smp_start_aps();
    do_pre_init_tests();
    go_idle();
}
