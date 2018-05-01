EXE = ext2dump ext2samples dumpconstants timesamples dumpargs testargs streamsamples waitsamples ttysamples dumpchars escsamples dumptty dumptermcap testenv ipsamples testserver testclient tcpsamples udpsamples  testlocale httpclient

all: $(EXE)

ext2dump: ext2dump.c ../kernel/fs_ext2.o ../kernel/blockcache.o
	gcc -o ext2dump ext2dump.c ../kernel/fs_ext2.o ../kernel/kprintf.o ../kernel/blockcache.o -iquote../include -fno-builtin -m32

ext2samples: ext2samples.c
	gcc -o ext2samples ext2samples.c -fno-builtin 
	
dumpconstants: dumpconstants.c
	gcc -o dumpconstants dumpconstants.c 
	
dumpchars: dumpchars.c
	gcc -o dumpchars dumpchars.c 
	
dumptty: dumptty.c
	gcc -o dumptty dumptty.c 

timesamples: timesamples.c
	gcc -o timesamples timesamples.c 
	
dumpargs: dumpargs.c
	gcc -o dumpargs dumpargs.c 
	
dumptermcap: dumptermcap.c
	gcc -o dumptermcap dumptermcap.c -lncurses 
	
escsamples: escsamples.c
	gcc -o escsamples escsamples.c
	
testargs: testargs.c
	gcc -o testargs testargs.c 

testenv: testenv.c
	gcc -o testenv testenv.c 

testserver: testserver.c
	gcc -o testserver testserver.c 
	
testclient: testclient.c
	gcc -o testclient testclient.c 

httpclient: httpclient.c
	gcc -o httpclient httpclient.c 


streamsamples: streamsamples.c
	gcc -o streamsamples streamsamples.c 

waitsamples: waitsamples.c
	gcc -o waitsamples -fno-builtin waitsamples.c 
	
ttysamples: ttysamples.c
	gcc -o ttysamples -fno-builtin ttysamples.c 


ipsamples: ipsamples.c
	gcc -o ipsamples -fno-builtin ipsamples.c 

tcpsamples: tcpsamples.c
	gcc -o tcpsamples -fno-builtin tcpsamples.c 

udpsamples: udpsamples.c
	gcc -o udpsamples udpsamples.c

testlocale: testlocale.c
	gcc -o testlocale testlocale.c

clean:
	rm -f $(EXE)

dep:
