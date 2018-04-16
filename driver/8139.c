/*
 * 8139.c
 *
 * This is a driver for network cards based on the Realtek 8139 chipset
 */


#include "ktypes.h"
#include "8139.h"
#include "mm.h"
#include "pci.h"
#include "lists.h"
#include "debug.h"
#include "irq.h"
#include "io.h"
#include "timer.h"
#include "lib/stdint.h"
#include "locks.h"
#include "lib/arpa/inet.h"
#include "timer.h"
#include "lib/os/errors.h"
#include "net.h"
#include "mm.h"
#include "arp.h"
#include "params.h"
#include "lib/string.h"
#include "net_if.h"

static char* __module = "8139  ";

extern int __eth_loglevel;
#define NET_DEBUG(...) do {if (__eth_loglevel > 0 ) { kprintf("DEBUG at %s@%d (%s): ", __FILE__, __LINE__, __FUNCTION__); \
        kprintf(__VA_ARGS__); }} while (0)

static void dump_config(nic_t* nic);

/*
 * A linked list of detected network cards managed by this driver
 */
static nic_t* nic_list_head;
static nic_t* nic_list_tail;

/*
 * Receive buffer
 */
static u8 recv_buffer[RECV_BUFFER_SIZE+16];

/*
 * We need four different send buffers. Each send buffer will have 2048 bytes
 */
static u8 send_buffer[4][SEND_BUFFER_SIZE];

/*
 * The public interface
 */
static net_dev_ops_t driver_ops;


/****************************************************************************************
 * Some utility functions to handle timeouts                                            *
 ***************************************************************************************/

/*
 * Wait for a register to take a specified value when
 * masked with a specific mask. The value is checked
 * approximately every 5 microseconds - note however
 * that the timeout value is specified in milliseconds
 * Parameters:
 * @reg - a pointer to the register
 * @mask - bitmask to apply
 * @value - value to wait for (after applying bitmask)
 * @timeout - number of milliseconds to wait
 * Return value:
 * Returns 0 if a timeout occurred and a positive number which reflects
 * the milliseconds left over otherwise
 */
static int wait_for_reg(u32 reg, u32 mask, u32 value, int timeout) {
    int i;
    int j;
    for (i = timeout; i > 0; i--) {
        for (j = 0; j < 200; j++) {
            if ((inb(reg) & mask) == value)
                return i;
            udelay(5);
        }
    }
    return 0;
}



/****************************************************************************************
 * Some basic operations on the device                                                  *
 ***************************************************************************************/


/*
 * Do a software reset
 * Parameter:
 * @nic - a pointer to the device
 * Return value:
 * 0 if the reset was successful, -1 otherwise
 */
static int do_reset(nic_t* nic) {
    u8 reg;
    /*
     * To reset the device, we have to write 1 to bit 4 (RST) of the command register (CR)
     * and then wait until the bit clears again
     */
    reg = inb(nic->base_address + NIC_8139_CR);
    reg = reg | CR_RST;
    outb(reg, nic->base_address + NIC_8139_CR);
    if (0 == wait_for_reg(nic->base_address + NIC_8139_CR, CR_RST, 0, 100)) {
        ERROR("Software reset timed out\n");
        return -1;
    }
    return 0;
}

/*
 * Set up the transmission and receive engine
 * Parameter:
 * @nic - the network card
 */
