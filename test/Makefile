TESTS = test_gdt test_idt test_string test_stdlib test_lists test_pagetables test_heap test_mm test_pm test_sched test_params test_dm test_fs test_fs_ext2 test_blockcache test_fs_stack test_tty test_keyboard test_hd test_irq test_time test_streams test_stdio test_stdio_baseline test_setjmp test_dirstreams test_env test_pipes test_string_baseline test_stdlib_baseline test_tools test_getopt  test_vga test_net test_inet test_inet_baseline test_tcp test_ip test_net_if test_udp test_resolv test_fnmatch test_fnmatch_baseline test_netdb test_netdb_baseline test_pwd test_math  test_mntent test_grp
INTERACTIVE = test_debug test_write test_memorder
all: $(TESTS) $(INTERACTIVE) testgrub

kunit.o: kunit.c
	gcc -c -o kunit.o kunit.c -iquote../include -m32 -Wno-implicit-function-declaration
	
test_gdt: test_gdt.c kunit.c ../hw/gdt.o
	gcc -o test_gdt kunit.c test_gdt.c ../hw/gdt.o -iquote../include/ -m32 -Wno-implicit-function-declaration
	
test_tools: test_tools.c kunit.c 
	gcc -o test_tools kunit.c test_tools.c -iquote../include/ -m32	-Wno-implicit-function-declaration
	
test_math: test_math.c ../lib/internal/math.o
	gcc -o test_math kunit.c test_math.c ../lib/internal/math.o -iquote../include -lm -m32 -Wno-implicit-function-declaration

test_idt: test_idt.c kunit.c ../hw/idt.o
	gcc -o test_idt ../hw/gates.o kunit.c test_idt.c ../hw/idt.o -iquote../include/ -m32 -Wno-implicit-function-declaration
	
test_string: test_string.c ../lib/std/string.o kunit.o ../lib/std/strdup.o
	gcc -o test_string test_string.c ../lib/std/string.o ../lib/std/strdup.o kunit.o -iquote../include/ -m32 -Wno-implicit-function-declaration
	
test_fnmatch: test_fnmatch.c ../lib/std/fnmatch.o kunit.o 
	gcc -o test_fnmatch test_fnmatch.c ../lib/std/fnmatch.o  kunit.o -iquote../include/ -m32 -Wno-implicit-function-declaration
		
test_string_baseline: test_string.c kunit.o
	gcc -o test_string_baseline test_string.c  kunit.o -iquote../include -m32 -Wno-implicit-function-declaration

test_fnmatch_baseline: test_fnmatch.c 
	gcc -o test_fnmatch_baseline test_fnmatch.c kunit.o -iquote../include -m32 -Wno-implicit-function-declaration

test_netdb: test_netdb.c ../lib/std/netdb.o kunit.o
	gcc -o test_netdb ../lib/std/netdb.o kunit.o test_netdb.c -iquote../include -m32 -Wno-implicit-function-declaration
	
test_netdb_baseline: test_netdb.c
	gcc -o test_netdb_baseline kunit.o test_netdb.c -iquote../include -m32 -Wno-implicit-function-declaration

test_debug: test_debug.c ../kernel/debug.o kunit.o ../hw/cpu.o
	gcc -o test_debug test_debug.c kunit.o ../kernel/debug.o ../kernel/kprintf.o ../hw/cpu.o -Wno-packed-bitfield-compat -fno-builtin -iquote../include -m32 -Wno-implicit-function-declaration
	
test_stdlib: test_stdlib.c ../lib/std/stdlib.o kunit.o
	gcc -o test_stdlib test_stdlib.c kunit.o ../lib/std/stdlib.o -iquote../include -fno-builtin -m32 -Wno-implicit-function-declaration
	
test_stdlib_baseline: test_stdlib.c kunit.o
	gcc -o test_stdlib_baseline test_stdlib.c kunit.o  -iquote../include -fno-builtin -m32 -Wno-implicit-function-declaration
	
test_lists: test_lists.c ../include/lists.h kunit.o
	gcc -o test_lists test_lists.c kunit.o -iquote../include -m32 -Wno-implicit-function-declaration
	
test_pagetables: test_pagetables.c ../include/pagetables.h ../hw/pagetables.o kunit.o
	gcc -o test_pagetables test_pagetables.c ../hw/pagetables.o kunit.o -Wno-packed-bitfield-compat -iquote../include/ -m32 -Wno-implicit-function-declaration
	
