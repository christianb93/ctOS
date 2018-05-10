# The ctOS boot process and multiboot

ctOS relies on a multiboot compliant boot loader for booting. Several scenarios are possible:

- boot using GRUB legacy 
- boot using QEMU and its `-kernel` switch 
- boot using GRUB2

Unfortunately, the boot process is complicated a bit by the fact that there are two versions of the multiboot specification around. The [first version](https://www.gnu.org/software/grub/manual/multiboot/multiboot.html) was the specification as originally introduced and is supported by all the three alternatives listed above. The [second version](https://www.gnu.org/software/grub/manual/multiboot2/multiboot.html), known as Multiboot2, was created to adapt to modern boot environments based on UEFI and ACPI.

Both versions define two data structures - the **multiboot header** which describes a data structure that needs to be present in the OS image and contains the information that a boot loader requires to boot, and the **boot information** which is a data structure that a boot loader hands over to the OS.

The original multiboot specification makes the following information available to the OS that is used by ctOS (or might be used in a later release, this list is not complete):

* Information on the memory layout (i.e. a memory map) so that the OS knows which pages it can use safely and which pages are used by BIOS, UEFI, ACPI, PCI etc. (needed by the memory manager)
* the command line used for the boot process which the kernel needs to extract the boot parameters (needed by params.c)
* information on the location of the ramdisk (needed by `locate_ramdisk` in the memory manager)
* information on the video mode (current mode, available VBE modes and the location of the framebuffer)

The following additional information is only provided by multiboot2:

* pointers to the EFI system table
* a copy of the ACPI RSDP

Unfortunately, qemu does not support the multiboot2 format, only the first version of the multiboot configuration. Therefore ctOS tries to be compatible with both versions. 


## The multiboot module

The multiboot module is the module responsible for interpreting the boot information - by design, this is the only module that should know whether the kernel has been booted with multiboot1 or multiboot2. 

At boot time, we have to observe that this module is called very early in the boot process, when the kernel heap is not yet set up and kmalloc is therefore not available, but also needs to deal with dynamic memory allocation to store copies of the multiboot information (which might be overwritten at a later point in time). Therefore the module goes through different stages during the boot process.


 * the first stage starts when we call multiboot_init - then the part of the information which is not dynamic and does therefore not require kmalloc is extracted and saved
 * the second stage starts after the memory manager has been set up. We then call multiboot_clone to clone all the other information that we might need, now using dynamic data structures and kmalloc

The following public functions are offered by the multiboot module.

| Function | Description |
|:---------|:------------|
|void multiboot_init(u32 multiboot_info_ptr, u32 magic); | Initialize the multiboot structure given a pointer to the multiboot information structure and the magic number that the bootloader stored in EAX |
|const char* multiboot_get_cmdline(); | get the command line used during booting |
| int multiboot_get_next_mmap_entry(memory_map_entry_t* next); | Get next memory map entry. This function can be used to iterate through the memory map provided by the bootloader. However, this can be done only once, then the entries are consumed and the function will only return 0. |
| int multiboot_locate_ramdisk(multiboot_ramdisk_info_block_t* multiboot_ramdisk_info_block); | retrieve information on the location of the ramdisk (if present) |
| void multiboot_clone(); | ask the multiboot module to clone all information that might still be needed and to release ownership of the memory in which the multiboot information structure was originally stored |


## Kernel versions

As mentioned above, QEMU does currently not support the Multiboot2 specification. However, during testing, it is very useful to be able to boot a kernel using QEMU directly via the `-kernel` switch. To maintain this option, the build scripts do actually generate two kernels.

* ctOSkernel - this is the main kernel 
* ctOSkernel1 - this kernel is identical to the first one, with the single exception that a different start.S module (namely startmb1.S) is linked into it. This allows us to migrate start.S to a multiboot2 format while at the same time ctOSkernel1 still has the old multiboot format

## Notes on the multiboot2 specification

While working on this code, I hit upon a few points which are at least not obvious from the specification.

* In the multiboot header in start.S, all tags must be aligned **on an 8-byte boundary**. This implies that at some points, additional padding is necessary
* The multiboot specification defines a header tag "Multiboot2 information request" (type 1). This tag has a variable length which is contained in its header. However, in addition, the last entry nees to be an entry with type 0, otherwise GRUB2 will read past this point and try to interpret the next few bytes as part of this structure as well
* In the multiboot information structure defined by the Multiboot2 protocol, all tags are aligned on an 8-byte boundary as well. Thus the next tag does not start at the location given by the address of the current tag plus its size, but this address must be rounded up to a multiple of eight
* GRUB2 will only load a multiboot2 kernel if the command `multiboot2` instead of `multiboot` is used
* The same is true for modules: a module that has been loaded with `module` instead of `module2` will not show up in the multiboot2 module list, and vice versa


