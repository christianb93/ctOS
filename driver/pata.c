/*
 * pata.c
 *
 * This module implements the part of the hard disk driver which is specific to the PATA protocol
 *
 */

#include "pata.h"
#include "pci.h"
#include "debug.h"
#include "io.h"
#include "lib/string.h"
#include "irq.h"
#include "hd.h"
#include "timer.h"
#include "mm.h"
#include "dm.h"
#include "drivers.h"
#include "keyboard.h"
#include "params.h"

static char* __module = "PATA  ";

/*
 * The public interface
 */
static blk_dev_ops_t ops;

/*
 * Array to hold all controllers
 */
static pata_cntl_t cntl[PATA_MAX_CNTL];
static int cntl_count = 0;

/*
 * This is an array of PRD tables. Each table contains 8192 entries and
 * is 64 kB in size. We have one array for each slot in the request queue
 * The entire table is aligned to a 64 kB boundary so that the same holds
 * for each entry
 */
static pata_dma_prd_t
        __attribute__ ((aligned(65536))) prdt[HD_QUEUE_SIZE][PATA_PRDT_COUNT];
/*
 * Array to hold channels. For each controller, we reserve space
 * for two channels (primary and secondary IDE channel)
 */
static pata_channel_t channels[PATA_MAX_CNTL * 2];

/*
 * Corresponding request queues, one per channel
 */
static hd_request_queue_t request_queues[PATA_MAX_CNTL * 2];

/*
 * Array to hold drives. To each channel, at most two drives
 * (master and slave) are attached
 */
static pata_drive_t drives[PATA_MAX_CNTL * 4];

/*
 * Array to hold partitions. We are able to handle at most
 * 16 partitions per drive
 */
static hd_partition_t partitions[PATA_PART_DRIVE * PATA_MAX_CNTL * 4];

/*
 * Validate a minor device number. Return 1 if
 * the device number is valid
 * Parameter:
 * @minor - the minor device number
 * Return value:
 * 1 if device is valid
 * 0 if device is not valid
 */
static int pata_device_valid(minor_dev_t minor) {
    int index = minor / 16;
    if (index >= PATA_MAX_CNTL * 4)
        return 0;
    /*
     * Partition 0 is valid if and only if the
     * drive is valid
     */
    if (0 == (minor & 0xf))
        return drives[index].used;
    return partitions[minor].used;
}

/*
 * Generic function to wait for the status of a channel
 * to equal the specified value when masked with the specified bitmask
 * Parameters:
 * @channel - channel to use
 * @bit_mask - bit mask for which to check
 * @value - value which we wait for
 * @timeout - timeout value in microseconds
 * @final_status - pointer to u8 in which we store the final status
 * Returns 0 if condition was met and 1 if timeout occurred
 * Return the final status in *status
 */
static int wait_for_status_register(pata_channel_t* channel, u8 bit_mask,
        u8 value, int timeout, u8* final_status) {
    int wait;
    u8 status;
    for (wait = timeout; (wait > 0); wait--) {
        status = inb(channel->ata_command_block + IDE_COMMAND_REGISTER);
        udelay(1);
        if (value == (status & bit_mask)) {
            break;
        }
    }
    *final_status = status;
    if (wait)
        return 0;
    return 1;
}

/*
 * Interrupt handler for the PATA driver
 * Parameter:
 * @ir_context - the IR context
 */
static int pata_handle_irq(ir_context_t* ir_context) {
    int channel;
    u8 status;
    u8 ide_status;
    int rc = 0;
    /*
     * First locate channel which has raised the interrupt
     */
    for (channel = 0; channel < 2 * PATA_MAX_CNTL; channel++) {
        if ((channels[channel].vector == ir_context->vector)
                && (channels[channel].used)) {
            status = inb(channels[channel].bus_master_status);
            /*
             * If this channel has no pending interrupt, proceed with next channel
             */
            if (0 == (status & BMS_INT))
                break;
            /*
             * Report error if needed
             */
            if (status & BMS_ERROR) {
                ERROR("Error during bus master operation, bus master status is %x\n", status);
                ERROR("Content of IDE error register is: %x\n", inb(channels[channel].ata_command_block + IDE_ERROR_REGISTER));
                ERROR("Physical address of PRDT is %x\n", inl(channels[channel].bus_master_prdt));
                rc = EIO;
            }
            /*
             * This channel has raised an interrupt - process it. First
             * read from the IDE status register to clear the interrupt there
             */
            DEBUG("Processing interrupt from channel %d\n", channel);
            ide_status = inb(channels[channel].ata_command_block
                    + IDE_COMMAND_REGISTER);
            if (ide_status & IDE_STATUS_ERR) {
                ERROR("Error while reading from drive, status is %x\n", ide_status);
                rc = EIO;
            }
            /*
             * Then clear INT flag in bus master status by writing to it
             */
            outb(BMS_INT, channels[channel].bus_master_status);
            /*
             * and call hd_handle_irq
             */
            hd_handle_irq(request_queues + channel, rc);
        }
    }
    return 0;
}

/*
 * Probe a drive for a given channel. This function assumes that we have done a
 * soft reset on the controller first
 * Parameter:
 * @master_slave - 0 = probe master, 1 = probe slave
 * @channel - the channel to which the device is attached
 * @data - a 512 byte buffer (256 words) in which the result of IDENTIFY DEVICE is stored
 * if a drive is found
 * Return value:
 * 0 - there is no drive
 * 1 - there is a drive
 */
