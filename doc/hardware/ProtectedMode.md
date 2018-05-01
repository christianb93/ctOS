# Protected mode programming

At startup, a x86 compatible CPU is in real mode. In this mode, it is essentially acting as an 8086 CPU, addressing only up to 1 MB memory. To be able to access 4 GB of physical memory directly and to use the advanced features of the x86 architecture like paging, memory protection and multitasking, the CPU has be be put into protected mode first.

In protected mode, the CPU uses three essential in-memory tables which control memory management and interrupt handling as well as access rights. These three tables are called **Global Descriptor Table** (GDT), **Interrupt Descriptor Table** (IDT) and **page directory / page tables**. In this document, we will briefly look at this structure and explain their usage. For details, the reader is referred to the comprehensive "Intel® 64 and IA-32 Architectures Software Developer’s Manual Volume 3A - System Programming Guide, Part 1" which is available for download at the Intel website.

If using a boot loader like GRUB2, the CPU will already be in protected mode when control is passed to the operating system. However, as the location and exact content of the GDT is not known, the operating system should still set up its own GDT and switch to it as soon as possible. Setting up IDT and page tables is not done by GRUB2 and therefore is part of the OS initialization procedure.

## Overview on memory management in protected mode

In protected mode, there are three different types of addresses which can be used to describe the location of a specific byte in memory. 

The first type of address is the **logical address**. Similarly to real mode, a logical address consists of a **segment selector** (16 bit) which specifies the memory segment the byte is located in and an **offset** (32 bit) which specifies the location of the memory space within the segment. 

A program running in user space usually only sees the offset - this is what appears for instance if you dump the value of a pointer in a userspace C program. The segment selector is set by the operating system and usually not changed by a user space program. So from the programs point of view, memory is entirely described by a 32 bit address and hence appears as a 4 GB virtual address space.

When accessing memory, the CPU uses a table called Global Descriptor Table (see below) to convert this logical address into a **linear address** which is simply a 32 bit value. This is similar to real mode where the segment and offset are combined into a linear 20 bit wide address. Note, however, that the translation mechanism is slightly more complex in protected mode as we will see below.

In fact, the GDT is not the only descriptor table which can be used for that purpose. When using hardware supported multitasking, each task can have its own descriptor table called LDT. We will not look in detail at LDTs in this document and restrict ourselves to describing the GDT instead.

Logical and linear address are still virtual addresses. To convert the linear address into a **physical address*, the CPU finally uses a **page table directory** and a **page table**. When the CPU has first been brought into protected mode, paging is still disabled, so this final translation step is skipped and linear and physical address are the same. Note, however, that the translation between logical and linear address cannot be turned off and is always active. So setting up this translation mechanism and in particular the GDT is one of the basis initialization step which needs to be done when switching to protected mode.

## The Global Descriptor Table (GDT)


As explained above, a logical address in protected mode is made up of a 16 bit segment selector and a 32 bit offset. In contrast to real mode, however, the selector is not added to the offset to obtain the linear address. Instead, the selector essentially contains a pointer into a table called GDT which contains the actual description of the segments.
The 16 bit segment descriptor is made up of three parts.

| Bits | Description |
|:-----|:------------|
| 0-1 | RPL - requested privilege level. This is the privilege level (see the section below for a short summary of privilege levels) |
|2 | TI - this bit is used to control whether the selector refers to the GDT (0) or the LDT (1) |
|3-15 |Index - this is the index of the entry in the GDT which describes this entry |

So for instance if the 16 bit segment descriptor is 0x8, it will refer to entry 1 in the global descriptor table (TI=0) with requested privilege level 0. As the size of an entry in the GDT is 8 bytes, taking the bitwise AND between the segment selector and 0xf8 also yields the offset of the entry to the start address of the GDT.

Note that the entry with index zero in the GDT is usually not used. Instead, the CPU expects a so called null selector in this position, i.e. a selector consisting entirely of zeros.

After the GDT has been set up, each of the 16 bit segment descriptors CS, SS, DS, ES, FS and GS should be loaded with a segment selector pointing to the respective entr in the GDT.

