/*
 * hd.c
 *
 * This module contains utility functions which are used by all hard disk drives.
 *
 */

#include "hd.h"
#include "kerrno.h"
#include "debug.h"
#include "lib/string.h"
#include "mm.h"

static char* __module = "HD    ";


/*
 * According to the ATA/ATAPI specification, ASCII strings are transferred
 * as a sequence of words through the data register. So the first character
 * is the most significant byte of the first word, the second character
 * is the least significant byte of the second word and so forth
 * As byte ordering is little endian on our target architecture (x86),
 * this means that what we actually get in memory starting at the lowest address is
 * - second character
 * - first character
 * - third character
 * - fourth character
 * and so forth. So we need to go through the string afterwards and swap
 * 0 and 1, 2 and 3 until 2n-2 and 2n-1 for a string of 2n characters
 * This function does this for an area of length length=2n
 * Parameters:
 * @string - string to fix
 * @length - length of string (an even number)
 */
void hd_fix_ata_string(char* string, int length) {
    int i;
    char tmp;
    for (i = 0; i < length / 2; i++) {
        tmp = *(string + 2 * i);
        *(string + 2 * i) = *(string + 2 * i + 1);
        *(string + 2 * i + 1) = tmp;
    }
}


/*
 * Initialize a HD request
 * Parameter:
 * @request - the request structure to be filled
 * @minor_dev - the minor device
 * @first_block - first sector to read or write
 * @blocks - number of sectors to read or write
 * @rw - read (0) or write (1)
 * @buffer - address of buffer
 */
static void init_request(hd_request_t* request, minor_dev_t minor_dev,
        u64 first_block, ssize_t blocks, int rw, u32 buffer) {
    request->blocks = blocks;
    request->buffer = buffer;
    request->first_block = first_block;
    request->minor_device = minor_dev;
    request->rw = rw;
    request->task_id = pm_get_task_id();
    request->status = HD_REQUEST_QUEUED;
    request->submitted_by_irq = 0;
    /*
     * Allocate memory for semaphore
     */
    request->semaphore = (semaphore_t*) kmalloc(sizeof(semaphore_t));
    KASSERT(request->semaphore);
    sem_init(request->semaphore, 0);
}

/*
 * Add a request to the request queue and initiate processing if necessary
 * Parameter:
 * @queue - request queue
 * @minor_dev - minor device
 * @first_block - first block to process
 * @blocks - number of blocks
 * @rw - read (0) or write (1)
 * @data - buffer to use
 * Return value:
 * 0 if processing was successful
 * ENOMEM if no memory could be allocated for the return code
 * EIO if any other read/write error occurred
 */
static int hd_put_request(hd_request_queue_t* queue, minor_dev_t minor_dev,
        u64 first_block, ssize_t blocks, int rw, void* data) {
    hd_request_t* request;
    u32 eflags;
    int rc = 0;
    semaphore_t* sem;
    int* rc_ptr;
    /*
     * Allocate memory for return code in kernel heap - this makes sure
     * that we are able to access the pointer from every context
     */
    rc_ptr = (int*) kmalloc(sizeof(int));
    if (0 == rc_ptr) {
        ERROR("Could not allocate memory for error code\n");
        return ENOMEM;
    }
    /*
     * Place new request in queue and call prepare_request
     * function
     */
    sem_down(&(queue->slots_available));
    spinlock_get(&(queue->device_lock), &eflags);
    /*
     * At this point, the queue should have at least one available slot
     */
    KASSERT(HD_QUEUE_SIZE != queue->tail - queue->head);
    request = queue->queue + (queue->tail % HD_QUEUE_SIZE);
    init_request(request, minor_dev, first_block, blocks, rw, (u32) data);
    queue->prepare_request(queue, request);
    request->rc = rc_ptr;
    queue->tail++;
    /*
     * If necessary trigger the processing - if device_busy==1 this
     * will be done by the interrupt handler
     */
    if (0 == queue->device_busy) {
        request->status = HD_REQUEST_PENDING;
        queue->submit_request(queue, request);
        queue->device_busy = 1;
    }
    /*
     * Release lock and wait for interrupt handler. We copy the reference to the semaphore
     * to the stack as when we continue, the request - being part of a
     * circular buffer - might be in use again
     */
    sem = request->semaphore;
    spinlock_release(&(queue->device_lock), &eflags);
    sem_down(sem);
    rc = *rc_ptr;
    kfree(rc_ptr);
    kfree(sem);
    if (rc)
        return EIO;
    return 0;
}

/*
 * Utility function to read/write one chunk of data from/to the hard drive
 * Parameters:
 * @chunk_start - start of chunks (sector number)
 * @chunk_blocks - sectors to read
 * @buffer - buffer to which result of read will be written
 * @request_queue - request queue to use
 * @minor - minor device
 * @rw - read/write flag (0=read, 1=write)
 * Return value:
 * 0 if the request was successful
 * -ENOMEM if no buffer could be allocated
 * -EIO of the operation failed
 */
