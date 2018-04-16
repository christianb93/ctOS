#!/bin/bash

#
# ctOS needs images at three different points, depending on the use case:
#
# - ctOS uses the GRUB2 multiboot protocol. To boot on real hardware or
#   on an emulator that does not suppport this, we need a CD IOS image
# - if you boot from CD, you will need a ram disk that contains the root
#   file system and the user space programs
# - for some tests, like the ext2 file system unit test, a test image is
#   required
# - finally, if you want to emulate a hard disk, you will need a 
#   HD image including a MBR
# 
# This script will build the first three of these images. Please make sure to execute it
# in the ctOS bin directory (where the kernel is located as well)
# (to build to test image, use init_test_images.sh in the tools directory)
#
ctOSUser=$(whoami)

if [ ! -e "ctOSkernel" ]
then
  echo "Could not find kernel, are you sure that you did execute me in the bin directory?"
  exit
fi

#
# Do not say that you have not been warned
#
echo "I will now create the ctOS disk images. Please note that I will use the loop device"
echo "   ----> /dev/loop7 <----"
echo "for this purpose. Please make sure that this device exists and is not already used"
echo "I will also need grub2 and xorriso installed."
echo ""
echo "You should also run this as the user owning the ctOS installation, but I will"
echo "use sudo at some points, so be prepared to enter your password."
echo "You are currently logged in as "  $ctOSUser
read -p "Ok and proceed? (Y/n): "
if [ "$REPLY" != "Y" ] ; then
    echo "Exiting, but you can restart at any point in time."
    exit
fi


#
# For building the CD image, we also need the ramdisk image, so we do
# this first. First we create an empty file 
#
rm -f ramdisk.img
dd if=/dev/zero of=ramdisk.img bs=512 count=19136

#
# Create an ext2 file system on it
#
mkfs -t ext2 -b 1024 -O none  ramdisk.img
#
# Now mount it 
#
sudo losetup /dev/loop7 ramdisk.img
mkdir -p mnt
sudo mount /dev/loop7 mnt
#
# Create a TTY device and copy some files from the userspace
# directory into it, then chown
#
sudo mkdir mnt/dev
sudo mknod -m 666 mnt/dev/tty c 2 0
sudo cp ../userspace/cli ../userspace/init ../userspace/testfork ../userspace/testfiles ../userspace/testmisc ../userspace/testwait mnt
echo "Hello" > /tmp/hello
sudo cp /tmp/hello mnt
sudo chown -R $ctOSUser mnt/*
#
# Clean up
#
sudo umount mnt
sudo losetup -d /dev/loop7

echo "Created ramdisk file (ramdisk.img)"
echo "Let me kow create an ISO image for you"

cp ctOSkernel ./iso
cp ramdisk.img ./iso
grub-mkrescue -o cdimage.iso iso

echo "Done, created CD image cdimage.iso"
ls -l cdimage.iso

#
# Now let us try to build the HD image
#
echo "Will now build a HD image"

#
# The layout of the image that we build is as follows (all blocks 512 bytes)
# - at block 0, there will be the MBR and the partition table
# - the following blocks are unused
# - starting at block 2048, we will have our file system
# So we first create an empty file of size 512*2048 and then append the
# ramdisk image to this
dd if=/dev/zero of=hdimage bs=512 count=2048
dd if=ramdisk.img of=hdimage oflag=append conv=notrunc

sudo losetup /dev/loop7 hdimage
sudo sfdisk --no-reread /dev/loop7 << EOF
2048,,83
EOF

#
# Clean up 
#
sudo losetup -d /dev/loop7