static int pata_probe_drive(int master_slave, pata_channel_t* channel,
        u16* data) {
    u8 status;
    int i;
    /*
     * First select drive by writing to the device register. Bit 4 of this register
     * selects the drive, where 0 is master and 1 is slave. Older versions of the specs say that
     * bits 7 and 5 need to be 1, whereas in newer versions, they are obsolete.  So we
     * set them to 1 to make old drives happy. Therefore 0xa0 is used to select the master
     * and 0xb0 is used to select the slave.
     */
    outb(IDE_DEVICE_LBA + master_slave * IDE_DEVICE_SELECT,
            channel->ata_command_block + IDE_DEVICE_REGISTER);
    /*
     * Now read status register
     * and wait until BSY bit clears
     * Note that we cannot wait for DRDY here as
     * ATAPI devices do not seem to set this bit
     */
    if (wait_for_status_register(channel, IDE_STATUS_BSY, 0x0, PATA_TIMEOUT_PROBE_SELECT, &status)) {
        /* Timeout. Assume that drive is not present */
        DEBUG("Timeout after drive selection - device %d not present\n", master_slave);
        return 0;
    }
    /*
     * We now send the command IDENTIFY DEVICE to the drive
     * and wait until BSY is cleared and DRQ is asserted
     */
    outb(IDE_IDENTIFY_DEVICE, channel->ata_command_block + IDE_COMMAND_REGISTER);
    udelay(1);
    if (wait_for_status_register(channel, IDE_STATUS_BSY + IDE_STATUS_DRQ
            + IDE_STATUS_ERR, IDE_STATUS_DRQ, PATA_TIMEOUT_PROBE_IDLE, &status)) {
        DEBUG("Timeout while processing IDENTIFY DEVICE - assuming no or ATAPI device\n");
        return 0;
    }
    for (i = 0; i < 256; i++)
        data[i] = inw(channel->ata_command_block + IDE_DATA_REGISTER);
    /*
     * Make sure that LBA is supported, i.e. that bit 9 of word 49 is set
     */
    if (0==(data[IDE_IDENTIFY_DEVICE_CAP_WORD] & IDE_IDENTIFY_DEVICE_CAP_LBA)) {
        MSG("Ignoring device %d as it does not support LBA\n", master_slave);
        return 0;
    }
    return 1;
}

/*
 * Do a soft reset on a controller and disable interrupts
 * Parameter:
 * @channel - the channel to be reset
 * Return value:
 * 0 upon success
 * 1 if reset failed
 */
static int pata_reset_channel(pata_channel_t* channel) {
    u8 status;
    /*
     * Do a software reset and wait until controller becomes ready again,
     * i.e. until BSY = 0 (bit 7 of status register).
     * According to the ATA specification, a software
     * reset is done by asserting bit SRST in the
     * device control register. We have to set the
     * bit to 1, wait for 5 microseconds, set the bit
     * to zero again, wait for 2ms and start polling
     * the status register until BSY is cleared
     * Note that by writing 0x2, we also disable interrupts
     * for the time being
     */
    outb(IDE_DEVICE_CONTROL_SRST + 0x2, channel->ata_alt_status);
    mdelay(1);
    outb(IDE_DEVICE_CONTROL_NIEN, channel->ata_alt_status);
    /*
     * On old devices, we might actually need a rather high timeout value
     */
    if (wait_for_status_register(channel, IDE_STATUS_BSY, 0, PATA_TIMEOUT_RESET, &status)) {
        DEBUG("Reset timed out\n");
        return 1;
    }
    return 0;
}

/*
 * Set up LBA registers and sector count register
 * for use with READ or WRITE command
 * Parameters:
 * @channel - the channel to use
 * @lba - 32 bit LBA address
 * @use_48bit_lba - whether we want to use 48 bit LBA
 * @sc - sector count
 * @master_slave - 0 for master, 1 for slave
 * We do not check whether the drive actually supports
 * 48 bit LBA, caller needs to do this
 */
static void pata_setup_params(pata_channel_t* channel, u32 lba,
        int use_48bit_lba, u16 sc, int master_slave) {
    if (use_48bit_lba) {
        /*
         * Write upper parts of LBA low, LBA mid and LBA high. As we only support
         * up to 32 bits, LBA mid and LBA high upper parts are zero, LBA low is
         * bits 24 - 31
         */
        outb((lba >> 24) & 0xff, channel->ata_command_block
                + IDE_LBA_LOW_REGISTER);
        outb(0x0, channel->ata_command_block + IDE_LBA_MID_REGISTER);
        outb(0x0, channel->ata_command_block + IDE_LBA_HIGH_REGISTER);
        outb(sc >> 8, channel->ata_command_block + IDE_SECTOR_COUNT_REGISTER);
    }
    outb(lba & 0xff, channel->ata_command_block + IDE_LBA_LOW_REGISTER);
    outb((lba >> 8) & 0xff, channel->ata_command_block + IDE_LBA_MID_REGISTER);
    outb((lba >> 16) & 0xff, channel->ata_command_block + IDE_LBA_HIGH_REGISTER);
    /*
     * Set LBA bit and fill bits 0-3 of device register
     * with bits 24-27 of LBA if we do not use 48 bits LBA
     */
    if (use_48bit_lba) {
        outb(IDE_DEVICE_OBS1 + IDE_DEVICE_OBS2 + IDE_DEVICE_LBA + master_slave
                * IDE_DEVICE_SELECT, channel->ata_command_block
                + IDE_DEVICE_REGISTER);
    }
    else {
        outb(IDE_DEVICE_OBS1 + IDE_DEVICE_OBS2 + IDE_DEVICE_LBA + (master_slave
                * IDE_DEVICE_SELECT) + ((lba >> 24) & 0xf),
                channel->ata_command_block + IDE_DEVICE_REGISTER);
    }
    /*
     * Set sector count bits 0-7
     */
    outb(sc & 0xff, channel->ata_command_block + IDE_SECTOR_COUNT_REGISTER);
}