static int hd_rw_chunk(u64 chunk_start, u32 chunk_blocks, void* buffer,
        hd_request_queue_t* request_queue, minor_dev_t minor, int rw) {
    void* chunk_buffer;
    int need_to_copy;
    int rc;
    chunk_buffer = buffer;
    if (((u32) chunk_buffer) % sizeof(u32)) {
        /*
         * Get an aligned buffer for this chunk and set a reminder
         * that we need to copy the data from this data back to
         * the provided buffer
         */
        chunk_buffer = kmalloc_aligned(
                chunk_blocks * request_queue->block_size, sizeof(u32));
        if (0 == chunk_buffer) {
            ERROR("Could not get enough memory to set up aligned DMA buffer\n");
            return -ENOMEM;
        }
        if (HD_READ == rw) {
            need_to_copy = 1;
        }
        else  {
            need_to_copy = 0;
            memcpy(chunk_buffer, buffer, chunk_blocks * request_queue->block_size);
        }
    }
    else {
        need_to_copy = 0;
    }
    rc = hd_put_request(request_queue, minor, chunk_start, chunk_blocks,
            rw, chunk_buffer);
    if (rc) {
        ERROR("hd_put_request returned error code, rc=%d\n", rc);
        if (need_to_copy)
            kfree(chunk_buffer);
        return -EIO;
    }
    if (need_to_copy) {
        memcpy(buffer, chunk_buffer, chunk_blocks * request_queue->block_size);
        kfree(chunk_buffer);
    }
    return 0;
}

/*
 * Process a read/write request. This function will perform the following actions:
 * - get the chunk size from the request queue
 * - split the request into chunks
 * - for each chunk, allocate a dword aligned buffer if the original buffer is
 * not aligned correctly
 * - for each chunk, call hd_put_request to place a request for the chunk in the
 * request queue and wait until is has been processed
 * - if needed, copy the data back to the original buffer
 * Parameters:
 * @request_queue - the request queue to use
 * @sectors - number of sectors to read/write (unit = sectors, i.e usually 512 bytes)
 * @first_sector - first sector to read/write
 * @rw - read/write (0=read)
 * @buffer - buffer to use
 * @minor - minor device
 * Return value:
 * 0 if operation was successful
 * -EIO if I/O operation failed
 * -ENOMEM if no memory could be allocated for a temporary buffer
 */
int hd_rw(hd_request_queue_t* request_queue, u32 sectors, u64 first_sector, int rw, void* buffer, minor_dev_t minor) {
    u32 chunk;
    u32 nr_of_chunks;
    u32 chunk_blocks;
    u64 chunk_start;
    int rc;
    /*
     * Split the request into chunks and call hd_put_request once
     * for each chunk. First get chunk size in terms of sectors
     */
    u32 chunk_size = request_queue->chunk_size;
    /*
     * Now determine number of chunks - if the number of sectors
     * to read is not a multiple of the chunk size, add one
     * partial chunk at the end
     */
    if (0 == (sectors % chunk_size)) {
        nr_of_chunks = sectors / chunk_size;
    }
    else {
        nr_of_chunks = sectors / chunk_size + 1;
    }
    /*
     * Process list of chunks. Determine start block
     * and number of blocks in each chunk and call
     * hd_rw for each
     */
    for (chunk = 1; chunk <= nr_of_chunks; chunk++) {
        chunk_start = (chunk - 1) * chunk_size + first_sector;
        if ((chunk < nr_of_chunks) || (0 == (sectors % chunk_size))) {
            /* Full chunk */
            chunk_blocks = chunk_size;
        }
        else {
            /* It is a partial chunk */
            chunk_blocks = sectors % chunk_size;
        }
        rc = hd_rw_chunk(chunk_start, chunk_blocks, buffer + (chunk - 1)
                * chunk_size * request_queue->block_size, request_queue, minor, rw);
        if (rc < 0)
            return rc;
    }
    return 0;
}


/*
 * Process an interrupt. This function wakes up the process waiting for the
 * entry at the head of the request queue and sends the next request in the
 * queue to the device
 * Parameters:
 * @request_queue - the queue to be processed
 * @rc - set to a non-zero value by the device specific interrupt handler if
 * an error has been detected
 */
