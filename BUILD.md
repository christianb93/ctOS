## Building ctOS from source


To build ctOS from source, you need a fairly recent version of the GNU C Compiler (GCC). I am currently using GCC 5.4.0 on an Ubuntu Linux x86_64 system, but any recent version should work - ctOS is mainly self contained and does (apart from the tests) not use any external libraries. 

In addition, you will need ld from the binutils package, I am currently using version 2.26.1. Make sure that both, gcc and ld, are in your current path. And of course you will need the make utility.

**Building the kernel and the userspace libraries**

To build, get the current version of the source code, change to the newly created ctOS directory and do a simple make.

```bash
git clone https://www.github.com/christianb93/ctOS
cd ctOS
make
```

Whe the build is successful, you should find that a file `bin/ctOSkernel` has been created. You now have a kernel in ELF Multiboot format, bootable by GRUB or QEMU, along with the userspace libraries and programs.

**Building the images**

To build the images, first make sure that you have the following tools installed.

* grub-mkrescue (for Ubuntu, this is in the package grub-common)
* xorriso (package xorriso)

Then execute the following commands:

```bash
cd bin
./init_images.sh
```

This will create a bootable ISO image as well as a hard disk image and an image for the ramdisk. These images are rather small, somewhere between 10 and 16 MB. During the process of building the images, the script will need to mount a loopback device and therefore needs to be root, so need a user with sudo privileges and will be prompted for your password.

Also note that towards the end of the script, we use sfdisk to add a partition table to the newly created image. At this point, the Linux kernel sees that we add a partition, but does not know that this is just on an image. So we get the warning

```
Calling ioctl() to re-read partition table.
Re-reading the partition table failed.: Invalid argument
The kernel still uses the old table. The new table will be used at the next reboot or after you run partprobe(8) or kpartx(8).
```

This is perfectly fine and can be ignored.

**Running**

Once you have created the images, it is time to run ctOS. The easiest way to do this is to use the run.sh script which is in the bin directory. This needs to be executed from the top level directory. So after having created the images, do

```bash 
cd ..
./bin/run.sh
```

Without any options, this will run ctOS on the QEMU emulator (assuming that it is installed). To see a list of available configurations, enter `./bin/run.sh ?`. Currently, all configurations require QEMU, but ctOS also works with Bochs and Virtualbox, and I will add more run targets in the future.

Of course, you can also try to run ctOS on real hardware. **Be very careful when you try this, as especially during installation, you can break your partition table and loose data!** Needless to say that I do not assume responsibility for any potential damage that your system incurs while trying this. 

One possible approach to get ctOS up and running on your system is to use an existing GRUB2 installation. Here are instructions to do this on an Ubuntu system.

First, as root, navigate to `/etc/grub.d/`. In this directory, you will find a couple of scripts that GRUB2 will run during the configuration process and that will spit out individual parts of the GRUB configuration file. The default installation already contains a file `40_custom` that you can use to build your own entries. This is a very simple script that looks as follows.

```bash
#!/bin/sh
exec tail -n +3 $0
# This file provides an easy way to add custom menu entries.  Simply type the
# menu entries you want to add after this comment.  Be careful not to change
# the 'exec tail' line above.
```

This script will simply print out everything below the third line. So we can make it print out our custom configuration by simply adding a GRUB2 configuration entry, like the following one.

```bash
#!/bin/sh
exec tail -n +3 $0
# This file provides an easy way to add custom menu entries.  Simply type the
# menu entries you want to add after this comment.  Be careful not to change
# the 'exec tail' line above.
menuentry "ctOS (RAMDISK)" {
  multiboot /boot/ctOSkernel use_debug_port=1 root=256 loglevel=0 do_test=0 
  module /boot/ctOS.ramdisk.img
}
```
This assumes that you have copied the current kernel to `/boot/ctOSkernel` and the ramdisk to `/boot/ctOS.ramdisk.img`. Then regenerate the GRUB2 configuration file using

```
sudo update-grub
```

and verify the result in `/boot/grub/grub.cfg`. If you are satisfied with the result, update the actual GRUB2 boot record by running

```
sudo grub-mkconfig
```

It is usually best to first boot using the ramdisk, as in the example above. Then use the kernel debugger to verify that ctOS has recognized your drives and partitions, and then exchange the boot partition using the parameter `root=` in the GRUB configuration. On my machine, for instance, I have a ctOS partition `/dev/sda10` on an AHCI drive which corresponds to `root=1034`. 

