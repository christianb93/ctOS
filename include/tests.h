/*
 * tests.h
 * This header file contains switches to turn specific
 * tests at boot time on and off
 */

#ifndef _TESTS_H_
#define _TESTS_H_

/*
 * Comment out any of the constants below to
 * skip a test
 * The tests are defined in main.c/do_tests
 */

/*
#define DO_EFLAGS_TEST
#define DO_XCHG_TEST
#define DO_ATTACH_TEST
#define DO_SMP_TEST
#define DO_DELAY_TEST
#define DO_THREAD_TEST
#define DO_PAGING_TEST
#define DO_RAMDISK_TEST
#define DO_PHYS_PAGES_TEST
#define DO_TIMER_TEST
#define DO_KHEAP_TEST
#define DO_FS_TEST
#define DO_PATA_TEST
#define DO_AHCI_TEST
#define DO_TTY_TEST
*/

#define DO_8139_TEST

#endif /* _TESTS_H_ */

void do_pre_init_tests();
void do_post_init_tests();
void do_smp_tests_boot_bsp();
void do_smp_tests_boot_ap();
void do_pre_init_tests_ap();