/*
 * Read sectors in PIO mode. This function does not use any
 * interrupts and does not put the currently running thread to sleep,
 * it can thus be used safely at early boot time before interrupts
 * and multitasking are enabled.
 * Note that partitions are not supported, i.e we always read
 * from the raw device
 * Parameters:
 * @minor - device to read from
 * @lba - sector which we want to read
 * @sectors - number of sectors to read
 * @buffer - sector buffer
 * Return value:
 * number of bytes read
 * -ENODEV if the minor device is not valid
 * -EIO if a timeout occurred
 * -EINVAL if 48 bit LBA is not supported but needed
 */
static int pata_read_sector(minor_dev_t minor, u32 lba, void* buffer) {
    u8 status;
    u8 cmd;
    pata_channel_t* channel;
    pata_drive_t* drive;
    int use_48bit_lba;
    int i;
    /*
     * First determine channel and drive
     */
    if (0 == pata_device_valid(minor)) {
        ERROR("Invalid minor device %x\n", minor);
        return -ENODEV;
    }
    int drive_no = minor >> 4;
    drive = drives + drive_no;
    channel = &(channels[drive_no / 2]);
    /*
     * Make sure that interrupts are disabled
     */
    outb(IDE_DEVICE_CONTROL_NIEN, channel->ata_alt_status);
    /*
     * Wait until BSY and DRQ are cleared
     */
    if (wait_for_status_register(channel, IDE_STATUS_BSY + IDE_STATUS_DRQ, 0x0,
            PATA_TIMEOUT_IDLE, &status)) {
        return -EIO;
    }
    /*
     * Select device
     */
    outb((drive_no & 0x1) * IDE_DEVICE_SELECT, channel->ata_command_block
            + IDE_DEVICE_REGISTER);
    if (wait_for_status_register(channel, IDE_STATUS_BSY + IDE_STATUS_DRQ, 0x0,
            PATA_TIMEOUT_IDLE, &status)) {
        return -EIO;
    }
    /*
     * Check whether we use 48 bit LBA
     */
    use_48bit_lba = 1;
    cmd = IDE_READ_SECTORS_EXT;
    if (0 == drive->lba_long) {
        if (lba >> 28) {
            ERROR("48 bit LBA needed but not supported\n");
            return -EINVAL;
        }
        use_48bit_lba = 0;
        cmd = IDE_READ_SECTORS;
    }
    /*
     * Set up LBA and sector count register and
     * issue read command
     */
    pata_setup_params(channel, lba, use_48bit_lba, 1, drive->master_slave);
    outb(cmd, channel->ata_command_block + IDE_COMMAND_REGISTER);
    /*
     * Wait until BSY and error are cleared and DRQ is set
     */
    if (wait_for_status_register(channel, IDE_STATUS_BSY + IDE_STATUS_ERR
            + IDE_STATUS_DRQ, IDE_STATUS_DRQ, PATA_TIMEOUT_IDLE, &status)) {
        return -EIO;
    }
    /*
     * Finally read data from drive
     */
    for (i = 0; i < (ATA_BLOCK_SIZE / 2) ; i++)
        *((u16*) buffer + i) = inw(channel->ata_command_block
                + IDE_DATA_REGISTER);
    return ATA_BLOCK_SIZE;
}

/*
 * Set up the drive structure for a specific drive
 * Parameters:
 * @index - the index (0-7) of the channel
 * @drive - the drive structure to be filled
 * @master_slave - 0 = master, 1 = slave
 * @data - 256 words containing the output of the command IDENTIFY DEVICE
 */
static void pata_setup_drive(u32 index, pata_drive_t* drive, int master_slave,
        u16* data) {
    drive->used = 1;
    drive->master_slave = master_slave;
    /*
     * Get some info from the output
     * of IDENTIFY DEVICE / IDENTIFY PACKET DEVICE
     */
    strncpy(drive->model, (const char*) &data[27], 40);
    strncpy(drive->serial, (const char*) &data[10], 20);
    hd_fix_ata_string(drive->model, 40);
    hd_fix_ata_string(drive->serial, 20);
    drive->model[40] = 0;
    drive->serial[20] = 0;
    if (data[83] & 1024) {
        drive->lba_long = 1;
    }
    else
        drive->lba_long = 0;
    MSG("IDE cntl. %d, channel %d:  %s\n", index / 2, index % 2,drive->model);
    /*
     * Apparently, the QEMU hard disk controller only accepts a PRDT size of
     * up to 4096 bytes, i.e. 512 entries. As a workaround, set the chunk size
     * to 4080 sectors
     */
    if (0 == strncmp(drive->model, "QEMU", 4)) {
        MSG("Applying workaround for reduced PRDT size in QEMU\n"); 
        request_queues[index].chunk_size = 4080; 
    }
    /*
     * Set queue chunk size to 255 sectors times 512 bytes if only 28
     * bit LBA is supported
     */
    if (0 == drive->lba_long) {
        MSG("Applying workaround for 28 bit LBA mode\n");
        request_queues[index].chunk_size = 255 * 512;
    }
}

/*
 * Probe and register all drives for a given channel
 * Parameter:
 * @pci_dev - the PCI device
 * @index - the index of the channel (0-7)
 */