test_heap: test_heap.c ../lib/internal/heap.c kunit.o
	gcc -o test_heap test_heap.c ../lib/internal/heap.c kunit.o  ../kernel/kprintf.o -iquote../include/ -m32 -Wno-implicit-function-declaration
	
test_mm: test_mm.c ../kernel/mm.o ../hw/pagetables.o ../include/mm.h ../lib/internal/heap.o kunit.o
	gcc -Wno-packed-bitfield-compat -o test_mm test_mm.c ../kernel/mm.o ../hw/pagetables.o ../kernel/kprintf.o ../lib/internal/heap.o kunit.o -iquote../include/ -m32 -Wno-implicit-function-declaration

test_pm: test_pm.c ../kernel/pm.o  ../include/pm.h kunit.o
	gcc -Wno-packed-bitfield-compat -o test_pm test_pm.c ../kernel/pm.o ../kernel/kprintf.o ../hw/sigreturn.o kunit.o -iquote../include/ -fno-builtin -m32 -Wno-implicit-function-declaration

test_sched: test_sched.c ../kernel/sched.o ../include/sched.h kunit.o
	gcc -o test_sched test_sched.c ../kernel/sched.o kunit.o ../kernel/kprintf.o -iquote../include -m32 -Wno-implicit-function-declaration
	
test_params: test_params.c ../kernel/params.c ../include/params.h ../kernel/params.o kunit.o 
	gcc -o test_params test_params.c kunit.o ../kernel/kprintf.o ../kernel/params.o -iquote../include -m32 -Wno-implicit-function-declaration
	
test_dm: test_dm.c ../kernel/dm.c ../include/drivers.h ../include/dm.h kunit.o
	gcc -o test_dm test_dm.c kunit.o ../kernel/dm.o ../kernel/kprintf.o -fno-builtin -iquote../include -m32 -Wno-implicit-function-declaration

test_net: test_net.c ../kernel/net.c ../include/net.h  kunit.o
	gcc -o test_net test_net.c kunit.o ../kernel/net.o ../kernel/kprintf.o -fno-builtin -iquote../include -m32 -Wno-implicit-function-declaration
	
test_net_if: test_net_if.c ../kernel/net_if.c ../include/net_if.h  kunit.o ../kernel/net.o
	gcc -o test_net_if test_net_if.c kunit.o ../kernel/net_if.o ../kernel/kprintf.o ../kernel/net.o -fno-builtin -iquote../include -m32 -Wno-implicit-function-declaration

	
test_fs: test_fs.c ../kernel/fs.o ../include/fs.h ../kernel/fs_pipe.o kunit.o
	gcc -o test_fs test_fs.c ../kernel/fs.o ../kernel/kprintf.o kunit.o  ../kernel/fs_pipe.o -fno-builtin -iquote../include -Wno-packed-bitfield-compat -m32 -Wno-implicit-function-declaration
	
test_fs_ext2: test_fs_ext2.c ../kernel/fs_ext2.o ../include/fs_ext2.h ../kernel/blockcache.o kunit.o
	gcc -o test_fs_ext2 test_fs_ext2.c ../kernel/fs_ext2.o ../kernel/kprintf.o kunit.o ../kernel/blockcache.o -fno-builtin -iquote../include -Wno-packed-bitfield-compat -m32 -Wno-implicit-function-declaration
	
test_blockcache: test_blockcache.c ../kernel/blockcache.o ../include/blockcache.h kunit.o
	gcc -o test_blockcache test_blockcache.c ../kernel/blockcache.o ../kernel/kprintf.o kunit.o -fno-builtin -iquote../include  -Wno-packed-bitfield-compat -m32 -Wno-implicit-function-declaration
		

test_fs_stack: test_fs_stack.c ../kernel/blockcache.o ../kernel/fs.o ../kernel/dm.o ../kernel/fs_ext2.o kunit.o
	gcc -o test_fs_stack test_fs_stack.c kunit.o ../kernel/blockcache.o ../kernel/fs.o ../kernel/fs_pipe.o ../kernel/dm.o ../kernel/fs_ext2.o ../kernel/fs_fat16.o ../kernel/kprintf.o -fno-builtin -iquote../include -Wno-packed-bitfield-compat -m32 -Wno-implicit-function-declaration
 

