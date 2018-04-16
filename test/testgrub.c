/*
 * testgrub.c
 *
 */

#include "ktypes.h"
#include "multiboot.h"
#include "vga.h"
#include "locks.h"
#include "rm.h"

#define REAL_MODE_FAR_PTR_TO_LINEAR(ptr) (((ptr) & 0xFFFF) + 16 * ((ptr) >> 16))

extern void kprintf(char* template, ...);

int cursor_x = 0;
int cursor_y = 0;

/*
 * Area to store mode list
 */
static u16 vbeMode[1024];

/****************************************************************************************
 * Some stubs                                                                           *
 ****************************************************************************************/

void sem_init(semaphore_t* sem, u32 value) {

}

void __sem_down(semaphore_t* sem, char* file, int line) {

}

void debug_lock_wait(u32 lock, int type, int rw, char* file, int line) {

}

void debug_lock_acquired(u32 lock, int rw) {

}

void debug_lock_released(u32 lock, int rw) {

}
void debug_lock_cancel(u32 lock_addr, int rw) {

}

void mutex_up(semaphore_t* sem) {

}

void trap() {

}

int cpu_has_feature(int cpuid, unsigned long long feature) {
    return 0;
}

u32 mm_map_memio(u32 phys_base, u32 size) {
    return 0;
}

int sched_get_load(int cpu) {
    return 20;
}

/*
 * Create RGB value
 */
u32 rgb(vbe_mode_info_t* vbe_mode, u32 red, u32 green, u32 blue) {
    u32 red_mask;
    u32 green_mask;
    u32 blue_mask;
    red_mask = (1 << vbe_mode->redMaskSize) - 1;
    red_mask = red_mask << vbe_mode->redFieldPosition;
    green_mask = (1 << vbe_mode->greenMaskSize) - 1;
    green_mask = green_mask << vbe_mode->greenFieldPosition;
    blue_mask = (1 << vbe_mode->blueMaskSize) - 1;
    blue_mask = blue_mask << vbe_mode->blueFieldPosition;
    return (red & 0xff) * red_mask + (green & 0xff) * green_mask + (blue & 0xff) * blue_mask;
}

/*
 * Put pixel on screen
 */
void put_pixel(vbe_mode_info_t* vbe_mode, u32 x, u32 y, u32 rgb) {
    u32 address = y * vbe_mode->bytesPerScanLine + x * (vbe_mode->bitsPerPixel / 8) + vbe_mode->physBasePtr;
    *((u32*) address) = rgb;
}

/*
 * These symbols are defined in testrm.S and mark the beginning and the
 * end of the code to return to real mode in the kernel ELF file
 */
extern u32 _rm_switch_start;
extern u32 _rm_switch_end;

/*
 * Return to real mode
 * Parameter:
 * @function - function to be executed
 */
static void go_to_rm(u16 function) {
    char* ptr;
    int i;
    /*
     * Copy our code to 0x7C00
     */
    int bytes = ((int)&_rm_switch_end) - ((int)&_rm_switch_start);
    i = 0;
    for (ptr = (char*) 0x7C00; ptr < (char*) (0x7C00 + bytes); ptr++) {
        *ptr = *((char*)((u32)(&_rm_switch_start) + i));
        i++;
    }
    /*
     * Store function at address 0x10000
     */
    *((u16*) 0x10000) = function;
    /*
     * and call code
     */
    asm("call 0x7c00");
}

/*
 * Main routine
 */
void run(u32 multiboot_ptr) {
    vbe_info_block_t* vbe_info;
    vbe_mode_info_t* vbe_mode;
    u16* videoModePtr = 0;
    vga_init(0);
    u16 good_mode = 0;
    u8 last_bpp = 0;
    int i;
    cons_init();
    kprintf("Hello World!\n");
    /*
     * Go to real mode, executing function 1 (get VBE info)
     */
    go_to_rm(1);
    /*
     * Get VBE info from address 0x10000
     */
    vbe_info = (vbe_info_block_t*) (0x10004);
    kprintf("Size of VBE info block: %d\n", sizeof(vbe_info_block_t));
    if (0 == *((u16*)0x10000)) {
        kprintf("VBE data available\n");
        kprintf("VBE signature: %c%c%c%c\n", vbe_info->vbeSignature[0], vbe_info->vbeSignature[1], vbe_info->vbeSignature[2],
                vbe_info->vbeSignature[3]);
        kprintf("Video memory: %d kB\n", 64 * vbe_info->totalMemory);
        kprintf("VBE version: %x\n", vbe_info->vbeVersion);
        kprintf("Software revision: %w\n", vbe_info->oemSoftwareRev);
        kprintf("OEM vendor name ptr: %x\n", vbe_info->oemVendorNamePtr);
        kprintf("OEM vendor name: %s\n", FAR_PTR_TO_ADDR(vbe_info->oemVendorNamePtr));
        /*
         * Do memory dump of entire block
         */
        for (i = 0; i < 256; i++) {
            kprintf("%h ", ((char*) vbe_info)[i]);
            if (0 == ((i+1) % 16))
                kprintf("\n");
        }
        kprintf("\n");
        videoModePtr = (u16*) FAR_PTR_TO_ADDR(vbe_info->videoModePtr);
        i = 0;
        while (videoModePtr[i] != 0xFFFF) {
            if (i < 1024)
                vbeMode[i]=videoModePtr[i];
            i++;
        }
        if (i < 1024)
            vbeMode[i]=0xFFFF;
        kprintf("Found %d video modes in total. Now looking for preferred modes:\n", i);
        /*
         * Now locate a mode with resolution 1024 x 768
         */
        i = 0;
        while (vbeMode[i] != 0xFFFF) {
            *((u16*)(0x10002)) = vbeMode[i];
            go_to_rm(2);
            vbe_mode = (vbe_mode_info_t*) (0x10004);
            /*
             * If the mode is the requested resolution and has a linear frame buffer
             */
            if ((vbe_mode->xResolution == 1024) && (vbe_mode->yResolution == 768) && (vbe_mode->modeAttributes & (1 << 7))) {
                if ((vbe_mode->bitsPerPixel >=16 ) && (vbe_mode->bitsPerPixel > last_bpp))
                    good_mode = vbeMode[i];
            }
            i++;
        }
        if (good_mode) {
            kprintf("Switching to mode %x\n", good_mode);
            /*
             * Write mode plus frame buffer bit at address 0x10002
             */
            *((u16*) 0x10002) = (good_mode & 0x1FF) + (1 << 14);
            /*
             * and call real mode function
             */
            // go_to_rm(3);
            kprintf("Return value: %w\n", *((u16*)(0x10000)));
        }
    }
    else {
        kprintf("No VBE data available\n");
    }
    asm("cli ; hlt");
}