static void pata_register_drives(pci_dev_t* pci_dev, int index) {
    u16 data[256];
    int rc;
    /*
     * Do a soft reset on channel
     */
    if (pata_reset_channel(channels + index)) {
        DEBUG("Soft reset on channel %d failed, assuming that no device is present\n", index);
        return;
    }
    DEBUG("Probing master for channel %d\n", index);
    if (pata_probe_drive(ATA_DEVICE_MASTER, channels + index, data)) {
        /*
         * Found master. Add drive at index index*2
         */
        pata_setup_drive(index, drives + index * 2, ATA_DEVICE_MASTER, data);
        /*
         * and read partition table
         */
        rc = hd_read_partitions(partitions + PATA_PART_DRIVE * index * 2, index
                * 2 * PATA_PART_DRIVE, pata_read_sector, PATA_PART_DRIVE);
        if (rc < 0) {
            ERROR("Could not read partition table, rc=-%d\n", (-1)*rc);
        }
    }
    /*
     * Repeat the same thing for the slave. We actually need another soft reset,
     * as otherwise the data of the master might remain in the registers and
     * we will detect a non-existing slave
     */
    DEBUG("Probing slave for channel %d\n", index);
    if (pata_reset_channel(channels + index)) {
        DEBUG("Soft reset on channel %d failed, assuming that no device is present\n", index);
        return;
    }
    if (pata_probe_drive(ATA_DEVICE_SLAVE, channels + index, data)) {
        pata_setup_drive(index, drives + index * 2 + 1, ATA_DEVICE_SLAVE, data);
        rc = hd_read_partitions(partitions + PATA_PART_DRIVE * (index * 2 + 1),
                (index * 2 + 1) * PATA_PART_DRIVE, pata_read_sector,
                PATA_PART_DRIVE);
        if (rc < 0) {
            ERROR("Could not read partition table, rc=-%d\n", (-1)*rc);
        }
    }
}

/*
 * Setup a channel, i.e. fill an empty channel structure with data
 * Parameter:
 * @channel - the channel structure to be filled
 * @cntl - the controller to which the channel is attached
 * @pci_dev - the PCI device
 * @primary - true if channel is primary ATA channel
 */
static void pata_setup_channel(pata_channel_t* channel, pata_cntl_t* cntl,
        pci_dev_t* pci_dev, int primary) {
    channel->used = 1;
    /*
     * Make sure that the DMA bus mastering configuration bit
     * is set for this device
     */
    pci_enable_bus_master_dma(pci_dev);
    /*
     * Set up channel
     */
    if (primary) {
        channel->bus_master_command = cntl->bus_master_base
                + IDE_BUS_MASTER_COMMAND_PRIMARY;
        channel->bus_master_status = cntl->bus_master_base
                + IDE_BUS_MASTER_STATUS_PRIMARY;
        channel->bus_master_prdt = cntl->bus_master_base
                + IDE_BUS_MASTER_PRDT_PRIMARY;
        channel->operating_mode = ((pci_dev->prog_if) & IDE_MODE_PRIMARY)
                / IDE_MODE_PRIMARY;
        if (IDE_MODE_NATIVE == channel->operating_mode) {
            /*
             * In native mode, get address of command block and alternate status register from
             * BARs according to PCI IDE specification
             */
            channel->ata_command_block = ((pci_dev->bars[0]) & 0xfffffffc);
            channel->ata_alt_status = ((pci_dev->bars[1]) & 0xfffffffc) + 0x2;
        }
        else {
            channel->ata_command_block = IDE_LEGACY_PRIMARY_DATA_REGISTER;
            channel->ata_alt_status = IDE_LEGACY_PRIMARY_ALT_STATUS_REGISTER;
        }
    }
    else {
        channel->bus_master_command = cntl->bus_master_base
                + IDE_BUS_MASTER_COMMAND_SECONDARY;
        channel->bus_master_status = cntl->bus_master_base
                + IDE_BUS_MASTER_STATUS_SECONDARY;
        channel->bus_master_prdt = cntl->bus_master_base
                + IDE_BUS_MASTER_PRDT_SECONDARY;
        channel->operating_mode = ((pci_dev->prog_if) & IDE_MODE_SECONDARY)
                / IDE_MODE_SECONDARY;
        if (IDE_MODE_NATIVE == channel->operating_mode) {
            /*
             * In native mode, get address of command block and alternate status register from
             * BARs according to PCI IDE specification
             */
            channel->ata_command_block = ((pci_dev->bars[2]) & 0xfffffffc);
            channel->ata_alt_status = ((pci_dev->bars[3]) & 0xfffffffc) + 0x2;
        }
        else {
            channel->ata_command_block = IDE_LEGACY_SECONDARY_DATA_REGISTER;
            channel->ata_alt_status = IDE_LEGACY_SECONDARY_ALT_STATUS_REGISTER;
        }
    }
    /*
     * Set up interrupt. If we are in native mode, use IRQ stored in
     * PCI configuration space or MP table, otherwise use legacy interrupts
     */
    if (IDE_MODE_NATIVE == channel->operating_mode) {
        MSG("Requesting interrupt handler for device %d:%d, pin %d\n", pci_dev->bus->bus_id, pci_dev->device, pci_dev->irq_pin);
        channel->vector = irq_add_handler_pci(pata_handle_irq, 1, pci_dev);
    }
    else {
        if (primary) {
            MSG("Requesting interrupt handler for legacy IRQ %d\n", IDE_LEGACY_IRQ_PRIMARY);
            if ((channel->vector = irq_add_handler_isa(pata_handle_irq, 1, IDE_LEGACY_IRQ_PRIMARY, 0)) < 0) {
                ERROR("Unable to register handler, rc = %d\n", channel->vector);
            }

        }
        else {
            MSG("Requesting interrupt handler for legacy IRQ %d\n", IDE_LEGACY_IRQ_SECONDARY);
            if ((channel->vector = irq_add_handler_isa(pata_handle_irq, 1, IDE_LEGACY_IRQ_SECONDARY, 0)) < 0) {
                ERROR("Unable to register interrupt handler, rc = %d\n", channel->vector);
            }
        }
    }
}

/*
 * Register all channels for a given controller.
 * Parameter:
 * @pci_dev - the PCI device
 * @index - the number of the controller (0-3)
 */