static void start_device(nic_t* nic) {
    u8 cr;
    u32 tcr;
    u32 rcr;
    /*
     * Place physical address of receive buffer into RBSTART
     */
    outl((u32) recv_buffer, nic->base_address + NIC_8139_RBSTART);
    /*
      * Set RE (receiver enable) and TE (transmitter enable) bit in CR
      * I used to do this only after setting up TCR and RCR. However, it seems that on real hardware
      * I have tested with, RCR is set to zero again when the receiver engine is brought up. Thus I have
      * reversed the order and now bring up both engines before writing into TCR and RCR
      */
     cr = inb(nic->base_address + NIC_8139_CR);
     cr |= (CR_TE + CR_RE);
     outb(cr, nic->base_address + NIC_8139_CR);
     if (0 == wait_for_reg(nic->base_address + NIC_8139_CR, CR_TE + CR_RE, CR_TE + CR_RE, 100)) {
         ERROR("Enabling of receiver and transmitter timed out\n");
         return;
     }
    /*
     * Set transmit configuration register (TCR)
     * We use the following values:
     * Bits 24 - 25 Interframe gap 11b (this is the only IEEE compliant value)
     * Bits 17 - 18 - loopback test 00b - normal operation
     * Bit 16 - CRC - set to 0 to automatically append a CRC
     * Bits 8 - 10 - transmit DMA burst size - 111b (i.e. 2kB)
     * Bits 4 - 7 - TXRR - transmission retry, set to 0 (default, i.e. do 16 retries)
     * Bit 0 - CLRABRT - set to 0 (this bit can be set to 1 when a transmission has been aborted to retransmit)
     */
    tcr = TCR_IFG_NORMAL + TCR_DMA_BURST_2KB;
    outl(tcr, nic->base_address + NIC_8139_TCR);
    /*
     * Set up receive configuration register (RCR)
     * We use the following values:
     * Bits 24 - 27 - Early RX threshold: 0 (no early RX)
     * Bit 17 - multiple early interrupt select - 0
     * Bit 16 - RER8 - 0
     * Bits 13 - 15 - RX Fifo Threshold - 000 (16 bytes)
     * Bits 11 - 12 - RX Buffer Length - 00 (8k + 16 byte, see RECV_BUFFER_SIZE)
     * Bits 8 - 10 - Max DMA burst - 111 (unlimited)
     * Bit 7 - Wrap - 0, i.e. use ring buffer
     * Bit 5 - Accept error packet - 0
     * Bit 4 - Accept runt - 0
     * Bit 3 - Accept broadcast - 1
     * Bit 2 - Accept multicast - 0
     * Bit 1 - Accept matching packages - 1
     * Bit 0 - Accept all packages - 0
     */
    rcr = RCR_DMA_BURST_UNLIMITED + RCR_ACCEPT_BROADCAST + RCR_ACCEPT_MATCH;
    outl(rcr, nic->base_address + NIC_8139_RCR);
    /*
     * Enable all interrupts by setting the IMR to 0xFFFF
     */
    outw(0xFFFF, nic->base_address + NIC_8139_IMR);
}

/*
 * Transmit a message
 * Parameter:
 * @net_msg - the message
 * Return value:
 * 0 if the message could be transmitted successfully
 * EIO if an I/O error occured
 * EAGAIN if no descriptors are currently available
 * EOVERFLOW if the message does not fit into the send buffer
 */
static int tx_msg(net_msg_t* net_msg) {
    int descriptor;
    u32 eflags;
    u32 size;
    nic_t* nic = net_msg->nic;
    if (0 == nic)
        return EIO;
    NET_DEBUG("Sending message via 8139\n");
    /*
     * Get lock for sending
     */
    spinlock_get(&nic->tx_lock, &eflags);
    /*
     * If there is not free descriptor, return EAGAIN so that interface
     * layer can queue the message for later delivery
     */
    if (nic->tx_queued >= nic->tx_sent + NR_OF_TX_DESC) {
        spinlock_release(&nic->tx_lock, &eflags);
        return EAGAIN;
    }
    descriptor = nic->tx_queued % NR_OF_TX_DESC;
    if (eth_create_header(net_msg)) {
        ERROR("Could not create Ethernet header\n");
        return EIO;
    }
    size = net_msg_get_size(net_msg);
    /*
     * Check whether message will fit into send buffer
     */
    if (size > SEND_BUFFER_SIZE) {
        NET_DEBUG("Send buffer too small\n");
        return EOVERFLOW;
    }
    /*
     * and copy entire message to send buffer
     */
    memcpy((void*) send_buffer[descriptor], net_msg_get_start(net_msg), size);
    /*
     * Pad if necessary
     */
    while (size < ETH_MIN_SIZE) {
        send_buffer[descriptor][size] = 0;
        size++;
    }
    /*
     * Write address of send buffer to TSAD
     */
    outl((u32) send_buffer[descriptor], nic->base_address + NIC_8139_TSAD0 + descriptor*sizeof(u32));
    /*
     * Write size of packet into TSD, bits 0-12. As this will clear bit 13 (OWN),
     * this will initiate the transmission. We do not wait for OWN to be reset to 1 (which indicates
     * the end of the DMA transfer from system memory to the cards internal FIFO), but wait for the
     * interrupt and check the flag in the interrupt handler
     */
    outl(size & 0x1FFF, nic->base_address + NIC_8139_TSD0 + descriptor*sizeof(u32));
    /*
     * Increase packet count so that next call will use a different descriptor
     */
    NET_DEBUG("Message written to descriptor %d\n", descriptor);
    nic->tx_queued++;
    /*
     * Release lock again
     */
    spinlock_release(&nic->tx_lock, &eflags);
    /*
     * and free net_msg
     */
    net_msg_destroy(net_msg);
    return 0;
}


