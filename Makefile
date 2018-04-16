export PREFIX=$(shell pwd)
export INC = $(PREFIX)/include
export ASFLAGS = -g -c -I$(INC) -m32
export AS = gcc $(ASFLAGS)
export CCFLAGS = -g -nostdinc -fno-stack-protector -mpush-args -Wno-packed-bitfield-compat -fno-builtin -Wall -Werror  -c -iquote$(INC) -m32 
export CC = gcc $(CCFLAGS) 
export QEMU = `which qemu-system-i386`

SUBDIRS = lib hw driver kernel userspace test tools bin
	
.PHONY:  $(SUBDIRS) tests install

all: $(SUBDIRS) kernel 

$(SUBDIRS):
	$(MAKE)  -C $@

clean:
	for i in $(SUBDIRS); do (cd $$i ; $(MAKE) clean) ; done

dep:
	for i in $(SUBDIRS); do (cd $$i ; make dep) ; done

########################################################################################################
# Create an ISO CDROM image from the current kernel
########################################################################################################	