test_tty: test_tty.c ../driver/tty.o ../driver/tty_ld.o kunit.o ../lib/std/termios.o
	gcc -o test_tty -g test_tty.c kunit.o ../driver/tty.o ../driver/tty_ld.o ../lib/std/termios.o ../kernel/kprintf.o ../lib/os/libos.a -iquote../include -m32 -Wno-implicit-function-declaration
	
test_keyboard: test_keyboard.c ../hw/keyboard.o kunit.o
	gcc -o test_keyboard test_keyboard.c ../hw/keyboard.o kunit.o ../kernel/kprintf.o -fno-builtin -iquote../include -m32 -Wno-implicit-function-declaration
	
test_hd: test_hd.c ../driver/hd.o kunit.o
	gcc -o test_hd test_hd.c ../driver/hd.o kunit.o ../kernel/kprintf.o -fno-builtin -iquote../include -m32 -Wno-implicit-function-declaration
 
test_irq: test_irq.c ../kernel/irq.o kunit.o ../hw/cpu.o ../kernel/mptables.o
	gcc -o test_irq test_irq.c ../kernel/irq.o ../kernel/mptables.o kunit.o ../kernel/kprintf.o ../hw/cpu.o -fno-builtin -iquote../include -m32 -Wno-implicit-function-declaration

	
test_time: test_time.c ../include/lib/time.h ../lib/std/time.o kunit.o 
	gcc -Wno-format -o test_time ../lib/std/time.o test_time.c kunit.o -iquote../include -m32 -Wno-implicit-function-declaration
	
test_write: test_write.c ../lib/std/crt.a 
	gcc -c -o test_write.o test_write.c -iquote ../include -m32 -Wno-implicit-function-declaration
	ld -o test_write ../lib/std/crt1.o test_write.o  ../lib/std/crt.a ../kernel/kprintf.o ../lib/linux/libos.a -e _start -melf_i386 
	
	
test_streams: ../lib/internal/streams.o test_streams.c kunit.o
	gcc -o test_streams test_streams.c ../lib/internal/streams.o ../kernel/kprintf.o -fno-builtin kunit.o -iquote../include -m32 -Wno-implicit-function-declaration
	
test_stdio: ../lib/internal/streams.o test_stdio.c ../lib/std/stdio.o kunit.o  ../lib/internal/math.o
	gcc -o test_stdio.o -c test_stdio.c -iquote../include/lib -iquote ../include -fno-builtin -nostdinc -fno-stack-protector -m32 -Wno-implicit-function-declaration
	ld -o test_stdio test_stdio.o ../kernel/kprintf.o  ../lib/linux/crt0.o ../lib/std/crt.a  ../lib/linux/libos.a  -e _start -melf_i386

test_stdio_baseline: test_stdio.c
	gcc -o test_stdio_baseline -fno-builtin -iquote ../include test_stdio.c ../kernel/kprintf.o -lm -m32 -Wno-implicit-function-declaration
	
test_getopt: test_getopt.c ../lib/std/getopt.o kunit.o
	gcc -o test_getopt -g test_getopt.c kunit.o ../lib/std/getopt.o -iquote../include/ -m32 -Wno-implicit-function-declaration

	
test_setjmp: test_setjmp.c kunit.o ../lib/internal/setjmp.o
	gcc -o test_setjmp test_setjmp.c kunit.o ../lib/internal/setjmp.o -iquote../include -m32 -Wno-implicit-function-declaration
	
test_dirstreams: test_dirstreams.c kunit.o ../lib/internal/dirstreams.o
	gcc -o test_dirstreams -fno-builtin test_dirstreams.c kunit.o ../lib/internal/dirstreams.o -iquote../include -m32 -Wno-implicit-function-declaration
	 
test_env: test_env.c kunit.o ../lib/std/env.o
	gcc -o test_env test_env.c -fno-builtin kunit.o ../lib/std/env.o -iquote../include -m32 -Wno-implicit-function-declaration
	
test_pipes: test_pipes.c ../kernel/fs_pipe.o kunit.o
	gcc -o test_pipes test_pipes.c ../kernel/fs_pipe.o kunit.o ../kernel/kprintf.o -fno-builtin -iquote../include -m32 -Wno-implicit-function-declaration
	
