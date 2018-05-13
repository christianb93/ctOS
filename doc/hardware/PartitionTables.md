# Reading the partition table


In this short document, we will look at the structure of the partition table of a hard disk and learn how to use tools like lde to analyze partition table entries.


## Introduction

Hard drives of a PC are usually broken down into one or more partitions. You can think of partitions as logical volumes - in fact, partitions are basically drives at the level of operating systems like Windows. This allows for an effective organization of the available space on hard drives, in particular it is a prerequisite for installing different operating systems on the same hard drive.

If a hard disk is partitioned, the information which partitions exist and which parts of the drive belong to which partition is stored in a dedicated area of the drive called the **partition table**. In the world of PCs, there are two different ways to do this. The traditional way is called the **MBR partition table**, and this is what we describe in this document. Today, many operating systems use a more advanced partition table structure called **GPT** which is not covered here, but there is an excellent [Wikipedia article](https://en.wikipedia.org/wiki/GUID_Partition_Table) which explains the GPT in detail and should be sufficient to understand the respective code in hd.c and hd.h.

If the traditional MBR partition table is used, the first 512 byte record of a hard disk contains a list of all partitions on the drive and their layout. This table describes up to four partitions which are called primary partitions. In many cases, this is not sufficient which lead to the introduction of logical partitions - but more in this below.

## Primary partitions

The primary partition table is located within the first sector of a hard disk starting at offset 0x1be. It consists of four entries each of which occupies 16 bytes. Each entry describes one primary partition. The following table explains the layout of a partition table entry.

<table>
<thead>
<tr class="header">
<th>Byte<br />
</th>
<th>Description<br />
</th>
</tr>
</thead>
<tbody>
<tr class="odd">
<td>0<br />
</td>
<td>This byte describes whether the partition is bootable (0x80) or not (0x0). Note that some boot loaders play around with this byte at run time to hide individual partitions.<br />
</td>
</tr>
<tr class="even">
<td>1-3<br />
</td>
<td>CHS address of first sector of partition<br />
</td>
</tr>
<tr class="odd">
<td>4<br />
</td>
<td>Partition type. This byte describes what filesystem the partition contains. Some values (list not exhaustive) are<br />
0x0 - empty, partition not used<br />
0x4 - FAT 16<br />
0x5 - Extended partition, containing one or more logical partitions, see below<br />
0x7 - NTFS<br />
0x81 - Minix<br />
0x82- Linux swap<br />
0x83 - Linux Ext<br />
</td>
</tr>
<tr class="even">
<td>5-7<br />
</td>
<td>CHS address of last sector of partition<br />
</td>
</tr>
<tr class="odd">
<td>8-11<br />
</td>
<td>First sector of partition as LBA value (32 bit)<br />
</td>
</tr>
<tr class="even">
<td>12-15<br />
</td>
<td>Length of partition in sector as LBA value<br />
</td>
</tr>
</tbody>
</table>

As LBA addressing has entirely replaced CHS addressing on newer PCs, it is recommended to ignore the CHS fields altogether and to only use the LBA fields.

Let us look at an example on my PC. To print out the first sector of the first hard drive, use

```
sudo hd /dev/sda
```

The relevant part of the output is 

```
000001b0  cd 10 ac 3c 00 75 f4 c3  b3 49 30 95 00 00 80 20  |...<.u...I0.... |
000001c0  21 00 07 df 13 0c 00 08  00 00 00 20 03 00 00 df  |!.......... ....|
000001d0  14 0c 07 fe ff ff 00 28  03 00 00 d8 31 0c 00 fe  |.......(....1...|
000001e0  ff ff 83 fe ff ff 00 00  35 0c 00 20 d2 05 00 fe  |........5.. ....|
000001f0  ff ff 05 fe ff ff fe 27  07 12 02 20 ab 1b 55 aa  |.......'... ..U.|
```

If we look at the output of this command and match with the field descriptions above, we get the following primary partition table.

<table>
<tbody>
<tr class="odd">
<td>Bootable<br />
</td>
<td>First sector CHS[0]<br />
</td>
<td>First sector CHS[1]<br />
</td>
<td>First sector CHS[2]<br />
</td>
<td>Type<br />
</td>
<td>Last sector CHS[0]<br />
</td>
<td>Last sector CHS[1]<br />
</td>
<td>Last sector CHS[2]<br />
</td>
<td>First sector LBA<br />
</td>
<td>Length<br />
</td>
</tr>
<tr class="even">
<td>0x80<br />
</td>
<td>0x20<br />
</td>
<td>0x21<br />
</td>
<td>0x00<br />
</td>
<td>0x7<br />
</td>
<td>0xdf<br />
</td>
<td>0x13<br />
</td>
<td>0x0c<br />
</td>
<td>0x00000800<br />
</td>
<td>0x32000<br />
</td>
</tr>
<tr class="odd">
<td>0x00<br />
</td>
<td>0xdf<br />
</td>
<td>0x14<br />
</td>
<td>0xc<br />
</td>
<td>0x7<br />
</td>
<td>0xfe<br />
</td>
<td>0xff<br />
</td>
<td>0xff<br />
</td>
<td>0x32800<br />
</td>
<td>0xc31d800<br />
</td>
</tr>
<tr class="even">
<td>0x00<br />
</td>
<td>0xfe<br />
</td>
<td>0xff<br />
</td>
<td>0xff<br />
</td>
<td>0x83<br />
</td>
<td>0xfe<br />
</td>
<td>0xff<br />
</td>
<td>0xff<br />
</td>
<td>0xc350000<br />
</td>
<td>0x5d22000<br />
</td>
</tr>
<tr class="odd">
<td>0x00<br />
</td>
<td>0xfe<br />
</td>
<td>0xff<br />
</td>
<td>0xff<br />
</td>
<td>0x5<br />
</td>
<td>0xfe<br />
</td>
<td>0xff<br />
</td>
<td>0xff<br />
</td>
<td>0x120727fe<br />
</td>
<td>0x1ba6f802<br />
</td>
</tr>
</tbody>
</table>

Remember that the 32 bit values are stored on the disk in little-endian byte order. So we see that we have in fact four primary partitions. The first partition is an NFTS partition with 0x32000 = 204800 sectors, i.e. 100 MB, starting at sector 0x800. The second partition is also an NFTS partition having 0xc31d800 sectors, i.e. 99.899 MB. The third partition has 47.684 MB and is a Linux native partition. Finally, the last partition is an extended partition, starting at sector 302458878 = 0x120727fe.

This example also shows that the CHS value are pure nonsense starting with the second partition table entry, demonstrating why the LBA values should be used.

## Logical partitions

Having four partitions is often not enough, especially when you plan to install more than one operating system on a hard drive. To be able to extend the number of partitions while at the same time staying backward compatible, **logical partitions** where introduced. 

The fourth partition in the example above is an extended partition which - by definition - is not a real partition in its own right, but simply a container into which we can place one or more logical partitions. So the entry above simply tells us that starting at sector 0x120727fe, the disk space is split into logical partitions.

To locate the logical partitions, we will have to follow a chain of pointers through the hard disk. In fact, each logical partition is described by an entire partition table containing two entries. The first entry describes the logical partition itself. The second entry points us to a sector on the hard drive in which the partition table for the next logical partition is located.

Let us look at that mechanism in detail using my drive as an example. The first pointer corresponding to the first logical partition is located in the first sector of the extended partition, which is 0x120727fe in this case. Let us print out this sector using hd. Again we only look at the last few bytes of the sector.

```
sudo dd if=/dev/sda count=1 bs=512 skip=302458878 | hd
1+0 records in
1+0 records out
512 bytes copied, 5.2664e-05 s, 9.7 MB/s
00000000  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
*
000001b0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 fe  |................|
000001c0  ff ff 83 fe ff ff 02 00  00 00 00 00 2a 01 00 fe  |............*...|
000001d0  ff ff 05 fe ff ff 02 00  2a 01 00 38 a4 0b 00 00  |........*..8....|
000001e0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
000001f0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 55 aa  |..............U.|
00000200
```

What we see is that we essentially have another partition table in this block, with two entries. The first entry describes the first logical partition which is of type 0x83, i.e. a Linux native partition. The second entry is again of type 0x5.

When we try to decipher the first entry, we see that it describes a partition with length 0x012a0000 blocks, i.e. 9536 kilobytes. However, the partition seems to start at sector 2, even though we expect it to start somewhere within the extended partition. The reason for that discrepancy is that in partition table entries describing logical partitions, the LBA entry for the first sector does not specify the location of the first sector in absolute terms, but **relative** to the location of the partition table entry itself. As the partition table is located at sector 0x120727fe, this is
0x120727fe + 2 = 0x12072800 = 302458880.

The second entry in the partition table is a pointer to the next logical partition. Again, the LBA start sector in the second partition table entry - 0x12a0002 - is only an offset. However, the offset is not relative to the location of the partition table entry itself, but relative to the start of the entire extended partition. For the first logical partition, this does not make a difference but it will make a difference for the upcoming entries. So the next partition table entry we need to look for is located in sector

0x120727fe + 0x12a0002 = 0x13312800

If we print this sector using

```
sudo dd if=/dev/sda count=1 bs=512 skip=321988608 | hd
```
we find the following few bytes close to the end of the sector.


```
000001b0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 fe  |................|
000001c0  ff ff 83 fe ff ff 00 08  00 00 00 30 a4 0b 00 fe  |...........0....|
000001d0  ff ff 05 fe ff ff 02 38  ce 0c 00 88 38 01 00 00  |.......8....8...|
000001e0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
000001f0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 55 aa  |..............U.|
```

Again, the table has two entries. The first is a partition with 0xba43000 sectors (i.e. 95366 MB) starting at sector

0x13312800 + 0x800 = 0x13313000 = 321990656

The second entry points us to the next logical partition table entry. Again the offset is with respect to the start of the entire extended partition, so the computation to find the next sector to read is

0x120727fe + 0xcce3802 = 0x1ED56000 = 517300224

This pattern repeats itself until we get to the partition table describing the last entry. For this table, only the first entry describing the logical partition itself is present. The second entry has type 0x0, indicating that there are no more logical partitions.

So we have seen that the field "First sector LBA" in the partition table entries can be absolute, relative with respect to the first sector of extended partition or relative with respect to the sector in which the partition table is located. The following table summarizes the meanings of this dword in the various contents.

<table>
<thead>
<tr class="header">
<th>Type of partition table entry<br />
</th>
<th>Calculation of corresponding absolute LBA address<br />
</th>
</tr>
</thead>
<tbody>
<tr class="odd">
<td>Primary partition table entry<br />
</td>
<td>Start of partition is &quot;First sector LBA&quot;<br />
</td>
</tr>
<tr class="even">
<td>Partition table entry for logical partition - first entry<br />
</td>
<td>Start of partition is &quot;First sector LBA&quot; + LBA sector number of sector in which the partition table is located<br />
</td>
</tr>
<tr class="odd">
<td>Partition table entry for logical partition - second entry (type 0x5)<br />
</td>
<td>Location of partition table for next logical partition is &quot;First sector LBA&quot; + LBA sector number of first sector of extended partition<br />
</td>
</tr>
</tbody>
</table>



