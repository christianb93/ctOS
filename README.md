## ctOS - a toy 32-bit operating system


**What is ctOS**

This repository contains the source code for a toy operating sytem called **ctOS** that I created many years ago (most of the work was actually done in 2011 and 2012, judging from timestamps of old backups that I found). As I have not been able to find the time for a couple of years now to actually continue working on it,  and also have other interests now, I decided to publish the code "as-is" on GitHub, hoping that it might be useful for other OS hobbyist developers (and if it be only to learn from my mistakes).

So I invested some time to dig out the old source code and find my way through the build system. Back in 2012, the build was done on a 32 bit system and I mostly used the [QEMU emulator][1] in version somewhere around v1.0. So I had to migrate the build scripts to create 32 bit code on a 64 bit host (easy) and to adapt the code so that the kernel would run on a recent version like 2.5 of QEMU (not so easy, as QMEU has changed a lot in the meantime, unveiling some errors in my code that I had to fix). Currently, most of the tests are green again, though there are probably still a few bugs hidden somewhere.

**Features**

Currently, the following features have been implemented:

* Boot using GRUBs multiboot configuration
* Built-in kernel debugger to inspect CPU state, memory contents, threads, kernel configuraton etc. (launch with F1)
* 32 bit mode (64 bit not supported and unlikely that I will ever find the time to port it)
* Virtual memory and multi-tasking
* Signal handling
* Pipes
* RAM disk
* Device drivers:
    * VGA
    * Keyboard
    * Ethernet adapter (RTL8139)
    * IDE hard drive
    * AHCI hard drive
* Support for ext2 and FAT16 file systems
* APIC support
* Support for SMP with up to eight CPUs 
* A full networking stack, supporting ARP, ICMP, IP, UDP and TCP 
* A simple user-space command line utility
* A basic POSIX compatible C library

I did also port a few tools (dash, wget, elvis) in that past, but this requires a cross-build toolchain and I have not yet found the time to set this up again, so I will probably make this available in a separate repository at a later point in time. The screenshot below, by the way, shows elvis running in ctOS on an QEMU emulator and editing `main.c` of the kernel code.

![ctOS Elvis][4]

**Running**

I have tested ctOS with three emulators (QEMU, VirtualBox and Bochs) and also did quite a few tests on my old machine back in 2012 (no idea whether it would still run on curernt hardware). The easiest way to run ctOS is to the QEMU. Please make sure that you get a comparatively recent version of QEMU installed (Ubuntu 16.04 for instance comes with QEMU 2.5 which works fine). 

The binary distribution (see the [release page][2]) comes as a GZIPPED TAR files. To install and run, simply enter

```bash
gzip -d ctOS.bin.tar.gz
tar xvf ctOS.bin.tar
chmod 700 ./bin/run.sh
./bin/run.sh
```

This should bring up a QEMU window with a GRUB menu inside. Select the first option (ctOS from ramdisk) and hit enter. You should then see the first kernel messages from ctOS, telling you that a few subsystems have been initialized, a root partition has been mounted and the init process has been started. If everything works fine, you should see the prompt

```
@>
```

from the user space command line interface. Entering 'help' gives you a list of available commands and you can start to play around. I will add a description of a few test cases that you can run later.

**Building**

Instructions to build ctOS from the source can be found in the file [BUILD.md][3]

[1]: http://www.qemu.org
[2]: http://www.github.com/christianb93/ctOS/releases
[3]: https://www.github.com/christianb93/ctOS/BUILD.md
[4]: https://leftasexercise.files.wordpress.com/2018/04/ctos_elvis.png