test_vga: test_vga.c ../hw/vga.o kunit.o ../driver/console.o 
	gcc -o test_vga test_vga.c ../hw/fonts.o ../hw/vga.o kunit.o ../hw/io.o  ../driver/console.o -fno-builtin -iquote../include -m32 -Wno-implicit-function-declaration
	
test_memorder: test_memorder.c
	gcc -O0 -o test_memorder test_memorder.c -lpthread -m32 -Wno-implicit-function-declaration

test_inet: test_inet.c kunit.o ../lib/std/net.c ../lib/std/inet.o
	gcc -o test_inet test_inet.c  kunit.o ../lib/std/net.o ../lib/std/inet.o ../kernel/kprintf.o -iquote../include -m32 -Wno-implicit-function-declaration

test_inet_baseline: test_inet.c kunit.o 
	gcc -o test_inet_baseline test_inet.c  kunit.o  -iquote../include -m32 -Wno-implicit-function-declaration
	
test_tcp: test_tcp.c kunit.o ../kernel/tcp.o ../kernel/net.o
	gcc -g -o test_tcp test_tcp.c  kunit.o ../kernel/tcp.o ../kernel/kprintf.o ../kernel/net.o -iquote../include -m32 -Wno-implicit-function-declaration

test_ip: test_ip.c kunit.o ../kernel/ip.o ../kernel/net.o
	gcc -g -o test_ip test_ip.c  kunit.o ../kernel/ip.o ../kernel/kprintf.o ../kernel/net.o -iquote../include -m32	-Wno-implicit-function-declaration
	

test_udp: test_udp.c kunit.o ../kernel/udp.o ../kernel/net.o
	gcc -g -o test_udp test_udp.c  kunit.o ../kernel/udp.o ../kernel/kprintf.o ../kernel/net.o -iquote../include -m32	-Wno-implicit-function-declaration
	

test_resolv: test_resolv.c ../lib/internal/resolv.o kunit.o
	gcc -o test_resolv test_resolv.c  ../lib/internal/resolv.o  kunit.o -fno-builtin -iquote../include -m32 -Wno-implicit-function-declaration
	
	
test_pwd: test_pwd.c ../lib/internal/pwddb.c kunit.o
	gcc -o test_pwd test_pwd.c ../lib/internal/pwddb.o kunit.o -fno-builtin -iquote../include/ -m32 -Wno-implicit-function-declaration
	
test_mntent: test_mntent.c  kunit.o ../lib/std/mntent.o
	gcc -o test_mntent test_mntent.c kunit.o ../lib/std/mntent.o -fno-builtin -iquote../include/ -m32 -Wno-implicit-function-declaration

test_grp: test_grp.c  kunit.o ../lib/std/grp.o
	gcc -o test_grp test_grp.c kunit.o ../lib/std/grp.o -fno-builtin -iquote../include/ -m32 -Wno-implicit-function-declaration


#	
# Build a minimum GRUB2 compliant kernel for testing purposes
#
teststart.o: teststart.S
	$(AS) -o teststart.o teststart.S -m32


testgrub.o: testgrub.c
	$(CC) -o testgrub.o testgrub.c -m32
	
	
testgrub: teststart.o ../kernel/multiboot.o ../hw/gdt.o ../lib/std/string.o testgrub.o  ../kernel/kprintf.o ../hw/io.o ../hw/fonts.o ../hw/vga.o ../kernel/locks.o ../hw/util.o ../lib/std/ctype.o ../kernel/params.o ../lib/std/stdlib.o ../lib/std/errno.o ../driver/console.o ../hw/rm.o 
	ld -o testgrub -Ttext 0x100000 teststart.o ../kernel/multiboot.o ../hw/gdt.o ../lib/std/string.o testgrub.o  ../kernel/kprintf.o ../hw/io.o  ../hw/fonts.o ../hw/vga.o ../kernel/locks.o ../hw/util.o ../lib/std/ctype.o ../kernel/params.o ../lib/std/stdlib.o ../lib/std/errno.o  ../driver/console.o ../hw/rm.o  -melf_i386
	

clean:
	rm -f $(TESTS) $(INTERACTIVE) *.o
	rm -f testgrub
	
testrun:
	# for test_stdio
	echo "Hello World!" > hello
	for i in $(TESTS); do ./$$i ; if [ "$$?" -ne "0" ]; then exit 1; fi ; done
	rm -f hello
	
include Makefile.dep
	
dep:
	gcc -M -iquote../include *.c  -m32  > Makefile.dep