/****************************************************************************************
 * The following functions handle interrupts raised by the card                         *
 ***************************************************************************************/


/*
 * This function is invoked by the interrupt handler when the interrupt status register (ISR)
 * indicates that a packet has been received and placed in the ring buffer. It needs to remove
 * the packet from the ring buffer and free the used space in the buffer again
 * Parameter:
 * @nic - the NIC structure for which the interrupt has been received
 */
static void rx_irq(nic_t* nic) {
    u16 buffer_header;
    u32 length;
    u32 cursor;
    u32 eflags;
    net_msg_t* msg = 0;
    int i;
    u8* data;
    /*
     * Get lock to protect data structure - remember that on an SMP system,
     * another CPU could potentially process another interrupt in parallel
     */
    spinlock_get(&nic->rx_lock, &eflags);
    /*
     * Now read from the ring buffer as long as bit BUFE in the command
     * register CR is clear (BUFE = buffer empty)
     */
    NET_DEBUG("Packet received and interrupt raised\n");
    while (0 == (inb(nic->base_address + NIC_8139_CR) & CR_BUFE)) {
        cursor =nic->rx_read;
        NET_DEBUG("Current value of receive buffer cursor: %d, CAPR = %d, CBR = %d\n", cursor, inw(nic->base_address + NIC_8139_CAPR),
                inw(nic->base_address + NIC_8139_CBR));
        /*
         * Read package header
         */
        buffer_header = recv_buffer[cursor % RECV_BUFFER_SIZE];
        cursor++;
        buffer_header = buffer_header + (((u16) recv_buffer[cursor % RECV_BUFFER_SIZE]) << 8);
        cursor++;
        /*
         * Next read two bytes length. Note that this
         * will give us the length including the 4 byte CRC checksum
         */
        length = recv_buffer[cursor % RECV_BUFFER_SIZE];
        cursor++;
        length = length + (((u16) recv_buffer[cursor % RECV_BUFFER_SIZE]) << 8);
        NET_DEBUG("Buffer header = %x, length = %d\n", buffer_header, length);
        cursor++;
        /*
         * Hand over packet to matching protocol - only do this if bit 0 in the buffer
         * entry header indicates that the packet is good
         */
        if (buffer_header & 0x1) {
            NET_DEBUG("Found good packet\n");
            /*
             * Allocate networking message
             */
            if (0 == (msg = net_msg_create(length, 0))) {
                ERROR("Packet discarded due to insufficient memory\n");
                cursor += length - 4;
            }
            else {
                msg->nic = nic;
                data = net_msg_append(msg, length - 4);
                KASSERT(data);
                for (i = 0; i < length - 4; i++) {
                    data[i] = recv_buffer[cursor % RECV_BUFFER_SIZE];
                    cursor++;
                }
                net_if_multiplex_msg(msg);
            }
        }
        cursor += 4;
        /*
         * Write new cursor position back to card - needs to be dword aligned
         * For some strange reason, CAPR points 16 bytes before the start of the actual package, i.e. the ring buffer is
         * considered empty if CBR = CAPR + 16 mod RECV_BUFFER_SIZE. Thus we need to subtract 16 from the new cursor position
         * before writing back to CAPR. I found this out by looking at the source code of QEMU (look at RxBufferEmpty in rtl8139.c),
         * but I have not yet found out the real reason for this behaviour. The Linux driver 8139too.c also substracts the 16 without
         * giving any comment. This explains the initial value of CAPR which is not zero, but 65520
         */
        cursor = (cursor + 3) & ~0x3;
        nic->rx_read = cursor % RECV_BUFFER_SIZE;
        NET_DEBUG("Writing %d back to CAPR\n", nic->rx_read - 16);
        outw(nic->rx_read - 16, nic->base_address + NIC_8139_CAPR);
    }
    /*
     * Release lock again
     */
    spinlock_release(&nic->rx_lock, &eflags);
}

/*
 * This function is called when a packet has been successfully sent. It needs to make the used
 * descriptor available again
 * Parameter:
 * @nic - the network card
 */
