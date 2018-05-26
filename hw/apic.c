/*
 * apic.c
 *
 * This module contains low-level functionality to deal with I/O APICs and local APICs.
 *
 * The I/O APIC can be set up in one of three modes.
 *
 * Mode 1: physical / fixed delivery mode to the BSP. This is the basic mode which will
 * route all interrupts to the BSP using physical destination mode.
 *
 * Mode 2: logical / fixed delivery mode. In this mode, each interrupt will be routed to
 * a dedicated CPU. The assignment of interrupts to CPUs is fixed and determined at boot time
 *
 * Mode 3: logical / lowest priority. In this mode, each interrupt will be routed dynamically to the CPU which
 * currently operates with lowest priority, i.e. for which the TPR register in the local APIC has the
 * smallest value.
 */

#include "apic.h"
#include "debug.h"
#include "mm.h"
#include "smp.h"
#include "timer.h"
#include "cpu.h"
#include "lib/limits.h"


static char* __module ="APIC  ";

/*
 * Start of the local APICs register area in
 * virtual memory. This is set once apic_init_bsp is called
 * for the first time
 */
static u32 local_apic_base = 0;

/****************************************************************************************
 * Basic functions to read from and write to an APIC register                           *
 ***************************************************************************************/

/*
 * Read from a register of the local APIC.
 * Parameters:
 * @offset - the offset of the register into the local APICs configuration space
 * Return value:
 * the result of the read
 */
static u32 lapic_read(u32 offset) {
    u32* reg;
    KASSERT(local_apic_base);
    reg = (u32*)(local_apic_base + offset);
    return *reg;
}

/*
 * Write to a register of the local APIC
 * Parameters:
 * @offset - the offset of the register within the local APIC configuration space
 * @value - the value to be written
 */
static void lapic_write(u32 offset, u32 value) {
    u32* reg;
    KASSERT(local_apic_base);
    reg = (u32*)(local_apic_base + offset);
    *reg = value;
}

/****************************************************************************************
 * The following functions initialize the local APIC of the BSP and of an AP            *
 ***************************************************************************************/

/*
 * Basic initialization of a local APIC. This contains all the basic setup
 * required for both BSP and AP, i.e. setting the TSR to zero and set
 * enable flag in the spurious interrupt register. The DFR register is set to
 * flat mode
 */
static void apic_init_local() {
    u32 tmp;
    /*
     * Set task priority register (TPR) to zero
     */
    lapic_write(LOCAL_APIC_TPR_REG,0x0);
    /*
     * Put logical APIC ID into bits 24 - 31 of
     * the logical destination register (LDR). As
     * logical APIC id, we use 1 << CPUID, i.e. 0x1
     * for the BSP, 0x2 for the first AP, 0x4 for the second
     * AP and so forth
     */
    tmp = (1 << smp_get_cpu());
    tmp = tmp << 24;
    lapic_write(LOCAL_APIC_LDR_REG, tmp);
    /*
     * Set up local APIC for flat model, i.e. put 0xFFFFFFFF
     * into destination format register (DFR))
     */
    lapic_write(LOCAL_APIC_LDF_REG, 0xffffffff);
    /*
     * Finally set enable bit in spurious interrupt register
     * to software-enable APIC
     */
     tmp = lapic_read(LOCAL_APIC_SPURIOUS_REG);
     tmp = tmp | (1 << 8);
     lapic_write(LOCAL_APIC_SPURIOUS_REG, tmp);
}

/*
 * Initialize the local APIC. This function will
 * map the memory mapped I/O registers of the local APIC
 * into virtual memory
 * Parameters:
 * @phys_base - the physical base address of the I/O area
 */
void apic_init_bsp(u32 phys_base) {
    /*
     * If we have already been called, ignore this 
     * call
     */
    if (local_apic_base)
        return;
    /*
     * The last register is the divide configuration register
     */
    local_apic_base = mm_map_memio(phys_base, LOCAL_APIC_DCR_REG+sizeof(u32));
    KASSERT(local_apic_base);
    /*
     * do basic setup
     */
    apic_init_local();
}

