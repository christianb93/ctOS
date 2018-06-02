#!/bin/bash

#
# Create a ctOS EFI boot image
#
# Please make sure to execute it in the ctOS bin directory (where the kernel is located as well)
# after having run init_images.sh which will create the ramdisk image that we need
# 
#
ctOSUser=$(whoami)

#
# Do not say that you have not been warned
#
echo "I will now create a ctOS EFI disk image. Please note that I will use the loop device"
echo "   ----> /dev/loop0 <----"
echo "for this purpose. Please make sure that this device exists and is not already used"
echo "I will also need to assume that you have run init_images.sh before as I need some"
echo "of the images that it produced".
echo "You should also run this as the user owning the ctOS installation, but I will"
echo "use sudo at some points, so be prepared to enter your password."
echo "You are currently logged in as "  $ctOSUser
read -p "Ok and proceed? (Y/n): "
if [ "$REPLY" != "Y" ] ; then
    echo "Exiting, but you can restart at any point in time."
    exit
fi

if [ ! -e "ctOSkernel" ]
then
  echo "Could not find kernel, are you sure that you did execute me in the bin directory?"
  exit
fi


if [ ! -e "ramdisk.img" ]
then
  echo "Could not find the ramdisk image, are you sure that you did run init_images.sh?"
  exit
fi


#
# Create an empty image. Note that the size of the image is given by
# three variables:
# - the first 2048 sectors will be the GPT
# - the next 204800 sectors will hold an FAT32 file system
# - the last part will be an ext2 image
# - finally we add a few sectors at the end as gdisk will add a GPT backup
#
dd if=/dev/zero of=efiimage bs=512 count=$((204800 + 2048 + 84740 + 33))
ls -l efiimage

#
# and partition
#
gdisk efiimage << EOF
n
1
2048
+100M
ef00
n
2



w
Y
EOF

#
# Now we mount the partitions and create filesystems
#
sudo losetup /dev/loop0 efiimage
sudo partprobe /dev/loop0
sudo mkfs.fat -F32 /dev/loop0p1
sudo mkfs -t ext2 -O none /dev/loop0p2

#
# Prepare the EFI partition
#
mkdir -p mnt
sudo mount /dev/loop0p1 mnt
sudo mkdir -p mnt/EFI/BOOT
sudo grub-mkimage \
       -d /usr/lib/grub/x86_64-efi \
       -o mnt/EFI/BOOT/bootx64.efi \
       -p /EFI/BOOT \
       -O x86_64-efi \
       search search_label normal ls multiboot multiboot2 ext2 fat \
       efifwsetup efi_gop efi_uga \
       part_gpt part_msdos boot elf configfile

#
# Get UUID of the file system
#
UUID=`lsblk -ln -o UUID /dev/loop0p2`

#
# Prepare boot.cfg
#
echo "search.fs_uuid $UUID root" > boot.cfg
cat <<EOF >> boot.cfg
set prefix=(\$root)/grub2
configfile (\$root)/grub2/grub.cfg
EOF
#
# and copy it
#
sudo cp boot.cfg mnt/EFI/BOOT/grub.cfg
sudo umount mnt

#
# Next we prepare our EXT2 partition. Inside that partition, we will store our kernel, the actual configuration file and the ramdisk. 
#
sudo mount /dev/loop0p2 mnt
sudo mkdir -p mnt/grub2
sudo cp ctOSkernel mnt/grub2/ctOSkernel
sudo cp ramdisk.img mnt/grub2/ramdisk.img
cat <<EOF > grub.cfg
menuentry "ctOS" {
    insmod multiboot2
    multiboot2 (\$root)/grub2/ctOSkernel use_acpi=1
    module2 (\$root)/grub2/ramdisk.img
    boot
}
menuentry "ctOS (IDE HD)" {
  multiboot2 (\$root)/grub2/ctOSkernel use_debug_port=1 root=770 loglevel=0 do_test=0 use_acpi=1
  boot
}
menuentry "ctOS (AHCI HD)" {
  multiboot2 (\$root)/grub2/ctOSkernel use_debug_port=1 root=1026 loglevel=0 do_test=0 use_acpi=1
  boot
}
EOF
sudo cp grub.cfg mnt/grub2/grub.cfg


#
# Set it up
#
source prepare_image.sh

#
# Unmount
#
sudo umount mnt
 

#
# Clean up 
#
rm -f grub.cfg
rm -f boot.cfg
rmdir mnt
sudo losetup -D



