# Testing ctOS


ctOS comes with various existing test scripts. First, there are some unit tests (roughly 1300 test cases at the time of writing) for components of the kernel and the libraries (obviously restricted to those parts that can resonably be tested on a host system). In addition, there are a couple of test scripts that execute within ctOS in userspace. Finally, this document describes some useful manual test scenarios.

## Unit tests

The unit tests are located in the directory `test`. To run them, a bit of preparation is required, as some tests (the tests for the EXT2 file system) use test images that need to be present. To create them, follow these instructions, starting at the ctOS root directory.

```
cd tools
./init_test_images.sh
```

You have to do this only once, then the files can be reused for any subsequent test (they are not modified), as long as the test cases do not change substantially.

After these preparations, run the tests by going back to the ctOS root and executing the make target tests.

```bash
cd ..
make tests
```


Some tests (for a feature called triple blocks) need a file that is at least roughly 130 MB in size. The script above expects that you have selected a large file (it does not matter which one, but avoid using `/dev/zero` as this will make the test pointless) and copied it to `tools/testfile.src`. If this file is not present, the script will warn you and stop.


## Automated userspace tests

In addition to the unit tests, there are a few tests that can semi-automatically be executed in the ctOS userspace (be careful, not all of these tests are currently part of the binary distribution, so you have to build ctOS yourself to use them). 

To run these tests, start an instance of ctOS in an emulator of your choice (I recommend QEMU, as some tests are quite intensive and will be extremely slow in Bochs). To run all fully automated tests, simply issue

```
cd tests
run testall
```

from the CLI prompt. This will run a couple of test programs and print out the results. Note that this can take some time - one program, `testfork`, creates a few thousand processes and will run for a few minutes, in particular on a single CPU setup. Of course you can also run the programs individually.


## Additional test scenarios

In addition to the fully automated tests, there are of course additional scenarios. In this section, I have collected some of these tests that I usually execute after making major changes.

### SMP tests

To run this test scenario, use the target qemu-smp in the run script to bring up a version of QEMU simulating eight available CPUs, i.e. enter

```
./bin/run.sh qemu-smp
```

This will also launch ctOS in VGA mode. In this mode, the screen splits and, apart from the console at the left, we see a window at the right showing CPU load and other performance data, and a window at the bottom with some configuration information.

When you now run the automated tests, in particular the test `testfork`, and bring up a system monitor on the host system in parallel, you will see that QEMU actually moves the load forth and back between different CPUs on the host system, as in the screenshot below.

![ctOS SMP test][1]

The screenshot below shows the same tests after completion on my real PC, which is equipped with a Core i7 Intel CPU with four cores and an AHCI hard disk (apologies for the poor quality, it was already a bit late in the evening with low light).

![ctOS SMP test on bare metal][6]


### Networking basics

Let us now test some of the basic networking capabilities of the ctOS kernel. For that purpose, we need a second virtual machine running a small Linux distribution - I use Alpine Linux for that purpose. 

So let us first go to [the Alpine download page][2] and get one of the slim kernels optimized for virtual machines from there (choose x86 as target architecture). 

Now let us try to start this. We use the CD as a Live-CD without a read/write file system.

```
qemu-system-i386 -cdrom alpine-virt-3.7.0-x86.iso -k de -m 512
```

(here and in all my standard configurations, I have added the parameter `-k de` to enable a german keyboard layout, obviously you might want to change this). Once the machine is up, log in as root. If everything works fine, let us stop the machine again.

Now let us start the actual test. First, open a terminal, navigate to the ctOS root directory and start a copy of ctOS in networking mode.

```
./bin/run.sh qemu-net
```

At the command prompt, enter the following command

```
net addr eth0 10.0.2.20
```

to assign the IP address 10.0.2.20 to the network device that ctOS has (hopefully) recognized at startup. Then, in a second terminal, bring up a Linux virtual machine using

```
qemu-system-i386 -cdrom alpine-virt-3.7.0-x86.iso -k de -m 512 \
                -net nic,vlan=1,macaddr=00:00:00:00:11:12,model=e1000 \
                -net socket,vlan=1,connect=127.0.0.1:9030 \
                -net dump,vlan=1,file=qemu_vlan1.pcap &
```

