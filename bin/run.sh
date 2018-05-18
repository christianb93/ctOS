#!/bin/bash

QEMU="qemu-system-i386 -k de  -debugcon stdio -m 512"
BOCHS="bochs -qf bin/bochs.rc"
CDROM=""
BOOT=""
HD=""
NET=""
SMP=""
APPEND=""
KERNEL=""

#
# To unregister a device with Vbox, we first have to 
# unregister all virtual machines using it
#
function cleanUpVbox() {
    vboxmanage unregistervm ctOS-ahci --delete
    vboxmanage unregistervm ctOS-ich9 --delete
    vboxmanage unregistervm ctOS-ide --delete
    vboxmanage closemedium disk bin/hdimage.vdi
 	rm -f hdimage.vdi
}


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
    qemu-debug: As the default, but bring up QEMU in debugging mode
    qemu-net:   QEMU multiboot with IDE drive (hdimage) and a network card
    qemu-smp:   QEMU with 8 simulated CPUs
    qemu-tap:   QEMU attached to a tap networking device (must be root to run this)
    qemu-ahci:  QEMU with an emulated AHCI drive
    vbox-ahci:  VirtualBox with an AHCI drive and the PIIX3 chipset
    vbox-ich9:  VirtualBox with an AHCI drive and the Intel ICH9  chipset 
    vbox-ide:   VirtualBox with an IDE drive
    bochs:      Run ctOS on the Bochs emulator
    efi:        Run ctOS on an emulated EFI platform (run efi_image.sh first)
    efi-smp:    EFI with more than one CPU
    
    
    
EOF
    exit
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
    KERNEL="-kernel bin/ctOSkernel1"
    APPEND="-append \"use_debug_port=1 root=769 loglevel=0 use_acpi=1\""
    ;;

qemu-debug)
    #
    # Default configuration - use QEMU to boot from CDROM
    # Required images: CD ROM ISO image
    #
    EMU=$QEMU
    CDROM="-cdrom bin/cdimage.iso"
    EMU="$QEMU -S -s"
    ;;


qemu-net)
    #
    # Use QEMUs multiboot capabilities to boot the kernel directly,
    # attach a PATA drive  and a network drive
    # Required images: kernel, hdimage
    #
    EMU=$QEMU
    HD="-drive id=disk,file=bin/hdimage,if=ide,media=disk"
    KERNEL="-kernel bin/ctOSkernel1"
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
    KERNEL="-kernel bin/ctOSkernel1"
    SMP="-smp sockets=8,threads=1 -enable-kvm"
    APPEND="-append \"use_debug_port=1 root=769 loglevel=0 vga=1 use_acpi=1\""
    ;;

qemu-tap)
    # 
    # Start a QEMU instance (IDE, 1 CPU) connected to a tap device
    # You need to be root to run this
    #
    EMU=$QEMU
    HD="-drive id=disk,file=bin/hdimage,if=ide,media=disk"
    KERNEL="-kernel bin/ctOSkernel1"
    NET="-net nic,vlan=1,macaddr=00:00:00:00:11:11,model=rtl8139 -net dump,vlan=1,file=qemu_vlan1.pcap -net tap,vlan=1,script=no,downscript=no"
    APPEND="-append \"use_debug_port=1 root=769 loglevel=0\""
    ;;
    
qemu-ahci)
    #
    # QEMU with one CPU and an AHCI hard disk
    # Required images: kernel, hdimage
    #
    EMU=$QEMU
    HD="-drive id=disk,file=bin/hdimage,if=none -device ahci,id=ahci -device ide-drive,drive=disk,bus=ahci.0"
    KERNEL="-kernel bin/ctOSkernel1"
    APPEND="-append \"use_debug_port=1 root=1025 loglevel=0 vga=1\""
    ;;


vbox-ahci)
    #
    # Use Virtualbox with the PIIX3 chipset and AHCI
    #
    cleanUpVbox
	vboxmanage convertfromraw bin/hdimage bin/hdimage.vdi --format=vdi
	vboxmanage createvm --name "ctOS-ahci" --ostype "Other" --register
	vboxmanage modifyvm "ctOS-ahci" --memory 512  --cpus 1   --boot1 dvd --chipset piix3
	vboxmanage storagectl "ctOS-ahci" --add sata --name "AHCI"
	vboxmanage storageattach "ctOS-ahci" --storagectl "AHCI" --port 0 --device 0 --medium `pwd`/bin//hdimage.vdi --type hdd
	vboxmanage storageattach "ctOS-ahci" --storagectl "AHCI" --port 1 --device 0 --medium `pwd`/bin/cdimage.iso --type dvddrive
    EMU="vboxmanage startvm ctOS-ahci"
    ;;
    
vbox-ich9)
    cleanUpVbox
	vboxmanage convertfromraw bin/hdimage bin/hdimage.vdi --format=vdi
	vboxmanage createvm --name "ctOS-ich9" --ostype "Other" --register
	vboxmanage modifyvm "ctOS-ich9" --memory 512  --cpus 1   --boot1 dvd --apic on --chipset ich9
	vboxmanage storagectl "ctOS-ich9" --add sata --name "AHCI"
	vboxmanage storageattach "ctOS-ich9" --storagectl "AHCI" --port 0 --device 0 --medium `pwd`/bin//hdimage.vdi --type hdd
	vboxmanage storageattach "ctOS-ich9" --storagectl "AHCI" --port 1 --device 0 --medium `pwd`/bin/cdimage.iso --type dvddrive
    EMU="vboxmanage startvm ctOS-ich9"
    ;;

vbox-ide)
    cleanUpVbox
	vboxmanage convertfromraw bin/hdimage bin/hdimage.vdi --format=vdi
	vboxmanage createvm --name "ctOS-ide" --ostype "Other" --register
	vboxmanage modifyvm "ctOS-ide" --memory 512  --cpus 1   --boot1 dvd --apic on --chipset ich9
	vboxmanage storagectl "ctOS-ide" --add ide --name "IDE"
	vboxmanage storageattach "ctOS-ide" --storagectl "IDE" --port 0 --device 0 --medium `pwd`/bin//hdimage.vdi --type hdd
	vboxmanage storageattach "ctOS-ide" --storagectl "IDE" --port 1 --device 0 --medium `pwd`/bin/cdimage.iso --type dvddrive
    EMU="vboxmanage startvm ctOS-ide"
    ;;

    
bochs)
    EMU=$BOCHS
    ;;
    
efi)
    if [ ! -e "bin/efiimage" ]
    then
        echo "Could not find bin/efiimage, please do (cd bin ; ./efi_image.sh) first"
    exit
    fi
    EMU="qemu-system-x86_64 --bios /usr/share/qemu/OVMF.fd -m 512 -debugcon stdio"
    HD="-drive id=disk,file=bin/efiimage,if=ide,media=disk "
    ;;
    
efi-smp)
    if [ ! -e "bin/efiimage" ]
    then
        echo "Could not find bin/efiimage, please do (cd bin ; ./efi_image.sh) first"
    exit
    fi
    EMU="qemu-system-x86_64 --bios /usr/share/qemu/OVMF.fd -m 512 -debugcon stdio"
    HD="-drive id=disk,file=bin/efiimage,if=ide,media=disk "
    SMP="-smp sockets=8,threads=1 -enable-kvm"
    ;;


*)
    echo "Unrecognized run target, please use the run target ? or help to get a full list"
    exit 1
    ;;

esac
    
CMD="$EMU $CDROM $HD $NET $SMP $KERNEL $APPEND"
echo "Running " $CMD
eval $CMD

