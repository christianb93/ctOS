#!/bin/bash

#
# This script will try to detect existing images and update them. This is faster
# and does not require root privileges. Note that this only works if you did run
# init_images.sh at least once!
#

#
# Use e2tools to copy our data to an image
# TODO: merge this somehow with prepare_image.sh
#
function copyFiles() {
    e2cp ../userspace/cli $1:bin
    e2cp ../userspace/init $1:bin
    e2cp ../userspace/args $1:bin
    for i in testjc testwait testfiles testsignals testpipes testfork testmisc testtty testatexit testall testnet
    do
        e2cp ../userspace/tests/$i $1:tests
    done
    if [ -d "import/bin" ]
    then
        for i in $(ls import/bin)
        do
            e2cp ./import/bin/$i $1:bin
        done
    fi
}


ctOSUser=$(whoami)

if [ ! -e "ctOSkernel" ]
then
  echo "Could not find kernel, are you sure that you did execute me in the bin directory?"
  exit
fi



#
# Let us start with the ramdisk image - if it is there
#
if [ -e "ramdisk.img" ]
then
    #
    # Copy all the updated files - make sure that this is in sync with prepare_image.sh!
    #
    copyFiles "ramdisk.img"
fi


echo "Updated ramdisk file (ramdisk.img)"
echo "Let me now create an ISO image for you"

cp ctOSkernel ./iso
cp ramdisk.img ./iso
grub-mkrescue -o cdimage.iso iso

echo "Done, created CD image cdimage.iso"
ls -l cdimage.iso



if [ -e "hdimage" ]
then
    echo "Now let us move on to the hard disk image"
    dd if=/dev/zero of=hdimage bs=512 count=2048
    dd if=ramdisk.img of=hdimage oflag=append conv=notrunc

    sfdisk --no-reread hdimage << EOF
2048,,83
EOF
fi

    
if [ -e "efiimage" ]
then
    echo "Finally I will patch the EFI image"
    #
    # We split the existing image into two parts first
    # The first part contains the MBR, the GPT and the
    # FAT file system, the second part everything else
    #
    dd if=efiimage of=part1 bs=512 skip=206848
    #
    # Next we treat the second part as an ext2 file system and
    # update it. We first do the kernel and the ramdisk
    #
    e2cp ctOSkernel part1:grub2
    e2cp ramdisk.img part1:grub2
    # 
    # and then all the other stuff 
    #
    copyFiles "part1"
    #
    # Now we patch the two parts together again
    #
    dd if=part1 of=efiimage bs=512 seek=206848
    rm -f part1
fi
