/*
 * cpu.c
 *
 * This module contains functions to detect and enumerate CPUs. Information is collected via three different channels.
 *
 * a) At boot time, cpu_init() is called by the startup sequence in main.c on the BSP. This function will collect some
 * information on the bootstrap CPU.
 * b) Then the interrupt manager scans the BIOS tables to detect additional CPUs. For each CPU found, it calls cpu_add to
 * add this CPU to the list of known CPUs
 * c) Finally when an AP comes up, it will call cpu_up which will mark the CPU as running and collect and store some additional data
 *
 * CPUs are identified using a logical ID which is a sequence starting at 0 for the bootstrap CPU. APs are numbered according to
 * the order in which cpu_add is called. Using cpu_get_apic_id() and cpu_is_ap(), other parts of the kernel can convert between
 * the logical identifier and the local APIC id
 */

#include "cpu.h"
#include "lists.h"
#include "mm.h"
#include "debug.h"
#include "locks.h"
#include "util.h"
#include "lib/string.h"

extern void (*debug_getline)(char* line, int max);

static char* __module = "CPU   ";

/*
 * A list of known CPUs
 */
static cpu_t* cpu_list_head = 0;
static cpu_t* cpu_list_tail = 0;
spinlock_t cpu_list_lock;


/*
 * A CPU information structure for the BSP. This entry is filled by cpu_init() and is always available, even if the
 * MP table structure is not present and thus cpu_add is never called
 */
static cpuinfo_t bsp_info;

/*
 * The local APIC id of the BSP
 */
static int bsp_apic_id = -1;


/****************************************************************************************
 * The following functions use the CPUID instruction and reads from MSRs to detect      *
 * available CPU features                                                               *
 ***************************************************************************************/

/*
 * Check whether CPUID is supported
 * Return value:
 * 0 if CPUID is not supported
 * 1 if CPUID is supported
 */
static int cpuid_supported() {
    u32 eflags;
    u32 check;
    u32 id;
    /*
     * CPUID is supported if  bit 21
     * in EFLAGS (ID) can be modified
     */
    save_eflags(&eflags);
    /*
     * Flip ID bit
     */
    id = eflags & (1 << 21);
    if (id)
        id = 0;
    else
        id = (1 << 21);
    eflags = (eflags & ~(1 << 21)) + id;
    check = eflags;
    /*
     * Write and read again
     */
    restore_eflags(&eflags);
    save_eflags(&check);
    if (check != eflags) {
        return 0;
    }
    return 1;
}

/*
 * Use CPUID function 0 to get the vendor string and the maximum
 * function number for basic CPUID information
 * Parameter:
 * @vendor_string - pointer to a 12 character string into which
 * we put the vendor string
 * Return value:
 * maximum function number below 0x80000000
 */
static u32 cpuid_vendor_string(char* vendor_string) {
    u32 ebx;
    u32 edx;
    u32 ecx;
    u32 rc;
    rc = cpuid(CPUID_FUN_VENDOR_STRING, &ebx, &ecx, &edx);
    /*
     * We now have the vendor string in EBX, EDX and ECX
     */
    memcpy(vendor_string, &ebx, 4);
    memcpy(vendor_string + 4, &edx, 4);
    memcpy(vendor_string + 8, &ecx, 4);
    vendor_string[12]=0;
    return rc;
}

/*
 * Get brand string
 * Parameter:
 * @brand_string - buffer in which the brand string (48 bit) and a trailing
 * zero is stored
 * Return value:
 * 1 - brand string supported
 * 0 - brand string not supported
 */
