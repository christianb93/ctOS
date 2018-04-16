/*
 * sysmon.c
 *
 */


#include "vga.h"
#include "console.h"
#include "kprintf.h"
#include "smp.h"
#include "sched.h"
#include "cpu.h"
#include "mm.h"
#include "lib/time.h"
#include "pm.h"
#include "lib/os/syscalls.h"
#include "timer.h"
#include "ahci.h"
#include "pata.h"
#include "net_if.h"

/*
 * The status window
 */
static win_t status_window;

/*
 * The information window
 */
static win_t info_window;

/*
 * Remember some data from previous iteration
 */
static u32 last_unix_time;
static u32 last_blocks;
static u32 last_packets;


/*
 * This function is called periodically by a kernel thread and updates the
 * system monitor window
 */
static void sysmon_update() {
    struct tm structured_time;
    u32 unix_time;
    u32 blocks;
    u32 packets;
    int net;
    int cpu;
    int load;
    int x;
    int y;
    int i;
    int io;
    u32 pixel;
    structured_time.tm_sec = do_time(0);
    unix_time = structured_time.tm_sec;
    structured_time.tm_min = 0;
    structured_time.tm_hour = 0;
    structured_time.tm_mday = 1;
    structured_time.tm_mon = 0;
    structured_time.tm_year = 70;
    mktime(&structured_time);
    for (cpu = 0; cpu < SMP_MAX_CPU; cpu++) {
        load = sched_get_load(cpu);
        /*
         * Each CPU is represented by an area which is 48 pixels high.
         * The first area starts at y-coordinate 48. Within each area,
         * the top and bottom 9 lines are empty. The 30 lines in between
         * are used for the actual CPU load indicator
         *
         */
        for (x = 70; x < 170; x++) {
            if ( (x - 70 < load) || (70 == x))
                pixel = vga_vesa_color(100, 100, 100);
            else
                pixel = vga_vesa_color(0, 0, 0);
            for (y = 48*cpu + 57; y < 48*cpu + 87; y++)
                vga_put_pixel(&status_window, x, y, pixel);
        }
    }
    /*
     * Update current I/O throughput and current net throughput
     */
    blocks = pata_processed_kbyte() + ahci_processed_kbyte();
    io = (blocks - last_blocks) / (unix_time - last_unix_time);
    packets = net_if_packets();
    net = (packets - last_packets) / (unix_time - last_unix_time);
    last_packets = packets;
    last_blocks = blocks;
    last_unix_time = unix_time;
    vga_set_cursor(&status_window, 0, 30);
    for (i = 0; i < 120; i++)
        wprintf(&status_window, " ");
    vga_set_cursor(&status_window, 0, 30);
    wprintf(&status_window, "Time: %0.2d:%0.2d:%0.2d\n", structured_time.tm_hour, structured_time.tm_min, structured_time.tm_sec);
    wprintf(&status_window, "Free mem. (MB):         %.6d\n", mm_phys_mem_available() / 1024);
    wprintf(&status_window, "I/O (kB/sec):           %.6d\n", io);
    wprintf(&status_window, "Network (Pkts./sec):    %.6d\n", net);
}

/*
 * This is the update thread which periodically wakes up and does the updates
 */
static void update_thread(void* arg) {
    while(1) {
        do_sleep(1);
        sysmon_update();
    }
}
/*
 * Initialization of the system monitor. This function will create the system information window
 * and start a thread which periodically redraws the system status window
 */
void sysmon_init() {
    u32 thread;
    int cpu;
    int drive;
    char* drive_name;
    int irq_mode;
    u32 x_res;
    u32 y_res;
    u32 bpp;
    /*
     * Create the status window
     */
    vga_init_win(&status_window, 750, 50, 250, 650);
    vga_clear_win(&status_window, 0, 0, 0);
    vga_no_cursor(&status_window);
    vga_decorate_window(&status_window, "System status");
    /*
     * Add some labels
     */
    vga_set_cursor(&status_window, 8, 2);
    wprintf(&status_window, "0");
    vga_set_cursor(&status_window, 20, 2);
    wprintf(&status_window, "100");
    for (cpu = 0; cpu < SMP_MAX_CPU; cpu++) {
        vga_set_cursor(&status_window, 1, cpu * 3 + 4);
        wprintf(&status_window, "CPU %d", cpu);
    }
    /*
     * Set up time and current block count
     */
    last_unix_time = do_time(0);
    last_blocks = pata_processed_kbyte() + ahci_processed_kbyte();
    last_packets = net_if_packets();
    /*
     * Now set up the system information window
     */
    vga_init_win(&info_window, 50, 500, 640, 200);
    vga_clear_win(&info_window, 0, 0, 0);
    vga_no_cursor(&info_window);
    vga_decorate_window(&info_window, "System information");
    wprintf(&info_window, "Running ctOS (build %s %s)\n", __DATE__, __TIME__);
    wprintf(&info_window, "CPU0:   %s\n", cpu_get_brand_string());
    if (0 == vga_get_mode(&x_res, &y_res, &bpp)) {
        wprintf(&info_window, "Screen: VGA text mode 80x25\n");
    }
    else {
        wprintf(&info_window, "Screen: VESA graphics mode %d x %d @ %d bpp\n", x_res, y_res, bpp);
    }
    wprintf(&info_window, "#CPUs:  %d   RAM: %.6d MB    ", cpu_get_cpu_count(), mm_phys_mem() / 1024);
    wprintf(&info_window, "IRQ mode:  ");
    irq_mode = irq_get_mode();
    switch(irq_mode) {
        case IRQ_MODE_PIC:
            wprintf(&info_window, "PIC\n");
            break;
        case IRQ_MODE_APIC:
            wprintf(&info_window, "APIC\n");
            break;
        default:
            wprintf(&info_window, "UNKNOWN\n");
            break;
    }
    /*
     * Scan AHCI drives
     */
    drive = 0;
    while ((drive_name = ahci_drive_name(drive))) {
        if (drive < 2)
            wprintf(&info_window, "AHCI drive %d: %s\n", drive, drive_name);
        drive++;
    }
    if (drive >= 2) {
        wprintf(&info_window, "Found %d additional AHCI drives\n", drive - 1);
    }
    /*
      * Scan PATA drives
      */
     drive = 0;
     while ((drive_name = pata_drive_name(drive))) {
         if (drive < 2)
             wprintf(&info_window, "PATA drive %d: %s\n", drive, drive_name);
         drive++;
     }
     if (drive >= 2) {
         wprintf(&info_window, "Found %d additional PATA drives\n", drive - 1);
     }
    /*
     * and launch thread
     */
    if (__ctOS_syscall(__SYSNO_PTHREAD_CREATE, 4, &thread, 0, update_thread, 0))
        ERROR("Error while launching system monitor thread\n");
}

