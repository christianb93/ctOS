OBJ = tty.o ramdisk.o pci.o hd.o pata.o ahci.o tty_ld.o console.o 8139.o eth.o

all: $(OBJ) 
	
	
%.o: %.c
	$(CC) -o $@ $<
		
clean:
	rm -f $(OBJ)

include Makefile.dep

dep:
	gcc -M -iquote../include *.c   > Makefile.dep
	