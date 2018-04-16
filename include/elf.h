/*
 * elf.h
 */

#ifndef _ELF_H_
#define _ELF_H_

#include "ktypes.h"

#define EI_NIDENT 16

typedef u32 elf32_addr;
typedef u16 elf32_half;
typedef u32 elf32_off;
typedef u32 elf32_word;


/*
 * Elf-Header.
 * See ELF documentation for a full description
 * Some often needed fields are:
 * e_entry: virtual address of entry point
 * e_phoff: offset to program header
 * e_shoff: offset of section header
 * e_shnum: number of section headers in the file
 * e_phentsize: size in byte of programm header entry
 * e_phnum: number of entries in the programm header table
 * e_shstrndx: the index of the section in the section header table
 *             which is the section header string table
 */
typedef struct {
    u8 e_ident[EI_NIDENT];
    elf32_half e_type;
    elf32_half e_machine;
    elf32_word e_version;
    elf32_addr e_entry;
    elf32_off e_phoff;
    elf32_off e_shoff;
    elf32_word e_flags;
    elf32_half e_ehsize;
    elf32_half e_phentsize;
    elf32_half e_phnum;
    elf32_half e_shentsize;
    elf32_half e_shnum;
    elf32_half e_shstrndx;
} elf32_ehdr_t;

/*
 * Program header. As opposed to sections, which describe
 * the file from a link editor point of view, the program
 * headers describe the file in terms of segments and
 * are used during program load.
 * Often used fields are:
 * - p_type: the type of the segment
 * - p_offset: the offset of the segment in the file
 * - p_vaddr: the virtual address to which the segment should be loaded
 * - p_paddr: physical target address, unused most of the time
 * - p_filesz: size of the segment in the file
 * - p_memsz: size of the segment in memory
 * - p_align: alignment in memory and file
 */
typedef struct {
    elf32_word p_type;
    elf32_off p_offset;
    elf32_addr p_vaddr;
    elf32_addr p_paddr;
    elf32_word p_filesz;
    elf32_word p_memsz;
    elf32_word p_flags;
    elf32_word p_align;
} elf32_phdr_t;

/*
 * This structure is used when an ELF file
 * is initially parsed by the library. It allows
 * for easy access to the most important parts of
 * the file without having to read that data from the
 * file itself again and again
 */
typedef struct {
    int fd;
    elf32_ehdr_t* file_header;
    elf32_phdr_t* program_header_table;
    unsigned int program_header_count;
    unsigned int program_header_size;
} elf_metadata_t;

/*
 * File types as stored in
 * elf32_ehdr_t.e_type
 */
#define ET_NONE 0
#define ET_REL 1 /* Relocatable file */
#define ET_EXEC 2 /* Executable */
#define ET_DYN 3 /* Shared object file */
#define ET_CORE 4 /* Core dump */
#define ET_LOPROC 0xff00
#define ET_HIPROC 0xffff

/*
 * Machine type as stored in
 elf32_ehdr_t.e_machine
 */
#define EM_NONE 0
#define EM_M32 1
#define EM_SPARC 2
#define EM_386 3
#define EM_68k 4
#define EM_88K 5
#define EM_860 7
#define EM_MIPS 8

/*
 * Magic numbers stored in the first
 * 4 bytes of the header
 */
#define ELFMAG0 0x7f
#define ELFMAG1 'E'
#define ELFMAG2 'L'
#define ELFMAG3 'F'

/*
 * Section header types, i.e. possible
 * values for elf32_shdr_t.sh_type
 */
#define SHT_NULL 0 /* Inactive header */
#define SHT_PROGBITS 1 /* Program specific data */
#define SHT_SYMTAB 2 /* Symbol table */
#define SHT_STRTAB 3 /* String table */
#define SHT_RELA 4 /* Relocation entries */
#define SHT_HASH 5
#define SHT_DYNAMIC 6
#define SHT_NOTE 7
#define SHT_NOBITS 8 /* Section does not occupy space in file*/
#define SHT_REL 9 /* Relocation entries */
#define SHT_SHLIB 10
#define SHT_DYNSYM 11

/*
 * Section header flags
 */
#define SHF_WRITE 0x1 /* Writable during execution */
#define SHF_ALLOC 0x2 /* Occupies memory in program image */
#define SHF_EXECINSTR 0x4 /* Executable instructions */

/*
 * Values for the program header type
 */

#define PT_NULL 0
#define PT_LOAD 1 /* Loadable segment */
#define PT_DYNAMIC 2 /* Dynamic linking information */
#define PT_INTERP 3 /* Interpreter */
#define PT_NOTE 4 /* Note */
#define PT_SHLIB 5
#define PT_PHDR 6

/*
 * Permissions for segments
 */
#define PF_R        0x4
#define PF_W        0x2
#define PF_X        0x1



int elf_get_metadata(int fd, elf_metadata_t* md);
void elf_free_metadata(elf_metadata_t* elf_metadata);
elf32_phdr_t* elf_get_program_header(elf_metadata_t* md, int index);
int elf_load_executable(char* path, u32* entry_point, int validate_only);

#endif /* _ELF_H_ */