static void tx_irq(nic_t* nic) {
    u32 eflags;
    u32 entry;
    u32 tsd;
    NET_DEBUG("tx_sent = %d, tx_queued = %d\n", nic->tx_sent, nic->tx_queued);
    /*
     * Get tx lock
     */
    spinlock_get(&nic->tx_lock, &eflags);
    /*
     * Scan descriptors and free them until we hit upon the first
     * descriptor which has not yet been processed
     */
    entry = nic->tx_sent;
    while (entry < nic->tx_queued) {
        tsd = inl(NIC_8139_TSD0 + (entry % NR_OF_TX_DESC)*sizeof(u32));
        NET_DEBUG("TSD%d = %x\n", entry % NR_OF_TX_DESC, tsd);
        /*
         * Did we hit upon the first incomplete descriptor?
         * Note that a descriptor has been processed if
         * 1) TSD_OWN is set again, and
         * 2) one of the bits TSD_OK, TSD_TUN or TSD_TABT is set
         */
        if (0 == (tsd & (TSD_TOK | TSD_TUN | TSD_TABT)))
            break;
        if (0 == (tsd & TSD_OWN))
            break;
        /*
         * Either TOK, TUN or TABT are set, and OWN is set. Free descriptor
         */
        NET_DEBUG("Freeing descriptor %d\n", entry % NR_OF_TX_DESC);
        entry++;
    }
    /*
     * If we have freed any entries, update NIC structure and
     * inform waiting threads in the network interface layer
     */
    if (entry > nic->tx_sent) {
        nic->tx_sent = entry;
        net_if_tx_event(nic);
    }
    spinlock_release(&nic->tx_lock, &eflags);
}

/*
 * Interrupt handler
 * Parameter:
 * @ir_context - the interrupt context
 */
static int nic_8139_isr(ir_context_t* ir_context) {
    nic_t* nic;
    u32 isr;
    NET_DEBUG("Got interrupt with vector %d\n", ir_context->vector);
    /*
     * Locate card from which the interrupt originates
     */
    LIST_FOREACH(nic_list_head, nic) {
        NET_DEBUG("Checking registered NIC, nic->irq_vector = %d\n", nic->irq_vector);
        if (nic->irq_vector == ir_context->vector) {
            /*
             * Read interrupt status word
             */
            isr = inw(nic->base_address + NIC_8139_ISR);
            NET_DEBUG("Found matching NIC, ISR = %x\n", isr);
            /*
             * Clear ISR for this card. Note that we clear the interrupt before processing the event so that additional
             * interrupts arriving while we are processing the event are not discarded by accident.
             */
             outw(isr, nic->base_address + NIC_8139_ISR);
            /*
             * If we have just received a packet, remove it from the ring buffer. Also
             * check the ring buffer if we have a Rx Overflow
             */
            if (isr & (ISR_ROK | ISR_RXOVW)) {
                NET_DEBUG("Calling rx_irq\n");
                rx_irq(nic);
            }
            else {
                NET_DEBUG("Looks like a spurious interrupt? ISR = %x\n", isr);
            }
            /*
             * Did we send a packet?
             */
            if (isr & (ISR_TOK  | ISR_TER)) {
                tx_irq(nic);
            }
        }
    }
    return 0;
}

/****************************************************************************************
 * Get and set card configuration                                                       *
 ***************************************************************************************/

/*
 * Get current configuration
 */
static int get_config(nic_t* nic, net_dev_conf_t* config) {
    /*
     * Read first two MII registers - we read twice, as some
     * bits are sticky and need to be read twice
     */
    u16 bmsr = inw(nic->base_address + NIC_8139_BMSR);
    bmsr = inw(nic->base_address + NIC_8139_BMSR);
    u16 bmcr = inw(nic->base_address + NIC_8139_BMCR);
    bmcr = inw(nic->base_address + NIC_8139_BMCR);
    /*
     * Fill configuration structure
     */
    config->speed = (bmcr & BMCR_SPD) ? IF_SPEED_100 : IF_SPEED_10;
    config->autoneg = (bmcr & BMCR_ANE) ? 1 : 0;
    config->duplex = (bmcr & BMCR_DUPLEX) ? IF_DUPLEX_FULL : IF_DUPLEX_HALF;
    config->link = (bmsr & BMSR_LINK) ? 1 : 0;
    switch(bmsr & BMSR_MEDIUM) {
        case BMSR_MEDIUM_AUTO:
            /*
             * As the 8139 has an internal PHY and according to the datasheet,
             * this takes precedence over an external PHY in auto mode, assume
             * that internal PHY is used
             */
            config->port = IF_PORT_TP;
            break;
        case BMSR_MEDIUM_MMI:
            config->port = IF_PORT_MII;
            break;
        case BMSR_MEDIUM_TP:
            config->port = IF_PORT_TP;
            break;
        default:
            config->port = IF_PORT_TP;
            break;
    }
    return 0;
}