void hd_handle_irq(hd_request_queue_t* queue, int rc) {
    hd_request_t* request;
    u32 eflags;
    spinlock_get(&(queue->device_lock), &eflags);
    /*
     * If device is not busy, something went wrong
     */
    if (0 == queue->device_busy) {
        ERROR("Interrupt handler called, but device not busy - what went wrong?");
        spinlock_release(&(queue->device_lock), &eflags);
        return;
    }
    /*
     * Locate request at head of queue and wake up process
     * sleeping for it. Also put error code passed in as rc
     * into the int variable pointed to by the rc field in
     * the request queue
     */
    request = queue->queue + (queue->head % HD_QUEUE_SIZE);
    *(request->rc) = rc;
    if (queue->complete_request)
        queue->complete_request(queue, request);

    sem_up(request->semaphore);
    /*
     * Increase counter of processed blocks
     */
    queue->processed_blocks += request->blocks;
    /*
     * Remove request from queue
     */
    queue->head++;
    sem_up(&(queue->slots_available));
    /*
     * If there is still a request in the queue, process it
     */
    if ((queue->tail != queue->head)) {
        request = queue->queue + (queue->head % HD_QUEUE_SIZE);
        request->status = HD_REQUEST_PENDING;
        request->submitted_by_irq = 1;
        queue->submit_request(queue, request);
        queue->device_busy = 1;
    }
    else {
        queue->device_busy = 0;
    }
    spinlock_release(&(queue->device_lock), &eflags);
}

/*
 * Given a partition table entry which describes
 * an extended partition, this function will read all
 * logical partitions contained in it and add them to the table
 * of partitions passed as first parameters. The first logical partition
 * will be added at index 5, the second at index 6 and so forth
 * Parameters:
 * @partitions - the table of partitions which we fill
 * @table_size - the maximum number of entries which this
 * table can hold
 * @lcount - a pointer to an integer which is incremented by one
 * for each logical partition found
 * @partition - the entry describing the extended partition
 * @minor - the minor device number of the disk
 * @read_sector - a function to read one or more sectors from the hard disk
 * which is supposed to return a non-negative number upon success and a negative
 * error code
 * Return value:
 * 0 upon success
 * a negative error code if an error occurred
 */
static int hd_read_logical_partitions(hd_partition_t* partitions,
        int table_size, int* lcount, part_table_entry_t partition,
        minor_dev_t minor, int(*read_sector)(minor_dev_t minor, u64 lba,
                void* buffer)) {
    part_table_entry_t ext_partition;
    int cont = 1;
    mbr_t mbr;
    int rc;
    *lcount = 0;
    u64 next = partition.first_sector;
    /* Read the linked list of partitions */
    while (1 == cont) {
        /* Read the next partition table from disk */
        rc = read_sector(minor, next,  (void*) &mbr);
        if (rc < 0)
            return rc;
        /*
         * The first entry of the partition table
         * stored in this sector describes the logical partition
         * and is relative to the current partition table
         */
        ext_partition = mbr.partition_table[0];
        ext_partition.first_sector += next;
        /*
         * Add entry to partition table starting at index 5
         * for the first logical partition
         */
        if (5 + (*lcount) < table_size) {
            partitions[5 + *lcount].first_sector = ext_partition.first_sector;
            partitions[5 + *lcount].last_sector = ext_partition.first_sector
                    + ext_partition.sector_count - 1;
            partitions[5 + *lcount].used = 1;
        }
        else {
            ERROR("Skipping logical partition %d as table size exceeded\n");
            return -EINVAL;
        }
        (*lcount)++;
        /*
         * The second entry may contain a pointer to the next
         * partition table and is relative to the beginning
         * of the entire extended partition
         */
        if ((PART_TYPE_EXTENDED == mbr.partition_table[1].type)
                || (PART_TYPE_WIN95_EXT_LBA == mbr.partition_table[1].type))
            next = mbr.partition_table[1].first_sector + partition.first_sector;
        else
            cont = 0;
    }
    return 0;
}

/*
 * Read a GPT from disk. Please refer to the documentation of the function
 * hd_read_partitions below for detaisl
 */