/*
 * Set up the timer within a local APIC. Here we assume that the PIT is already running
 * and delivering ticks to the BSP. We use these ticks to calibrate the timer of the local
 * APIC
 * Parameter:
 * @vector - the interrupt vector used by the timer on the BSP
 */
void apic_init_timer(int vector) {
    u32 timer_lvt;
    u32 apic_ticks;
    u32 apic_ticks_per_second;
    /*
     * Set up the local APIC timer. First we set up the
     * the LVT for one-shot mode with interrupts masked
     */
    timer_lvt = APIC_LVT_DELIVERY_MODE_FIXED + APIC_LVT_MASK + APIC_LVT_TIMER_MODE_ONE_SHOT + APIC_LVT_VECTOR*vector;
    lapic_write(LOCAL_APIC_TIMER_LVT_REG, timer_lvt);
    /*
     * Now set the DCR to 128
     */
    lapic_write(LOCAL_APIC_DCR_REG, 0xa);
    /*
     * Set up initial count register
     */
    lapic_write(LOCAL_APIC_INIT_COUNT_REG, UINT_MAX);
    /*
     * Wait for APIC_CALIBRATE_TICKS global ticks
     */
    timer_wait_ticks(APIC_CALIBRATE_TICKS);
    apic_ticks = lapic_read(LOCAL_APIC_CURRENT_COUNT_REG);
    apic_ticks = ~apic_ticks;
    apic_ticks_per_second = apic_ticks * HZ / APIC_CALIBRATE_TICKS;
    MSG("Completed calibration for CPU %d, measured CPU bus clock: %d MHz\n", smp_get_cpu(), (apic_ticks_per_second)/(1000000/128));
    /*
     * Set up timer in periodic mode with interrupts enabled
     */
    timer_lvt = APIC_LVT_DELIVERY_MODE_FIXED +  APIC_LVT_TIMER_MODE_PERIODIC + APIC_LVT_VECTOR*vector;
    lapic_write(LOCAL_APIC_TIMER_LVT_REG, timer_lvt);
    /*
     * and adapt initial counter register so that one tick of the local clock is one tick of the
     * global clock
     */
    lapic_write(LOCAL_APIC_INIT_COUNT_REG, apic_ticks / APIC_CALIBRATE_TICKS);
}

/*
 * Initialize the local APIC of an AP. This assumes that the AP thread
 * has already joined the common kernel memory and has therefore access
 * to the virtual address mapping set up by a previous call to apic_init_bsp
 * on the BSP
 */
void apic_init_ap() {
    /*
     * Verify that the local APIC has the expected state
     * after the startup, i.e.
     * - software disabled
     * - TPR 0
     * - LDF 0xFFFFFFFF
     * - LDR 0x0
     */
    if (lapic_read(LOCAL_APIC_SPURIOUS_REG) & (1 << 8))
        ERROR("LAPIC is already software enabled\n");
    if (lapic_read(LOCAL_APIC_TPR_REG))
        ERROR("TPR register is different from zero\n");
    if (0xFFFFFFFF != lapic_read(LOCAL_APIC_LDF_REG))
        ERROR("LDF register (%x) is not as expected\n", lapic_read(LOCAL_APIC_LDF_REG));
    if (lapic_read(LOCAL_APIC_LDR_REG))
            ERROR("LDR register (%x) is different from zero\n", lapic_read(LOCAL_APIC_LDR_REG));
    /*
     * do basic setup
     */
    apic_init_local();
}

/****************************************************************************************
 * Support for identifying the CPU on which we are running by reading the APIC ID       *
 ***************************************************************************************/

/*
 * Read the local APIC id. Only use this if paging has
 * already been enabled!
 */
u8 apic_get_id() {
    u32 reg_value;
    reg_value = lapic_read(LOCAL_APIC_ID_REG);
    return reg_value >> 24;
}