Now let us take a closer look at the GDT entries. Each entry specifies base address, size and access rights for one segment. An entry in the GDT is made up of eight bytes and structured as follows.

<table>
<thead>
<tr class="header">
<th>Bits<br />
</th>
<th>Description<br />
</th>
</tr>
</thead>
<tbody>
<tr class="odd">
<td>0-15<br />
</td>
<td>Bits 0-15 of the 20 bit segment limit information. Note that the interpretation of the segment limit depends on other flags in the GDT entry, see below<br />
</td>
</tr>
<tr class="even">
<td>16-39<br />
</td>
<td>The first three bytes of the 32 bit base address of the segment<br />
</td>
</tr>
<tr class="odd">
<td>40-43<br />
</td>
<td>Segment type. The highest bit determines whether the segment is a data segment or a code segment<br />
Bit 43: data segment (0) or code segment (1)<br />
The interpretation of the remaining bits is different for data segment or code segment. For data segments, the meaning of the bits is as follows<br />
Bit 42: expansion direction E - used to interpret the segment limit field, see below<br />
Bit 41: write enable W - if this is set to 1, write access is allowed<br />
Bit 40: accessed A - this bit is set by the CPU when the segment has been accessed<br />
For the code segment, the meaning of the bits is as follows.<br />
Bit 42: conforming C - controls application of privilege levels, should be 0 most of the time<br />
Bit 41: read R - if this bit is 1, read access is allowed, otherwise only execution is allowed<br />
Bit 40: accessed A - as for data segment<br />
</td>
</tr>
<tr class="even">
<td>44<br />
</td>
<td>Descriptor type S - 1 to indicate a segment descriptor<br />
</td>
</tr>
<tr class="odd">
<td>45-46<br />
</td>
<td>Descriptor privilege level DPL - see section on privileges below<br />
</td>
</tr>
<tr class="even">
<td>47<br />
</td>
<td>Segment present P - should be set to 1 in most cases<br />
</td>
</tr>
<tr class="odd">
<td>48-51<br />
</td>
<td>Bits 16-19 of segment limit<br />
</td>
</tr>
<tr class="even">
<td>52<br />
</td>
<td>Available for use by the operating system<br />
</td>
</tr>
<tr class="odd">
<td>53<br />
</td>
<td>Reserved for protected mode (used for long mode)<br />
</td>
</tr>
<tr class="even">
<td>54<br />
</td>
<td>Default operation size D - should usually be 1 for protected mode 32 bit code<br />
</td>
</tr>
<tr class="odd">
<td>55<br />
</td>
<td>Granularity G - controls interpretation of segment limit, see below<br />
</td>
</tr>
<tr class="even">
<td>56-63<br />
</td>
<td>Bits 24-31 of the base address<br />
</td>
</tr>
</tbody>
</table>

The actual size of a segment depends on the combination of segment limit, granularity and expansion direction of the segment. First, an effective segment limit is formed which is equal to the segment limit if the granularity is 0 and is obtained by multiplying the segment limit by 0xfff if the granularity flag is 1 - in other words, if the granularity flag is one, the unit for the segment limit is not bytes but pages. So if the segment limit is 0xfffff, the effective segment limit is 4GB which is the full address space.

The way how this effective segment limit is used now depends on the value of the expansion flag. If this flag is cleared, the effective segment limit specifies the size of the segment, so the segment ranges from the base address to the base address + (effective segment limit). If the expansion flag is set, the effective segment limit is interpreted downwards from the top of the 32 bit memory space. In other words, when accessing this segment, the offset must always exceed the effective segment limit for the access to be valid.

The CPU stores the location of the global register table in a special register called GDTR. To load this register, we need to construct a 6 byte wide data structure in memory which is called pseudo-descriptor in the Intel documentation and describes location and size of the GDT. The first two bytes of this structure contain the size of the entire GDT in bytes. The remaining four bytes specify the physical base address of the GDT in memory.