This will create a second instance of QEMU that is connected to the first one via a virtual switch that QEMU emulates (behind the scene, QEMU will use port 9030 to connect the two instances). 

The packets going forth and back will be captured in the file `qemu_vlan1.pcap`. To monitor the traffic, the command

```
tail -f qemu_vlan1.pcap | tcpdump -r -
```

can be used. Then, in the emulated Linux instance, set up the network device with address 10.0.2.21 as follows.

```
ifconfig eth0 10.0.2.21
```

If you ping the ctOS box from the Linux virtual machine, using `ping -c 5 10.0.2.20`, you should get replies and, in the dump of the network traffice, should see that ICMP echo replies are exchanged. Similarly, you can ping the Linux machine from the ctOS machine using the built-in ping command. 

The following screenshot shows the outcome of this test. The upper window on the left hand side is ctOS, the window on the right hand side the emulated Linux box, and the third windows shows the result of the tcpdump.


![ctOS Basic networking test][3]

Finally, let us try out a TCP/IP connection. To to this, we will now connect our Linux host to a QEMU virtual machine running ctOS. 

For this purpose, we need to instruct QEMU to attach a tap device to its virtual ethernet bridge. This is essentially a pipe, where one end is a file descriptor to which QEMU connects and the other hand is a virtual networking device. Thus this device is effectively connected to the same virtual bridge that is also used by our virtual machine.

So bring up a QEMU instance with this configuration as follows.

```bash
sudo ./bin/run.sh qemu-tap
```
Note that creating a tap device requires root privileges, therefore we use sudo to run this. Next, open a terminal on the host and bring up the tap device and the testserver:

```bash
sudo ifconfig tap0 10.0.2.21
./tools/testserver 10.0.2.21 30000
```
This will instruct the testserver that is part of the ctOS distribution to bind itself to this address on the host, i.e. it is now listening on the tap device. In the ctOS window, we can now set up the network and start the test client as follows.

```
@> net addr eth0 10.0.2.20
@> cd tests
@> run testnet
```

Now the test client (running in ctOS) and the test server (running on the host) will start to exchange various messages (IP, ICMP and TCP). In the ctOS window, you should see the test cases completing, on the host, the testserver will print out status information. Part of the test suite is a testcase (testcase 5) where both parties exchange 8 MB of information over a TCP connection. The output should look similar to the screenshot below.

![ctOS Advanced networking test][4]

On this screenshot, I have also started Wireshark to dump the traffic on the tap0 device, and we can see how the messages are exchanged between the two hosts.

Now let us try to connect ctOS running in QEMU with the outside world. The exact configuration depends a bit on the setup of your LAN and your exact host system, here are instructions for Ubuntu 16.04 running in a typical home network setup.

Again, let us start QEMU using the tap networking configuration.

```bash
sudo ./bin/run.sh qemu-tap
```

Next we again set up the network on the Linux host first. This time, we do not only bring up a tap device, but also add some firewall (iptables) rules so that traffic is forwarded from the tap device to the local network. 

```
sudo ifconfig tap0 10.0.2.22
sudo sysctl -w net.ipv4.ip_forward=1
sudo iptables -A FORWARD -o tap0 -m conntrack --ctstate RELATED,ESTABLISHED -j ACCEPT
sudo iptables -A FORWARD -i tap0 ! -o tap0 -j ACCEPT
sudo iptables -A POSTROUTING -s 10.0.0.0/16 ! -o tap0 -j MASQUERADE -t nat
```

Instead of typing in these commands manually, you can also run the script if_up.sh in /bin.

Now, in the ctOS system, we set up the local interface as 10.0.2.20 again. However, this time, we also add a default route, using the tap0 device as a gateway.

```
@> net addr eth0 10.0.2.20
@> route add 0.0.0.0 0.0.0.0 10.0.2.22 eth0
```

Now you should be able to ping the gateway in your LAN. If, for instance, your gateway is 192.168.178.1, a 

```
@> ping 192.168.178.1
```

should work. The next setup that we need is a DNS server. Unfortunately, this now depends a bit on your setup. Most Linux machines come with a DNS server (named) set up and running, but most of them bind the DNS server to localhost only, so it is not reachable from the QEMU machine. Chances are, however, that your gateway is a router that has a built-in nameserver. So let us set this up as a nameserver for ctOS (again, replace 192.168.178.1 by your router).