/****************************************************************************************
 * The following functions support interrupt handling, i.e. delivering and              *
 * acknowledging interrupts                                                             *
 ***************************************************************************************/

/*
 * Acknowledge an interrupt received via the local APIC
 * by writing 0 to its EOI register
 */
void apic_eoi() {
    lapic_write(LOCAL_APIC_EOI, 0);
}

/*
 * Send an IPI to a specific CPU
 * Parameter:
 * @apic_id - ID of local APIC to which IPI should be sent
 * @ipi - IPI to be sent, i.e. the 3-bit delivery mode
 * @vector - vector number
 * @deassert - distinguish between assert and de-assert for INIT IPI
 * Return value:
 * 0 upon success
 */
int apic_send_ipi(u8 apic_id, u8 ipi, u8 vector, int deassert) {
    u32 icr_higher_dword = 0;
    u32 icr_lower_dword = 0;
    int timeout = 0;
    /*
     * First assemble higher dword of ICR
     */
    icr_higher_dword = apic_id;
    icr_higher_dword = icr_higher_dword << 24;
    /*
     * Write this to APIC
     */
    lapic_write(LOCAL_APIC_ICR_HIGH_REG, icr_higher_dword);
    /*
     * Assemble lower dword. We use
     * shorthand = 0 (bits 18 - 19)
     * trigger mode = 0 (bit 15)
     * level = 1 (bit 14)
     * destination mode = 0 (bit 11)
     * delivery mode = ipi (bits 8 - 10)
     * vector number = vector
     */
    icr_lower_dword = (1 << 14) + ((ipi & 0x7) << 8) + vector;
    if (IPI_INIT == ipi) {
        /*
         * Set trigger mode to 1
         */
        icr_lower_dword |= (1 << 15);
        if (deassert) {
            /*
             * Set level to 0 to issue an INIT level
             * de-assert IPI
             */
            icr_lower_dword &= ~(1 << 14);
    }
    }
    /*
     * Write to APIC
     */
    lapic_write(LOCAL_APIC_ICR_LOW_REG, icr_lower_dword);
    /*
     * and spin around pending bit
     */
    while (1) {
        timeout++;
        if (0 == (lapic_read(LOCAL_APIC_ICR_LOW_REG) & (1 << 12)))
            break;
        if (timeout > 1000) {
            return 1;
        }
    }
    return 0;
}

/*
 * Send an IPI to all CPUs except the CPU on which the code is running
 * Parameter:
 * @ipi - IPI to be sent, i.e. the 3-bit delivery mode
 * @vector - vector number
 * Return value:
 * 0 upon success
 */
int apic_send_ipi_others(u8 ipi, u8 vector) {
    u8 self;
    int cpu;
    int apic_id;
    int rc = 0;
    /*
     * First get own APIC id
     */
    self = apic_get_id();
    /*
     * Walk all other CPUs
     */
    for (cpu = 0; cpu < SMP_MAX_CPU; cpu++) {
        if (-1 != (apic_id = cpu_get_apic_id(cpu))) {
            if (apic_id != self) {
                if ((rc = apic_send_ipi(apic_id, ipi, vector, 0)))
                    return rc;
            }
        }
    }
    return 0;
}

/****************************************************************************************
 * Support for the I/O APIC                                                             *
 ***************************************************************************************/

/*
 * Write to a register of the I/O APIC
 * Parameter:
 * @io_apic - a pointer to the I/O APIC structure
 * @index - the offset of the register
 * @value - the value to be written
 * Note: we do not lock the I/O APIC here, so this function only should be
 * used at boot time when no other thread concurrently accesses the I/O APIC
 */
static void io_apic_write(io_apic_t* io_apic, u32 index, u32 value) {
    void* io_apic_base = (u32*) io_apic->base_address;
    u32* index_register = (u32*) (io_apic_base + IO_APIC_IND);
    u32* data_register = (u32*) (io_apic_base + IO_APIC_DATA);
    /*
     * Write index into index register
     */
    *index_register = index;
    /*
     * and data into data register
     */
    *data_register = value;
}

