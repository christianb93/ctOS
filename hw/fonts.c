/*
 * Manage fonts for the VGA module
 * 
 * This module can manage two types of fonts
 * - fonts read from the VGA bios (which the VGA driver is able to do)
 * - hardcoded fonts (for instance from a BDF)
 * 
 * It offers the following functions:
 * - fonts_store_bios_font - this function will hand over a BIOS font to the font module
 *                      which is stored here
 * - fonts_get_char_ptr     - return a pointer to a 16x8 character, where each line is one byte
 */
 
#include "fonts.h"
 

 
 /*
 * Font data from a VGA BIOS
 */
static u8 __bios_font_data[MAX_CHARS*16];
static u8 __have_bios_data = 0;

/*
 * Store the font data handed over by the VGA driver which 
 * the driver has taken from the BIOS. This data is assumed
 * to be a stream of 256*16 bytes, where each character is
 * represented by 16 bytes, one byte per line
 * Parameter
 * @bios_font_data - a pointer to the BIOS font data 
 */
 
void fonts_store_bios_font(u8* bios_font_data) {
    int i;
    for (i = 0; i < MAX_CHARS*16; i++) {
        __bios_font_data[i] = (bios_font_data)[i];
    }
    __have_bios_data = 1;
}

/*
 * Provide a pointer to a font for a character
 * Parameter:
 * c - the number of the character
 */
u8* fonts_get_char_ptr(int c) {
    if (c < MAX_CHARS) {
        if (__have_bios_data)
            return __bios_font_data + c*16;
    }
    return 0;
}