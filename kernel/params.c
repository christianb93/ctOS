/*
 * params.c
 *
 * Contains function to parse the kernel command line
 * The command line consists of key=value pairs, separated
 * by one or more blanks
 *
 * See the table kparm for a list of currently supported parameters and
 * their default values
 */

#include "params.h"
#include "lib/string.h"
#include "lib/stdlib.h"
#include "debug.h"
#include "multiboot.h"

static char cmd_line[MULTIBOOT_MAX_CMD_LINE];

/*
 * This table is used to hold the kernel parameters. At boot time, the values
 * are filled from the command line
 */
static char parm_heap_validate[2];
static char parm_use_debug_port[2];
static char parm_use_vbox_port[2];
static char parm_do_test[2];
static char parm_root[8];
static char parm_apic[2];
static char parm_loglevel[2];
static char parm_pata_ro[2];
static char parm_ahci_ro[2];
static char parm_sched_ipi[2];
static char parm_irq_log[2];
static char parm_vga[2];
static char parm_net_loglevel[2];
static char parm_eth_loglevel[2];
static char parm_tcp_disable_cc[2];
static char parm_irq_watch[8];
static char parm_use_bios_font[2];
static char parm_use_acpi[2];
static char parm_use_msi[2];


/*
 *
 * heap_validate: validate the heap whenever memory is allocated
 * use_debug_port: duplicate console output to Bochs / QEMU debug port
 * use_vbox_port: duplicate console output to Vbox debugging port (see vmmdevBackdoorLog in the VBox source code)
 * do_test: run kernel level tests at startup
 * apic: 0 = do not use apic, 1 = send all IRQs to BSP, 2 = use fixed assignment, 3 = lowest priority delivery mode
 * loglevel: set the global loglevel
 * pata_ro: block all writes to PATA devices
 * ahci_ro: block all writes to AHCI devices
 * sched_ipi: inform CPUs via IPI when they have a high priority task in their queue
 * irq_log: turn on logging in the interrupt manager
 * vga: determine vga mode (see vga.c for a complete list)
 * net_loglevel: enable logging in network stack
 * irq_watch: define a vector for which all IRQs will be logged
 * eth_loglevel: enable logging in eth layer
 * tcp_disable_cc: disable tcp congestion control
 * use_bios_font: use VGA bios font 
 * use_acpi: use ACPI as leading configuration source
 * use_msi: use MSI whenever a device supports this
 */
 
 


static kparm_t kparm[] = {
        { "heap_validate", parm_heap_validate, 1, "0", 0 },
        { "use_debug_port", parm_use_debug_port, 1, "1", 1 },
        { "do_test", parm_do_test, 1, "0", 0 },
        { "root", parm_root, 6, "0x100", 0x100},
        { "apic", parm_apic, 1, "2", 2},
        { "loglevel", parm_loglevel, 1, "0", 0},
        { "pata_ro", parm_pata_ro, 1, "0", 0},
        { "ahci_ro", parm_ahci_ro, 1, "0", 0},
        { "sched_ipi", parm_sched_ipi, 1, "1", 1},
        { "irq_log", parm_irq_log, 1, "0", 0},
        { "vga", parm_vga, 1, "0", 0},
        { "net_loglevel", parm_net_loglevel, 1, "0", 0},
        { "irq_watch", parm_irq_watch, 6, "0", 0},
        { "eth_loglevel", parm_eth_loglevel, 1, "0", 0},
        { "tcp_disable_cc", parm_tcp_disable_cc, 1, "0", 0},
        { "use_vbox_port", parm_use_vbox_port, 1, "0", 0 },
        { "use_bios_font", parm_use_bios_font, 1, "0", 0 },
        { "use_acpi", parm_use_acpi, 1, "1", 1 },
        { "use_msi", parm_use_msi, 1, "1", 1 },
};

#define NR_KPARM (sizeof(kparm) / sizeof(kparm_t))

/*
 * Parse command line and set up default values
 */
void params_parse() {
    char* token;
    char* ptr;
    int i;
    char* endptr;
    /*
     * Get command line from the multiboot module
     * and create a local copy
     */
    strncpy(cmd_line, multiboot_get_cmdline(), MULTIBOOT_MAX_CMD_LINE - 1);
    /*
     * First set up all default values
     */
    for (i = 0; i < NR_KPARM; i++) {
        strncpy(kparm[i].value, kparm[i].default_string, kparm[i].length);
        (kparm[i].value)[kparm[i].length] = 0;
    }
    /*
     * Now parse command line
     */
    token = strtok(cmd_line, " ");
    while (token) {
        ptr = token;
        /*
         * Advance until we hit upon an equality sign
         * or the end of the token
         */
        while ((*ptr != '=') && (*ptr != 0))
            ptr++;
        if ((*ptr == '=') && (ptr > token) && (*(ptr + 1))) {
            /*
             * Create a temporary string in the command line
             * containing only the key by replacing = with 0.
             * We need to revert this later on in order not to
             * confuse strtok
             */
            *ptr = 0;
            /*
             * Scan table of existing parameters until we find the current
             * key
             */
            for (i = 0; i < NR_KPARM; i++)
                if (0 == strcmp(kparm[i].name, token)) {
                    /*
                     * Copy the value from the command line into the parameter table
                     */
                    strncpy(kparm[i].value, ptr + 1, kparm[i].length);
                    kparm[i].int_value = strtol(ptr + 1, &endptr, 10);
                    /*
                     * For the special case of loglevel, set up global loglevel
                     */
                    if (0==strncmp(kparm[i].name, "loglevel", 8))
                        __loglevel = kparm[i].int_value;
                }
            *ptr = '=';
        }
        token = strtok(0, " ");
    }
}

/*
 * Get the value of a parameter
 * Parameters:
 * @name - name of parameter
 * Return value:
 * value of parameter as string or 0 if the
 * parameter could not be found
 */
char* params_get(char* name) {
    int i;
    for (i = 0; i < NR_KPARM; i++)
        if (0 == strcmp(kparm[i].name, name)) {
            return kparm[i].value;
        }
    return 0;
}

/*
 * Get the integer value of a parameter
 * Parameters:
 * @name - the name of the parameter
 * Return value:
 * the integer value of the parameter
 * -1 if the parameter could not be found
 */
u32 params_get_int(char* name) {
    int i;
    for (i = 0; i < NR_KPARM; i++)
        if (strcmp(kparm[i].name, name) == 0)
            return kparm[i].int_value;
    return 0;
}