static int cpuid_brand_string(char* brand_string) {
    u32 rc;
    u32 buffer;
    /*
     * Determine whether extended functions are supported
     */
    rc = cpuid(CPUID_FUN_EXT_FEATURES, &buffer, &buffer, &buffer);
    if (rc > CPUID_FUN_EXT_FEATURES + 3) {
        /*
         * Brand string supported. Execute brand string functions
         */
        *((u32*)(brand_string)) = cpuid(CPUID_FUN_EXT_FEATURES + 2, (u32*) (brand_string+4),
                (u32*)(brand_string + 8), (u32*)(brand_string + 12));
        *((u32*)(brand_string + 16)) = cpuid(CPUID_FUN_EXT_FEATURES + 3, (u32*) (brand_string + 20),
                (u32*)(brand_string + 24), (u32*)(brand_string + 28));
        *((u32*)(brand_string + 32)) = cpuid(CPUID_FUN_EXT_FEATURES + 4, (u32*) (brand_string + 36),
                (u32*)(brand_string + 40), (u32*)(brand_string + 44));
        return 1;
    }
    return 0;
}

/*
 * Use the CPUID instruction to get some basic data about
 * the CPU on which we are running
 * Parameter:
 * @cpuinfo - CPU information structure to be filled
 */
static void run_cpuid(cpuinfo_t* cpuinfo) {
    u32 max_fun;
    u32 ebx;
    u32 ecx;
    u32 edx;
    if (!(cpuid_supported())) {
        ERROR("CPUID not supported\n");
        return;
    }
    /*
     * Get vendor string and maximum supported function
     */
    max_fun = cpuid_vendor_string(cpuinfo->vendor_string);
    /*
     * Set vendor ID
     */
    if (0 == strcmp(cpuinfo->vendor_string, "GenuineIntel")) {
        cpuinfo->vendor = CPU_VENDOR_INTEL;
    }
    else if (0 == strcmp(cpuinfo->vendor_string, "AuthenticAMD")) {
        cpuinfo->vendor = CPU_VENDOR_AMD;
    }
    else {
        cpuinfo->vendor = CPU_VENDOR_UNKNOWN;
    }
    /*
     * If supported, get features and signature
     */
    if (max_fun >= CPUID_FUN_FEATURES) {
        cpuinfo->signature = cpuid(CPUID_FUN_FEATURES, &ebx, &ecx, &edx);
        cpuinfo->features = ecx;
        cpuinfo->features = cpuinfo->features << 32;
        cpuinfo->features += ((unsigned long long) (edx));
        cpuinfo->stepping = (cpuinfo->signature & 0xf);
        cpuinfo->model = (cpuinfo->signature >> 4) & 0xf;
        cpuinfo->family = (cpuinfo->signature >> 8) & 0xf;
        cpuinfo->ext_model = (cpuinfo->signature >> 16) & 0xf;
        cpuinfo->ext_family = (cpuinfo->signature >> 20) & 0xff;
    }
    else {
        ERROR("Could not read CPU signature\n");
    }
    /*
     * Try to get brand string
     */
    cpuid_brand_string(cpuinfo->brand_string);
}

/*
 * This function will detect thermal management features on Intel CPUs and issue
 * a warning if the TCC is present, but not enabled
 * Parameter:
 * @cpuinfo - the CPU information structure with filled feature flags and vendor ID
 */
