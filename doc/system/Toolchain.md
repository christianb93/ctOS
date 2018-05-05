# Building a toolchain for ctOS

## Building binutils

To prepare for building binutils, we first need to make sure that the packages which we depend upon are installed on our system. On Debian or Ubuntu, use the following commands to install texinfo, flex and bison.

```
sudo apt-get install texinfo
sudo apt-get install flex
sudo apt-get install bison
```

Next, we need to obtain a copy of the binutils source code. I have used version 2.27, which you can download using

```
wget https://ftp.wrz.de/pub/gnu/binutils/binutils-2.27.tar.gz
gzip -d binutils-2.27.tar.gz
tar xvf binutils-2.27.tar  
```
This will create a directory called binutils-2.27 in your current working directory and extract the source code there. Let us call this directory SRC for the remainder of this documentation.

After the binutils package has been extracted to a directory which we will denote by SRC throughout the remainder of this document and looking at the contents of the root directory, it is obvious that - like most GNU packages - binutils uses the autoconf mechanism, i.e. a shell script called configure which is supposed to set variables and create a Makefile adapted to the execution and target environment. At www.gnu.org, a rather comprehensive documentation for the autoconf package is available from which we learn two basic facts relevant for our undertaking.

* we are expected to run configure with the switch --target to define the target architecture for which the resulting binutils should generate code
* all known target architectures are defined in the file config.sub which is essentially the same for all GNU packages

Here, a target architecture is specified by a string like i386-pc-linux-gnu, or more general either CPU-VENDOR-OS or CPU-VENDOR-OS-KERNEL. So the first thing we need to do is to define a target string for the ctOS platform and to make it known to the configure script by adding it to config.sub. We will use the target name i686-pc-ctOS for our purposes.

So open config.sub (located in the SRC directory) and locate he case statement starting with

```
# Decode manufacturer-specific aliases for certain operating systems.

if [ x"$os" != x"" ]
then
case $os in
```

Right above the lines

```
    -none)
        ;;
```

add a line for ctOS:

```
    -ctOS*)
        os=-ctOS
        ;;
```

The next patch we need to make is related to the BFD library. The BFD library is the library used by all binutils to handle object files and executable files. Obviously, we need to declare at some point that our architecture uses the executable file format ELF32. This is done in the file SRC/bfdconfig.bfd.

The header of this file contains a short documentation, explaining which shell variables are set by this script depending on the target name. To add an entry for our target, locate the line starting with

```
  i[3-7]86-*-linux-*)
```

Below this line, we add the description of our new target by simply taking over the value of targ_defvec, i.e.:

```
  i[3-7]86-*-ctOS-*)
    targ_defvec=i386_elf32_vec
    ;;
```

No additional vectors are supported apart from the default vector. Next, we need to tell GAS what object format to use for our target. This mapping is done in the script gas/configure.tgt which parses the target and assigns the appropriate object format. We will have to add a line to the case statement starting with

```
# Assign object format.  Set fmt, em, and bfd_gas.
generic_target=${cpu_type}-$vendor-$os
# Note: This table is alpha-sorted, please try to keep it that way.
case ${generic_target} in
```

Below the line for i386-*-coff, we add the line for our target, again using one of the existing lines as template:

```
  i386-*-ctOS)                fmt=elf ;;
```

The next thing that we need to do is to create an **emulation script** for ld. This script is used during the build process to create a linker script that ld will use later and that therefore determines the layout of our executables. So we create a script called SRC/ld/emulparams/elf_ctOS.sh with the following content.

```
. ${srcdir}/emulparams/plt_unwind.sh
SCRIPT_NAME=elf
OUTPUT_FORMAT="elf32-i386"
NO_RELA_RELOCS=yes
TEXT_START_ADDR=0x40000000
MAXPAGESIZE="CONSTANT (MAXPAGESIZE)"
COMMONPAGESIZE="CONSTANT (COMMONPAGESIZE)"
ARCH=i386
MACHINE=
NOP=0x90909090
TEMPLATE_NAME=elf32
GENERATE_SHLIB_SCRIPT=no
GENERATE_PIE_SCRIPT=no
NO_SMALL_DATA=yes
SEPARATE_GOTPLT=12
IREL_IN_PLT=
```

This script needs to be referred in ld/configure.tgt. Thus we need to add the following line to configure.tgt in the ld subdirectory:

```
i[3-7]86-*-ctOS)    targ_emul=elf_ctOS ;;
```

In order to also run genscripts.sh with the matching parameters, we need an additional line in Makefile.in in the ld subdirectory. Again we can use an existing line as template, for instance the line for eelf_i386.c. Our new line looks as follows:

```
eelf_ctOS.c: $(srcdir)/emulparams/elf_ctOS.sh \
  $(ELF_DEPS) $(srcdir)/scripttempl/elf.sc ${GEN_DEPENDS}
```

The target eelf_ctOS.c also needs to be added to ALL_EMULATION_SOURCES in order to be included in the build process.

We also need to define a location in which we place the resulting toolchain. For this guide, I will assume that you have defined an environment variable CTOS_PREFIX that points to the directory were we will locate the toolchain (for me, this is $HOME/ctOS_toolchain). Inside that directory, we will use the following structure.

![Toolchain directories](images/ToolchainDirectories.png)


Now we can finally run our build. Assuming that you have defined the environment variable CTOS_PREFIX (and created the respective directory), execute the following commands.

