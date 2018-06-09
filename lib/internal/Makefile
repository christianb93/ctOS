OBJ =  heap.o streams.o setjmp.o dirstreams.o resolv.o pwddb.o math.o exp.o

all: $(OBJ) 
	
	
%.o: %.c
	$(CC) -o $@ $<
		
setjmp.o: setjmp.S
	$(AS) -o setjmp.o setjmp.S		
		
exp.o: exp.S
	$(AS) -o exp.o exp.S		
		
		
clean:
	rm -f $(OBJ)

include Makefile.dep

dep:
	gcc -m32 -M -iquote../../include *.c   > Makefile.dep
	