/****************************************************************************************
 * Card initialization                                                                  *
 ***************************************************************************************/

/*
 * Callback function for the PCI device driver. This function handles the actual setup
 * of the card
 * Parameter:
 * @pci_dev - the PCI device associated with the card
 */
static void nic_8139_register_cntl(const pci_dev_t* pci_dev) {
    nic_t* nic;
    int vector;
    u32 bar;
    int i;
    if ((PCI_VENDOR_REALTEK == pci_dev->vendor_id) && (PCI_DEVICE_8139 == pci_dev->device_id)) {
        if (0 == (nic = kmalloc(sizeof(nic_t)))) {
            ERROR("Could not allocate memory for NIC\n");
            return;
        }
        nic->pci_dev = (pci_dev_t*) pci_dev;
        spinlock_init(&nic->tx_lock);
        spinlock_init(&nic->rx_lock);
        nic->rx_read = 0;
        nic->tx_queued = 0;
        nic->tx_sent = 0;
        nic->hw_type = HW_TYPE_ETH;
        nic->mtu = MTU_ETH;
        /*
         * Make sure that bus mastering is enabled
         */
        pci_enable_bus_master_dma((pci_dev_t*) pci_dev);
        /*
         * Register interrupt handler
         */
        MSG("Registering interrupt handler for RTL 8139\n");
        if (-1 == (vector = irq_add_handler_pci(nic_8139_isr, 1, (pci_dev_t*) pci_dev))) {
            ERROR("Could not register interrupt handler\n");
            return;
        }
        nic->irq_vector = vector;
        /*
         * Get base of I/O address space from BAR 0
         */
        bar = pci_dev->bars[0];
        if (0 == (bar & 0x1)) {
            ERROR("Device not mapped into I/O space\n");
            return;
        }
        nic->base_address = bar & ~0x3;
        /*
         * Add device to internal list
         */
        LIST_ADD_END(nic_list_head, nic_list_tail, nic);
        MSG("Found 8139 PCI network card at %d:%d.%d (IRQ = %d)\n", pci_dev->bus->bus_id, pci_dev->device, pci_dev->function, vector);
        /*
         * Get MAC address
         */
        for (i = 0; i < 6; i++) {
            nic->mac_address[i] = inb(nic->base_address + NIC_8139_IDR0 + i);
        }
        MSG("MAC address: %h:%h:%h:%h:%h:%h\n", nic->mac_address[0], nic->mac_address[1], nic->mac_address[2],
                nic->mac_address[3], nic->mac_address[4], nic->mac_address[5]);
        /*
         * Register device with network interface layer
         */
        driver_ops.nic_tx_msg = tx_msg;
        driver_ops.nic_get_config = get_config;
        driver_ops.nic_debug = dump_config;
        net_if_add_nic(nic, &driver_ops);
        /*
         * Reset the device
         */
        do_reset(nic);
        /*
         * Set up engines
         */
        start_device(nic);
    }
}

/*
 * Initialize the driver. This function will scan the PCI bus for available
 * 8193 based network cards
 */
void nic_8139_init() {
    /*
     * Scan the PCI bus to find all network devices. Note that the actual
     * setup of the card is done in the callback functions
     */
    pci_query_by_class(nic_8139_register_cntl, PCI_BASE_CLASS_NIC, ETH_SUB_CLASS);
}


/****************************************************************************************
 * Used for debugging                                                                   *
 ***************************************************************************************/

static void dump_config(nic_t* nic) {
    PRINT("Command register:                  %h\n", inb(nic->base_address + NIC_8139_CR));
    PRINT("Interrupt mask register:           %w\n", inw(nic->base_address + NIC_8139_IMR));
    PRINT("Interrupt status register:         %w\n", inw(nic->base_address + NIC_8139_ISR));
    PRINT("Transmit configuration register:   %x\n", inl(nic->base_address + NIC_8139_TCR));
    PRINT("Receive configuration register:    %x\n", inl(nic->base_address + NIC_8139_RCR));
}
