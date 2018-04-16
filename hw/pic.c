/*
 * pic.c
 *
 * This module contains functions to set and
 * and control an 8259 compatible PIC
 */

#include "pic.h"
#include "io.h"
#include "debug.h"
#include "params.h"

/*
 * Set up PIC
 * We set up all interrupts starting at vector 0x20, so that ISA interrupts
 * occupy vectors 0x20 - 0x2f
 * Parameters:
 * @pic_vector_base - the vector to which HW IRQ 0 will be
 * mapped (0x20 by default)
 */
void pic_init(u32 pic_vector_base) {
    /*
     * First set up master
     */
    outb(0x11, PIC_MASTER_CMD);
    outb(pic_vector_base, PIC_MASTER_DATA);
    outb(0x4, PIC_MASTER_DATA);
    outb(0x1, PIC_MASTER_DATA);
    /*
     * Do the same for the slave
     */
    outb(0x11, PIC_SLAVE_CMD);
    outb(pic_vector_base + 0x8, PIC_SLAVE_DATA);
    outb(0x2, PIC_SLAVE_DATA);
    outb(0x1, PIC_SLAVE_DATA);
    /*
     * Finally set bitmasks to zero
     */
    outb(0x0, PIC_MASTER_DATA);
    outb(0x0, PIC_SLAVE_DATA);
}

/*
 * Disable PIC by masking all interrupts
 * in master and slave
 */
void pic_disable() {
    outb(0xff, PIC_MASTER_DATA);
    outb(0xff, PIC_SLAVE_DATA);
}

/*
 * Acknowledge an interrupt
 * Parameters:
 * @vector - the interrupt vector to be acknowledged
 * @pic_vector_base - the vector corresponding to HW IRQ 0
 */
void pic_eoi(u32 vector, int pic_vector_base) {
    /*
     * If yes, we need to acknowledge receipt
     * Write to master first
     */
    outb(0x20, PIC_MASTER_CMD);
    if (params_get_int("irq_watch") == vector) {
        DEBUG("Ackknowledge  vector %d\n", vector);
    }
    /* Did we receive the signal from the slave? */
    if (vector > pic_vector_base + 0x7) {
        outb(0x20, PIC_SLAVE_CMD);
    }
}
