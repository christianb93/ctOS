/*
 * ahci.c
 *
 * This module contains the code for the AHCI device driver.
 */

#include "ahci.h"
#include "drivers.h"
#include "debug.h"
#include "dm.h"
#include "lists.h"
#include "mm.h"
#include "hd.h"
#include "lib/string.h"
#include "timer.h"
#include "params.h"

/*
 * Module name used for messages
 */
static char* __module = "AHCI  ";

/*
 * Turn on detailed logging (independent of global loglevel
 * and only valid for this module)
 */
static int __ahci_log = 0;

/*
 * Sector buffer used during initialization, for instance for IDENTIFY DEVICE
 */
static u16 __attribute__ ((aligned(256))) sector_buffer[512];

/*
 * Block device operations structure representing this driver
 */
static blk_dev_ops_t ops;

/*
 * This is a list of known AHCI controllers
 */
static ahci_cntl_t* ahci_cntl_list_head = 0;
static ahci_cntl_t* ahci_cntl_list_tail = 0;

/*
 * A list of known AHCI ports
 */
static ahci_port_t* ahci_port_list_head = 0;
static ahci_port_t* ahci_port_list_tail = 0;

/*
 * A lock to make sure that the interrupt handler is not called
 * simultaneously on two different CPUs
 */
static spinlock_t handler_lock;

/*
 * For each port, we are going to have one request queue. For each slot in
 * one of the queues, we need one command table. Thus we have AHCI_MAX_PORTS * HD_QUEUE_SIZE
 * command tables, indexed by port number and slot number. The entire array is aligned at a
 * 128 byte boundary. As the size of each structure is a multiple of 128 as well, all elements
 * are aligned on a 128 byte boundary as required by the specification
 */
static ahci_command_table_t
        __attribute__ ((aligned(128))) command_tables[AHCI_MAX_PORTS][HD_QUEUE_SIZE];

/*
 * Number of ports which have already been registered
 */
static int port_count = 0;

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
static int wait_for_reg(u32* reg, u32 mask, u32 value, int timeout_ms) {
    int i;
    int j;
    for (i = timeout_ms; i > 0; i--) {
        for (j = 0; j < 200; j++) {
            if (((*reg) & mask) == value)
                return i;
            udelay(5);
        }
    }
    return 0;
}



/*
 * Stop the command DMA engine for an AHCI port
 * Parameters:
 * @port - the port for which we wish to stop the engine
 * Return value:
 * 0 if the operation was successful
 * EIO if the operation timed out
 */
static int ahci_stop_cmd(ahci_port_t* port) {
    u32 temp;
    /*
     * Clear PxCMD.ST
     * and wait for at most 500 milliseconds until
     * PxCMD.CR = 0
     */
    temp = (port->regs->pxcmd) & ~(PXCMD_ST);
    port->regs->pxcmd = temp;
    if (0 == wait_for_reg(&(port->regs->pxcmd), PXCMD_CR, 0, AHCI_TIMEOUT_STOP_CMD))
        return EIO;
    return 0;
}

/*
 * Start the command DMA engine
 * Parameters:
 * @port - the AHCI port
 * Return value:
 * 0 if operation was successful
 * EIO if device could not be started
 */
static int ahci_start_cmd(ahci_port_t* port) {
    u32 temp;
    /*
     * Wait for BSY and DRQ to clear
     */
    if (0 == wait_for_reg(&(port->regs->pxtfd), IDE_STATUS_ERR + IDE_STATUS_DRQ
            + IDE_STATUS_BSY, 0x0, AHCI_TIMEOUT_IDLE)) {
        DEBUG("Request timed out\n");
        /*
         * If port supports CLO, try to override TFD
         * by setting CLO to one.
         * The AHCI specification says that we  need to wait for CLO to clear again
         * before we set PxCMD.ST.
         */
        if (port->ahci_cntl->sclo) {
            DEBUG("Will try CLO to start engine\n");
            temp = port->regs->pxcmd;
            temp = temp | PXCMD_CLO;
            DEBUG("PxCMD.ST is %h, PxCMD.CLO = %h, writing %x to PxCMD...",
                    port->regs->pxcmd & PXCMD_ST, (port->regs->pxcmd & PXCMD_CLO) / PXCMD_CLO,
                    temp);
            port->regs->pxcmd = temp;
            DEBUG("done\n");
            if (0 == wait_for_reg(&(port->regs->pxcmd), PXCMD_CLO, 0, AHCI_TIMEOUT_IDLE)) {
                ERROR("Error, request for CLO timed out (PxCMD=%x)\n",
                        port->regs->pxcmd);
                return EIO;
            }
        }
        else {
            ERROR("BSY flag is set but device not support CLO - help me, I am lost...\n");
            return EIO;
        }
    }
    DEBUG("Starting engine again\n");
    temp = (port->regs->pxcmd) | PXCMD_ST;
    port->regs->pxcmd = temp;
    DEBUG("%x written, waiting for PXCMD.CR to be set\n", temp);
    if (0 == wait_for_reg(&(port->regs->pxcmd), PXCMD_CR, PXCMD_CR, AHCI_TIMEOUT_START_CMD)) {
        ERROR("request timed out, PxCMD=%x\n", port->regs->pxcmd);
        return EIO;
    }
    return 0;
}

/*
 * Initialize an AHCI port and set up memory data structures.
 * Parameters:
 * @port - the port to be started
 * Return value:
 * 0 if operation was successful
 * EIO if the operation timed out
 */
