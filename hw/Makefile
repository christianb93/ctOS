OBJ = vga.o keyboard.o idt.o gdt.o gates.o util.o pic.o pagetables.o io.o reboot.o pit.o apic.o rtc.o sigreturn.o smp.o trampoline.o cpu.o  rm.o

all: $(OBJ)


gates.o: gates.S
	$(AS) -o gates.o gates.S
	

util.o: util.S
	$(AS) -o util.o util.S

trampoline.o: trampoline.S
	$(AS) -o trampoline.o trampoline.S
	
rm.o: rm.S
	$(AS) -o rm.o rm.S

	
%.o: %.c
	$(CC) -o $@ $<
	
clean:
	rm -f $(OBJ)

include Makefile.dep

dep:
	gcc -m32 -M -iquote../include *.c   > Makefile.dep
	