/*
 * Program a redirection entry in the I/O APIC
 * Parameters:
 * @io_apic - I/O APIC to be programmed
 * @irq - input line of I/O APIC
 * @polarity - polarity
 * @trigger - trigger mode, i.e. edge (0) or level triggered (1)
 * @vector - IDT offset to use for this interrupt
 * @apic_mode - APIC mode used for this vector, see comments at the header of this file
 */
void apic_add_redir_entry(io_apic_t* io_apic, int irq, int polarity,
        int trigger, int vector, int apic_mode) {
    u32 redir_entry_high = 0;
    u32 redir_entry_low = 0;
    /*
     * Get local APIC ID of BSP
     */
    int dest_id = cpu_get_apic_id(0);
    u32 nr_of_cpus = cpu_get_cpu_count();
    /*
     * First write into lowest dword to disable a potentially existing entry
     */
    redir_entry_low = 1 << 16;
    io_apic_write(io_apic, APIC_IND_REDIR + 2*irq, redir_entry_low);
    io_apic_write(io_apic, APIC_IND_REDIR + 2*irq + 1, 0);
    /*
     * Assemble new lower and higher dword of 64 bit redirection entry
     * We start with higher double word (bits 32-63 of entry)
     * Physical delivery mode:
     * Bit 27-24 is 4-bit target APIC id, bit 28-31 is 0
     * Lowest priority delivery mode:
     * Set bits 24 - 31 to 1
     * Logical delivery mode targeted at BSP:
     * set bits 24 - 31 to 0x1
     * The other bits in the higher dword are not relevant for us
     */
    switch (apic_mode) {
        case 1:
            /*
             * Use 4 bit APIC ID of BSP as destination ID
             */
            redir_entry_high = (dest_id & 0xf) << 24;
            break;
        case 2:
            /*
             * Use logical APIC ID (matching the value of the LDR
             * register in the local APIC) of CPU (vector mod nr_of_cpus)
             */
            redir_entry_high = ( 1 << (vector % nr_of_cpus)) << 24;
            break;
        case 3:
            /*
             * Use bitmask matching all existing CPUs as MDA for lowest
             * priority delivery mode. Theoretically, we could always use 0xFF here
             * to address all CPUs. This works on QEMU, however on my PC (Core i7 with
             * X58 chipset), no interrupts are received at all if we do this.
             */
            redir_entry_high = ((1 << nr_of_cpus) - 1) << 24;
           break;
        default:
            PANIC("Invalid apic mode %d\n", apic_mode);
            break;
    }
    /*
     * Now do the lower dword.
     * Bit 16 is the mask bit and set to zero
     * Bit 15 is the trigger mode
     * Bit 14 is remote IRR, set this to zero
     * Bit 13 is polarity
     * Bit 11 is destination mode (0 = physical, 1 = logical)
     * Bits 8 - 10 are the delivery mode (0 = fixed, 1 = lowest priority)
     * Bit 0-7 is the vector
     */
    redir_entry_low = (trigger << 15) + (polarity << 13) + (vector & 0xff);
    switch (apic_mode) {
        case 1:
            break;
        case 2:
            redir_entry_low = redir_entry_low | (1 << 11);
            break;
        case 3:
            redir_entry_low = redir_entry_low | (1 << 11) | (1 << 8);
            break;
        default:
            break;
    }
    /*
     * First program higher dword of redirection entry
     * This will leave the entry masked
     * until setup is complete
     */
    io_apic_write(io_apic, APIC_IND_REDIR + 2*irq + 1, redir_entry_high);
    /*
     * then second dword
     */
    io_apic_write(io_apic, APIC_IND_REDIR + 2*irq, redir_entry_low);
}


/***************************************************************
 * Everything below this line is for debugging only            *
 **************************************************************/

/*
 * Print out configuration of I/O APIC
 * Parameters:
 * @io_apic - the I/O APIC to be used
 */