static int ahci_init_device(ahci_port_t* port) {
    u32 temp;
    /*
     * Step 1: stop engine
     */
    if (ahci_stop_cmd(port)) {
        ERROR("Request to stop command engine returned with timeout\n");
        return EIO;
    }
    /*
     * Step 2: clear PxCMD.FRE and wait
     * until PxCMD.FR clears
     */
    DEBUG("\nClearing PxCMD.FRE\n");
    temp = (port->regs->pxcmd) & PXCMD_FRE;
    port->regs->pxcmd = temp;
    if (0 == wait_for_reg(&(port->regs->pxcmd), PXCMD_FR, 0x0, AHCI_TIMEOUT_STOP_FIS)) {
        ERROR("Request to stop FIS engine timed out\n");
        return EIO;
    }
    /*
     * Step 3: spin up / power up device
     * Note that this will initiate a port reset if
     * PxCMD.SUD was zero
     */
    temp = (port->regs->pxcmd) | (PXCMD_SUD + PXCMD_POD);
    port->regs->pxcmd = temp;
    mdelay(1);
    DEBUG("PxTFD=%x, PxCMD=%x\n", port->regs->pxtfd, port->regs->pxcmd);
    /*
     * Step 4: wait for PxSSST.DET to become 0x3
     */
    DEBUG("Waiting for PxSSTS.DET...");
    if (0 == wait_for_reg(&(port->regs->pxssts), PXSSTS_DET, PXSSTS_DET_PRESENT
            + PXSSTS_DET_PHY, AHCI_TIMEOUT_IDLE)) {
        ERROR("request timed out\n");
        return EIO;
    }
    /*
     * Step 5: set PxFB to address of received FIS structure
     * and PxCLB to address of command list
     */
    port->regs->pxfb = (u32) mm_virt_to_phys((u32) port->received_fis);
    port->regs->pxclb = (u32) mm_virt_to_phys((u32) port->command_list);
    port->regs->pxfbu = 0;
    port->regs->pxclbu = 0;
    DEBUG("PxFB = %p, PxCLB = %p\n", port->regs->pxfb, port->regs->pxclb);
    /*
     * Step 6: clear error register PxSERR.
     * Clearing the error register is necessary for PxTFD to clear.
     * This is especially important if we did a soft reset before, as the BSY bit
     * will remain set after a soft reset up to this point and PxSERR.DIAG.X is set
     * Specification:
     * Section 10.4.2: ...upon receiving a COMINIT from the attached device,
     * PxTFD.BSY shall be set to '1' by the HBA
     * Section 10.1:"Note that to enable the PxTFD register to be updated with the
     * initial Register FIS for a port, the PxSERR.DIAG.X bit must be cleared to ‘0’"
     * Section 5.3.2.3. State P:Running. PxCMD.FRE written to ‘1’ from a ‘0’ and previously processed
     * Register FIS is in receive FIFO and PxSERR.DIAG.X = ‘0’ -> P:RegFisPostToMem
     */
    DEBUG("PxTFD=%x, PxCMD=%x\n", port->regs->pxtfd, port->regs->pxcmd);
    DEBUG("Clearing error register (was: %x)...\n", port->regs->pxserr);
    port->regs->pxserr = ~((u32) 0);
    /*
     * Step 7: set PxCMD.FRE  again
     */
    DEBUG("Setting PxCMD.FRE...");
    temp = (port->regs->pxcmd) | PXCMD_FRE;
    port->regs->pxcmd = temp;
    /*
     * Step 8: start engine again and clear PxIS
     */
    if (ahci_start_cmd(port)) {
        ERROR("Could not start command engine\n");
        return EIO;
    }
    port->regs->pxis = ~((u32) 0);
    return 0;
}

/*
 * Do a port reset (COMRESET) for a drive
 * Parameter:
 * @port - the port to be reset
 * Return value:
 * 0 if the operation is successful
 * EIO if the request timed out
 */
static int ahci_comreset(ahci_port_t* port) {
    u32 temp;
    int timeout;
    /*
     * Step 1: stop command list DMA engine
     * and clear PxCMD.FRE
     */
    DEBUG("Stopping command engine\n");
    if (ahci_stop_cmd(port)) {
        ERROR("Request timed out\n");
        return EIO;
    }
    temp = (port->regs->pxcmd) & ~(PXCMD_FRE);
    port->regs->pxcmd = temp;
    /*
     * Step 2: perform port reset by either
     * 1) writing 0x1 to PxSCTL.DET
     * 2) after 1 ms, clear PxSCTL.DET again
     * if PxCMD.SUD is set OR
     * by raising PxCMD.SUD from 0 to 1 if the bit is not yet set
     */
    if (port->regs->pxcmd & PXCMD_SUD) {
        DEBUG("PXCMD_SUD already set, raising and clearing PxSCTL.DET\n");
        temp = port->regs->pxsctl;
        temp = (temp & ~(PXSSTS_DET)) | PXSSTS_DET_PRESENT;
        port->regs->pxsctl = temp;
        for (timeout = 0; timeout < 10; timeout++)
            mdelay(1);
        temp = (port->regs->pxsctl) & ~(PXSSTS_DET);
        port->regs->pxsctl = temp;
    }
    else {
        temp = (port->regs->pxcmd) | PXCMD_SUD;
        port->regs->pxcmd = temp;
    }
    /*
     * Step 3: wait for bit 0
     * of PxSSTS.DET to become 1 signaling
     * completion of reset
     */
    if (0 == wait_for_reg(&(port->regs->pxssts), PXSSTS_DET_PRESENT, 1, AHCI_TIMEOUT_IDLE)) {
        ERROR("Request timed out\n");
        return EIO;
    }
    DEBUG("PXTFD.STS is now %x\n", port->regs->pxtfd & 0xFF);
    /*
     * Clear error register
     */
    port->regs->pxserr = ~((u32) 0);
    return 0;
}

/*
 * Fill a command header structure
 * Parameter:
 * @header - the header to be set up
 * @write - 1 for a write operation, 0 for a read operation
 * @prdtl - length of PRDT in entries
 * @command_table - command table
 */
static void ahci_setup_cmd_header(ahci_command_header_t* header, int write,
        int prdtl, ahci_command_table_t* command_table) {
    memset((void*) header, 0, sizeof(ahci_command_header_t));
    /*
     * Clear busy upon ok is set to 0 and reset bit is set to 0 by default
     */
    header->c = 0;
    header->reset = 0;
    /*
     * Size of command fis
     */
    header->cfisl = sizeof(h2d_register_fis_t) / 4;
    /*
     * Is this a read or a write?
     */
    header->write = write;
    /*
     * Length of physical region descriptor table
     */
    header->prdtl = prdtl;
    /*
     * Address of command list
     */
    header->command_table_base = (u32) command_table;
    header->command_table_base_upper = 0;
}

