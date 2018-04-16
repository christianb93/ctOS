#!/bin/bash

QEMU="qemu-system-i386 -k de  -debugcon stdio -m 512"
CDROM=""
BOOT=""
HD=""
NET=""
APPEND=""
KERNEL=""

#
# Are we in the right directory?
#

if [ ! -e "bin/cdimage.iso" ]
then
  echo "Could not find image, are you sure that you did execute me in the ctOS home directory?"
  exit
fi

#
# Run one of several configurations of ctOS with different emulators
#
if [ $# -gt 0 ]; then
    CONF=$1
    echo "Using configuration " $CONF
else
    echo "No configuration specified, using default 0"
    CONF=0
fi

#
# Determine configuration
#
case $CONF in
0)
    #
    # Default configuration - use QEMU to boot from CDROM
    # Required images: CD ROM ISO image
    #
    EMU=$QEMU
    CDROM="-cdrom bin/cdimage.iso"
    ;;
1)
    #
    # Use QEMUs multiboot capabilities to boot the kernel directly,
    # attach a PATA drive 
    # Required images: kernel, hdimage
    #
    EMU=$QEMU
    HD="-drive id=disk,file=bin/hdimage,if=ide,media=disk"
    KERNEL="-kernel bin/ctOSkernel"
    APPEND="-append \"use_debug_port=1 root=769 loglevel=0\""
    ;;

2)
    #
    # Use QEMUs multiboot capabilities to boot the kernel directly,
    # attach a PATA drive  and a network drive
    # Required images: kernel, hdimage
    #
    EMU=$QEMU
    HD="-drive id=disk,file=bin/hdimage,if=ide,media=disk"
    KERNEL="-kernel bin/ctOSkernel"
    APPEND="-append \"use_debug_port=1 root=769 loglevel=0\""
    NET="-net nic,vlan=1,macaddr=00:00:00:00:11:11,model=rtl8139 -net socket,vlan=1,listen=127.0.0.1:9030"
    ;;



esac
    
CMD="$EMU $CDROM $HD $NET $KERNEL $APPEND"
echo "Running " $CMD
eval $CMD

