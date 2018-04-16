/*
 * cpu.h
 *
 */

#ifndef _CPU_H_
#define _CPU_H_

#include "ktypes.h"

/*
 * This structure contains some basic information on a CPU
 */
typedef struct _cpuinfo_t {
    int vendor;                    // the vendor
    char vendor_string[13];        // vendor string
    char brand_string[49];         // brand string
    u32 signature;                 // Signature as returned by CPUID.EAX = 1
    u8 stepping;                   // Stepping (bits 0 - 3 of signature)
    u8 model;                      // Model number (bits 4 - 7 of signature)
    u8 family;                     // Family (bits 8 - 11 of signature)
    u8 ext_model;                  // Extended model (bits 16 - 19 of signature)
    u8 ext_family;                 // Extended family (bits 20 - 27 of signature)
    unsigned long long features;   // feature flags as returned by CPUID.EAX = 1 (bits 0 - 31 = EDX, bits 32 - 63 = ECX)
    int tm1_enabled;               // Thermal management 1 enabled
    int tm1_present;               // TM 1 supported
    int tm2_enabled;               // Thermal management 2 enabled
    int tm2_present;               // TM 2 supported
} cpuinfo_t;

/*
 * An entry in our internal table of CPUs
 */
typedef struct _cpu_t {
    char lapic_id;
    char bsp;
    int apic_ver;
    int status;
    cpuinfo_t* cpuinfo;
    struct _cpu_t* next;
    struct _cpu_t* prev;
} cpu_t;

/*
 * CPU status
 */
#define CPU_STATUS_IDENTIFIED 0
#define CPU_STATUS_UP 1

/*
 * Some MSRs
 */
#define IA32_MISC_ENABLE 0x1A0


/*
 * MSR bits
 */
#define IA32_MISC_ENABLE_TM1 (1 << 3)
#define IA32_MISC_ENABLE_TM2 (1 << 13)

/*
 * Vendors
 */
#define CPU_VENDOR_UNKNOWN 0x0
#define CPU_VENDOR_INTEL 0x1
#define CPU_VENDOR_AMD 0x2

/*
 * CPUID functions
 */
#define CPUID_FUN_VENDOR_STRING 0x0
#define CPUID_FUN_FEATURES 0x1
#define CPUID_FUN_EXT_FEATURES 0x80000000

/*
 * CPUID Feature flags - valid for AMD and Intel
 * Note that we store features as a 64 bit integer, where
 * bits 0 - 31 are the feature flags returned in EDX, whereas
 * bits 32 - 63 are used to store the feature flags returned in
 * ECX by CPUID.EAX=1.
 */
#define CPUID_FEATURE_TSC (1 << 4)
#define CPUID_FEATURE_MSR (1 << 5)
#define CPUID_FEATURE_FXSAVE (1 << 24)
#define CPUID_FEATURE_SSE (1 << 25)
/*
 * CPUID Feature flags - Intel specific
 */
#define CPUID_FEATURE_ACPI (1 << 22)
#define CPUID_FEATURE_TM (1 << 29)
#define CPUID_FEATURE_TM2 (1LL << 40)

void cpu_init();
void cpu_add(u8 apic_id, int bsp, u32 apic_ver);
int cpu_is_ap(u8 apic_id);
int cpu_is_bsp(u8 apic_id);
void cpu_print_list();
void cpu_up(u8 lapic_id);
int cpu_get_apic_id(int cpuid);
int cpu_external_apic(int cpuid);
int cpu_get_cpu_count();
int cpu_has_feature(int cpuid, unsigned long long feature);
char* cpu_get_brand_string();

#endif /* _CPU_H_ */
