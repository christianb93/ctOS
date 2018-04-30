INC = $(PREFIX)/include/lib
CCFLAGS = -g -nostdinc -fno-stack-protector -mpush-args -Wno-packed -Wno-packed-bitfield-compat -fno-builtin -Wall -Werror -c -I$(INC) -m32
CC = gcc $(CCFLAGS)
LD = ld -Ttext 0x40000000 -melf_i386 -e _start
LIBS = ../lib/std/crt0.o ../lib/std/crt.a ../lib/os/libos.a

OBJ = init.o cli.o uname.o lc.o loop.o host.o args.o
EXEC = init cli uname lc loop args

.PHONY: tests

all: $(OBJ) $(EXEC) tests

%.o: %.c
	$(CC) -o $@ $<


init: init.o ../lib/std/string.o ../lib/std/unistd.o
	$(LD) init.o  $(LIBS) -o init

cli: cli.o
	$(LD) cli.o $(LIBS) -o cli

args: args.o
	$(LD) args.o $(LIBS) -o args

uname: uname.o
	$(LD) uname.o $(LIBS) -o uname

lc: lc.o
	$(LD) lc.o $(LIBS) -o lc

loop: loop.o
	$(LD) loop.o $(LIBS) -o loop


tests:
	(cd ./tests; make all)

clean:
	rm -f $(OBJ) $(EXEC)
	(cd ./tests ; make clean)


include Makefile.dep

dep:
	gcc -M -I../include/lib *.c   > Makefile.dep
	(cd ./tests ; make dep)