To actually load the GDT address into the GDTR, the assembler instruction lgdt can be used. This instruction will accept a pseudo-descriptor and store it in the GDT register. Conceptually, the GDTR is an 48 bit register holding the entire pseudo-descriptor. So assuming that you have stored the address of the pseudo-descriptor in the register eax, you would code

```
lgdt (%eax)
```

in AT&T assembler syntax to make the GDT known to the CPU. After loading the new GDT, you will have to reload the segment registers to contain the new segment selectors. In particular, you will have to execute a far jump to force the CPU to reload the CS register. As an example, I use the following code to load the GDT, assuming that the address of the GDT 48 bit pseudo-descriptor is stored  in register eax and that the first segment is the code segment, the second segment the data segment and the third segment the stack segment.

```

    /*******************************************
    * Load 48 bit GDT register                 *
    ********************************************/
    lgdt %ds:(%eax)
    /*******************************************
    * We need to do a long jump                *
    * so that our new CS becomes effective     *
    *******************************************/
    jmpl $8, $next
next:
    /*******************************************
    * Set up remaining segment registers       *
    *******************************************/
    movw $16, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ds
    movw $24, %ax
    movw %ax, %ss
```
    


Again this is AT&T assembler syntax for use with GCC and GNU AS.

## The interrupt descriptor table (IDT)

When the CPU receives an interrupt, it needs to know the address of the interrupt handler to be invoked and the respective code segment. For this purpose, the interrupt descriptor table is used. This table, similarly to the GDT, contains of 8 byte wide entries and can contain up to 256 entries (starting with index 0), representing the 256 interrupt vectors of the x86 CPU. The following table describes the layout of an entry in the IDT. Note that the Intel manual mentions that the start of the in memory table should be 8 byte aligned.

<table>
<thead>
<tr class="header">
<th>Bits<br />
</th>
<th>Description<br />
</th>
</tr>
</thead>
<tbody>
<tr class="odd">
<td>0-15<br />
</td>
<td>Bits 0-15 of the offset of the logical address of the interrupt handler<br />
</td>
</tr>
<tr class="even">
<td>16-31<br />
</td>
<td>The 16 bit segment selector specifying the segment in which the interrupt handler is located<br />
</td>
</tr>
<tr class="odd">
<td>32-39<br />
</td>
<td>Reserved, should be set to zero<br />
</td>
</tr>
<tr class="even">
<td>40<br />
</td>
<td>Gate type. If this bit is set, the entry describes a so called trap gate, meaning that further interrupts will be allowed during its execution. If it is cleared, it is an interrupt gate and the CPU will turn off interrupts prior to entering the handler<br />
</td>
</tr>
<tr class="odd">
<td>41-42<br />
</td>
<td>Always 1<br />
</td>
</tr>
<tr class="even">
<td>43<br />
</td>
<td>Gate size D - should be one for 32 bit gates<br />
</td>
</tr>
<tr class="odd">
<td>44<br />
</td>
<td>Descriptor type S - should be 0 to indicate a system descriptor<br />
</td>
</tr>
<tr class="even">
<td>45-46<br />
</td>
<td>Descriptor privilege level DPL - determines from which level the interrupt can be invoked, see section on privileges below<br />
</td>
</tr>
<tr class="odd">
<td>47<br />
</td>
<td>Segment present P - should be set to 1 in most cases<br />
</td>
</tr>
<tr class="even">
<td>48-63<br />
</td>
<td>Bits 16-31 of the logical address of the interrupt handler<br />
</td>
</tr>
</tbody>
</table>

Loading an interrupt descriptor table is very similar to loading a GDT. Again, there is a special register called IDTR in the CPU which holds an 48 bit structure pointing to the IDT. As for the GDT, the 48 bit pseudo-descriptor used consists of a 16 bit limit field and a 32 base address field. The offset holds the base address of the IDT. The limit field, however, has a slightly different meaning because it does not store the size of the table, but the offset of the last valid byte of the table, i.e. the size of the table minus 1.
Assuming that the 48 bit pseudo-descriptor has been assembled in memory and its address is stored in eax, the IDT can then be loaded with