static void pata_register_channels(pci_dev_t* pci_dev, int index) {
    u8 bus_master_status_value_primary;
    u8 bus_master_status_value_secondary;
    int index_primary;
    int index_secondary;
    /*
     * First read simplex bit in bus master status register. If this
     * bit is set, do not register any of the channels
     */
    bus_master_status_value_primary = inb(cntl[index].bus_master_base
            + IDE_BUS_MASTER_STATUS_PRIMARY);
    bus_master_status_value_secondary = inb(cntl[index].bus_master_base
            + IDE_BUS_MASTER_STATUS_SECONDARY);
    if ((bus_master_status_value_primary >> 7)
            || (bus_master_status_value_secondary >> 7)) {
        MSG("Ignoring controller as simplex bit is set\n");
        return;
    }
    /*
     * Now register both channels
     */
    index_primary = index << 1;
    index_secondary = index_primary + 1;
    pata_setup_channel(channels + index_primary, cntl + index, pci_dev, 1);
    pata_register_drives(pci_dev, index_primary);
    pata_setup_channel(channels + index_secondary, cntl + index, pci_dev, 0);
    pata_register_drives(pci_dev, index_secondary);
    /*
     * Finally write 0 to the alternate status registers of both channels to turn on
     * interrupts (i.e. set nIEN flag to zero), but read from status before to
     * clear all pending interrupts
     */
    inb(channels[index_primary].ata_command_block + IDE_COMMAND_REGISTER);
    inb(channels[index_secondary].ata_command_block + IDE_COMMAND_REGISTER);
    outb(0x0, channels[index_primary].ata_alt_status);
    outb(0x0, channels[index_secondary].ata_alt_status);
}

/*
 * Callback function used by the PCI bus driver. This function is called once for
 * each PCI IDE controller on the bus
 * Parameter:
 * @pci_dev - the PCI device
 */
static void pata_register_cntl(const pci_dev_t* pci_dev) {
    int index;
    /*
     * Only process devices which support bus mastering IDE
     */
    if (pci_dev->prog_if >> 7) {
        if (cntl_count >= PATA_MAX_CNTL) {
            MSG("Found more than %d controller, this is not supported\n", PATA_MAX_CNTL);
            return;
        }
        MSG("Found IDE controller at device %d:%d.%d, command register is %x\n", pci_dev->bus->bus_id, pci_dev->device, pci_dev->function, pci_dev->command);
        cntl_count++;
        index = cntl_count - 1;
        cntl[index].used = 1;
        cntl[index].bus_master_base = (pci_dev->bars[4]) & 0xfffffffc;
        /*
         * Now scan controller to determine channels and devices
         */
        pata_register_channels((pci_dev_t*) pci_dev, index);
    }
    else
        MSG("Controller does not support PCI IDE specification\n");
}

/*
 * Submit a request to a PATA channel. The channel to use is determined from the
 * minor device number stored in the request
 * Parameter:
 * @request_queue - the queue on which the request has been placed
 * @request - the request to be submitted
 */
static void pata_submit_request(hd_request_queue_t* request_queue,
        hd_request_t* request) {
    u8 status;
    u8 cmd;
    pata_channel_t* channel;
    pata_drive_t* drive;
    int use_48bit_lba;
    u8 temp;
    /*
     * First determine channel and drive
     */
    if (0 == pata_device_valid(request->minor_device)) {
        PANIC("Invalid minor device %x\n", request->minor_device);
    }
    int drive_no = (request->minor_device) >> 4;
    drive = drives + drive_no;
    channel = &(channels[drive_no / 2]);
    /*
     * Load address of PRDT into PRDT pointer register
     */
    outl((u32) prdt[request - request_queue->queue], channel->bus_master_prdt);
    /*
     * Set read/write control bit according to direction of operation (write to drive is read
     * from the DMA controllers point of view) and clear start/stop bit
     */
    if (HD_READ == request->rw)
        outb(BMC_WRITE, channel->bus_master_command);
    else
        outb(0, channel->bus_master_command);
    /*
     * Write 1 to interrupt bit and error bit in bus master status
     * register to clear them
     */
    outb(BMS_INT + BMS_ERROR, channel->bus_master_status);
    /*
     * Wait until BSY and DRQ are cleared
     */
    if (wait_for_status_register(channel, IDE_STATUS_BSY + IDE_STATUS_DRQ, 0x0,
            PATA_TIMEOUT_IDLE, &status)) {
        PANIC("Drive not ready - giving up, last status is %x\n", status);
    }
    /*
     * Select device
     */
    outb((drive_no & 0x1) * IDE_DEVICE_SELECT, channel->ata_command_block
            + IDE_DEVICE_REGISTER);
    if (wait_for_status_register(channel, IDE_STATUS_BSY + IDE_STATUS_DRQ, 0x0,
            PATA_TIMEOUT_IDLE, &status)) {
        PANIC("Drive not ready - giving up\n");
    }
    /*
     * Make sure that interrupts are enabled by writing
     * 0x0 to device control register
     */
    outb(0x0, channel->ata_alt_status);
    /*
     * Check whether we use 48 bit LBA
     */
    use_48bit_lba = 1;
    if (0 == drive->lba_long) {
        if (request->first_block >> 28)
            PANIC("48 bit LBA not supported, but needed\n");
        use_48bit_lba = 0;
    }
    /*
     * Set up LBA and sector count register and
     * issue read or write command
     */
    pata_setup_params(channel, request->first_block, use_48bit_lba,
            request->blocks, drive->master_slave);
    if (HD_WRITE == request->rw) {
        if (use_48bit_lba) {
            cmd = IDE_WRITE_DMA_EXT;
        }
        else {
            cmd = IDE_WRITE_DMA;
        }
    }
    else {
        if (use_48bit_lba) {
            cmd = IDE_READ_DMA_EXT;
        }
        else {
            cmd = IDE_READ_DMA;
        }
    }
    outb(cmd, channel->ata_command_block + IDE_COMMAND_REGISTER);
    /*
     * Start bus master transfer
     */
    temp = inb(channel->bus_master_command) | BMC_START;
    outb(temp, channel->bus_master_command);
    DEBUG("Waiting for interrupt (block = %d, size = %d)\n", request->first_block, request->blocks);
}