/*
 * Set up a command FIS within a command table
 * Parameters:
 * @command_table - the command table to filled
 * @ata_cmd - the ATA command
 * @lba - the first block
 * @sector_count - number of sectors
 */
static void ahci_setup_cmd_fis(ahci_command_table_t* command_table,
        int ata_cmd, u64 lba, u16 sector_count) {
    if (__ahci_log)
        PRINT("ata_cmd=%x, lba_low=%x, lba_high=%x, sector_count=%x\n", ata_cmd,(u32) lba, (u32)(lba >> 32), sector_count);
    /*
     * FIS type is always Host to device for our purposes
     */
    command_table->cfis.fis_type = FIS_TYPE_H2D;
    /*
     * No port multipliers - port multipliers are not yet supported
     */
    command_table->cfis.pm = 0;
    command_table->cfis.reserved0 = 0;
    /*
     * Its a command
     */
    command_table->cfis.c = 1;
    /*
     * ATA command
     */
    command_table->cfis.command = ata_cmd;
    /*
     * These are the ATA registers
     */
    command_table->cfis.feature = 0;
    command_table->cfis.lba_low = lba;
    command_table->cfis.lba_mid = (lba >> 8);
    command_table->cfis.lba_high = (lba >> 16);
    command_table->cfis.device = IDE_DEVICE_LBA;
    command_table->cfis.lba_low_ext = (lba >> 24);
    command_table->cfis.lba_mid_ext = (lba >> 32);
    command_table->cfis.lba_high_ext = (lba >> 40);
    command_table->cfis.feature_ext = 0;
    command_table->cfis.sector_count = sector_count;
    command_table->cfis.sector_count_ext = sector_count >> 8;
    command_table->cfis.device_control = 0;
}

/*
 * Generic function to issue a command and wait for completion
 * without using any interrupts
 * Parameters:
 * @port - the port to which the command is to be written
 * @ata_cmd - the ATA command
 * @write - 1 to write to device, 0 to read from device
 * @lba - lba value which will be written into LBA register in FIS
 * @sector_count - sector count
 * @buffer - buffer to use
 * Return value:
 * EIO if the operation timed out
 * 0 upon success
 */
static int ahci_issue_sync_cmd(ahci_port_t* port, int ata_cmd, int write,
        u64 lba, u16 sector_count, void* buffer) {
    u32 temp;
    ahci_command_table_t* command_table = port->command_tables;
    /*
     * Make sure that interrupts are disabled
     */
    port->regs->pxie = 0x0;
    /*
     * Step 1: wait for PxCI bit 0 to be cleared
     */
    DEBUG("Waiting for PxCI[0] to clear...\n");
    if (0 == wait_for_reg(&(port->regs->pxci), 0x1, 0, AHCI_TIMEOUT_IDLE)) {
        PANIC("Request timed out\n");
        return EIO;
    }
    /*
     * Step 2: setting up command table. We use the first command
     * table out of the HD_QUEUE_SIZE tables that are reserved for this port
     */
    ahci_setup_cmd_fis(command_table, ata_cmd, lba, sector_count);
    /*
     * Step 3:
     * set up one entry in the PRDT which is part of the command table
     * Here we only use one entry
     */
    DEBUG("Using buffer at virtual address %x, physical address %x\n", buffer, mm_virt_to_phys((u32) buffer));
    command_table->prd->base_address = mm_virt_to_phys((u32) buffer);
    command_table->prd->base_address_upper = 0;
    command_table->prd->dbc = 512 * sector_count - 1;
    command_table->prd->i = 0;
    command_table->prd->reserved0 = 0;
    command_table->prd->reserved1 = 0;
    /*
     * Step 4: set up command table header to point to command table
     */
    ahci_setup_cmd_header(port->command_list, write, 1, command_table);
    /*
     * Step 5: set CI bit
     */
    DEBUG("Setting CI bit\n");
    temp = (port->regs->pxci) | 0x1;
    port->regs->pxci = temp;
    /*
     * Step 6: wait until CI clears again
     */
    DEBUG("Waiting for CI to clear again\n");
    if (0 == wait_for_reg(&(port->regs->pxci), 0x1, 0, AHCI_TIMEOUT_IDLE)) {
        PANIC("Request timed out\n");
        return EIO;
    }
    return 0;
}

/*
 * Get a port for a given minor device number
 * Parameters:
 * @minor - the minor device number
 * Return value:
 * the port or 0 if no port could be found
 */
static ahci_port_t* ahci_get_port(minor_dev_t minor) {
    ahci_port_t* port;
    int partition;
    LIST_FOREACH(ahci_port_list_head, port) {
        if ((port->minor / AHCI_MAX_PARTITIONS)
                == (minor / AHCI_MAX_PARTITIONS)) {
            /*
             * We found the right port. Now see whether the partition the device refers to
             * is in use
             */
            partition = minor % AHCI_MAX_PARTITIONS;
            /*
             * Partition 0 is the raw device and always there by definition
             */
            if (0 == partition)
                return port;
            if (port->partitions[partition].used)
                return port;
        }
    }
    return 0;
}

/*
 * Read sectors from an AHCI device without using interrupts
 * Thus we can use this function to read during early initialization
 * phase when interrupts are not yet disabled or while working with the
 * debugger
 * It does not read from partitions, but always from the raw device
 * Parameters:
 * @minor - the minor device number of the device
 * @lba - the sector number
 * @sectors - number of sectors to read
 * @buffer - buffer to which we write the data
 * Return values:
 * number of bytes read upon success
 * -ENODEV if minor device number is not valid
 * -ENOMEM if buffer is not aligned and a new buffer could not be reserved
 * -EIO if the operation timed out
 */