static void check_thermal_management(cpuinfo_t* cpuinfo) {
    u32 low;
    u32 high;
    /*
     * If TM1 is supported, check whether it is enabled
     */
    if (cpuinfo->features & CPUID_FEATURE_TM) {
        cpuinfo->tm1_present = 1;
        /*
         * Read IA32_MISC_ENABLE
         */
        if (0 == (cpuinfo->features & CPUID_FEATURE_MSR)) {
            ERROR("Strange, we have a CPU with TCC but MSR not supported\n");
            return;
        }
        rdmsr(IA32_MISC_ENABLE, &low, &high);
        if (low & IA32_MISC_ENABLE_TM1) {
            cpuinfo->tm1_enabled = 1;
        }
    }
    /*
     * The same for TM2
     */
    if (cpuinfo->features & CPUID_FEATURE_TM2) {
        cpuinfo->tm2_present = 1;
        /*
         * Note: we ignore the special case of certain Pentium M CPUs mentioned in the
         * Intel manuals here as we are not able to test this anyway
         */
        if (0 == (cpuinfo->features & CPUID_FEATURE_MSR)) {
            ERROR("Strange, we have a CPU with TCC but MSR not supported\n");
            return;
        }
        rdmsr(IA32_MISC_ENABLE, &low, &high);
        if (low & IA32_MISC_ENABLE_TM2) {
            cpuinfo->tm2_enabled = 1;
        }
    }
    /*
     * Print a warning if TCC is present but neither TM1 nor TM2 are enabled
     */
    if ((cpuinfo->tm1_present || cpuinfo->tm2_present) &&
            ((0 == cpuinfo->tm1_enabled) && (0 == cpuinfo->tm2_enabled))) {
        MSG("Warning: TCC present but neither TM1 nor TM2 enabled\n");
    }
}

/*
 * Fill a CPU info structure for the CPU which is currently running
 * Parameter:
 * @cpuinfo - the CPU information structure to be filled
 */
static void get_cpuinfo(cpuinfo_t* cpuinfo) {
    memset((void*) cpuinfo, 0, sizeof(cpuinfo_t));
    /*
     * Use CPUID to get some basic information on CPU
     */
    run_cpuid(cpuinfo);
    /*
     * Some Intel specific checks
     */
    if (CPU_VENDOR_INTEL == cpuinfo->vendor) {
        check_thermal_management(cpuinfo);
    }
}

/****************************************************************************************
 * The following functions are used at boot time to built up a list of CPUs and to      *
 * collect some basic information on the CPUs                                           *
 ***************************************************************************************/

/*
 * Add a CPU to our internal list
 * Parameter:
 * @lapic - ID of local APIC
 * @bsp - BSP flag
 * @apic_ver - content of local APIC version register as stored in MP table
 */
void cpu_add(u8 lapic, int bsp, u32 apic_ver) {
    cpu_t* cpu = 0;
    u32 eflags;
    if (0 == (cpu = (cpu_t*) kmalloc(sizeof(cpu_t)))) {
        ERROR("Could not allocate memory for CPU entry\n");
        return;
    }
    spinlock_get(&cpu_list_lock, &eflags);
    LIST_ADD_END(cpu_list_head, cpu_list_tail, cpu);
    cpu->lapic_id = lapic;
    cpu->bsp = bsp;
    cpu->apic_ver = apic_ver;
    if (bsp) {
        bsp_apic_id = lapic;
        cpu->status = CPU_STATUS_UP;
        cpu->cpuinfo = &bsp_info;
    }
    else {
        /*
         * cpuinfo structure is filled in cpu_up
         */
        cpu->status = CPU_STATUS_IDENTIFIED;
        cpu->cpuinfo = 0;
    }
    spinlock_release(&cpu_list_lock, &eflags);
}

/*
 * Validate whether a CPU is supported by ctOS. Currently we only check for
 * FXSAVE / FXRESTOR
 */
static void validate_cpu(cpuinfo_t* cpuinfo) {
    /*
     * Check for FXSAVE / FXRESTOR
     */
    if (0 == (cpuinfo->features & CPUID_FEATURE_FXSAVE)) {
        PANIC("This CPU does not support FXSAVE and FXRESTOR\n");
    }
}

/*
 * Mark a CPU as being up and running and add some more status information to the entry. This function is
 * called once for each AP in smp_ap_main(). Never call this for the BSP!
 */
void cpu_up(u8 lapic_id) {
    cpu_t* cpu;
    u32 eflags;
    spinlock_get(&cpu_list_lock, &eflags);
    LIST_FOREACH(cpu_list_head, cpu) {
        if (lapic_id == cpu->lapic_id) {
            cpu->status = CPU_STATUS_UP;
            cpu->cpuinfo = (cpuinfo_t*) kmalloc(sizeof(cpuinfo_t));
            KASSERT(cpu->cpuinfo);
            get_cpuinfo(cpu->cpuinfo);
            validate_cpu(cpu->cpuinfo);
        }
    }
    spinlock_release(&cpu_list_lock, &eflags);
}

