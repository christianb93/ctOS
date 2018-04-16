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
# This script will build all these images. Please make sure to execute it
# in the ctOS bin directory (where the kernel is located as well)
#
ctOSUser=$(whoami)

if [ ! -e "ext2samples" ]
then
  echo "Could not find ext2samples tool, are you sure that you did execute me in the bin directory?"
  exit
fi


#
# Get the testfile
#
if [ ! -e "testfile.src" ]
then
  echo "To test certain features of the ext2 file system (triple blocks), I need a test which"
  echo "has at least 127 MB. So please get a file like this and copy it to testfile.src" 
  echo "in this directory. You can clean up the file afterwards (but I will create a file"
  echo "testfile which has to stay there). If you do not know which file to take, run"
  echo "wget https://sourceforge.net/projects/gparted/files/gparted-live-stable/0.8.1-3/gparted-live-0.8.1-3.iso/download -O testfile.src"
  exit
fi




#
# Do not say that you have not been warned
#
echo "I will now create a unit test image. Please note that I will use the loop device"
echo "   ----> /dev/loop7 <----"
echo "for this purpose. Please make sure that this device exists and is not already used"
echo "You should also run this as the user owning the ctOS installation, but I will"
echo "use sudo at some points, so be prepared to enter your password."
echo "You are currently logged in as "  $ctOSUser
read -p "Ok and proceed? (Y/n): "
if [ "$REPLY" != "Y" ] ; then
    echo "Exiting, but you can restart at any point in time."
    exit
fi


rm -f rdimage
dd if=/dev/zero of=rdimage bs=512 count=407200

#
# Create an ext2 file system on it
#
mkfs -t ext2 -b 1024 -O none rdimage


#
# Now mount it 
#
mkdir -p mnt
sudo losetup /dev/loop7 rdimage
sudo mount /dev/loop7 mnt


#
# Next create the samples
#
dd if=testfile.src of=../test/testfile bs=512 count=261024
sudo cp ../test/testfile mnt/test
sudo ./ext2samples "A" mnt/sampleA
sudo ./ext2samples "B" mnt/sampleB
sudo ./ext2samples "C" mnt/sampleC
sudo ./ext2samples "D" mnt/sampleD
echo "Done, now showing you current content of the image: "
ls -li mnt

#
# Clean up
#
sudo umount mnt
sudo losetup -d /dev/loop7
mv rdimage ../test/rdimage

#
# Now we build two additional images for some of the tests
#
echo "Now creating two additional test images"
cp ../bin/ramdisk.img rdimage0
cp ../bin/ramdisk.img rdimage1


#
# Add some additional files
#
echo "hello" > hello
sudo losetup /dev/loop7 rdimage0
sudo mount /dev/loop7 mnt
sudo mkdir mnt/tmp
sudo cp hello mnt/hello
sudo umount /dev/loop7
sudo losetup -d /dev/loop7

sudo losetup /dev/loop7 rdimage1
sudo mount /dev/loop7 mnt
sudo cp hello mnt/mounted
sudo mkdir mnt/dir
sudo umount /dev/loop7
sudo losetup -d /dev/loop7
rm -rf hello


#
# Remove mount dir
# 
rm -rf mnt

#
# Relocate images
#
mv rdimage0 ../test/rdimage0
mv rdimage1 ../test/rdimage1