static int ahci_read_sector(minor_dev_t minor, u64 lba,
        void* buffer) {
    ahci_port_t* port;
    void* mybuffer = buffer;
    int use_own_buffer = 0;
    DEBUG("Read of sector %d requested\n", lba);
    /*
     * First get port for minor device
     */
    port = ahci_get_port(minor);
    if (0 == port)
        return -ENODEV;
    /*
     * Make sure that buffer is aligned
     * to a dword boundary
     */
    if (((u32) mybuffer) % sizeof(u32)) {
        mybuffer = kmalloc_aligned(ATA_BLOCK_SIZE, sizeof(u32));
        if (0 == mybuffer) {
            ERROR("Could not allocate aligned DMA buffer\n");
            return -ENOMEM;
        }
        use_own_buffer = 1;
    }
    /*
     * Issue READ DMA EXT command
     */
    DEBUG("Issuing read command\n");
    if (ahci_issue_sync_cmd(port, IDE_READ_DMA_EXT, 0, lba, 1, mybuffer)) {
        ERROR("Operation timed out\n");
        if (use_own_buffer)
            kfree(mybuffer);
        return -EIO;
    }
    if (use_own_buffer) {
        memcpy(buffer, mybuffer, ATA_BLOCK_SIZE);
        kfree(mybuffer);
    }
    return ATA_BLOCK_SIZE;
}

/*
 * Submit a request to an AHCI port. The port to use is determined from the
 * minor device number stored in the request. Then the data structures are
 * filled as required by the AHCI specification and the command issue bit in
 * the PxCI register is set to initiate the processing of the request
 * Parameter:
 * @request_queue - the queue on which the request has been placed
 * @request - the request to be submitted
 */
static void ahci_submit_request(hd_request_queue_t* request_queue,
        hd_request_t* request) {
    u32 temp;
    ahci_port_t* port = ahci_get_port(request->minor_device);
    KASSERT(port);
    int index = request - request_queue->queue;
    /*
     * Get the command table structure which is reserved for
     * use with this request slot
     */
    ahci_command_table_t* command_table = port->command_tables + index;
    /*
     * Step 1: wait for PxCI bit 0 to be cleared
     */
    DEBUG("Waiting for PxCI[0] to clear...\n");
    if (0 == wait_for_reg(&(port->regs->pxci), 0x1, 0, AHCI_TIMEOUT_IDLE)) {
        PANIC("Request timed out\n");
    }
    /*
     * Step 2: set up command table header to point to command table
     */
    ahci_setup_cmd_header(port->command_list, request->rw, request->data,
            command_table);
    /*
     * Step 3: set CI bit
     */
    DEBUG("Setting CI bit\n");
    temp = (port->regs->pxci) | 0x1;
    port->regs->pxci = temp;
}

/*
 * Complete a request. This function does nothing at the moment
 * Parameter:
 * @request_queue - queue in which the request is placed
 * @request - the request to be completed
 */
static void ahci_complete_request(hd_request_queue_t* request_queue,
        hd_request_t* request) {
}

/*
 * Prepare the command table for a request, i.e. split the buffer pointed to by the
 * request into chunks, translate them into virtual addresses and set up the
 * PRDT accordingly
 * Parameters:
 * @request_queue - the request queue
 * @request - the request
 */
static void ahci_prepare_request(hd_request_queue_t* request_queue,
        hd_request_t* request) {
    u32 chunk_start;
    u32 chunk_end;
    u32 page_end;
    u32 buffer_start;
    u32 buffer_end;
    ahci_command_table_t* command_table;
    int index = request - request_queue->queue;
    ahci_port_t* port = ahci_get_port(request->minor_device);
    KASSERT(port);
    int prdt_index = 0;
    int eot = 0;
    KASSERT(index>=0);
    KASSERT(index<HD_QUEUE_SIZE);
    command_table = port->command_tables + index;
    /*
     * We now split the buffer pointed to by the request into chunks.
     * By definition, each chunk is the overlap of a page in virtual memory
     * with a part of the buffer. For each chunk, we add an entry to the PRDT
     * First determine buffer boundaries and let first chunk start at start of buffer
     */
    buffer_start = request->buffer;
    buffer_end = buffer_start + request->blocks * ATA_BLOCK_SIZE - 1;
    chunk_start = buffer_start;
    while (1) {
        if (prdt_index >= AHCI_PRDT_COUNT) {
            PANIC("PRDT maximum size exceeded\n");
            return;
        }
        /*
         * Determine last address of page in which chunk start is located
         */
        page_end = MM_PAGE_SIZE - (chunk_start % MM_PAGE_SIZE) + chunk_start - 1;
        /*
         * If necessary cut off chunk to end there
         */
        if (page_end < buffer_end) {
            chunk_end = page_end;
            eot = 0;
        }
        else {
            chunk_end = buffer_end;
            eot = 1;
        }
        /*
         * Add entry to PRDT within command table. Start with physical (!) address of buffer
         */
        DEBUG("Adding PRDT entry for virtual address %x, physical address %x\n", chunk_start, mm_virt_to_phys(chunk_start));
        command_table->prd[prdt_index].base_address = mm_virt_to_phys(
                chunk_start);
        command_table->prd[prdt_index].base_address_upper = 0;
        /*
         * DBC is the number of bytes contained in the region
         */
        DEBUG("DBC = %d\n", chunk_end - chunk_start);
        command_table->prd[prdt_index].dbc = chunk_end - chunk_start;
        /*
         * Do not raise an interrupt after processing this request
         * We rely on the interrupt raised if the D2H FIS is received
         * from the device which sets BSY back to 0
         */
        command_table->prd[prdt_index].i = 0;
        command_table->prd[prdt_index].reserved0 = 0;
        command_table->prd[prdt_index].reserved1 = 0;
        /*
         * If that was the last entry, leave loop,
         * otherwise increment index into PRDT and continue
         */
        if (eot)
            break;
        prdt_index++;
        chunk_start = chunk_end + 1;
    }
    /*
     * Set up command FIS with correct command, depending on
     * whether a read or write has been requested
     */
    if (HD_READ == request->rw) {
        ahci_setup_cmd_fis(command_table, IDE_READ_DMA_EXT,
                request->first_block, request->blocks);
    }
    else {
        ahci_setup_cmd_fis(command_table, IDE_WRITE_DMA_EXT,
                request->first_block, request->blocks);
    }
    /*
     * We put the number of entries in the PRDT into the data field of the
     * request as we will need it later when submitting the request
     */
    request->data = (prdt_index + 1);
    if (__ahci_log)
        PRINT("PRDT has %d entries\n", prdt_index+1);
}

