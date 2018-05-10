OBJ = start.o main.o debug.o  irq.o locks.o mm.o kprintf.o systemcalls.o pm.o sched.o params.o dm.o fs.o fs_fat16.o blockcache.o fs_ext2.o elf.o tests.o fs_pipe.o timer.o sysmon.o arp.o net.o net_if.o wq.o ip.o icmp.o tcp.o udp.o multiboot.o
HW_OBJ =  ../hw/vga.o ../hw/keyboard.o ../hw/idt.o ../hw/gdt.o ../hw/gates.o ../hw/util.o ../hw/pic.o ../hw/pagetables.o ../hw/io.o ../hw/reboot.o ../hw/pit.o ../hw/apic.o ../hw/rtc.o ../hw/sigreturn.o ../hw/smp.o ../hw/trampoline.o ../hw/cpu.o  ../hw/rm.o
LIB_OBJ = ../lib/std/string.o  ../lib/std/stdlib.o ../lib/internal/heap.o  ../lib/std/time.o ../lib/os/syscall.o ../lib/os/fork.o ../lib/os/do_syscall.o ../lib/std/ctype.o ../lib/std/net.o 
DRIVER_OBJ = ../driver/tty.o ../driver/ramdisk.o  ../driver/pci.o ../driver/pata.o ../driver/hd.o ../driver/ahci.o ../driver/tty_ld.o ../driver/console.o ../driver/8139.o ../driver/eth.o
KERNEL_OBJ = $(OBJ)  $(LIB_OBJ) $(HW_OBJ) $(DRIVER_OBJ)

all: $(OBJ) kernel

start.o: start.S
	$(AS) -o start.o start.S

%.o: %.c
	$(CC) -o $@ $<
	
kernel: $(KERNEL_OBJ)
	ld -o ../bin/ctOSkernel -Ttext 0x100000 $(KERNEL_OBJ) -melf_i386
	
clean:
	rm -f $(OBJ)
	
include Makefile.dep
	
dep:
	gcc -M -iquote../include *.c   > Makefile.dep
	
