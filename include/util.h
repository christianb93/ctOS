/*
 * util.h
 */

#ifndef _UTIL_H_
#define _UTIL_H_

/*
 * Some useful macros
 */

#define MIN(x,y)  (((x) < (y) ? (x) : (y)))
#define MAX(x,y)  (((x) > (y) ? (x) : (y)))

void save_eflags(u32* flags);
void restore_eflags(u32* flags);
u32 get_eflags();
u32 get_gs();
void set_gs(u16 gs);
u32 xchg(u32 reg, u32* mem);
void atomic_incr(reg_t* mem);
void atomic_decr(int* mem);
void cli();
void sti();
void clts();
void setts();
void fpu_restore(unsigned int);
void fpu_save(unsigned int);
u32 get_cr3();
void put_cr3(u32 cr3);
int enable_paging();
int disable_paging();
u32 get_cr0();
u32 put_cr0();
u32 reload_cr3();
void invlpg(u32 virtual_address);
void goto_ring3(u32 entry_point, u32 esp);
void halt();
void reschedule();
void rdmsr(u32 msr, u32* low, u32* high);
u32 cpuid(u32 eax, u32* ebx, u32* ecx, u32* edx);
void load_tss();

#endif /* _UTIL_H_ */
