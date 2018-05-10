all:
	
clean:
	rm -rf ctOSkernel
	rm -rf ctOSkernel1


dist:
	(cd .. ; tar cvf /tmp/ctOS.bin.tar bin/ctOSkernel bin/ctOSkernel1 bin/hdimage bin/cdimage.iso bin/run.sh bin/if_up.sh bin/if_down.sh bin/bochs.rc ; )
	cp /tmp/ctOS.bin.tar .
	gzip ctOS.bin.tar
	rm -f ctOS.bin.tar

distclean:
	rm -rf ctOSkernel ctOSkernel1 ramdisk.img iso/ctOSkernel iso/ramdisk.img cdimage.iso hdimage ctOS.bin.tar.gz

dep:
	# do nothing
	