```
cd $CTOS_PREFIX
mkdir src
mkdir sysroot
mkdir install
mkdir src/binutils-2.27
cd binutils-2.27
<<now copy the source code to this directory and make the changes indidicated above>>
cd ..
mkdir build
mkdir build/binutils
cd build/binutils
../../src/binutils-2.27/configure --target=i686-pc-ctOS --prefix=$CTOS_PREFIX/install --with-sysroot=/$CTOS_PREFIX/sysroot
make
```

## Building GCC

Next, we will build GCC. Again, we first download the source. I use GCC 5.4 for this purpose, which is a bit outdated but still used by comparatively recent distributions, like Ubuntu 16.40.

```
wget https://ftpmirror.gnu.org/gcc/gcc-5.4.0/gcc-5.4.0.tar.gz
```

We will also need a few packages on the build platform. These libraries are GMP, MPC and MPFR. Use the package manager of your distribution to get these packages, remember to install the "development" packages as well which contain the header files. On Ubuntu, this is done by

```
sudo apt-get install libgmp-dev libmpc-de
```

After the download of the gcc package has completed, extract the source. We use the same directory layout as above, i.e. we execute the tar command in $CTOS_PREFIX/src and thus create a directory gcc-5.4.0 there.

Now let us start to make the necessary changes. The first thing after extracting the GCC core package which we have to do is to make our platform once more known to the configure scripts. Thus we need to make the changes that we made to the config.sub file for binutils here as well.

The second file that we need to change is the file gcc/config.gcc. This file seems to map the target to some variables used for the configuration process. The first case statement sets the CPU type based on the CPU part of the target string. As we use an existing CPU i386-pc, we do not have to change anything here. However, in the second case statement, which starts with

```
# Common parts for widely ported systems.
case ${target} in
*-*-darwin*)
```

we have to add a line for our OS:

```
*-*-ctOS)
  extra_parts="crtbegin.o crtend.o"
  ;;
```

Note that the extra_parts variable instructs GCC to build the object files crtbegin.o and crtend.o which are not needed for the ctOS runtime library (usually they would be called from _start, see [the OSDEV Wiki on this](https://wiki.osdev.org/Creating_a_C_Library)), but expected by  GCC.

We will also have to add a branch to the next case statement by adding the line.

```
i[34567]86-*-ctOS*)
    tm_file="${tm_file} i386/unix.h i386/att.h dbxelf.h elfos.h i386/i386elf.h"
    ;;
```

There is also the file libgcc/config.host to which we have to add a few statements. Again we first add another line to the case statement starting with

```
# Common parts for widely ported systems.
case ${host} in
```

namely

```
*-*-ctOS)
  ;;
```

The second statement which we need to change is the case statement right afterwards

```
case ${host} in
```

to which we add the line

```
i[34567]86-*-ctOS)
    ;;
```

The next problem which we face is due to the fact the GCC comes with its own include files which have uncommon standard definitions for some integer types. Specifically, in the file stddef.h in gcc/ginclude (which is copied to build/gcc/gcc/include), in line 212, size_t is defined as long unsigned int and not as unsigned int. This needs to be changed to unsigned int.

To successfully build GCC, we also have to copy some libraries and tools from the actual ctOS root to our newly created sysroot.


```
mkdir -p %CTOS_PREFIX/sysroot/usr/include
cp -v ~/Projects/github/ctOS/include/lib/*.h ~/ctOS_toolchain/sysroot/usr/include/
mkdir ~/ctOS_toolchain/sysroot/usr/include/os/
mkdir ~/ctOS_toolchain/sysroot/usr/include/sys/
cp -v ~/Projects/github/ctOS/include/lib/os/*.h ~/ctOS_toolchain/sysroot/usr/include/os/
cp -v ~/Projects/github/ctOS/include/lib/sys/*.h ~/ctOS_toolchain/sysroot/usr/include/sys/
mkdir ~/ctOS_toolchain/sysroot/lib/
cp -v ~/Projects/github/ctOS/lib/std/crt0.o ~/ctOS_toolchain/sysroot/lib/
cp -v ~/Projects/github/ctOS/lib/std/libc.a ~/ctOS_toolchain/sysroot/lib/
```

Again, we add a build directory, cd there and start the actual build and install.

```
../../src/gcc-5.4.0/configure --target=i686-pc-ctOS --prefix=$CTOS_PREFIX/install --with-sysroot=$CTOS_PREFIX/sysroot --with-gnu-as --with-gnu-ld --enable-languages=c
make -j 4
make install
```

This might take some time, depending on the number of CPUs you have. When the smoke clears, you should see a gcc executable in $CTOS_PREFIX/install. 


### Appendix - creating and using patches

For those of you who do not use diff and patch on a regular basis, here is how you can use these tools to automate the changes that we have done. Let me explain this using binutils as an example.

To apply diff, you need a directory which contains the original code and a directory that contains the changed code. In the case of the binutils port, binutils-2.27 contains the original source code and  binutils-2.27_new contains the adapted source code. We can now create patchfile capturing those changes using

```
diff -c -r -N binutils-2.27 binutils-2.27_new > binutils.patch
```

from the directory in which both these directories are located. Then the patches can be replayed by extracting the tar file once more to get a clean copy binutils-2.27 and run

```
patch -p0 < binutils.patch
```

This will apply the patches inside the directory binutils-2.27! Thus it is important that the directory to which we eventually apply the patches has the same name as the directory in which the baseline code was contained when creating the patches. The name of the directory in which the new version is located when the diff is created is also stored in the patch file but not used.