/*
 * Complete a request by resetting the start/stop
 * bit in the bus master command register
 * Parameter:
 * @request_queue - queue in which the request is placed
 * @request - the request to be completed
 */
static void pata_complete_request(hd_request_queue_t* request_queue,
        hd_request_t* request) {
    pata_channel_t* channel;
    u8 temp;
    /*
     * First determine channel and drive
     */
    if (0 == pata_device_valid(request->minor_device)) {
        PANIC("Invalid minor device %x\n", request->minor_device);
    }
    int drive_no = (request->minor_device) >> 4;
    channel = &(channels[drive_no / 2]);
    /*
     * Reset the start/stop bit in the DMA command register
     */
    temp = inb(channel->bus_master_command) & (~BMC_START);
    outb(temp, channel->bus_master_command);
}

/*
 * Prepare the PRDT for a request, i.e. split the buffer pointed to by the
 * request into chunks, translate them into virtual addresses and set up the
 * PRDT accordingly
 * Parameters:
 * @request_queue - the request queue
 * @request - the request
 */
static void pata_prepare_request(hd_request_queue_t* request_queue,
        hd_request_t* request) {
    u32 chunk_start;
    u32 chunk_end;
    u32 page_end;
    u32 buffer_start;
    u32 buffer_end;
    int index = request - request_queue->queue;
    int prdt_index = 0;
    int eot = 0;
    KASSERT(index>=0);
    KASSERT(index<HD_QUEUE_SIZE);
    /*
     * We now split the buffer pointed to by the request into chunks.
     * By definition, each chunk is the overlap of a page in virtual memory
     * with a part of the buffer. For each chunk, we add an entry to the PRDT.
     * We first determine the start and end of the buffer and make the first chunk
     * start at the start of the buffer
     */
    buffer_start = request->buffer;
    buffer_end = buffer_start + request->blocks * ATA_BLOCK_SIZE - 1;
    chunk_start = buffer_start;
    while (1) {
        if (prdt_index >= PATA_PRDT_COUNT) {
            PANIC("PRDT maximum size exceeded\n");
        }
        /*
         * Determine last address of page in which chunk start is located
         */
        page_end = MM_PAGE_SIZE - (chunk_start % MM_PAGE_SIZE) + chunk_start
                - 1;
        /*
         * If necessary cut off chunk to end there
         */
        if (page_end < buffer_end) {
            chunk_end = page_end;
        }
        else {
            chunk_end = buffer_end;
            eot = DMA_PRD_EOT;
        }
        /*
         * Add entry to PRDT
         */
        prdt[index][prdt_index].region_base = mm_virt_to_phys(chunk_start);
        prdt[index][prdt_index].region_size = chunk_end - chunk_start + 1;
        prdt[index][prdt_index].reserved = 0;
        prdt[index][prdt_index].eot = eot;
        /*
         * If that was the last entry, leave loop,
         * otherwise increment index into PRDT and continue
         */
        if (eot)
            break;
        prdt_index++;
        chunk_start = chunk_end + 1;
    }
}

/*
 * Initialize a request queue
 * Parameter:
 * @request_queue - the queue to be initialized
 */
static void pata_init_queue(hd_request_queue_t* request_queue) {
    request_queue->block_size = ATA_BLOCK_SIZE;
    request_queue->chunk_size = PATA_CHUNK_SIZE;
    request_queue->device_busy = 0;
    spinlock_init(&(request_queue->device_lock));
    request_queue->complete_request = pata_complete_request;
    request_queue->prepare_request = pata_prepare_request;
    request_queue->submit_request = pata_submit_request;
    request_queue->head = 0;
    request_queue->tail = 0;
    sem_init(&(request_queue->slots_available), HD_QUEUE_SIZE);
}

/*
 * Common read/write interface function for the PATA driver
 * Parameter:
 * @minor_dev - the minor device to use
 * @blocks - number of blocks to read (block size 1024 bytes!)
 * @first_block - first block to read
 * @buffer - buffer in which we place the data
 * @rw - 0 to read, 1 to write
 * Return values:
 * number of bytes read upon successful completion
 * -ENODEV if the device is not valid
 * -EIO if a read/write error occured
 * -EINVAL if the read crosses a partition boundary or the block number is zero
 * -ENOMEM if no memory could be allocated for a needed temporary buffer
 */
static int pata_rw(minor_dev_t minor, ssize_t blocks, ssize_t first_block,
        void* buffer, int rw) {
    u32 hd_blocks;
    u32 hd_first_block;
    int rc;
    hd_request_queue_t* request_queue;
    if (0 == pata_device_valid(minor)) {
        return -ENODEV;
    }
    if (0 == blocks)
        return -EINVAL;
    int drive_no = minor >> 4;
    /*
     * Convert the blocks in units of BLOCK_SIZE into
     * blocks in units of 512 bytes and handle partition mapping
     */
    int factor = BLOCK_SIZE / ATA_BLOCK_SIZE;
    hd_blocks = blocks * factor;
    hd_first_block = first_block * factor;
    if (minor % PATA_PART_DRIVE) {
        hd_first_block += partitions[minor].first_sector;
        if (hd_first_block + hd_blocks - 1 > partitions[minor].last_sector) {
            return -EINVAL;
        }
    }
    /*
     * Check for kernel parameter pata_ro
     */
    if  ((HD_WRITE==rw) &&  (1==params_get_int("pata_ro"))) {
        PANIC("pata_ro is set\nDetected attempt to write %d sectors starting at sector %d\n", hd_blocks, hd_first_block);
    }
    /*
     * Determine request queue to use
     */
    request_queue = request_queues + (drive_no / 2);
    /*
     * Trigger processing
     */
    rc = hd_rw(request_queue, hd_blocks, hd_first_block, rw, buffer, minor);
    if (rc < 0)
        return rc;
    return blocks * BLOCK_SIZE;

}