/*
 * Utility function to set up a port structure, i.e.
 * - allocate memory for the request queue and set up the request queue
 * - allocate memory for the command list
 * - allocate memory for the received FIS structure
 * Parameters:
 * @ahci_port - the port to be set up
 * @command_tables - an array of command tables to be used for this port
 */
static void ahci_setup_port(ahci_port_t* ahci_port,
        ahci_command_table_t* command_tables) {
    /*
     * Each command table needs to be aligned at a 128 byte boundary.
     * As the entire array is aligned, this will work as long as the size
     * of the command table structure itself is a multiple of 128 - assert this
     */
    KASSERT(0==(sizeof(ahci_command_table_t) % 128));
    ahci_port->command_tables = command_tables;
    ahci_port->minor = (port_count) * AHCI_MAX_PARTITIONS;
    ahci_port->request_queue = (hd_request_queue_t*) kmalloc(
            sizeof(hd_request_queue_t));
    KASSERT(ahci_port->request_queue);
    ahci_port->request_queue->block_size = ATA_BLOCK_SIZE;
    ahci_port->request_queue->chunk_size = AHCI_CHUNK_SIZE;
    ahci_port->request_queue->device_busy = 0;
    spinlock_init(&(ahci_port->request_queue->device_lock));
    sem_init(&(ahci_port->request_queue->slots_available), HD_QUEUE_SIZE);
    ahci_port->request_queue->head = 0;
    ahci_port->request_queue->tail = 0;
    ahci_port->request_queue->submit_request = ahci_submit_request;
    ahci_port->request_queue->prepare_request = ahci_prepare_request;
    ahci_port->request_queue->complete_request = ahci_complete_request;
    /*
     * Get memory for the command list. As a command list is less than one
     * page, we can use kmalloc_aligned to get a list which is entirely in
     * one page
     */
    KASSERT(sizeof(ahci_command_header_t)*AHCI_COMMAND_LIST_ENTRIES<MM_PAGE_SIZE);
    ahci_port->command_list = (ahci_command_header_t*) kmalloc_aligned(
            sizeof(ahci_command_header_t) * AHCI_COMMAND_LIST_ENTRIES,
            MM_PAGE_SIZE);
    KASSERT(ahci_port->command_list);
    KASSERT(AHCI_RECEIVED_FIS_SIZE<=256);
    ahci_port->received_fis
            = (u8*) kmalloc_aligned(AHCI_RECEIVED_FIS_SIZE, 256);
    KASSERT(ahci_port->received_fis);
}

/*
 * Tear down a port again, i.e. release associated memory
 */
static void ahci_teardown_port(ahci_port_t* port) {
    if (port->request_queue)
        kfree((void*) port->request_queue);
    if (port->command_list)
        kfree((void*) port->command_list);
    if (port->received_fis)
        kfree((void*) port->received_fis);
}

/*
 * Register a port
 * Parameters:
 * @ahci_cntl - the controller to which the port is attached
 * @port - the port number (0 - 31)
 */
static void ahci_register_port(ahci_cntl_t* ahci_cntl, int port) {
    int rc;
    int i;
    ahci_port_t* ahci_port;
    /*
     * Check whether we are about to exceed the maximum number of ports
     */
    if (port_count >= AHCI_MAX_PORTS) {
        ERROR("Exceeding maximum number of ports this driver can support\n");
        return;
    }
    /*
     * Get pointer to AHCI in-memory register set for this port. Note that the entire AHCI register
     * set has been mapped to consecutive pages of virtual memory by ahci_setup_cntl, thus we can easily
     * get a pointer to the port specific register set
     */
    ahci_port_regs_t* ahci_port_regs =
            (ahci_port_regs_t*) (ahci_cntl->ahci_base_address
                    + AHCI_OFFSET_PORT(port));
    DEBUG("AHCI register set is located at %x, status is %x\n", ahci_port_regs, ahci_port_regs->pxssts);
    /*
     * Now check whether a device is attached to this port
     */
    if ((PXSSTS_DET_PHY + PXSSTS_DET_PRESENT) != (ahci_port_regs->pxssts
            & PXSSTS_DET)) {
        DEBUG("No device present on port %d or PHY connection not established\n", port);
        return;
    }
    /*
     * Set up port structure
     */
    ahci_port = (ahci_port_t*) kmalloc(sizeof(ahci_port_t));
    KASSERT(ahci_port);
    ahci_port->ahci_cntl = ahci_cntl;
    ahci_port->index = port;
    ahci_port->regs = ahci_port_regs;
    ahci_setup_port(ahci_port, command_tables[port_count]);
    /*
     * Reset device - this will also fill the signature register
     */
    DEBUG("Doing COMRESET\n");
    if (ahci_comreset(ahci_port)) {
        DEBUG("COMRESET not successful, assuming that no device is attached\n");
        ahci_teardown_port(ahci_port);
        kfree(ahci_port);
        return;
    }
 
    /*
     * Start device
     */
    
    if (ahci_init_device(ahci_port)) {
        ERROR("Could not start up device attached to port %d\n", port);
        ahci_teardown_port(ahci_port);
        kfree(ahci_port);
        return;
    }
   /*
     * If the signature is not that of an ATA
     * device, do not complete initialization
     */
    if (ahci_port_regs->pxsig != AHCI_SIG_ATA) {
        DEBUG("Port %d; this does not look like an ATA device (signature is %x)\n", port,ahci_port_regs->pxsig);
        ahci_teardown_port(ahci_port);
        kfree(ahci_port);
        return;
    }    
    DEBUG("Signature %x looks like an ATA hard disk, proceeeding with setup for port %d\n", ahci_port_regs->pxsig, port);
    /*
     * Issue IDENTIFY DEVICE
     */
    if(ahci_issue_sync_cmd(ahci_port, IDE_IDENTIFY_DEVICE, HD_READ, 0, 1,
            sector_buffer)) {
        ERROR("Could not execute command IDENTIFY DEVICE\n");
        ahci_teardown_port(ahci_port);
        kfree(ahci_port);
        return;
    }
    /*
     * Copy model string
     */
    strncpy(ahci_port->model, (const char*) &sector_buffer[27], 40);
    ahci_port->model[40] = 0;
    hd_fix_ata_string(ahci_port->model, 40);
    MSG("Detected model %s\n", ahci_port->model);
    /*
     * If necessary adapt chunk size as a workaround for QEMU reduced
     * PRDT size. Apparently, QEMU only allows for up to 168 entries
     * in the PRDT, i.e. 168 pages. Subtracting two pages for partial
     * chunks, this amounts to 1328 sectors
     */
    if (0 == strncmp(ahci_port->model, "QEMU", 4)) {
        MSG("Applying workaround for QEMU PRDT size issue\n");
        ahci_port->request_queue->chunk_size = 1328;
    }
    /*
     * Add item to list
     */
    LIST_ADD_END(ahci_port_list_head, ahci_port_list_tail, ahci_port);
    /*
     * Read partition table
     */
    DEBUG("Reading partition table\n");
    for (i = 0; i < AHCI_MAX_PARTITIONS; i++)
        ahci_port->partitions[i].used = 0;
    rc = hd_read_partitions(ahci_port->partitions, ahci_port->minor,
            ahci_read_sector, AHCI_MAX_PARTITIONS);
    if (rc < 0) {
        ERROR("Could not read partition table, rc=-%d\n", (-1)*rc);
    }
    port_count++;
}