```
lidt (%eax)
```

## Paging

We have seen above that programs in userspace and actually most parts of the kernel as well operate on virtual memory instead of physical memory. To translate between a virtual (linear) address and a physical address, the CPU uses a collection of translation tables called page tables.

To avoid the need of a large contiguous area of physical memory to store the page tables, the page tables are split and organized in two levels. At the top level, there is the **page table directory**. An entry in a page table directory represents 4 MB of virtual memory and essentially contains a pointer to a **page table**. 

The page table defines for this part of virtual memory the mapping of virtual pages to physical pages. Each entry describes the properties of one 4 KB page of physical memory. Both page table directory and page table consist of 1024 entries with 4 byte each, so that a page table and a page table directory both consume exactly one page. 

Let us first look at the page table entries in detail. The following table describes its bits.

<table>
<thead>
<tr class="header">
<th>Bits<br />
</th>
<th>Description<br />
</th>
</tr>
</thead>
<tbody>
<tr class="odd">
<td>0<br />
</td>
<td>Present bit, must always be 1<br />
</td>
</tr>
<tr class="even">
<td>1<br />
</td>
<td>R/W flag, if 0, writes are not allowed to this page<br />
</td>
</tr>
<tr class="odd">
<td>2<br />
</td>
<td>User/supervisor flag, if the flag is 0, accesses from user space are not allowed<br />
</td>
</tr>
<tr class="even">
<td>3<br />
</td>
<td>PWT - page level write through. If this bit is set, write-through is enforced for this memory area, i.e. upon a write, memory is updated immediately<br />
</td>
</tr>
<tr class="odd">
<td>4<br />
</td>
<td>PCD - page level cache disable. If this bit is set, caching of this memory region is not allowed, this can for instance be used for memory mapped I/O<br />
</td>
</tr>
<tr class="even">
<td>5<br />
</td>
<td>A - accessed, set by the CPU if software accesses the memory area described by this entry<br />
</td>
</tr>
<tr class="odd">
<td>6<br />
</td>
<td>D - dirty, set by the CPU when software has written to memory described by this entry<br />
</td>
</tr>
<tr class="even">
<td>7<br />
</td>
<td>reserved, should be zero (used for PAT in other paging modes)<br />
</td>
</tr>
<tr class="odd">
<td>8-11<br />
</td>
<td>ignored in ordinary 32 bit paging mode<br />
</td>
</tr>
<tr class="even">
<td>12-31<br />
</td>
<td>Physical address of the 4 KB page referenced by this entry<br />
</td>
</tr>
</tbody>
</table>

The entry in a page table directory is very similar. In fact, the physical layout is exactly the same, with a few differences with respect to the meaning of the individual flags.

-   the r/w flag and supervisor flag govern access to the entire 4 MB region controlled by this page table
-   the PWT and PCD flags apply to access to the page table pointed to by this entry, not to the respective memory
-   the dirty flag is not used
-   the base address field specificies the upper 20 bits of the location of a page table in memory

Page tables as well as page table directory always need to be aligned to 4096 bytes so that they take up exactly one page.
  
Now let us look at how the address translation actually works. The idea is that given a 32 bit linear address, the lowest 12 bits (bits 0-11) are used to determine the offset within the page which is the same for the physical as well for the virtual page. The remaining 20 bits are used to determine the page table entry. The highest 10 bits (bits 22-31) are used as index into the page table directory and select one of its 1024 entries. This entry refers to a page table. The next 10 bits (bits 12-21) are then used as index into this page table to select an entry for the actual page. Bits 12-31 of this entry are shifted by 12 bits to the left and give bits 12-31 of the physical address. The remaining 10 bits of the physical address are then taken from the linear address.

To illustrate this, let us consider an example. Suppose we wanted to translate the linear adress 0xabcd1234 into a virtual address. First, we split this address into three parts.

