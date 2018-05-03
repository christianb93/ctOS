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
# in the ctOS tools directory 
#
# Also note that this script requires the e2tools package, so please install it
# using for instance apt-get install e2tools on an Ubuntu machine
#
#

if [ ! -e "ext2samples" ]
then
  echo "Could not find ext2samples tool, are you sure that you did execute me in the tools directory?"
  exit
fi


#
# Get the testfile
#
if [ ! -e "testfile.src" ]
then
  echo "Creating large random file"
  dd if=/dev/urandom of=testfile.src bs=512 count=407200
fi


#
# Create an empty image
#
rm -f rdimage
dd if=/dev/zero of=rdimage bs=512 count=500000

#
# and an ext2 file system on it
#
mkfs -t ext2 -b 1024 -O none rdimage


#
# Next create the samples and copy them 
#
dd if=testfile.src of=../test/testfile bs=512 count=261024
e2cp ../test/testfile rdimage:test
./ext2samples "A" sampleA
e2cp sampleA rdimage:sampleA
./ext2samples "B" sampleB
e2cp sampleB rdimage:sampleB
./ext2samples "C" sampleC
e2cp sampleC rdimage:sampleC
./ext2samples "D" sampleD
e2cp sampleD rdimage:sampleD
rm -f sample{A,B,C,D}
echo "Done, now showing you current content of the image: "
e2ls -li rdimage:*

#
# Clean up
#
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
echo "Setting up rdimage0"
echo "hello" > hello
e2mkdir rdimage0:/tmp
e2cp hello rdimage0:hello

echo "Setting up rdimage1"
e2cp hello rdimage1:mounted
e2mkdir rdimage1:/dir
rm -f hello



#
# Relocate images
#
mv rdimage0 ../test/rdimage0
mv rdimage1 ../test/rdimage1
