#!/bin/bash

QEMU="qemu-system-i386 -k de  -debugcon stdio -m 512"
CDROM=""
BOOT=""
HD=""
NET=""
SMP=""
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
else
    echo "No configuration specified, using default configuration"
    CONF="default"
fi

#
# Determine configuration
#
case $CONF in
help | ?)
    # 
    # Print list of configurations and exit
    #
    echo "Usage run.sh [configuration]"
    echo ""
    echo "Available configurations:"
    cat << EOF
    
    default:    Boot using QEMU from the CDROM image and use ramdisk as root device
    qemu-ide:   Boot directly using multiboot, attach an IDE drive and use hard disk image
    qemu-net:   QEMU multiboot with IDE drive (hdimage) and a network card
    qemu-smp:   QEMU with 8 simulated CPUs
    qemu-tap:   QEMU attached to a tap networking device (must be root to run this)
    
    
EOF
    ;;
default)
    #
    # Default configuration - use QEMU to boot from CDROM
    # Required images: CD ROM ISO image
    #
    EMU=$QEMU
    CDROM="-cdrom bin/cdimage.iso"
    ;;
    
qemu-ide)
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

qemu-net)
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

qemu-smp)
    #
    # Use QEMUs multiboot capabilities to boot the kernel directly,
    # attach a PATA drive and simulate 8 CPUs
    # Required images: kernel, hdimage
    #
    EMU=$QEMU
    HD="-drive id=disk,file=bin/hdimage,if=ide,media=disk"
    KERNEL="-kernel bin/ctOSkernel"
    SMP="-smp sockets=8,threads=1 -enable-kvm"
    APPEND="-append \"use_debug_port=1 root=769 loglevel=0 vga=1\""
    ;;

qemu-tap)
    # 
    # Start a QEMU instance (IDE, 1 CPU) connected to a tap device
    # You need to be root to run this
    #
    EMU=$QEMU
    HD="-drive id=disk,file=bin/hdimage,if=ide,media=disk"
    KERNEL="-kernel bin/ctOSkernel"
    NET="-net nic,vlan=1,macaddr=00:00:00:00:11:11,model=rtl8139 -net dump,vlan=1,file=qemu_vlan1.pcap -net tap,vlan=1,script=no,downscript=no"
    APPEND="-append \"use_debug_port=1 root=769 loglevel=0\""

esac
    
CMD="$EMU $CDROM $HD $NET $SMP $KERNEL $APPEND"
echo "Running " $CMD
eval $CMD

