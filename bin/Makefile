all:
	
clean:
	rm -rf ctOSkernel


dist:
	(cd .. ; tar cvf /tmp/ctOS.bin.tar bin/ctOSkernel bin/hdimage bin/cdimage.iso bin/run.sh ; )
	cp /tmp/ctOS.bin.tar .
	gzip ctOS.bin.tar
	rm -f ctOS.bin.tar

distclean:
	rm -rf ctOSkernel ramdisk.img iso/ctOSkernel iso/ramdisk.img cdimage.iso hdimage ctOS.bin.tar.gz

	
