#ifndef _FONTS_H
#define _FONTS_H

#define FONTS_MAX_CHARS 256

#include "ktypes.h"

void fonts_store_bios_font(u8* bios_font_data);
u8* fonts_get_char_ptr(int c);

#endif
