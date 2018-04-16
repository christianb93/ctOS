INC = $(PREFIX)/include/lib
CCFLAGS = -g -nostdinc -fno-stack-protector -mpush-args -Wno-packed -Wno-packed-bitfield-compat -fno-builtin -Wall -Werror -c -I$(INC) -m32
CC = gcc $(CCFLAGS) 
LD = ld -Ttext 0x40000000 -melf_i386 -e _start
LIBS = kunit.o ../../lib/std/crt0.o ../../lib/std/crt.a ../../lib/os/libos.a 
UNATTENDED = testjc.o testwait.o testfile.o testsignals.o testpipes.o testfork.o testmisc.o 
OBJ = kunit.o $(UNATTEND) testrawcons.o testnet.o testall.o testtty.o
EXEC = testfiles testfork testmisc testpipes testrawcons testtty testsignals testwait testjc testall testnet


kunit.o: ../../test/kunit.c
	gcc -c -o kunit.o ../../test/kunit.c -iquote../../include -m32

testfiles: testfiles.o kunit.o
	$(LD) testfiles.o $(LIBS) -o testfiles 

testfork: testfork.o kunit.o
	$(LD) testfork.o $(LIBS) -o testfork

testmisc: testmisc.o kunit.o
	$(LD) testmisc.o $(LIBS) -o testmisc
	
testpipes: testpipes.o kunit.o
	$(LD) testpipes.o $(LIBS) -o testpipes

testsignals: testsignals.o kunit.o
	$(LD) testsignals.o $(LIBS) -o testsignals 

testtty: testtty.o kunit.o
	$(LD) testtty.o $(LIBS) -o testtty 
	
testwait: testwait.o kunit.o
	$(LD) testwait.o $(LIBS) -o testwait 
	
testjc: testjc.o kunit.o
	$(LD) testjc.o $(LIBS) -o testjc 

testall: kunit.o 
	$(LD) testall.o $(LIBS) -o testall

testrawcons: testrawcons.o kunit.o
	$(LD) testrawcons.o $(LIBS) -o testrawcons
	
testnet: testnet.o kunit.o
	$(LD) testnet.o $(LIBS) -o testnet



all: $(OBJ) $(EXEC) 

%.o: %.c
	$(CC) -o $@ $<
	
	
clean:
	rm -f $(OBJ) $(EXEC)
	
	
include Makefile.dep	
	
dep:
	gcc -M -I../include/lib *.c   > Makefile.dep
	
