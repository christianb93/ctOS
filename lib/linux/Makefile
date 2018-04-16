OBJ =  write.o exit.o read.o open.o close.o unlink.o sbrk.o lseek.o fork.o exec.o sleep.o wait.o signals.o unistd.o getdent.o stat.o ioctl.o

all: $(OBJ) libos.a crt0.o
	
	
%.o: %.c
	$(CC) -o $@ $<
		
libos.a: $(OBJ)
	ar rc libos.a $(OBJ)		
		
clean:
	rm -f $(OBJ) crt0.o libos.a

include Makefile.dep

dep:
	gcc -m32 -M -iquote../../include *.c   > Makefile.dep
	