/*
 * AHCI interrupt handler
 * Parameters:
 * @ir_context - interrupt context
 */
int ahci_handle_irq(ir_context_t* ir_context) {
    ahci_port_t* port;
    ahci_cntl_t* cntl;
    u32 pxis;
    u32 pxci;
    u32 is;
    u32 pxserr;
    int rc = 0;
    int timeout;
    u32 eflags;
    /*
     * Lock interrupt handler
     */
    spinlock_get(&handler_lock, &eflags);
    /*
     * Go through all controllers which have registered for this interrupt
     */
    LIST_FOREACH(ahci_cntl_list_head, cntl) {
        if (cntl->irq == ir_context->vector) {
            /*
             * Read interrupt status register on controller level
             */
            is = *(cntl->is);
            /*
             * Walk through list of ports and locate ports for
             * which the bit in the register is set
             */
            LIST_FOREACH(ahci_port_list_head, port) {
                if ((port->ahci_cntl == cntl) && (is & (1 << port->index))) {
                    /*
                     * Clear corresponding bit in the interrupt register on controller level
                     */
                    *(cntl->is) = ( 1 << port->index);
                    /*
                     * Read PxIS and PxSERR register from this port
                     */
                    pxis = port->regs->pxis;
                    pxserr = port->regs->pxserr;
                    pxci = port->regs->pxci;
                    if (__ahci_log)
                        PRINT("Interrupt from minor device %d\nPxCI = %x, PxIS = %x (DPS=%d, DSE=%d, DHRS=%d)\n",
                                port->minor, pxci, pxis,
                                (pxis >> 5) & 0x1,
                                (pxis >> 2) & 0x1,
                                pxis & 0x1);
                    if (IDE_STATUS_ERR & port->regs->pxtfd) {
                        ERROR("Error occurred during processing of request, PxTFD = %x\n", port->regs->pxtfd);
                        rc = EIO;
                    }
                    /*
                     * Do actual interrupt processing
                     */
                    if (pxis & 0x1) {
                        /*
                         * The following code is a workaround for a strange behaviour which I observed in QEMU. My understanding
                         * of the specs is that PxCI will be cleared before the interrupt is generated. This is also what happens on
                         * real hardware I have tested with
                         * In QEMU, we reach the interrupt handler sometimes before the bit in PxCI is cleared again and thus
                         * read stale data from the DMA buffer if we do not wait for PxCI to clear
                         */
                        timeout
                                = wait_for_reg(&(port->regs->pxci), 0x1, 0, 500);
                        if (__ahci_log && timeout) {
                            PRINT("PxCI bit clear after waiting for %d milliseconds\n", 500-timeout);
                        }
                        if (0 == timeout) {
                            PANIC("PxCI[0] does not clear, even though we have received an interrupt - what went wrong?\n");
                        }
                        /*
                         * Clear interrupt flag in PxIS. We need to do this before calling hd_handle_irq as
                         * during the actual interrupt processing, we might send the next request to the device and
                         * thus create the next interrupt while this handler is active. If PxIS is not yet cleared at
                         * this point, this interrupt is lost as the PCI interrupt line simply remains asserted
                         */
                        port->regs->pxis = 0x1;
                        /*
                         * Clear error register
                         */
                        port->regs->pxserr = pxserr;
                        /*
                         * Handle interrupt
                         */
                        hd_handle_irq(port->request_queue, rc);
                    }
                }
            }

        }
    }
    spinlock_release(&handler_lock, &eflags);
    return 0;
}

/*
 * Register a controller
 * Parameters:
 * @pci_dev - the corresponding PCI device
 */