void apic_print_configuration(io_apic_t* io_apic) {
    void* io_apic_base = 0;
    u32* index_register;
    u32* data_register;
    u32 id = 0xff;
    u32 redir_entry_high;
    u32 redir_entry_low;
    u32 version = 0;
    int i;
    int masked;
    int vector;
    /*
     * Determine address of registers
     * Note that even though the index register is only 8 bit wide,
     * it must be accessed as a fullword (32 bit)
     */
    io_apic_base = (void*) (io_apic->base_address);
    index_register = (u32*) (io_apic_base + IO_APIC_IND);
    data_register = (u32*) (io_apic_base + IO_APIC_DATA);
    PRINT("Virtual IO APIC base address: %x\n", io_apic->base_address);
    PRINT("Address of index register: %p\n", index_register);
    PRINT("Address of data register: %p\n", data_register);
    /*
     * Read ID first. Write index 0 to index register
     * then read from 32 bit data register. ID is bit 24-27
     */
    *index_register = APIC_IND_ID;
    id = *(data_register);
    id = (id >> 24) & 0xf;
    /*
     * Next read version
     */
    *index_register = APIC_IND_VER;
    version = *(data_register);
    PRINT("IO APIC ID: %x\n", id);
    PRINT("IO APIC Version: %x\n", version);
    /*
     * Now print out all redirection entries
     */
    PRINT("IRQ REDIR                Vector Masked  IRQ REDIR                Vector Masked\n");
    PRINT("------------------------------------------------------------------------------\n");
    for (i = 0; i < 24; i++) {
        *index_register = (APIC_IND_REDIR + 2 * i);
        redir_entry_low = *data_register;
        *index_register = (APIC_IND_REDIR + 2 * i + 1);
        redir_entry_high = *data_register;
        masked = (redir_entry_low & 0x10000) / 65536;
        vector = (redir_entry_low & 0xff);
        PRINT("%h  %x:%x  %h     %d", i, redir_entry_high,
                redir_entry_low, vector, masked);
        if (i%2)
            PRINT("\n");
        else
            PRINT("       ");
    }
}

/*
 * Print configuration of local APIC
 */
void lapic_print_configuration() {
    u32 reg;
    u32 ver;
    PRINT("Local APIC ID:                  %x\n", apic_get_id());
    reg = lapic_read(LOCAL_APIC_VER_REG);
    ver = reg & 0xff;
    PRINT("Local APIC version:             ");
    if ((ver & 0xf0) == 0x10) {
        PRINT("On-chip\n");
    }
    else if ((ver & 0xf0) == 0) {
        PRINT("486DX\n");
    }
    else
        PRINT("Unknown (%w)\n", ver);
    PRINT("TPR register:                   %x\n", lapic_read(LOCAL_APIC_TPR_REG));
    reg = lapic_read(LOCAL_APIC_SPURIOUS_REG);
    PRINT("Spurios interrupt register:     %x\n", reg);
    PRINT("Local APIC enable flag:         %x\n", (reg >> 8) & 0x1);
    PRINT("Logical destination format:     %x\n", lapic_read(LOCAL_APIC_LDF_REG));
    PRINT("Logical destination register:   %x\n", lapic_read(LOCAL_APIC_LDR_REG));
    PRINT("\nLocal vector tables: \n");
    PRINT("Name      LVT            Masked     Delivery Mode    Vector\n");
    PRINT("-----------------------------------------------------------\n");
    reg = lapic_read(LOCAL_APIC_TIMER_LVT_REG);
    PRINT("TIMER     %x      %h         %h               %h\n", reg,
            (reg & 0x10000) / 0x10000, (reg & 0x700) / 0x100,
            reg & 0xff);
    reg = lapic_read(LOCAL_APIC_TM_LVT_REG);
    PRINT("THERM     %x      %h         %h               %h\n", reg,
             (reg & 0x10000) / 0x10000, (reg & 0x700) / 0x100,
             reg & 0xff);
}