static int hd_read_partitions_gpt(hd_partition_t* partitions, minor_dev_t minor,
        int(*read_sector)(minor_dev_t minor, u64 lba,  void* buffer),
        int table_size) {
    u8 buffer[512];
    gpt_header_t* gpt_header;
    gpt_entry_t* gpt_entry;
    int count = 0;
    u32 part_table_size = 0;
    void* part_table = 0;
    int blocks = 0;
    int i;
    int used = 0;
    /*
     * Read LBA 1 - this should be the GPT header
     */ 
    if (read_sector(minor, 1, buffer) < 0) {
        ERROR("Could not read from disk\n");
        return -EIO;
    }
    gpt_header = (gpt_header_t*) buffer;
    /*
     * Check signature
     */
    if (gpt_header->signature != GPT_SIGNATURE) {
        ERROR("Wrong signature in GPT header, giving up\n");
        return -EIO;
    }
    DEBUG("Partition table starts at LBA %D and has %d entries of size %d\n", 
            gpt_header->part_table_first_lba, 
            gpt_header->part_table_entries,
            gpt_header->part_table_entry_size);
    /*
     * Now go through all the entries sequentially. We first read all the required blocks 
     */
    part_table_size = gpt_header->part_table_entries * gpt_header->part_table_entry_size;
    if (0 == (part_table = kmalloc(part_table_size))) {
        ERROR("Could not allocate enough memory for partition table\n");
    }
    /*
     * We read the entire table into memory. This is not very efficient, but easy, and
     * we have kmalloc at this point in time
     */ 
    blocks = part_table_size / 512;
    if (part_table_size % 512)
        blocks +=1;
    DEBUG("Reading %d blocks from disk, partition table size = %d\n", blocks, part_table_size);
    for (i = 0; i < blocks; i++) {
        if (read_sector(minor, i + gpt_header->part_table_first_lba, part_table + i*512) < 0) {
            ERROR("Could not read sector %D from disk\n", i + gpt_header->part_table_first_lba);
            kfree(part_table);
            return -EIO;
        }
    }
    /*
     * Now we go through the entries in memory one by one
     */
    for (i = 0; i < gpt_header->part_table_entries; i++) {
        gpt_entry = (gpt_entry_t*) (part_table + i*gpt_header->part_table_entry_size);
        /*
         * See whether it is used
         */
        used = 0;
        for (int j = 0; j < GPT_GUID_LENGTH; j++) {
            if (0 != gpt_entry->part_type_guid[j])
                used = 1;
        }
        if (0 == used)
            continue;
        DEBUG("Found used partition %d\n", i);
        count++;
        if (count >= table_size)
            break;
        partitions[count].first_sector = gpt_entry->first_lba;
        partitions[count].last_sector = gpt_entry->last_lba;
        partitions[count].used = 1;
    }
    kfree(part_table);
    return count;
}

/*
 * This function will read a partition table from disk and fill the
 * provided partition list accordingly. The first parameter is a pointer
 * to an array of partitions which will be filled with the partitions found.
 * The function read_sector is supposed to read a sector from disc without
 * sleeping and without raising any interrupts. It should return a positive
 * number upon success and a negative error code upon failure
 * Note that the entry with index 0 in the partition table is not used as this
 * is reserved for the raw drive
 * Parameters:
 * @partitions - an array of partitions to be filled
 * @minor - the minor device which we scan
 * @read_sector - function to use for reading a sector
 * @table_size - number of entries in the array @partitions, i.e. we do
 * not read more than this number of partitions even if there are any (should be at least 4)
 * Return value:
 * the number of partitions found upon success
 * -EIO if an error occurred
 */
int hd_read_partitions(hd_partition_t* partitions, minor_dev_t minor,
        int(*read_sector)(minor_dev_t minor, u64 lba,  void* buffer),
        int table_size) {
    mbr_t mbr;
    part_table_entry_t partition;
    int rc;
    int i;
    /*
     * This counts the number of primary partitions
     */
    int count = 0;
    /*
     * This counts the number of logical partitions
     */
    int lcount = 0;
    /*
     * We assume at least four entries not counting 0
     */
    KASSERT(table_size>=5);
    /*
     * Read master boot record
     */
    rc = read_sector(minor, 0, (void*) &mbr);
    if (rc < 0) {
        ERROR("Could not read from drive\n");
        return -EIO;
    }
    if (mbr.magic != MBR_MAGIC_COOKIE) {
        ERROR("This is not a valid MBR\n");
        return -EIO;
    }
    /*
     * See whether we are dealing with a protective GPT - if yes
     * delegate to the function for that
     */
    for (i = 0; i < 4; i++) {
        partition = mbr.partition_table[i];
        if (partition.type == PART_TYPE_GPT) {
            MSG("Found protective GPT\n");
            return hd_read_partitions_gpt(partitions, minor, read_sector, table_size);
        }
    }
    /*
     * If we get to this point, we are assuming an 
     * MBR partition table
     */
    for (i = 0; i < 4; i++) {
        partition = mbr.partition_table[i];
        if (partition.type != PART_TYPE_EMPTY) {
            if ((PART_TYPE_WIN95_EXT_LBA == partition.type) || (PART_TYPE_EXTENDED == partition.type)) {
                /*
                 * The entry refers to an extended partition
                 */
                DEBUG("Found extended partition\n");
                rc = hd_read_logical_partitions(partitions, table_size,
                        &lcount, partition, minor, read_sector);
                if (rc < 0)
                    return -EIO;
            }
            else {
                DEBUG("Found primary partition %d starting at sector %d\n", i+1, partition.first_sector);
                partitions[i+1].first_sector = partition.first_sector;
                partitions[i+1].last_sector = partition.first_sector
                        + partition.sector_count - 1;
                partitions[i+1].used = 1;
                count++;
            }
        }
    }
    return count+lcount;
}
