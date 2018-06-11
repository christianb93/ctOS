# ctOS - a toy 32-bit operating system


## What is ctOS

This repository contains the source code for a toy operating sytem called **ctOS** that I created many years ago (most of the work was actually done in 2011 and 2012, judging from timestamps of old backups that I found). As I have not been able to find the time for a couple of years now to actually continue working on it,  and also have other interests now, I decided to publish the code "as-is" on GitHub, hoping that it might be useful for other OS hobbyist developers (and if it be only to learn from my mistakes).

So I invested some time to dig out the old source code and find my way through the build system. Back in 2012, the build was done on a 32 bit system and I mostly used the [QEMU emulator][1] in version somewhere around v1.0. So I had to migrate the build scripts to create 32 bit code on a 64 bit host (easy) and to adapt the code so that the kernel would run on a recent version like 2.5 of QEMU (not so easy, as QMEU has changed a lot in the meantime, unveiling some errors in my code that I had to fix). Currently, most of the tests are green again, though there are probably still a few bugs hidden somewhere.

## Features

Currently, the following features have been implemented:

* Boot using GRUBs multiboot configuration (has support for multiboot and multiboot2)
* Support for BIOS and (U)EFI based systems
* Basic ACPI support
* Support for SMP with up to eight CPUs 
* Virtual memory and multi-tasking
* Built-in kernel debugger to inspect CPU state, memory contents, threads, kernel configuration etc. (launch with F1)
* 32 bit protected mode (64 bit not supported and unlikely that I will ever find the time to port it)
* Virtual memory and multi-tasking
* Kernel threads and threaded interrupt handling (queues)
* POSIX Signal handling
* Job control a la POSIX
* Pipes
* RAM disk
* Device drivers:
    * VGA (using VBE)
    * Keyboard
    * RTC
    * Ethernet adapter (RTL8139)
    * IDE hard drive
    * AHCI hard drive
    * PCI bus
* Support for ext2 file system
* Interrupt routing via PIC, APIC and MSI
* A full networking stack, supporting ARP, ICMP, IP, UDP, TCP and DNS resolution
* A simple user-space command line utility
* A basic POSIX compatible C library
* A small floating point math library

I did also port a few userspace tools (dash, wget, elvis, most of the coreutils, a basic interpreter, ...) to work with ctOS. For licensing reasons, the required patches and build scripts are not contained in this repository but in a [separate repository](https://github.com/christianb93/ctOS_ports) - please have a look at the respective [documentation](https://github.com/christianb93/ctOS_ports/blob/master/README.md) on how to build the ports. The screenshot below, by the way, shows elvis running in ctOS on an QEMU emulator and editing `main.c` of the kernel code.

![ctOS Elvis][4]

## Running

I have tested ctOS with three emulators (QEMU, VirtualBox and Bochs) and also did quite a few tests on three different real machines. The easiest way to run ctOS is to use the emulator QEMU. Please make sure that you get a comparatively recent version of QEMU installed (Ubuntu 16.04 for instance comes with QEMU 2.5 which works fine). 

The binary distribution (see the [release page][2]) comes as a GZIPPED TAR files. To install and run, simply enter

```bash
gzip -d ctOS.bin.tar.gz
tar xvf ctOS.bin.tar
./bin/run.sh
```

This should bring up a QEMU window with a GRUB menu inside. Select the first option (ctOS from ramdisk) and hit enter. You should then see the first kernel messages from ctOS, telling you that a few subsystems have been initialized, a root partition has been mounted and the init process has been started. If everything works fine, you should see the prompt

```
@>
```

from the user space command line interface. Entering 'help' gives you a list of available commands and you can start to play around. 

## Additional run targets and tests

The run script that comes with ctOS supports different **run targets** which are combinations of emulators and settings. Currently, run targets for [QEMU](https://www.qemu.org/), [VirtualBox](https://www.virtualbox.org/) and [Bochs](http://bochs.sourceforge.net) are included, covering different configurations (attached drives, CPUs, networking). You can get a full list of supported targets by running

```
./bin/run.sh help
```

The file [TESTING.md][5] contains a few advanced scenarios that you can try out.

## Building

Instructions to build ctOS from the source can be found in the file [BUILD.md][3]

[1]: http://www.qemu.org
[2]: http://www.github.com/christianb93/ctOS/releases
[3]: https://github.com/christianb93/ctOS/blob/master/BUILD.md
[4]: https://leftasexercise.files.wordpress.com/2018/04/ctos_elvis.png
[5]: https://github.com/christianb93/ctOS/blob/master/TESTING.md
