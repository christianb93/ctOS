# Using and installing GRUB2 for kernel development


This document serves as a guide to using GRUB2 to run a multiboot compatible kernel. We will look briefly at the multiboot specification to understand the minimum requirements a kernel must fulfill to be booted by GRUB2. We will then look at different ways to install GRUB2 and a custom kernel on various boot media, like a CD-ROM image for use with emulators like Bochs and QEMU or real hardware, a hard disk image for Bochs and QEMU and an USB stick for use with real hardware.

## Relevant information from the multiboot specification


To load and execute a piece of code via GRUB2, this piece of code needs to be compliant with the [multiboot2 specification](https://www.gnu.org/software/grub/manual/multiboot2/multiboot.html) (GRUB2 is also able to work with the older multiboot version 1 specification, but ctOS uses multiboot2 in all releases after 0.2). Essentially, GRUB2 will load of ELF binaries as long as they contain a data structure called the **multiboot header**. This header must be contained entirely within the first 8192 bytes of the image.
The multiboot header has four  mandatory fields which are described in the following table.

<table>
<thead>
<tr class="header">
<th>Offset<br />
</th>
<th>Length<br />
</th>
<th>Field name<br />
</th>
<th>Description<br />
</th>
</tr>
</thead>
<tbody>
<tr class="odd">
<td>0<br />
</td>
<td>Fullword<br />
</td>
<td>Magic cookie<br />
</td>
<td>A string which is scanned for by the boot loader to detect the multiboot header. By definition, this is 0xe85250d6</td>
</tr>
<tr class="even">
<td>4<br />
</td>
<td>Fullword<br />
</td>
<td>Architecture<br />
</td>
<td>This field defines the target architecture for the boot process. 0x0 is i386 (32 bit)<br />
</td>
</tr>
<tr class="odd">
<td>4<br />
</td>
<td>Fullword<br />
</td>
<td>Header length<br />
</td>
<td>Length of the full header<br />
</td>
</tr>
<tr class="even">
<td>8<br />
</td>
<td>Fullword<br />
</td>
<td>Checksum<br />
</td>
<td>Required<br />
</td>
</tr>
</tbody>
</table>

This structure is followed by a list of **tags** which contain additional information and which are optional. Each tag has a type and a size, and the list of tags is concluded by a tag with type 0. 

At boot time, the bootloader hands over the system in the following state to the OS.

-   EAX contains the magic value <span class="samp">0x36d76289</span> to signal to the OS that is has been loaded by a multiboot compliant loader
-   EBX contains the physical address of the so-called **multiboot information structure**
-   All segment register point to a read/write code segment with offset 0x0
-   the system is in protected mode, paging is turned off
-   A20 gate is enabled
-   no stack present, i.e. the OS must allocate space for a stack and set up ESP as soon as possible

So basically, the first module linked into your kernel needs to be something like this.

```
_start:
	cli
	/* Skip over multiboot header */
	jmp go
	/****************************************
	* This is the multiboot2 header which   *
	* the bootloader (GRUB) will search for *
	****************************************/
	.align 8
multiboot_header:    
    # The first 4 bytes are the magic number
    .long MULTIBOOT2_HEADER_MAGIC
    # Next field is the architecture field. We ask
    # to be loaded in 32bit protected mode, i.e. 0
    .long 0
    # Next field is the length of the header
    .long go - multiboot_header
    # Next field is the checksum. This should be such that
    # all magic fields (i.e. the first four fields including
    # the checksum itself) sum up to zero
    .long -(MULTIBOOT2_HEADER_MAGIC + (go  - multiboot_header))
```

These are actually the first few lines from start.S in the ctOS source code.

Note that even though GRUB2 will guarantee that the system is in protected mode, the location of the GDT is not known, not even the selector numbers. So you should set up your own GDT as soon as possible and do a long jump to load the new code segment.

## GRUB2 basics


Before we go about installing GRUB2, let us take a brief look at how the GRUB2 boot process works. Essentially, there are four different things GRUB2 needs to boot your kernel.

-   boot image
-   core image
-   modules
-   configuration file

The **boot image** is a short piece of code which fits into the boot sector of a PC. The code within the boot image will locate the next stage of the boot loader, called the core image and load and execute it. GRUB2 comes with a ready made boot image which is padded to 512 bytes so that it can be copied to the MBR. For my Linux distribution, this images is located in /usr/lib/grub/i386-pc and is called boot.img.

The **core image** contains the basic functionality of GRUB2. Usually, the core image is placed right after the MBR in the area between the MBR and the start of the first partition. The core image is generated dynamically by a tool called `grub-mkimage` and usually called core.img.

GRUB2 is a modular system by design. Most of the actual functionality is placed in modules. On my system, you can get a complete list of all modules by typing

```
ls /usr/lib/grub/i386-pc/*.mod
```

The most important modules for our purposes are

-   normal - this is the normal for the normal GRUB2 CLI as opposed to the code for the GRUB2 rescue CLI which is built directly into core.img
-   search - this module offers some functionality to search dynamically for partitions and devices and even files. It can for instance be used to identify a device by UUID
-   ls - this module offers an ls command as part of the CLI
-   multiboot - needed to load multiboot compliant kernels
-   part_msdos - this module contains the code to parse the partition table
-   biosdisk - used to read from hard disk and floppy via the BIOS routines
-   ext2 - handle ext2 file system, similar modules exist for other file systems
-   elf - code to load ELF modules
-   configfile - parse configuration files

Finally, GRUB2 needs a **configuration file** which specifies the boot menu visible to the user and other settings. If no configuration file is present, GRUB2 will still start but fall through to its command line interface.

As GRUB2 has modules for most existing file systems, you can place modules and the configuration file in any partition on your PC and GRUB2 will be able to use them. However, as often with boot loaders, there is a chicken egg problem at this point. We can store modules on an ext2 or NTFS filesystem, but we need modules to be able to read these filesystems. Similarly, we can store configuration files in an arbitrary partition, but GRUB2 needs some starting point to search for these files.
To overcome this problem, the tool which generates the core image - grub-mkimage - is capable of embedding both modules and a stripped down configuration file into the core image. We will see later when building a hard disk image how that works.

## Creating a CDROM GRUB2 image


### Booting to the GRUB2 CLI

First let us try to create a CDROM image which can be used to boot GRUB2 with an emulator as well as with a real PC. We will do this in two steps. In the first step, we will create a CDROM boot image which takes us into a GRUB2 command line interface so that we can issue the appropriate commands to boot our kernel. In the second step, we will then extend the image by placing kernel and a configuration file in the image as well.

Creating a simple CD image with GRUB2 is very easy thanks to the tool grub-mkrescue which is part of the GRUB2 distribution. Simply start a shell and type

```
grub-mkrescue -o cdimage
```

This will create an ISO9660 CDROM image called cdimage which you can either burn to a CDROM to boot from a real machine (I use brasero for this purpose, make sure to chose the option "Burn Image") or use it within an emulator. For instance, to test our image, we can use QEMU as follows.

```
qemu-system-i386 -cdrom cdimage
```

If you add hard disk images in addition, you will have to use the -boot option to select the boot medium - often this is mapped as D if you have one hard disk, so you would for instance type something like

```
qemu-system-i386 -hda my.hdimage -cdrom cdimage -boot d
```

For Bochs, you first need to setup the CD image in the Bochs configuration file as either master or slave of an existing ATA controller. Assuming that the master of the first controller is your hard drive, this amounts to a line like

```
ata0-slave: type=cdrom, path="cdimage", status=inserted
boot: cdrom
```

in the Bochs config file.

This will take us to a GRUB2 CLI. If you now enter ls, you will see a device called (cd) - this is the CD from which we booted. You can use ls to inspect the content of this CD, for instance typing

```
ls (cd)/boot/grub/i386-pc/
```

reveals that grub-mkrescue has placed all available modules in this directory. However, all your other hard drives are available as well and you can use the CLI commands to boot any multiboot compliant kernel located on any of these drives. If, for instance, your kernel is called mykernel and located in the root directory of the first partition on your first hard drive, you can use the following commands to boot

```
multiboot2 (hd0,msdos1)/mykernel
boot
```

This method can be used to boot from an emulator as well as from a real PC, but it assumes that you have placed the kernel somewhere on the real or emulated PC and requires that you enter the path manually at boot time. This is a little bit tiresome and sometimes not possible. Therefore we will now enhance our image to include a kernel and a configuration file telling GRUB2 to load this kernel in the CD image.

### Adding the kernel and the configuration data to the image

For that purpose, we can use the fact that directories passed as argument to grub-mkrescue are added to the image. So let us create a directory iso somewhere in our working directory, copy the kernel into that directory and add it to the image (replace ctOSkernel below by the full path to a ctOS kernel).

```
mkdir iso
cp ctOSkernel ./iso
grub-mkrescue -o cdimage iso
```

If you now set up a loopback device pointing to that image and mount it, you will see that in addition to the directory boot created by grub-mkrescue, the file `ctOSkernel` has been placed in the root directory of the image. You can now boot from that image as before, but now enter

```
multiboot2 (cd)/ctOSkernel
boot
```

to boot the kernel (which will fail in this case as we do not have a ramdisk yet).

Now thats nice - but still we have to enter all these commands manually after booting into GRUB2. To fix this, we need a grub.cfg file. The final stage of the GRUB2 loader will look for this file in the directory /boot/grub on the device from which we boot and execute the commands in that file. So we create a directory iso/boot/grub in our iso-subdirectory and create a file grub.cfg within that directory asking GRUB2 to boot our kernel

```
mkdir iso/boot
mkdir iso/boot/grub
echo "multiboot2 (cd)/ctOSkernel ; boot" > iso/boot/grub/grub.cfg
grub-mkrescue -o cdimage iso
```

If you now boot from that image, you will be taken directly to our kernel. Unfortunately, there is still no ramdisk. To fix this, we apply the same trick again - we add the ramdisk to the image by placing it into the `iso` directory and add a line to the configuration file asking GRUB2 to load it.

```
cp my.ramdisk ./iso/ramdisk.img
echo "multiboot2 (cd)/ctOSkernel ; module2 (cd)/ramdisk.img ; boot" > iso/boot/grub/grub.cfg
grub-mkrescue -o cdimage iso
```

where again you need to replace my.ramdisk by the path to a valid ctOS ramdisk (generated for instance by the helper script `bin/init_images.h` which uses a very similar command to create a ctOS boot image).


## Creating a USB bootable image


Booting from CD is a rather reliable way of getting your kernel to run on virtually all PCs that still support a legacy BIOS or UEFI in CSM mode, as most BIOSes will support booting from CD. However, if you change your kernel often, it is probably more efficient to use an USB stick to boot instead of burning a new CD each time the kernel is changed. In this section, we will set up a bootable image on an USB stick.

As an USB stick is handled as a hard drive most of the time by the BIOS (most USB sticks actually have a MBR and a partition table in their first sector), we will first go through the process of setting up GRUB2 from scratch on a hard disk image.

### Making a hard disk image bootable with GRUB2

First let us create a hard disk image which can be used with Bochs and QEMU. We will create an image with 20 MB capacity. First we simply create an empty file of that size.

```
$ dd if=/dev/zero of=hdimage bs=512 count=38272
```

The strategy to make this image bootable is as follows.

-   We will create an ext2 file system on this disk in which we store our configuration file,
-   To be able to access this data, we need a core.img in which we embed the modules GRUB2 needs to access this data
-   The location of the configuration file is stored in a minimal configuration file which we are going to embed into the image as well

Let us now go through these steps one by one. First we will partition our hard disk to be able to create a filesystem on it. We will create one primary partition starting at sector 2048 so that there is enough space after the MBR to place our core image there. This will allows us to include the modules which we need directly in our core image. However, before we generate our partition table, we copy the boot image of GRUB2 to the first 512 bytes as doing this later would overwrite our newly created partition table. So enter

```
$ sudo losetup /dev/loop0 hdimage
$ sudo dd if=/usr/lib/grub/i386-pc/boot.img of=/dev/loop0
$ sudo fdisk /dev/loop0
```

This will copy the image to the first sector of our virtual hard drive and start fdisk to add a partition. Now enter the following commands in this order to first delete the existing partitions
```
d
1
d
2
d
3
d
n
p
1
2048
<hit return to accept default>
w
```


Now let us set up the file system. As the first partition starts at sector 2048, we need to create a second loopback device starting at this sector first. As a sector is 512 bytes, the needed offset is 2048\*512. We mount this partition at /mnt.

```
$ sudo losetup -o $((512*2048)) /dev/loop1 hdimage
$ sudo mkfs -t ext2 /dev/loop1
```


We can now start to set up the data on the disk. We create a directory called /grub2 (note the non-standard naming - this makes it easier to try out a few things, you could give this directory any name) and copy our kernel into this directory.

```
$ sudo mount /dev/loop1 /mnt
$ sudo mkdir /mnt/grub2
$ sudo cp my.kernel /mnt/grub2/ctOSkernel
```

Here, again we need to replace my.kernel by the full path name of the kernel we want to use.

Next, we create our embedded configuration file. This configuration file will use the search command of GRUB2 to locate our root directory by UUID and set the root and prefix variables accordingly. So create a file in your working directory called boot.cfg with the following content.

```
search.fs_uuid da12ae0d-10c4-4d27-a7b1-983ca516b25e root
set prefix=($root)/grub2
```

Note the UUID in the search command - you need to run blkid as root while the image is mounted to obtain the UUID of the filesystem which we have just created, like this

```
sudo blkid -c /dev/null /dev/loop1
```

and replace the value above by the output.

After all these preparations, we are now ready to create our image. To create the core image including needed modules and place it in our disk image right after the MBR, run

```
$ grub-mkimage -O i386-pc search normal ls multiboot multiboot2 ext2 part_msdos  biosdisk elf configfile -c boot.cfg > core.img
$ sudo dd if=core.img seek=1 of=/dev/loop0
```

Note that we use `/dev/loop0` here, i.e. we place the image between the MBR and the ext2 file system. Now we copy the ramdisk image to `/mnt/grub2`

```
$ sudo cp my.ramdisk /mnt/grub2/ramdisk.img
```

Finally, we create a config file grub.cfg with the following data and place it in /mnt/grub2. Do not forget to unmount after doing that to flush the file system and make sure that everything gets written to the image.

```
menuentry "ctOS" {
    insmod multiboot2
    multiboot2 ($root)/grub2/ctOSkernel
    module2 ($root)/grub2/ramdisk.img
    boot
}
```


If you now boot from this image with qemu or Bochs, you should see the GRUB2 menu with only one entry, and selecting that entry should boot into ctOS.

Starting from that working configuration, you can now decrease the size of the core image by moving modules to /grub2. At this point, it is important to consider that modules have dependencies. So for instance if you want to load the module multiboot dynamically, you will also have to put the modules boot, video, relocator, mmap, lsapm and vbe into /grub2. These dependencies are document in the file moddep.lst which is part of the GRUB2 documentation and located in /usr/lib/grub/i386-pc.

Assuming that you have enough space on the device, the easiest approach to resolve dependencies it to simply copy **all** modules from /boot/grub on a running Linux system to the /grub2 folder in the image. After doing this, you should be able to load all modules via insmod from the CLI.

In this case, you only need to include a minimum set of modules in the core image, namely search, ext2, part_msdos and biosdisk, which reduced its size to less than 32 kb - this would also be sufficient if the first partition started already at sector 63.


### Creating a bootable USB stick

To create a bootable USB stick is now very easy. However, please use a stick which does not contain any data, as what we are going to do now will wipe out everything which used to be on the disk!

Most USB sticks seem to be organized similar to a hard disk. Read and write is done in sectors of 512 bytes. The first sector is a MBR which can optionally contain a boot sector and contains a partition table. After the MBR, you can theoretically place any kind of filesystem you want on the disk.

So an easy approach to make an USB stick bootable is to just copy the hard disk image created above on a USB stick, starting at the first partion. To do this, first plug in your USB and type df -k to see to which device your stick is mapped. You should see something like

```
/dev/sdb1              1924048      2880   1823428   1% /media/76398c6a-fa6e-4a81-a523-0d8aedf63992
```

If, as in this example, the stick is mapped to /dev/sdb1, this means that the raw device is accessible as /dev/sdb - the logic is similar to /dev/sda representing your entire disk and /dev/sda1 representing the first partition.

Copying our generated image to the stick is now as easy as unmount the file system (most of the time mounted somewhere under /media, but this depends on your distribution), and copying our image to /dev/sdb using dd

```
$ umount /media/76398c6a-fa6e-4a81-a523-0d8aedf63992
$ sudo dd if=hdimage of=/dev/sdb
```

Again, note that this will **overwrite the partition table and the start of the filesystem, effectively wiping all previous data from the disk**.

Now unplug the stick and plug it in again. You should now see a partition being mounted which contains our grub2 folder. So the MBR which we have just copied on our stick is recognized and the first and only partition in it is found and mounted.

We can test that this works using QEMU again. While the stick is mounted, run

```
sudo qemu-system-i386 -usb -usbdevice disk:/dev/sdb 
```

Once in QEMU, hit F12 to enter the boot menu and select the USB drive. You should now be taken to the GRUB boot menu and be able to start ctOS.

When you are done, reboot your PC. Enter the BIOS and change the boot order to boot from something called USB-HDD first - and you should see the beautiful GRUB2 menu again.

The nice thing is that once you have finished this initial setup, you can simply mount the stick again - it will now appear a a single ext2 partition with 20 MB capacity - and copy new versions of the kernel to it.

However, our approach to initially copy the entire image to the stick has a major disadvantage - it will restrict the use as an ordinary USB stick considerably. The partition type we have chosen is ext2, so Windows will not be able to recognize your stick. Moreover, the partition is only 20 MB in size, so you can only use up to 20 MB on your stick.
To avoid this, you can use the following procedure. We will not explain each of its steps in detail, as they are very similar to what we have done before.

1.  Copy the boot image from /usr/lib/grub/i386-pc to the stick as before
2.  Repartition the stick, create a partition table with only one FAT32 partition (type 0xb) starting at sector 2048 (again, this will leave enough space for our core image)
3.  Use mkfs to format the FAT32 partition (use -t vfat)
4.  Create a new embedded configuration file - you will have to update the UUID
5.  Create a new GRUB2 core image, containing the FAT module instead of the EXT2 module
6.  Copy that image to your stick. again starting at block 1
7.  Create a grub2 directory in the FAT32 partition on your stick and put the configuration file, the ramdisk and the kernel into that directory

You should now be able to boot from that stick and at the same time use it under Windows and Linux as "ordinary" USB stick (when testing this with QEMU, I sometimes had read errors, I assume that the QEMU BIOS has problems with recent and therefore large USB drives, the same drive worked on real hardware).

Note: sometimes fdisk makes problems when trying to partition an USB stick if the existing content of the MBR is to weird to be interpreted correctly. In this case, you can use dd to zero out the MBR entirely before recreating the MBR from scratch.

### Loading modules with GRUB2

In addition to the main kernel image, GRUB2 offers the option to load arbitrary files called **modules** at boot time. In contrast to the main image, these files are not supposed to be binary ELF files or to contain a multiboot header, any file can be used. The boot loader will place these files somewhere in memory and - as part of the multiboot information structure - inform the operating system kernel about their location.

To load a module, use the command `module2` inside your GRUB2 configuration file, similar to the following example.

```
menuentry "Own OS" {           
    insmod ext2           
    insmod multiboot2           
    multiboot2  (hd0,msdos6)/ctOS/kernel/mykernel
    module2     (hd0,msdos6)/ctOS/ramdisk.img       }
```

This will load the file /ctOS/ramdisk.img located on the same device as the kernel itself. Additional parameters can be added after the file name which will then be accessible for the operating system kernel as part of the "module string" saved in the multiboot information structure. Note, however, that different implementations of the multiboot specification handle this differently: while GRUB2 places only the part after the file name in the module string, QEMU places the entire command including the filename in the module string.


## Booting with UEFI and GRUB2


So far we have been looking at how GRUB2 can be used when booting via the good old BIOS. However, essentially all modern PCs use UEFI instead of the BIOS, and more and more of them do not offer a BIOS emulation (CSM) any more, so that it is time to look at how we can boot on an UEFI-only PC. Here are the instructions for doing this (note, however, that ctOS will boot, but not display any text or graphics, as the VGA configuration still uses the BIOS - I plan to fix this in a future release).

First, we will have to install the EFI version of GRUB and OVMF which is an open source EFI firmware based on TianoCore. On Ubuntu, you can get the relevant packages as follows.

```
sudo apt-get install ovmf
sudo apt-get install grub-efi-amd64-bin
```

Next, let us discuss our disk layout. EFI systems use a different format of the partition table called **GPT**. With GPT, the MBR only holds a dummy entry, and the actual partition table is located in the sectors after the MBR.

To make our system boot, we will need at least two partitions. The first partition is our **EFI boot partition**. This is the partition in which the EFI firmware will try to locate our GRUB2 installation. Booting from EFI is much easier than booting from the BIOS - it is no longer necessary to squeeze our code into the MBR and work with several stages. Instead, EFI is able to read a FAT32 partition - the said EFI boot partition - and to load any image that we place there which is in the PE executable format. So our approach will be as follows.

* we will create an EFI partition and an EXT2 partition
* in the EFI partition, we will place a GRUB2 executable in PE format and an initial GRUB2 configuration file
* EFI will then detect the GRUB2 executable and run it - at this point, we are still in 64 bit mode
* Then GRUB2 will locate our ctOS kernel image, load the image, switch back to protected mode and execute the image

Let us first create a disk image again. As a FAT32 file system requires at least 100 MB, our image will be a bit larger than before.

```
dd if=/dev/zero of=efiimage bs=512 count=1048576
```

Now we partition the image. As we need to create a partition table in the new GPT format, we do not use fdisk but gdisk which is very similar but is able to deal with GPT partition tables. So run

```
gdisk efiimage
```

and enter the following commands to create our two partitions (note that an EFI partition has partition type EF00).

```
n
1
2048
+100M
ef00
n
2
<accept default>
<accept default>
<enter>
w
```

We can verify that everything worked using again gdisk

```
$ gdisk -l efiimage 
GPT fdisk (gdisk) version 1.0.1

Partition table scan:
  MBR: protective
  BSD: not present
  APM: not present
  GPT: present

Found valid GPT with protective MBR; using GPT.
Disk efiimage: 1048576 sectors, 512.0 MiB
Logical sector size: 512 bytes
Disk identifier (GUID): 09F27C6B-3CB4-40E3-B7D3-4C570B0FFAA6
Partition table holds up to 128 entries
First usable sector is 34, last usable sector is 1048542
Partitions will be aligned on 2048-sector boundaries
Total free space is 2014 sectors (1007.0 KiB)

Number  Start (sector)    End (sector)  Size       Code  Name
   1            2048          206847   100.0 MiB   EF00  EFI System
   2          206848         1048542   411.0 MiB   8300  Linux filesystem
```


Now we mount our newly created partitions and create the file systems.

```
$ sudo losetup /dev/loop0 efiimage
$ sudo partprobe /dev/loop0
$ sudo mkfs.fat -F32 /dev/loop0p1
$ sudo mkfs -t ext2 -O none -L "root" /dev/loop0p2
```

Next, we will prepare the EFI partition. Inside the EFI partition, we will create a directory /EFI/BOOT. By convention, EFI will try to detect a file called bootx64.efi in that directory and load it is it exists. So we create a GRUB2 image and place it in that directory on the EFI partition.

```
$ mkdir -p mnt
$ sudo mount /dev/loop0p1 mnt
$ sudo mkdir -p mnt/EFI/BOOT
$ sudo grub-mkimage \
       -d /usr/lib/grub/x86_64-efi \
       -o mnt/EFI/BOOT/bootx64.efi \
       -p /EFI/BOOT \
       -O x86_64-efi \
       search search_label normal ls multiboot multiboot2 ext2 fat \
       efifwsetup efi_gop efi_uga \
       part_gpt part_msdos boot elf configfile
```

In the same partition, we also place a minimal configuration file which essentially sets the root partition and then loads our main configuration file that we will store in the EXT2 partition. Again, replace the UUID below by the actual UUID of the EXT2 file system.

```
$ cat <<EOF > boot.cfg
search.fs_uuid ff64f2a8-61ea-49f3-878d-d5cb75dc5a3e root
set prefix=(\$root)/grub2
configfile (\$root)/grub2/grub.cfg
EOF
$ sudo cp boot.cfg mnt/EFI/BOOT/grub.cfg
$ sudo umount mnt
```

Next we prepare our EXT2 partition. Inside that partition, we will store our kernel, the actual configuration file and the ramdisk. The configuration file is exactly as before.

```
$ sudo mount /dev/loop0p2 mnt
$ sudo mkdir -p mnt/grub2
$ sudo cp <path_to_kernel> mnt/grub2/ctOSkernel
$ sudo cp <path_to_ramdisk> mnt/grub2/ramdisk.img
$ sudo cp grub.cfg mnt/grub2/grub.cfg
$ sudo umount mnt
$ sudo losetup -D
```
 
Now we can test our installation. We will use the 64 bit version of QEMU and ask it to use OVMF as BIOS.
 
```
qemu-system-x86_64 --bios /usr/share/qemu/OVMF.fd -drive id=disk,file=efiimage,if=ide,media=disk -m 512 -debugcon stdio
```

Instead of doing all this manually, you can also use the scripts that come with the ctOS source code. The script `bin/efi_image.sh` will use the method described above to create an EFI bootable image. In the script `bin/run.sh`, there is a run target `efi` which brings up an EFI based QEMU instance running ctOS with a simulated IDE disk and a network device. Finally, the target `efi-smp` is an EFI based SMP system with an emulated AHCI drive.

Further reading
---------------

-   The multiboot specification - available at <http://www.gnu.org/software/grub/manual/multiboot/multiboot.html>
-   The GRUB2 documentation - online at <http://www.gnu.org/software/grub/manual/grub.html>
- The [blog post](https://blog.heckel.xyz/2017/05/28/creating-a-bios-gpt-and-uefi-gpt-grub-bootable-linux-system/) by P. Heckel