static void ahci_register_cntl(const pci_dev_t* dev) {
    u32 ahci_base_address;
    ahci_cntl_t* ahci_cntl;
    ahci_port_t* port;
    u32 tmp;
    int i;
    int irq;
    MSG("Found AHCI controller (%d:%d.%d)\n", dev->bus->bus_id, dev->device, dev->function);
    /*
     * Set up everything else
     */
    ahci_cntl = (ahci_cntl_t*) kmalloc(sizeof(ahci_cntl_t));
    KASSERT(ahci_cntl);
    ahci_cntl->ahci_base_address = mm_map_memio(dev->bars[5] & 0xfffffff0,
            AHCI_REGISTER_SET_SIZE);
    KASSERT(ahci_cntl->ahci_base_address);
    ahci_base_address = ahci_cntl->ahci_base_address;
    ahci_cntl->cap = ((u32*) (ahci_base_address));
    ahci_cntl->ghc = ((u32*) (ahci_base_address + AHCI_GHC));
    ahci_cntl->is = ((u32*) (ahci_base_address + AHCI_IS));
    ahci_cntl->pi = ((u32*) (ahci_base_address + AHCI_PI));
    ahci_cntl->sclo = (*(ahci_cntl->cap) >> 24) & 0x1;
    /*
     * Check whether AHCI is enabled (bit 31 in GHC)
     * and set enable bit if not yet done. At the same time,
     * disable interrupts
     */
    tmp = *(ahci_cntl->ghc);
    tmp = tmp & (~AHCI_GHC_IE);
    tmp = tmp | AHCI_GHC_ENABLED;
    *(ahci_cntl->ghc) = tmp;
    tmp = *(ahci_cntl->ghc);
    if ((tmp & AHCI_GHC_IE) || (!(tmp & AHCI_GHC_ENABLED))) {
        ERROR("Could not set up AHCI controller\n");
        kfree(ahci_cntl);
        return;
    }
    LIST_ADD_END(ahci_cntl_list_head, ahci_cntl_list_tail, ahci_cntl);
    /*
     * Now go through the bits in the PI register and
     * set up  the port for each bit which is set
     */
    tmp = *(ahci_cntl->pi);
    for (i = 0; i < 32; i++) {
        if (tmp & (1 << i)) {
            ahci_register_port(ahci_cntl, i);
        }
    }
    /*
     * Set up interrupt processing for this port. We first clear
     * all pending interrupts and then enable interrupts
     */
    DEBUG("Clearing all interrupts and enabling interrupts on port level\n");
    mdelay(1);
    LIST_FOREACH(ahci_port_list_head, port) {
        if (port->ahci_cntl == ahci_cntl) {
            port->regs->pxis = ~((u32) 0);
            port->regs->pxie = 0x1;
        }
    }
    /*
     * Turn on interrupts in controller
     */
    tmp = *(ahci_cntl->ghc) | AHCI_GHC_IE;
    *(ahci_cntl->ghc) = tmp;
    /*
     * Register interrupt handler
     */
    irq = irq_add_handler_pci(ahci_handle_irq, 1, (pci_dev_t*) dev);
    if (irq < 0) {
        ERROR("Could not get valid interrupt vector for this device\n");
    }
    else {
        ahci_cntl->irq = irq;
        MSG("Using interrupt vector %x\n", ahci_cntl->irq);
    }
}

/*
 * Empty open and close functions
 */
static int ahci_open(minor_dev_t device) {
    return 0;
}
static int ahci_close(minor_dev_t device) {
    return 0;
}

/*
 * Common read/write processing
 * Parameter:
 * @minor_dev - the minor device to use
 * @blocks - number of blocks to read (block size 1024 bytes!)
 * @first_block - first block to read
 * @buffer - buffer in which we place the data
 * @rw - read (0) or write (1)
 * Return values:
 * number of bytes read upon successful completion
 * -ENODEV if the device is not valid
 * -EIO if a read/write error occured
 * -EINVAL if the read crosses a partition boundary or the block number is zero
 */
ssize_t ahci_rw(minor_dev_t minor, ssize_t blocks, ssize_t first_block,
        void* buffer, int rw) {
    u32 hd_blocks;
    u64 hd_first_block;
    ahci_port_t* port;
    int rc;
    hd_request_queue_t* request_queue;
    port = ahci_get_port(minor);
    if (0 == port) {
        return -ENODEV;
    }
    if (0 == blocks)
        return -EINVAL;
    /*
     * Convert the blocks in units of BLOCK_SIZE into
     * blocks in units of 512 bytes and handle partition mapping
     */
    int factor = BLOCK_SIZE / ATA_BLOCK_SIZE;
    hd_blocks = blocks * factor;
    hd_first_block = first_block * factor;
    if (minor % AHCI_MAX_PARTITIONS) {
        hd_first_block
                += port->partitions[minor % AHCI_MAX_PARTITIONS].first_sector;
        if (hd_first_block + hd_blocks - 1 > port->partitions[minor
                % AHCI_MAX_PARTITIONS].last_sector) {
            return -EINVAL;
        }
    }
    /*
     * Check for kernel parameter pata_ro. We PANIC if we try to write and the parameter is set,
     * but do not return after the PANIC, thus the user can override this using the debugger
     * by just typing exit
     */
    if  ((HD_WRITE==rw) &&  (1==params_get_int("ahci_ro"))) {
        PANIC("ahci_ro is set\nDetected attempt to write %d sectors starting at sector %d\n", hd_blocks, hd_first_block);
    }
    /*
     * Determine request queue to use
     */
    request_queue = port->request_queue;
    /*
     * Start processing
     */
    rc = hd_rw(request_queue, hd_blocks, hd_first_block, rw, buffer, minor);
    if (rc < 0)
        return rc;
    return blocks * BLOCK_SIZE;
}

/*
 * Write a given number of blocks to the device
 * Parameters:
 * @minor - minor device number
 * @blocks - blocks to write
 * @first_block - first block to write
 * @buffer - pointer to buffer to be used
 * Return value:
 * number of bytes written or a negative error code
 */

ssize_t ahci_write(minor_dev_t minor, ssize_t blocks, ssize_t first_block,
        void* buffer) {
    return ahci_rw(minor, blocks, first_block, buffer, HD_WRITE);
}

/*
 * This is the read interface function for the AHCI controller and
 * thus the main entry point used by the kernel to submit a read
 * request.
 * Parameter:
 * @minor_dev - the minor device to use
 * @blocks - number of blocks to read (block size 1024 bytes!)
 * @first_block - first block to read
 * @buffer - buffer in which we place the data
 * Return values:
 * number of bytes read upon successful completion
 * -ENODEV if the device is not valid
 * -EIO if a read/write error occured
 * -EINVAL if the read crosses a partition boundary or the block number is zero
 */
ssize_t ahci_read(minor_dev_t minor, ssize_t blocks, ssize_t first_block,
        void* buffer) {
    return ahci_rw(minor, blocks, first_block, buffer, HD_READ);
}

/*
 * Initialize the AHCI driver. This function scans the PCI bus for all AHCI controllers and
 * builds up the internal data structures of the driver
 */
void ahci_init() {
    /*
     * Scan PCI devices with base class 0x1 and sub class 0x1
     */
    pci_query_by_class(&ahci_register_cntl, PCI_BASE_CLASS_MASS_STORAGE, AHCI_SUB_CLASS);
    /*
     * and register with device manager
     */
    ops.close = ahci_close;
    ops.open = ahci_open;
    ops.read = ahci_read;
    ops.write = ahci_write;
    dm_register_blk_dev(MAJOR_AHCI, &ops);
    spinlock_init(&handler_lock);
}