/*
 * This is the read interface function for the PATA controller and
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
 * -ENOMEM if no memory could be allocated for a needed temporary buffer
 */
ssize_t pata_read(minor_dev_t minor, ssize_t blocks, ssize_t first_block,
        void* buffer) {
    return pata_rw(minor, blocks, first_block, buffer, HD_READ);
}

/*
 * Dummy open function
 */
int pata_open(minor_dev_t minor) {
    return 0;
}

/*
 * Dummy close function
 */
int pata_close(minor_dev_t minor) {
    return 0;
}

/*
 * This is the write interface function for the PATA controller and
 * thus the main entry point used by the kernel to submit a write
 * request.
 * Parameter:
 * @minor_dev - the minor device to use
 * @blocks - number of blocks to write (block size 1024 bytes!)
 * @first_block - first block to write
 * @buffer - buffer in which we have placed the data
 * Return values:
 * number of bytes read upon successful completion
 * -ENODEV if the device is not valid
 * -EIO if a read/write error occured
 * -EINVAL if the write crosses a partition boundary or the block number is zero
 * -ENOMEM if no memory could be allocated for a needed temporary buffer
 */
ssize_t pata_write(minor_dev_t minor, ssize_t blocks, ssize_t first_block,
        void* buffer) {
    return pata_rw(minor, blocks, first_block, buffer, HD_WRITE);
}

/*
 * Initialize the PATA driver. This function scans the PCI bus for all ATA/IDE drives and
 * builds up the internal data structures of the driver
 */
void pata_init() {
    int i;
    /*
     * First mark all controller, channels, drives and partitions as used
     */
    for (i = 0; i < PATA_MAX_CNTL; i++)
        cntl[i].used = 0;
    cntl_count = 0;
    for (i = 0; i < PATA_MAX_CNTL * 2; i++) {
        channels[i].used = 0;
        pata_init_queue(request_queues + i);
    }
    for (i = 0; i < PATA_MAX_CNTL * 4; i++)
        drives[i].used = 0;
    for (i = 0; i < PATA_MAX_CNTL * 4 * PATA_PART_DRIVE; i++)
        partitions[i].used = 0;
    /*
     * Now scan PCI devices with base class 0x1 and sub class 0x1
     */
    pci_query_by_class(&pata_register_cntl, PCI_BASE_CLASS_MASS_STORAGE, PATA_SUB_CLASS);
    /*
     * and register with device manager
     */
    ops.close = pata_close;
    ops.open = pata_open;
    ops.read = pata_read;
    ops.write = pata_write;
    dm_register_blk_dev(MAJOR_ATA, &ops);
}

/*
 * Get the name of a drive, given the drive number (first drive
 * is 0) or NULL if there is no such drive
 */
char* pata_drive_name(int n) {
    int count = 0;
    int i;
    for (i = 0; i < PATA_MAX_CNTL * 4; i++) {
        if (drives[i].used) {
            if (count == n)
                return drives[i].model;
        }
    }
    return 0;
}

/*
 * Return the 1k blocks already processed by PATA devices
 */
u32 pata_processed_kbyte() {
    u32 blocks = 0;
    int i;
    hd_request_queue_t* queue;
    for (i = 0; i < PATA_MAX_CNTL * 2; i++) {
         if (channels[i].used) {
             queue = request_queues + i;
             blocks += ((queue->processed_blocks * queue->block_size) / 1024);
         }
    }
    return blocks;
}

/************************************************************
 * The entire code below this line is only used for the     *
 * internal debugger                                        *
 ***********************************************************/

/*
 * Print a list of all controllers, channels and devices
 *
 */
void pata_print_devices() {
    int i;
    PRINT("               Bus master      Bus master      Bus master     Native\n");
    PRINT("Cntl  Channel  Command reg.    Status reg.     PRDT reg.      mode    IRQ\n");
    PRINT("-------------------------------------------------------------------------\n");
    for (i = 0; i < PATA_MAX_CNTL * 2; i++)
        if (channels[i].used)
            PRINT("%h    %h       %x       %x       %x      %d       %h\n", i >> 1, i & 0x1,
                    channels[i].bus_master_command, channels[i].bus_master_status, channels[i].bus_master_prdt, channels[i].operating_mode, channels[i].vector);
    PRINT("\n");
    PRINT("                                                             48 bit  Alt.\n");
    PRINT("Cntl  Ch.  Dev.   Model                                      LBA     Status\n");
    PRINT("---------------------------------------------------------------------------\n");
    for (i = 0; i < PATA_MAX_CNTL * 4; i++)
        if (drives[i].used)
            PRINT("%h    %h   %h     %s   %d       %h\n", i >> 2, (i >> 1) & 0x1, i & 0x1, drives[i].model, drives[i].lba_long,
                    inb(channels[i/2].ata_alt_status));
    PRINT("\n");
    PRINT("                             First         Last\n");
    PRINT("Cntl Ch.  Dev.  Partition    Sector        Sector       Size (MB)\n");
    PRINT("-----------------------------------------------------------------\n");
    for (i = 0; i < PATA_MAX_CNTL * 4 * PATA_PART_DRIVE; i++) {
        if (partitions[i].used) {
            PRINT("%h   %h   %h    %h           %x     %x    %d\n",
                    i/(PATA_PART_DRIVE*4), (i >> 5) & 0x1, (i >> 4) & 0x1, i % PATA_PART_DRIVE,
                    partitions[i].first_sector, partitions[i].last_sector,
                    (partitions[i].last_sector+1-partitions[i].first_sector)/2048);
        }
    }
}

/*
 * Print the content of the request queues
 */
