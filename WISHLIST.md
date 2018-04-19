## Limitations and wish list


ctOS is far from complete (and, as any OS project, will never be complete...). As development slowed down and finally came to a halt at some point in 2012, there were many things that were on my initial scope list but did not make it into the system, plus there are of course things that I never really planned to build but which would be nice. So here is a list of things that would require support, just in case you would like to contribute.

* Automatic stack extension: currently, the stack size of a user space process is statically determined and the stack cannot grow, it would be much better to extend the page fault handler to the effect that a page fault due to growing stack is detected and additional pages are mapped up to a certain point (question: how to tell whether accessed memory is supposed to be part of the stack or the heap?)
* The entire block cache is a stub at the moment and would have to be implemented - this is probably non-trivial on an SMP system because we need to synchronize access and maintain cache coherence
* ctOS has a UID and an EUID, but the entire file system is unproteced and ownership and access rights have to be implemented
* Something like the /proc and /sys filesystems would be nice
* We have virtual memory, but no swapping to disk
* Drivers for more network cards - currently only the RTL8139 is supported because it is simple and available in QEMU, but no real hardware uses that any more. The RTL8169 or the NE2000 would be good to start with.
* Of course additional ports would be great, like Lynx or even binutils and GCC
* There is no support for dynamic libraries which would be very beneficial if we port more software
* And finally, a framebuffer device and a real window manager 

