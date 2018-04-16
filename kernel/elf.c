/*
 * elf.c
 *
 * This module is part of the process and task manager. It contains functions to read
 * ELF files, parse the ELF header and load an ELF executable into memory
 */

#include "elf.h"
#include "fs.h"
#include "kerrno.h"
#include "mm.h"
#include "debug.h"

/*
 * Read metadata from an ELF file
 * The function does not open the file itself,
 * but reads from the file descriptor FD which is
 * assumed to point to an open file
 * The caller is responsible for calling elf_free_metadata
 * on the metadata structure to clean up
 * Parameter:
 * @fd - the file descriptor linking to the ELF file
 * @md - the elf_metadata structure to fill
 * Return value:
 * ENOMEM if no memory could be allocated for the ELF data structures
 * ENOEXEC if the file descriptor does not point to an ELF file
 * EIO if the read from the file failed
 * 0 upon success
 */
int elf_get_metadata(int fd, elf_metadata_t* md) {
    u32 size;
    ssize_t rc;
    md->fd = fd;
    md->file_header = (elf32_ehdr_t*) kmalloc(sizeof(elf32_ehdr_t));
    if (0 == md->file_header)
        return ENOMEM;
    /* First read the file header */
    size = do_read(fd, (unsigned char*) md->file_header, sizeof(elf32_ehdr_t));
    if ((md->file_header->e_ident[0] != ELFMAG0)
            || (md->file_header->e_ident[1] != ELFMAG1)
            || (md->file_header->e_ident[2] != ELFMAG2)
            || (md->file_header->e_ident[3] != ELFMAG3)) {
        ERROR("Invalid ELF header\n");
        return ENOEXEC;
    }
    /*
     * Allocate memory for program headers
     */
    md->program_header_count = md->file_header->e_phnum;
    md->program_header_size = md->file_header->e_phentsize;
    size = (md->program_header_count) * (md->program_header_size);
    md->program_header_table = (elf32_phdr_t*) kmalloc(size);
    if (0 == md->program_header_table)
        return ENOMEM;
    /*
     * Seek to start of program header table and read
     * it into memory
     */
    rc = do_lseek(fd, md->file_header->e_phoff, SEEK_SET);
    if (rc <= 0) {
        ERROR("Seek failed\n");
        return EIO;
    }
    rc = do_read(fd, (unsigned char*) md->program_header_table, size);
    if (rc <= 0) {
        ERROR("Read from ELF file failed\n");
        return EIO;
    }
    return 0;
}

/*
 * Free all memory associated with an elf metadata structure
 * Parameter:
 * @elf_metadata - the structure to be freed
 */
void elf_free_metadata(elf_metadata_t* elf_metadata) {
    if (elf_metadata->file_header)
        kfree(elf_metadata->file_header);
    if (elf_metadata->program_header_table)
        kfree(elf_metadata->program_header_table);
}

/*
 * Get a pointer to a program header
 * from the metadata structure
 * The header is specified by its index (starting with 0)
 * If the index is out of range, 0 is returned
 * Parameters:
 * @md - the elf metadata
 * @index - the index of the requested program header
 * Return value:
 * a pointer to the program header or 0 if no program header
 * could be located
 */
elf32_phdr_t* elf_get_program_header(elf_metadata_t* md, int index) {
    if (index >= md->program_header_count)
        return 0;
    return (elf32_phdr_t*) ((void*) md->program_header_table
            + md->program_header_size * index);
}

/*
 * Utility function to read a segment from an ELF
 * executable into memory
 * Parameter:
 * @fd - file descriptor to read from
 * @phdr - ELF program header of segment to read from
 * Return value:
 * ENOMEM if no memory could be allocated for the ELF segments
 * ENOEXEC if the read from the ELF file failed
 * 0 upon success
 */
static int elf_read_segment(int fd, elf32_phdr_t* phdr) {
    u32 segment_base;
    u32 segment_end;
    u32 segment_top;
    u32 mem_base;
    u32 page_offset;
    ssize_t rc;
    char* ptr;
    u32 i;
    /*
     * Determine layout of segment in memory
     */
    segment_base = (phdr->p_vaddr / phdr->p_align) * phdr->p_align;
    segment_end = phdr->p_vaddr + phdr->p_memsz - 1;
    segment_top = (segment_end / phdr->p_align + 1) * phdr->p_align - 1;
    page_offset = (phdr->p_vaddr % phdr->p_align);
    /*
     * Get memory for segment
     */
    mem_base = mm_map_user_segment(segment_base, segment_top);
    if (0 == mem_base) {
        ERROR("Could not allocate memory for ELF segment\n");
        return ENOMEM;
    }
    /*
     * Read segment into memory
     */
    rc = do_lseek(fd, (phdr->p_offset - page_offset), SEEK_SET);
    if (rc < 0) {
        ERROR("Seek failed with rc=%d, phdr->p_offset=%d, page_offset=%d\n", rc, phdr->p_offset, page_offset);
        return ENOEXEC;
    }
    rc = do_read(fd, (void*) mem_base, phdr->p_filesz + page_offset);
    if (rc <= 0) {
        ERROR("Read failed with rc=%d\n", rc);
        return ENOEXEC;
    }
    /*
     * Finally fill up with zeroes
     */
    ptr = (char*) (mem_base + phdr->p_filesz + page_offset);
    for (i = 0; i < (phdr->p_memsz - phdr->p_filesz); i++)
        ptr[i] = 0;
    return 0;
}

/*
 * Load a program from an ELF executable into
 * memory
 * Parameters:
 * @path - name of executable
 * @entry_point - used to return the program entry point
 * @validate_only - set this to 1 to only perform validations
 * Return value:
 * ENOEXEC if the executable could not be read or is invalid
 * 0 upon success
 */
int elf_load_executable(char* path, u32* entry_point, int validate_only) {
    int fd;
    int rc;
    elf_metadata_t elf_meta;
    elf32_phdr_t* phdr;
    int index = 0;
    /*
     * Open file and read ELF metadata
     */
    fd = do_open(path, 0, 0);
    if (fd < 0) {
        return ENOEXEC;
    }
    rc = elf_get_metadata(fd, &elf_meta);
    if (rc) {
        return ENOEXEC;
    }
    while ((phdr = elf_get_program_header(&elf_meta, index))) {
        if (phdr->p_type == PT_LOAD) {
            if ((phdr->p_align % MM_PAGE_SIZE)) {
                elf_free_metadata(&elf_meta);
                return ENOEXEC;
            }
            /*
             * Only read data if validate_only is false
             */
            if (0 == validate_only) {
                rc = elf_read_segment(fd, phdr);
                if (rc) {
                    elf_free_metadata(&elf_meta);
                    return ENOEXEC;
                }
            }
        }
        /*
         * Dynamic libraries are not yet supported
         */
        if (phdr->p_type == PT_INTERP) {
            elf_free_metadata(&elf_meta);
            return ENOEXEC;
        }
        index++;
    }
    *entry_point = elf_meta.file_header->e_entry;
    elf_free_metadata(&elf_meta);
    return 0;
}