/*
 * Initialize this module
 */
void cpu_init() {
    spinlock_init(&cpu_list_lock);
    /*
     * Fill CPU structure for BSP
     */
    get_cpuinfo(&bsp_info);
    /*
     * and validate BSP
     */
    validate_cpu(&bsp_info);
}

/****************************************************************************************
 * These functions can be used by other parts of the kernel to inquire the existing     *
 * CPUs                                                                                 *
 ***************************************************************************************/

/*
 * Check whether the provided local APIC id is the APIC ID
 * of a detected AP. If yes, return the CPU number starting
 * with 1, ordered by occurrence in the ACPI / MP table
 * No locks are acquired as we assume that the list does not
 * change after boot time
 * Parameter:
 * @apic_id - apic ID to be validated
 */
int cpu_is_ap(u8 apic_id) {
    int count = 0;
    cpu_t* cpu;
    LIST_FOREACH(cpu_list_head, cpu) {
        if (0 == cpu->bsp)
            count++;
        if ((0 == cpu->bsp) && (apic_id == cpu->lapic_id)) {
            return count;
        }
    }
    return 0;
}

/*
 * Check whether a CPU is the BSP
 */
int cpu_is_bsp(u8 apic_id) {
    if (-1 == bsp_apic_id)
        return 1;
    return (apic_id == bsp_apic_id);
}

/*
 * Return number of available CPUs in the system
 */
int cpu_get_cpu_count() {
    int count = 0;
    cpu_t* cpu;
    /*
     * Assume 1 if we are not yet fully initialized
     */
    if (-1 == bsp_apic_id)
        return 1;
    LIST_FOREACH(cpu_list_head, cpu) {
        count++;
    }
    return count;
}

/*
 * Return the local APIC id of a CPU
 * Parameter:
 * @run_cpuid - the ID of the CPU, 0 = BSP
 * Return value:
 * the local APIC id
 * -1 if there is no such CPU
 */
int cpu_get_apic_id(int cpuid) {
    int rc = -1;
    cpu_t* cpu;
    int count = 0;
    if (0 == cpuid)
        return bsp_apic_id;
    LIST_FOREACH(cpu_list_head, cpu) {
        if (cpu->bsp)
            continue;
        count++;
        if (cpuid == count)
            rc = cpu->lapic_id;
    }
    return rc;
}

/*
 * Determine whether the local APIC of this CPU is an
 * external APIC as used for the 486DX
 * Only call this for an AP
 * Parameter:
 * @cpuid - the logical CPU id
 * Return value:
 * the local APIC version or -1 if the CPU could not be found
 */
int cpu_external_apic(int cpuid) {
    int rc = -1;
    int lapic_version;
    cpu_t* cpu;
    int count = 0;
    LIST_FOREACH(cpu_list_head, cpu) {
        if (cpuid == count) {
            lapic_version = cpu->apic_ver & 0xff;
            if ((lapic_version & 0xf0) == 0x10) {
                rc = 0;
            }
            else if ((lapic_version & 0xf0) == 0x0) {
                rc = 1;
            }
            else
                ERROR("Could not determine type of local APIC, version register is %x\n", lapic_version);
            break;
        }
        count++;
    }
    return rc;
}

/*
 * See whether we support a specific feature
 * Parameter:
 * @cpuid - logical ID of CPU
 * @feature - 64 bit bitmask of feature
 * Return value:
 * 1 if feature is supported
 * 0 otherwise
 */