/*
 * Return the model name for AHCI drive nr. n or null if there is no
 * such drive. Numbering starts at zero
 */
char* ahci_drive_name(int n) {
    int i = 0;
    ahci_port_t* port;
    LIST_FOREACH(ahci_port_list_head, port) {
        if (i == n)
            return port->model;
        i++;
    }
    return 0;
}

/*
 * Return the 1k blocks already processed by AHCI devices
 */
u32 ahci_processed_kbyte() {
    u32 blocks = 0;
    ahci_port_t* port;
    hd_request_queue_t* queue;
    LIST_FOREACH(ahci_port_list_head, port) {
        queue = port->request_queue;
        blocks += ((queue->processed_blocks * queue->block_size) / 1024);
    }
    return blocks;
}

/***************************************************************
 * Everything below this line is for debugging only            *
 **************************************************************/

/*
 * Print a list of all registered AHCI ports
 */
void ahci_print_ports() {
    ahci_port_t* port;
    int i;
    PRINT("Model\n");
    PRINT("Port PxCLB     PxSSTS    PxIS      PxCMD     PxSERR    PxCI      BSY DRQ ERR\n");
    PRINT("-----------------------------------------------------------------------------\n");
    LIST_FOREACH(ahci_port_list_head, port) {
        PRINT("%s\n", port->model);
        PRINT("%h   %x %x %x %x %x %x %d   %d   %d\n", (port->minor) / AHCI_MAX_PARTITIONS, port->regs->pxclb,
                port->regs->pxssts, port->regs->pxis, port->regs->pxcmd, port->regs->pxserr, port->regs->pxci,
                (port->regs->pxtfd >> 7) & 0x1, (port->regs->pxtfd >> 3) & 0x1, port->regs->pxtfd & 0x1);
        PRINT("\n");
        PRINT("               First                Last\n");
        PRINT("Port  Part.    Sector               Sector                 Size (MB)\n");
        PRINT("----------------------------------------------------------------------\n");
    }
    LIST_FOREACH(ahci_port_list_head, port) {
        for (i = 1; i <= AHCI_MAX_PARTITIONS; i++) {
            if (port->partitions[i].used == 1) {
                PRINT("%h    %h       %P  %P    %d\n",
                        (port->minor) / AHCI_MAX_PARTITIONS, i,
                        port->partitions[i].first_sector, port->partitions[i].last_sector,
                        (u32) (port->partitions[i].last_sector+1-port->partitions[i].first_sector)/2048);
            }
        }
    }
}

/*
 * Print the content of the request queues
 */
void ahci_print_queue() {
    int j;
    hd_request_queue_t* queue = 0;
    hd_request_t* request = 0;
    ahci_port_t* port;
    LIST_FOREACH(ahci_port_list_head, port) {
        queue = port->request_queue;
        if (queue->head != queue->tail) {
            PRINT("Head: %d  Tail: %d\n", queue->head % HD_QUEUE_SIZE, queue->tail % HD_QUEUE_SIZE);
            PRINT("-----------------------------------\n");
            PRINT("Slot   R/W   Blocks        Task    Sem         STS  IRQ  First block\n");
            PRINT("---------------------------------------------------------------------\n");
            for (j = queue->head; j < queue->tail; j++) {
                request = &queue->queue[j % HD_QUEUE_SIZE];
                PRINT("%h     %h    %x     %w    %x     %d    %d  %P\n", 
                       j % HD_QUEUE_SIZE, 
                       request-> rw, 
                       request->blocks, 
                       request->task_id,
                       request->semaphore, request->status, 
                       request->submitted_by_irq, 
                       request->first_block);
            }
        }
    }
}


/*
 * Do some tests
 */
void ahci_do_tests() {
    int rc;
    u32* mybuffer = (u32*) sector_buffer;
    char* ahci_test_buffer = (char*) kmalloc_aligned(32 * 1024 * 1024, 4);
    KASSERT(ahci_test_buffer);
    if (0 == ahci_get_port(0))
        return;
    if (0 == ahci_get_port(1))
        return;
    PRINT("Starting AHCI driver tests\n");
    PRINT("--------------------------\n");
    PRINT("Trying to read first block from minor device 0\n");
    __ahci_log = 1;
    rc = ahci_read(0, 1, 0, (void*) mybuffer);
    if (rc < 0)
        ERROR("Read failed\n");
    __ahci_log = 0;
    PRINT("Printing bytes 504 - 511 \n");
    int bc;
    for (bc = 0; bc < 8; bc++) {
        PRINT("%h ", ((char*)mybuffer)[bc+504]);
    }
    PRINT("\n");
    PRINT("Now reading a large block of 1 MB\n");
    __ahci_log = 1;
    rc = ahci_read(0, 1024, 0, ahci_test_buffer);
    __ahci_log = 0;
    if (rc < 0)
        ERROR("Read failed\n");
    PRINT("\n");
    if (strncmp("QEMU", ahci_get_port(0)->model, 4)) {
        PRINT("Reading a large block of 32 MB\n");
        memset(ahci_test_buffer, 0, 32 * 1024 * 1024);
        __ahci_log = 1;
        rc = ahci_read(0, 65536 / 2, 0, ahci_test_buffer);
        __ahci_log = 0;
        if (rc < 0)
            ERROR("Read failed\n");
        PRINT("First 8 bytes of sector 2050:\n");
        for (bc = 0; bc < 8; bc++) {
            PRINT("%h ", ((char*)ahci_test_buffer)[bc+2050*512]);
        }
        PRINT("\n");
    }
    else {
        PRINT("Detected QEMU drive, skipping maximum sector number read test\n");
    }
    PRINT("Now reading 10*1024 times 64 kB");
    for (bc = 0; bc < 10 * 1024; bc++) {
        if (ahci_read(0, 64, (bc % 1024) * 64, ahci_test_buffer) < 0) {
            ERROR("Error while reading from drive at block %d (bc=%d)\n", bc*64, bc);
        }
        if (0 == (bc % 512))
            PRINT(".");
    }
    PRINT("\n");
    kfree(ahci_test_buffer);
}