void pata_print_queue() {
    int i;
    int j;
    hd_request_queue_t* queue = 0;
    hd_request_t* request = 0;
    for (i = 0; i < PATA_MAX_CNTL * 2; i++) {
        if (channels[i].used) {
            queue = request_queues + i;
            if (queue->head != queue->tail) {
                PRINT("Request queue for channel %d: \n",i);
                PRINT("Head: %d  Tail: %d\n", queue->head % HD_QUEUE_SIZE, queue->tail % HD_QUEUE_SIZE);
                PRINT("-----------------------------------\n");
                PRINT("Slot   R/W   Blocks        First block\n");
                PRINT("------------------------------------------\n");
                for (j = queue->head; j < queue->tail; j++) {
                    request = &queue->queue[j % HD_QUEUE_SIZE];
                    PRINT("%h   %h      %x     %d\n", j % HD_QUEUE_SIZE, request-> rw, request->blocks, request->first_block);
                }
            }
        }
    }
}

void pata_do_tests() {
    int pata_rc;
    if (pata_device_valid(0) == 0)
        return;
    if (pata_device_valid(1) == 0)
        return;
    PRINT("Starting PATA driver tests\n");
    PRINT("--------------------------\n");
    PRINT("Reading block 0 (MBR) from primary master\n");
    char* pata_test_buffer = (char*) kmalloc_aligned(65536, 4);
    KASSERT(pata_test_buffer);
    if ((pata_rc = pata_read(0, 1, 0, pata_test_buffer)) < 0) {
        ERROR("Negative return code -%d received\n", (-1)*pata_rc);
    }
    PRINT("Printing bytes 504 - 511 \n");
    int bc;
    for (bc = 0; bc < 8; bc++) {
        PRINT("%h ", pata_test_buffer[bc+504]);
    }
    PRINT("\nDoing unaligned read\n");
    if ((pata_rc = pata_read(0, 1, 0, pata_test_buffer + 1)) < 0) {
        ERROR("Negative return code -%d received\n", (-1)*pata_rc);
    }
    PRINT("Printing bytes 504 - 511 \n");
    for (bc = 0; bc < 8; bc++) {
        PRINT("%h ", pata_test_buffer[bc+505]);
    }
    PRINT("\nReading block 1025 and 1026 from primary master\n");
    pata_rc = pata_read(0, 2, 1025, pata_test_buffer);
    if (pata_rc < 0) {
        PANIC("Negative return code -%d received\n", (-1)*pata_rc);
    }
    PRINT("Printing first 8 bytes of data\n");
    for (bc = 0; bc < 8; bc++) {
        PRINT("%h ", pata_test_buffer[bc]);
    }
    PRINT("\n");
    PRINT("Printing first 8 bytes of block 1026\n");
    for (bc = 0; bc < 8; bc++) {
        PRINT("%h ", pata_test_buffer[bc+1024]);
    }
    PRINT("\n");
    PRINT("Reading first 8 bytes from superblock of partition 1\n");
    if ((pata_rc = pata_read(1, 1, 1, pata_test_buffer)) < 0) {
        PANIC("Negative return code -%d received\n", (-1)*pata_rc);
    }
    for (bc = 0; bc < 8; bc++) {
        PRINT("%h ", pata_test_buffer[bc]);
    }
    PRINT("\n");
    PRINT("Now reading 10*1024 times 64 kB");
    for (bc = 0; bc < 10 * 1024; bc++) {
        if (pata_read(0, 64, (bc % 1024) * 64, pata_test_buffer) < 0) {
            PANIC("Error while reading from drive at block %d (bc=%d)\n", bc*64, bc);
        }
        if (0 == (bc % 512))
            PRINT(".");
    }
    kfree(pata_test_buffer);
    pata_test_buffer = kmalloc_aligned(528 * 512, 4096);
    KASSERT(pata_test_buffer);
    PRINT("\nReading 528 sectors from primary master\n");
    pata_rc = pata_read(0, 528 / 2, 0, pata_test_buffer);
    if (pata_rc < 0) {
        PANIC("Negative return code -%d received\n", (-1)*pata_rc);
    }
    kfree(pata_test_buffer);
    PRINT("Now doing unaligned read (buffer at n*512+4 bytes)\n");
    pata_test_buffer = kmalloc_aligned(1028, 4096);
    KASSERT(pata_test_buffer);
    pata_rc = pata_read(0, 1, 0, pata_test_buffer + 4);
    if (pata_rc < 0) {
        PANIC("Negative return code -%d received\n", (-1)*pata_rc);
    }
    PRINT("Printing bytes 504 - 511 \n");
    for (bc = 0; bc < 8; bc++) {
        PRINT("%h ", pata_test_buffer[bc+504+4]);
    }
    PRINT("\n");
    kfree(pata_test_buffer);
    PRINT("I will now read sector 2050 (block 1025) again\n");
    pata_test_buffer = kmalloc_aligned(512 * 512, 4);
    KASSERT(pata_test_buffer);
    pata_rc = pata_read(0, 1, 1025, pata_test_buffer);
    if (pata_rc < 0) {
        PANIC("Negative return code -%d received\n", (-1)*pata_rc);
    }
    PRINT("Printing first 8 bytes of sector 2050\n");
    for (bc = 0; bc < 8; bc++) {
        PRINT("%h ", pata_test_buffer[bc]);
    }
    PRINT("\n");
    if (drives[0].lba_long) {
        PRINT("Now I will read 512 sectors, specifically sectors 1540 - 2051\n");
        memset((void*) pata_test_buffer, 0xff, 512 * 512);
        pata_rc = pata_read(0, 512 / 2, 1540 / 2, pata_test_buffer);
        if (pata_rc < 0) {
            PANIC("Negative return code -%d received\n", (-1)*pata_rc);
        }
        PRINT("Printing first 8 bytes of sector 2050\n");
        for (bc = 0; bc < 8; bc++) {
            PRINT("%h ", pata_test_buffer[bc+512*510]);
        }
        PRINT("\n");
    }
    else
        PRINT("Skipping read of 512 sectors, as 48 bit LBA not supported by this device\n");
}