int cpu_has_feature(int cpuid, unsigned long long feature) {
    unsigned long long cpu_features = 0;
    cpu_t* cpu;
    int count = 0;
    if (0 == cpuid) {
        cpu_features = bsp_info.features;
    }
    else {
        count = 0;
        LIST_FOREACH(cpu_list_head, cpu) {
            if (cpuid == count) {
                cpu_features = cpu->cpuinfo->features;
                break;
            }
            count++;
        }
    }
    if (cpu_features & feature)
        return 1;
    return 0;
}

/*
 * Return the brand string of the BSP
 */
char* cpu_get_brand_string() {
    return bsp_info.brand_string;
}

/********************************************************
 * Everything below this line is for debugging and      *
 * testing only                                         *
 *******************************************************/

/*
 * Print details about a CPU
 */
static void cpu_print(cpu_t* cpu) {
    PRINT("Vendor string:                %s\n", cpu->cpuinfo->vendor_string);
    PRINT("Signature:                    %x\n", cpu->cpuinfo->signature);
    PRINT("Brand string:                 %s\n", cpu->cpuinfo->brand_string);
    PRINT("Features (EDX):               %x\n", (u32)(cpu->cpuinfo->features & 0xFFFFFFFF));
    PRINT("Features (ECX):               %x\n", (u32)(cpu->cpuinfo->features >> 32));
    PRINT("TSC:                          %d\n", (cpu->cpuinfo->features & CPUID_FEATURE_TSC) ? 1 : 0);
    PRINT("MSR:                          %d\n", (cpu->cpuinfo->features & CPUID_FEATURE_MSR) ? 1 : 0);
    PRINT("ACPI:                         %d\n", (cpu->cpuinfo->features & CPUID_FEATURE_ACPI) ? 1 : 0);
    PRINT("TCC:                          %d\n", (cpu->cpuinfo->features & CPUID_FEATURE_TM) ? 1 : 0);
    PRINT("Family / model:               %h / %h\n", cpu->cpuinfo->family, cpu->cpuinfo->model);
    PRINT("Ext Family / Ext Model:       %h / %h\n", cpu->cpuinfo->ext_family, cpu->cpuinfo->ext_model);
    PRINT("Thermal management 1 present: %d\n", cpu->cpuinfo->tm1_present);
    PRINT("Thermal management 1 enabled: %d\n", cpu->cpuinfo->tm1_enabled);
    PRINT("Thermal management 2 present: %d\n", cpu->cpuinfo->tm2_present);
    PRINT("Thermal management 2 enabled: %d\n", cpu->cpuinfo->tm2_enabled);
}

/*
 * Print a list of all CPUs found
 */
void cpu_print_list() {
    cpu_t* cpu;
    int cont = 1;
    unsigned char c[2];
    int nr = 0;
    while (cont) {
        cont = 0;
        nr = 0;
        PRINT("NR  LAPIC ID   BSP  Status\n");
        PRINT("-----------------------------------------\n");
        LIST_FOREACH(cpu_list_head, cpu) {
            nr++;
            PRINT("%d   %x  %d    ", nr, cpu->lapic_id, cpu->bsp);
            switch(cpu->status) {
                case CPU_STATUS_IDENTIFIED:
                    PRINT("IDENTIFIED\n");
                    break;
                case CPU_STATUS_UP:
                    PRINT("UP        \n");
                    break;
                default:
                    PRINT("UNKNOWN   \n");
                    break;
            }
        }
        PRINT("Hit a number to print details about that CPU or q to return to prompt\n");
        debug_getline((void*) c, 1);
        if ((c[0] > '0') && (c[0] <= '0' + nr)) {
            cont = 1;
            nr = 0;
            LIST_FOREACH(cpu_list_head, cpu) {
                nr++;
                if (nr == (c[0] - '0')) {
                    PRINT("\nCPU details for CPU %d:\n", nr);
                    PRINT("----------------------------\n");
                    cpu_print(cpu);
                    PRINT("Hit ENTER to continue\n");
                    debug_getline((void*) c, 1);
                    break;
                }
            }
        }
    }
}