-   Bits 0 - 11 = 0x1234
-   Bits 12 - 21 = 0x3cd
-   Bits 22 - 31 = 0x2a

Then we locate entry 0x2a in the page table directory and read base address field (bits 12-31) from this table. We shift this by 12 bits to the left to obtain the address of page table. Next we locate entry 0x3cd within the page table. Again we take the base address field from there, let us call the value B. We then compute the physical address as B\*4096+0x1234.

This algorithm needs with the address of the page table directory itself. This address is stored in the special register CR3 of the CPU and can be read and written using ordinary MOV instructions. Note that the address in CR3 as well as the base addresses in page table directory entries and page table entries are all physical addresses.

Once the address of the page table directory has been stored in CR3, paging can be enabled by setting bit 31 (PG) in the CR0 register to one. As soon as this bit is set, all memory accesses are done through the paging unit.

Special care has to be taken when entries of the page table are modified as the CPU caches these entries in a special cache called Translation Lookaside Buffer (TLB). When the CR3 register is written, the entire TLB is invalidated. In addition, there is a special instruction called INVLPG which takes a linear (virtual) address as argument and invalidates all entries in the TLB which refer to this entry. So if you change the mapping of a physical page, call INVLPG with the address of the virtual page to make sure that the TLB is up to date.

## The task status segment (TSS)


Even though it is not used by most operating systems, the x86 architecture comes with builtin support for hardware task switching. When such a task switch is done, the CPU saves information on the current state of the CPU in a special data structure called a task data segment (TSS).

When software multitasking is used, the TSS is not used with one important exception. Whenever an interrupt occurs which leads to a decrease of the privilege level, the CPU reads the address of a stack to be used while executing this interrupt from the task status segment and sets ESP and SS to the address and code segment stored there. 

More precisely, for X=0,1,2 the TSS contains fields `ESP<X>` and `SS<X>`. Whenever an interrupt occurs which leads to a change of the privilege level from a higher privilege level to a new level X, the CPU puts these values into ESP and SS.

As most operating systems execute the kernel in ring 0 and the user space code in ring 3, this implies that at least one TSS needs to be created at boot time and the fields ESP0 and SS0 need to be filled with the address and segment of the kernel stack. The CPU will then automatically switch the stack to the kernel stack when executing an interrupt and back once the interrupt has been completed.

The following C structure describes the layout of the TSS.


```
typedef struct {
    u32 back_link;
    u32 esp0;
    u32 ss0;
    u32 esp1;
    u32 ss1;
    u32 esp2;
    u32 ss2;
    u32 cr3;
    u32 eip;
    u32 eflags;
    u32 eax;
    u32 ecx;
    u32 edx;
    u32 ebx;
    u32 esp;
    u32 ebp;
    u32 esi;
    u32 edi;
    u32 es;
    u32 cs;
    u32 ss;
    u32 ds;
    u32 fs;
    u32 gs;
    u32 ldt;
    u32 io_map_offset;
} tss_t;
```

To inform the CPU about the location of the TSS, a special entry needs to be placed in the GDT. The base address of this entry is the physical address of the TSS (the Intel Manual recommends to avoid that the TSS crosses a page boundary, so this should, for instance, be page aligned). The other bits of the GDT entry are as described above, with the following exceptions.

-   the descriptor type bit S (Bit 44) is 0
-   bit 43 is 1 (code segment)
-   bit 42 is 0 (conforming)
-   bit 41 is called the busy flag and is used to indicate whether this task is currently executing. If used with software task switching so that there is only one physical task, this should be set to 0 - otherwise the CPU will believe that it is tried to switch to an already active task and a general protection fault will occur when the TSS is loaded
-   bit 40 is 1

The CPU uses a special register called the task register to hold the 16 bit segment descriptor of the TSS (and a hidden part which is loaded from the GDT when the register is initialized). It is loaded with the special CPU instruction LTR.

## Further reading


-   Intel® 64 and IA-32 Architectures Software Developer’s Manual Volume 3A - System Programming Guide, Part 1 - available at <http://www.intel.com/products/processor/manuals/>

