OBJ = read.o write.o syscall.o open.o exit.o close.o fork.o unlink.o sbrk.o lseek.o exec.o sleep.o wait.o signals.o unistd.o getdent.o fcntl.o stat.o do_syscall.o ioctl.o times.o termios.o time.o socket.o 

all: $(OBJ) libos.a

do_syscall.o: do_syscall.S
	$(AS) -o do_syscall.o do_syscall.S	
	
%.o: %.c
	$(CC) -o $@ $<
		
libos.a: $(OBJ)
	ar rc libos.a $(OBJ) 		

clean:
	rm -f $(OBJ) libos.a

include Makefile.dep

dep:
	gcc -m32 -M -iquote../../include *.c   > Makefile.dep
	
