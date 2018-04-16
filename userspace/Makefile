INC = $(PREFIX)/include/lib
CCFLAGS = -g -nostdinc -fno-stack-protector -mpush-args -Wno-packed -Wno-packed-bitfield-compat -fno-builtin -Wall -Werror -c -I$(INC) -m32
CC = gcc $(CCFLAGS) 

OBJ = init.o cli.o uname.o  testsignals.o kunit.o testwait.o testpipes.o testtty.o lc.o testjc.o loop.o testelf.o testall.o testfiles.o testrawcons.o testnet.o testmisc.o host.o testfork.o
EXEC = init cli uname testsignals testwait testpipes testtty lc testjc loop testelf testall testfiles testrawcons testfork testnet testmisc

all: $(OBJ) $(EXEC) 

%.o: %.c
	$(CC) -o $@ $<
	
kunit.o: ../test/kunit.c
	gcc -c -o kunit.o ../test/kunit.c -iquote../include -m32
	
init: init.o ../lib/std/string.o ../lib/std/unistd.o 
	ld -Ttext 0x40000000 init.o ../lib/std/crt0.o  ../lib/std/crt.a  ../lib/os/libos.a -o init -e _start -melf_i386

testall: testall.o
	ld -Ttext 0x40000000 testall.o  ../lib/std/crt0.o  ../lib/std/crt.a  ../lib/os/libos.a -e _start -o testall -melf_i386

testrawcons: testrawcons.o
	ld -Ttext 0x40000000 testrawcons.o  ../lib/std/crt0.o  ../lib/std/crt.a  ../lib/os/libos.a -e _start -o testrawcons -melf_i386

testelf: testelf.o
	ld -Ttext 0x40000000 testelf.o  -o testelf -e main -melf_i386

testmisc: testmisc.o kunit.o
	ld -Ttext 0x40000000 testmisc.o kunit.o ../lib/std/crt0.o ../lib/std/crt.a ../lib/os/libos.a -o testmisc -e _start -melf_i386	 


testfork: testfork.o kunit.o
	ld -Ttext 0x40000000 testfork.o kunit.o ../lib/std/crt0.o ../lib/std/crt.a ../lib/os/libos.a -o testfork -e _start -melf_i386	
	
testnet: testnet.o 
	ld -Ttext 0x40000000 testnet.o ../lib/std/crt0.o ../lib/std/crt.a ../lib/os/libos.a kunit.o -o testnet -e _start -melf_i386	
	
cli: cli.o  
	ld -Ttext 0x40000000 cli.o ../lib/std/crt0.o  ../lib/std/crt.a  ../lib/os/libos.a  -o cli -e _start -melf_i386
	#/home/chr/Projects/ctOS_toolchain/install/bin/i686-pc-ctOS-gcc -o cli cli.c
	
uname: uname.o
	ld -Ttext 0x40000000 uname.o ../lib/std/crt0.o  ../lib/std/crt.a  ../lib/os/libos.a -o uname -e _start -melf_i386

lc: lc.o
	ld -Ttext 0x40000000 lc.o ../lib/std/crt0.o  ../lib/std/crt.a  ../lib/os/libos.a -o lc -e _start -melf_i386		

testsignals: testsignals.o kunit.o
	ld -Ttext 0x40000000 testsignals.o kunit.o ../lib/std/crt0.o ../lib/std/crt.a ../lib/os/libos.a -o testsignals -e _start -melf_i386
	
testwait: testwait.o kunit.o
	ld -Ttext 0x40000000 testwait.o kunit.o ../lib/std/crt0.o ../lib/std/crt.a ../lib/os/libos.a -o testwait -e _start -melf_i386
	
testpipes: testpipes.o kunit.o
	ld -Ttext 0x40000000 testpipes.o kunit.o ../lib/std/crt0.o ../lib/std/crt.a ../lib/os/libos.a -o testpipes -e _start -melf_i386
	
testjc: testjc.o kunit.o
	ld -Ttext 0x40000000 testjc.o kunit.o ../lib/std/crt0.o ../lib/std/crt.a ../lib/os/libos.a -o testjc -e _start	-melf_i386

testfiles: testfiles.o kunit.o
	ld -Ttext 0x40000000 testfiles.o kunit.o ../lib/std/crt0.o ../lib/std/crt.a ../lib/os/libos.a -o testfiles -e _start -melf_i386	

	
testtty: testtty.o kunit.o
	ld -Ttext 0x40000000 testtty.o kunit.o ../lib/std/crt0.o ../lib/std/crt.a ../lib/os/libos.a -o testtty -e _start -melf_i386
	
loop: loop.o
	ld -Ttext 0x40000000 loop.o ../lib/std/crt0.o  ../lib/std/crt.a  ../lib/os/libos.a -o loop -e _start -melf_i386		
	
	
	
clean:
	rm -f $(OBJ) $(EXEC)
	
	
include Makefile.dep	
	
dep:
	gcc -M -I../include/lib *.c   > Makefile.dep
	
