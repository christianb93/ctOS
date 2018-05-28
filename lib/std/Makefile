OBJ =  stdlib.o unistd.o time.o string.o read.o write.o open.o exit.o close.o fork.o errno.o exit.o unlink.o malloc.o stdio.o lseek.o wait.o signals.o setjmp.o dirent.o fcntl.o stat.o abort.o env.o ioctl.o ctype.o times.o strdup.o termios.o getopt.o  pwd.o asctime.o net.o socket.o inet.o netdb.o locale.o fnmatch.o  crti.o crtn.o mntent.o
all: $(OBJ) crt.a crt0.o crt1.o libc.a
	
	
%.o: %.c
	$(CC) -o $@ $<
	
crt1.o: crt0.c
	$(CC) -DSKIP_INIT_CALL -o crt1.o  crt0.c

crt.a: $(OBJ)
	rm -f crt.a
	ar rc crt.a $(OBJ) ../internal/heap.o ../internal/streams.o ../internal/setjmp.o ../internal/dirstreams.o ../internal/resolv.o ../internal/pwddb.o ../internal/math.o
	
crti.o: crti.S
	$(AS) -o crti.o crti.S
	
crtn.o: crtn.S
	$(AS) -o crtn.o crtn.S

	
libc.a: $(OBJ)
	rm -f libc.a
	ar rc libc.a $(OBJ) ../internal/heap.o ../internal/streams.o ../internal/setjmp.o ../internal/dirstreams.o ../internal/resolv.o ../internal/pwddb.o ../internal/math.o ../os/*.o 
	
		
clean:
	rm -f $(OBJ) crt0.o libc.a crt.a 

include Makefile.dep

dep:
	gcc -m32 -M -iquote../../include *.c   > Makefile.dep
	
