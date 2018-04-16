/*
 * 8139.h
 *
 */

#ifndef _8139_H_
#define _8139_H_

#include "pci.h"
#include "eth.h"

/*
 * Identifier for Realtek Chipset
 */
#define PCI_VENDOR_REALTEK 0x10ec
#define PCI_DEVICE_8139 0x8139

/*
 * Number of TX descriptors
 */
#define NR_OF_TX_DESC 4

/*
 * Register offsets
 */
#define NIC_8139_IDR0 0x0
#define NIC_8139_IDR1 0x1
#define NIC_8139_IDR2 0x2
#define NIC_8139_IDR3 0x3
#define NIC_8139_IDR4 0x4
#define NIC_8139_IDR5 0x5
#define NIC_8139_TSD0 0x10
#define NIC_8139_TSAD0 0x20
#define NIC_8139_RBSTART 0x30
#define NIC_8139_CR 0x37
#define NIC_8139_CAPR 0x38
#define NIC_8139_CBR 0x3a
#define NIC_8139_IMR 0x3c
#define NIC_8139_ISR 0x3e
#define NIC_8139_TCR 0x40
#define NIC_8139_RCR 0x44

/*
 * The following registers seem to be according to the MII standard
 */
#define NIC_8139_BMCR 0x62      // MII control register
#define NIC_8139_BMSR 0x64      // MII status register

/*
 * Some flags
 */
#define CR_RST (1 << 4)
#define CR_RE (1 << 3)
#define CR_TE (1 << 2)
#define CR_BUFE 0x1
#define ISR_ROK 0x1
#define ISR_TOK (1 << 2)
#define ISR_TER (1 << 3)
#define ISR_RXOVW (1 << 4)
#define TSD_OWN (1 << 13)
#define TSD_TOK (1 << 15)
#define TSD_TUN (1 << 14)
#define TSD_TABT (1 << 30)
#define TCR_IFG_NORMAL (0x3 << 24)
#define TCR_DMA_BURST_2KB (0x7 << 8)
#define RCR_DMA_BURST_UNLIMITED (0x7 << 8)
#define RCR_ACCEPT_BROADCAST (1 << 3)
#define RCR_ACCEPT_MATCH (1 << 1)

/*
 * Again these flags are mostly according to the MII standard IEEE 802.3-2008, even though
 * the 8139 does not seem to support all flags required by MII
 */
#define BMCR_RST (1 << 15)            // PHY reset
#define BMCR_SPD (1 << 13)            // Speed selection (1 = 100 MBs), manual, only used if auto-negotiation is disabled
#define BMCR_ANE (1 << 12)            // Auto-negotiation
#define BMCR_ANE_RST (1 << 9)         // set this to one to restart auto-negotiation
#define BMCR_DUPLEX (1 << 8)          // duplex mode, 1 = full duplex
#define BMSR_LINK (1 << 2)            // link established?
#define BMSR_MEDIUM (0x3 << 6)        // medium select
#define BMSR_MEDIUM_AUTO (0)
#define BMSR_MEDIUM_MMI (0x2 << 6)    // connected to external PHY via MMI
#define BMSR_MEDIUM_TP (0x1 << 6)     // directly connected to twisted pair, i.e. internal PHY



/*
 * Size of receive buffer
 */
#define RECV_BUFFER_SIZE (8192)

/*
 * Size of send buffer
 */
#define SEND_BUFFER_SIZE 2048

void nic_8139_init();
void nic_8139_test_send_payload(u8* payload, int size, u16 ethertype);

#endif /* _8139_H_ */
