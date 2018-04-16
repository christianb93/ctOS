/*
 * test_dm.c
 */

#include "kunit.h"
#include "vga.h"
#include "dm.h"
#include <stdio.h>


/*
 * Stub for win_putchar
 */
static int do_print = 0;
void win_putchar(win_t* win, u8 c) {
    if (do_print)
        printf("%c", c);
}

/*
 * Stub for tty_init
 */
static int tty_init_called = 0;
void tty_init() {
    tty_init_called = 1;
}

/*
 * Stub for ramdisk_init
 */
static int ramdisk_init_called = 0;
void ramdisk_init() {
    ramdisk_init_called = 1;
}

/*
 * Stub for rtc_init
 */
void rtc_init() {

}

/*
 * Stub for pci_init
 */
void pci_init() {

}

/*
 * Stub for pata_init
 */
void pata_init() {

}

/*
 * Stub for ahci_init
 */
void ahci_init() {

}

/*
 * Stub for 8193 init
 */
void nic_8139_init() {

}

/*
 * Testcase 1:
 * Get block device operations while device is not yet registered
 */
int testcase1() {
    dm_init();
    ASSERT(0==dm_get_blk_dev_ops(10));
    return 0;
}

/*
 * Testcase 2:
 * Get block device operations for a device which is registered
 */
int testcase2() {
    blk_dev_ops_t ops;
    dm_init();
    ASSERT(0==dm_register_blk_dev(10, &ops));
    ASSERT(&ops==dm_get_blk_dev_ops(10));
    return 0;
}

/*
 * Testcase 3:
 * Test initialization
 */
int testcase3() {
    tty_init_called = 0;
    ramdisk_init_called = 0;
    dm_init();
    ASSERT(1==tty_init_called);
    ASSERT(1==ramdisk_init_called);
    return 0;
}

/*
 * Testcase 4:
 * Try to register the same device twice
 * and verify that the second call returns an error
 */
int testcase4() {
    blk_dev_ops_t ops;
    dm_init();
    ASSERT(0==dm_register_blk_dev(10, &ops));
    ASSERT(dm_register_blk_dev(10, &ops));
    return 0;
}

/*
 * Testcase 5:
 * Get block device operations while device is not yet registered
 */
int testcase5() {
    dm_init();
    ASSERT(0==dm_get_char_dev_ops(10));
    return 0;
}

/*
 * Testcase 6:
 * Get character device operations for a device which is registered
 */
int testcase6() {
    char_dev_ops_t ops;
    dm_init();
    ASSERT(0==dm_register_char_dev(10, &ops));
    ASSERT(&ops==dm_get_char_dev_ops(10));
    return 0;
}

int main() {
    INIT;
    RUN_CASE(1);
    RUN_CASE(2);
    RUN_CASE(3);
    RUN_CASE(4);
    RUN_CASE(5);
    RUN_CASE(6);
    END;
}