```
@> dns add 192.168.178.1
```

Now a 

```
@> ping www.github.com
```

should work in the ctOS CLI. You should also be able to resolve a hostname using the built-in command `host` of the CLI, for instance

```
@> host httpbin.org
```

Finally, let us try out the embedded simple HTTP client. In the CLI, enter the command

```
@>http httpbin.org/ip
```

This will fetch the content of the URL httpbin.org/ip and print out the resulting data, producing an output similar to the one displayed in the screenshot below.

![ctOS HTTP testing](https://leftasexercise.files.wordpress.com/2018/05/ctos_http_test.png)


The first part of the output is the HTTP header that we send over the line. The second part is the HTTP header that we get back, followed by the actual HTTP data. Note that the built-in http function is simply assembling a GET request and printing out the answer including all header information and is not yet able to parse the header. In particular, it does not respect the content length in the header but does a non-blocking read and returns if no new data has arrived for five seconds.



## Debugging options

If tests fail, we need a way to debug. Some emulators, like QEMU and Bochs, have the option to attach a debugger like gdb to them and debug the execution step by step. However, in my experience, other approaches are usually more efficient

First, there is the good old **printf-debugging** - add log statements to the code to see what is going on. QEMU and Bochs both offer a nice feature that supports this approach - all output written to a specific port (the debugging port 0xe9) is written to the standard output. I have used this a lot to be able to read output long after it has disappeared from the screen. ctOS offers some built in loglevels, see the file `params.c`.

Next, we can use ctOS built-in **kernel debugger**. In the screenshot below, I have created an error (trying to mount the root file system onto a non-existing disk) that results in a page fault and makes ctOS enter the debugger (which can also be entered manually at any point in time using F1). 

![ctOS Kernel debugger][5]

The most important information that you can get from there is the current value of the EIP. Using `objdump -S` on the compiled kernel, we can easily find out where exactly the exception was raised. For instance, in this case, this gives us

```
static void init_super(ext2_metadata_t* meta) {
  11a5a2:	55                   	push   %ebp
  11a5a3:	89 e5                	mov    %esp,%ebp
    meta->super->device = meta->device;
  11a5a5:	8b 45 08             	mov    0x8(%ebp),%eax
  11a5a8:	8b 40 04             	mov    0x4(%eax),%eax
  11a5ab:	8b 55 08             	mov    0x8(%ebp),%edx
  11a5ae:	8b 52 0c             	mov    0xc(%edx),%edx
  11a5b1:	89 10                	mov    %edx,(%eax)
```

So we see that we try to access the meta data structure of the ext2 file system, which, however, has not completed initialization. To see how we got to this point, we can use the call stack that is printed below. Here we can see that the last return address on the stack is 0x11a6cf. Using again the output of the dump, we find that this is

```
        /*
         * Set up generic superblock structure
         */
        init_super(meta);
  11a6c4:	83 ec 0c             	sub    $0xc,%esp
  11a6c7:	ff 75 f4             	pushl  -0xc(%ebp)
  11a6ca:	e8 d3 fe ff ff       	call   11a5a2 <init_super>
```

So we see that we have called `init_super` from this code location, which is part of `fs_ext2_get_superblock` and so forth. 

In addition, the debugger offers a few commands to dump memory, print out register contents, CPU state, task table and scheduler information, device information etc. There is even a PCI browser that can list and examine all devices detected on the PCI bus. And if everything else fails, you can set a software breakpoint by inserting an `int 0x3` into the code which will take you to the debugger and thus slowly work your way towards the issue.

[1]: https://leftasexercise.files.wordpress.com/2018/04/ctos_smp_test.png
[2]: https://alpinelinux.org/downloads/
[3]: https://leftasexercise.files.wordpress.com/2018/04/ctos_basic_networking_test.png
[4]: https://leftasexercise.files.wordpress.com/2018/04/ctos_advanced_networking_test.png
[5]: https://leftasexercise.files.wordpress.com/2018/04/ctos_stack_analysis.png
[6]: https://leftasexercise.files.wordpress.com/2018/04/ctos_bare_metal.jpg

