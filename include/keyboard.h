#ifndef _keyboard_h
#define _keyboard_h

#include "lib/sys/types.h"
#include "irq.h"
#include "lib/termios.h"

typedef struct
{
  u32 scancode;
  u32 ascii;
  u32 shift;
  u32 ctrl;
} keyboard_map_entry_t;

#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64


void keyboard_enable_idle_wait();
void kbd_init();
int kbd_isr(ir_context_t*);
void kbd_getattr(struct termios* settings);
void kbd_setattr(struct termios* settings);
u8 early_getchar();



#endif
