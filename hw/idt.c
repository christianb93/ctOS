/*
 * idt.c
 *
 * This module contains some functions to set up the interrupt descriptor table (IDT)
 */

#include "ktypes.h"
#include "idt.h"
#include "gdt.h"
#include "lib/string.h"

/*
 * Interrupt handlers
 * These handlers are defined in gate.S
 */

extern void gate_0();
extern void gate_1();
extern void gate_2();
extern void gate_3();
extern void gate_4();
extern void gate_5();
extern void gate_6();
extern void gate_7();
extern void gate_8();
extern void gate_9();
extern void gate_10();
extern void gate_11();
extern void gate_12();
extern void gate_13();
extern void gate_14();
extern void gate_15();
extern void gate_16();
extern void gate_17();


extern void gate_32();
extern void gate_33();
extern void gate_34();
extern void gate_35();
extern void gate_36();
extern void gate_37();
extern void gate_38();
extern void gate_39();
extern void gate_40();
extern void gate_41();
extern void gate_42();
extern void gate_43();
extern void gate_44();
extern void gate_45();
extern void gate_46();
extern void gate_47();
extern void gate_48();
extern void gate_49();
extern void gate_50();
extern void gate_51();
extern void gate_52();
extern void gate_53();
extern void gate_54();
extern void gate_55();
extern void gate_56();
extern void gate_57();
extern void gate_58();
extern void gate_59();
extern void gate_60();
extern void gate_61();
extern void gate_62();
extern void gate_63();
extern void gate_64();
extern void gate_65();
extern void gate_66();
extern void gate_67();
extern void gate_68();
extern void gate_69();
extern void gate_70();
extern void gate_71();
extern void gate_72();
extern void gate_73();
extern void gate_74();
extern void gate_75();
extern void gate_76();
extern void gate_77();
extern void gate_78();
extern void gate_79();
extern void gate_80();
extern void gate_81();
extern void gate_82();
extern void gate_83();
extern void gate_84();
extern void gate_85();
extern void gate_86();
extern void gate_87();
extern void gate_88();
extern void gate_89();
extern void gate_90();
extern void gate_91();
extern void gate_92();
extern void gate_93();
extern void gate_94();
extern void gate_95();
extern void gate_96();
extern void gate_97();
extern void gate_98();
extern void gate_99();
extern void gate_100();
extern void gate_101();
extern void gate_102();
extern void gate_103();
extern void gate_104();
extern void gate_105();
extern void gate_106();
extern void gate_107();
extern void gate_108();
extern void gate_109();
extern void gate_110();
extern void gate_111();
extern void gate_112();
extern void gate_113();
extern void gate_114();
extern void gate_115();
extern void gate_116();
extern void gate_117();
extern void gate_118();
extern void gate_119();
extern void gate_120();
extern void gate_121();
extern void gate_122();
extern void gate_123();
extern void gate_124();
extern void gate_125();
extern void gate_126();
extern void gate_127();
extern void gate_128();
extern void gate_129();
extern void gate_130();
extern void gate_131();
extern void gate_132();
extern void gate_133();
extern void gate_134();
extern void gate_135();
extern void gate_136();
extern void gate_137();
extern void gate_138();
extern void gate_139();
extern void gate_140();
extern void gate_141();
extern void gate_142();
extern void gate_143();
extern void gate_144();
extern void gate_145();
extern void gate_146();
extern void gate_147();
extern void gate_148();
extern void gate_149();
extern void gate_150();
extern void gate_151();
extern void gate_152();
extern void gate_153();
extern void gate_154();
extern void gate_155();
extern void gate_156();
extern void gate_157();
extern void gate_158();
extern void gate_159();
extern void gate_160();
extern void gate_161();
extern void gate_162();
extern void gate_163();
extern void gate_164();
extern void gate_165();
extern void gate_166();
extern void gate_167();
extern void gate_168();
extern void gate_169();
extern void gate_170();
extern void gate_171();
extern void gate_172();
extern void gate_173();
extern void gate_174();
extern void gate_175();
extern void gate_176();
extern void gate_177();
extern void gate_178();
extern void gate_179();
extern void gate_180();
extern void gate_181();
extern void gate_182();
extern void gate_183();
extern void gate_184();
extern void gate_185();
extern void gate_186();
extern void gate_187();
extern void gate_188();
extern void gate_189();
extern void gate_190();
extern void gate_191();
extern void gate_192();
extern void gate_193();
extern void gate_194();
extern void gate_195();
extern void gate_196();
extern void gate_197();
extern void gate_198();
extern void gate_199();
extern void gate_200();
extern void gate_201();
extern void gate_202();
extern void gate_203();
extern void gate_204();
extern void gate_205();
extern void gate_206();
extern void gate_207();
extern void gate_208();
extern void gate_209();
extern void gate_210();
extern void gate_211();
extern void gate_212();
extern void gate_213();
extern void gate_214();
extern void gate_215();
extern void gate_216();
extern void gate_217();
extern void gate_218();
extern void gate_219();
extern void gate_220();
extern void gate_221();
extern void gate_222();
extern void gate_223();
extern void gate_224();
extern void gate_225();
extern void gate_226();
extern void gate_227();
extern void gate_228();
extern void gate_229();
extern void gate_230();
extern void gate_231();
extern void gate_232();
extern void gate_233();
extern void gate_234();
extern void gate_235();
extern void gate_236();
extern void gate_237();
extern void gate_238();
extern void gate_239();
extern void gate_240();
extern void gate_241();
extern void gate_242();
extern void gate_243();
extern void gate_244();
extern void gate_245();
extern void gate_246();
extern void gate_247();
extern void gate_248();
extern void gate_249();
extern void gate_250();
extern void gate_251();
extern void gate_252();
extern void gate_253();
extern void gate_254();
extern void gate_255();

/*
 * IDT entries
 */
static idt_entry_t __attribute__ ((aligned (8))) idt[256];

/*
 * Pointer structure to be loaded
 * by LIDT
 */
static idt_ptr_t idt_ptr;

/*
 * Create an IDT entry
 * Parameters:
 * @offset: offset of the interrupt handler within the target code segment
 * @selector: 16 bit selector specifying the code segment of the interrupt handler
 * @trap: if this is 1, a trap gate will be generated, i.e. additional interrupts can occur within the handler
 * @dpl: 0-3, specifies which privilege level a caller needs to have
 * The fields in the IDT structure which are not covered by the parameters are set as follows.
 * Default operation size = 1
 * Present = 1
 * Descriptor type = 0 (system descriptor)
 * Return value:
 * the newly created IDT entry
 */
idt_entry_t idt_create_entry(u32 offset, u16 selector, u8 trap, u8 dpl) {
    idt_entry_t entry;
    entry.offset_12 = (u16) offset;
    entry.offset_34 = (u16) (offset >> 16);
    entry.selector = selector;
    entry.d = 1;
    entry.dpl = dpl;
    entry.fixed0 = 0x3;
    entry.p = 1;
    entry.s = 0;
    entry.reserved0 = 0;
    entry.trap = trap;
    return entry;
}

/*
 * Set up IDT
 * Return value:
 * the physical address to an IDT pointer structure
 */
u32 idt_create_table() {
    /*
     * Clear with zeros
     */
    memset((void*) idt, 0, 256 * sizeof(idt_entry_t));
    /*
     * Set up handler for CPU interrupts
     * We set up all handlers with trap = 0 and dpl = 0,
     * except 0x80, which has dpl = 3
     */
    idt[0] = idt_create_entry((u32) gate_0, SELECTOR_CODE_KERNEL, 0, 0);
    idt[1] = idt_create_entry((u32) gate_1, SELECTOR_CODE_KERNEL, 0, 0);
    idt[2] = idt_create_entry((u32) gate_2, SELECTOR_CODE_KERNEL, 0, 0);
    idt[3] = idt_create_entry((u32) gate_3, SELECTOR_CODE_KERNEL, 0, 3);
    idt[4] = idt_create_entry((u32) gate_4, SELECTOR_CODE_KERNEL, 0, 0);
    idt[5] = idt_create_entry((u32) gate_5, SELECTOR_CODE_KERNEL, 0, 0);
    idt[6] = idt_create_entry((u32) gate_6, SELECTOR_CODE_KERNEL, 0, 0);
    idt[7] = idt_create_entry((u32) gate_7, SELECTOR_CODE_KERNEL, 0, 0);
    idt[8] = idt_create_entry((u32) gate_8, SELECTOR_CODE_KERNEL, 0, 0);
    idt[9] = idt_create_entry((u32) gate_9, SELECTOR_CODE_KERNEL, 0, 0);
    idt[10] = idt_create_entry((u32) gate_10, SELECTOR_CODE_KERNEL, 0, 0);
    idt[11] = idt_create_entry((u32) gate_11, SELECTOR_CODE_KERNEL, 0, 0);
    idt[12] = idt_create_entry((u32) gate_12, SELECTOR_CODE_KERNEL, 0, 0);
    idt[13] = idt_create_entry((u32) gate_13, SELECTOR_CODE_KERNEL, 0, 0);
    idt[14] = idt_create_entry((u32) gate_14, SELECTOR_CODE_KERNEL, 0, 0);
    idt[15] = idt_create_entry((u32) gate_15, SELECTOR_CODE_KERNEL, 0, 0);
    idt[16] = idt_create_entry((u32) gate_16, SELECTOR_CODE_KERNEL, 0, 0);
    idt[17] = idt_create_entry((u32) gate_17, SELECTOR_CODE_KERNEL, 0, 0);
    idt[32] = idt_create_entry((u32) gate_32, SELECTOR_CODE_KERNEL, 0, 0);
    idt[33] = idt_create_entry((u32) gate_33, SELECTOR_CODE_KERNEL, 0, 0);
    idt[34] = idt_create_entry((u32) gate_34, SELECTOR_CODE_KERNEL, 0, 0);
    idt[35] = idt_create_entry((u32) gate_35, SELECTOR_CODE_KERNEL, 0, 0);
    idt[36] = idt_create_entry((u32) gate_36, SELECTOR_CODE_KERNEL, 0, 0);
    idt[37] = idt_create_entry((u32) gate_37, SELECTOR_CODE_KERNEL, 0, 0);
    idt[38] = idt_create_entry((u32) gate_38, SELECTOR_CODE_KERNEL, 0, 0);
    idt[39] = idt_create_entry((u32) gate_39, SELECTOR_CODE_KERNEL, 0, 0);
    idt[40] = idt_create_entry((u32) gate_40, SELECTOR_CODE_KERNEL, 0, 0);
    idt[41] = idt_create_entry((u32) gate_41, SELECTOR_CODE_KERNEL, 0, 0);
    idt[42] = idt_create_entry((u32) gate_42, SELECTOR_CODE_KERNEL, 0, 0);
    idt[43] = idt_create_entry((u32) gate_43, SELECTOR_CODE_KERNEL, 0, 0);
    idt[44] = idt_create_entry((u32) gate_44, SELECTOR_CODE_KERNEL, 0, 0);
    idt[45] = idt_create_entry((u32) gate_45, SELECTOR_CODE_KERNEL, 0, 0);
    idt[46] = idt_create_entry((u32) gate_46, SELECTOR_CODE_KERNEL, 0, 0);
    idt[47] = idt_create_entry((u32) gate_47, SELECTOR_CODE_KERNEL, 0, 0);
    idt[48] = idt_create_entry((u32) gate_48, SELECTOR_CODE_KERNEL, 0, 0);
    idt[49] = idt_create_entry((u32) gate_49, SELECTOR_CODE_KERNEL, 0, 0);
    idt[50] = idt_create_entry((u32) gate_50, SELECTOR_CODE_KERNEL, 0, 0);
    idt[51] = idt_create_entry((u32) gate_51, SELECTOR_CODE_KERNEL, 0, 0);
    idt[52] = idt_create_entry((u32) gate_52, SELECTOR_CODE_KERNEL, 0, 0);
    idt[53] = idt_create_entry((u32) gate_53, SELECTOR_CODE_KERNEL, 0, 0);
    idt[54] = idt_create_entry((u32) gate_54, SELECTOR_CODE_KERNEL, 0, 0);
    idt[55] = idt_create_entry((u32) gate_55, SELECTOR_CODE_KERNEL, 0, 0);
    idt[56] = idt_create_entry((u32) gate_56, SELECTOR_CODE_KERNEL, 0, 0);
    idt[57] = idt_create_entry((u32) gate_57, SELECTOR_CODE_KERNEL, 0, 0);
    idt[58] = idt_create_entry((u32) gate_58, SELECTOR_CODE_KERNEL, 0, 0);
    idt[59] = idt_create_entry((u32) gate_59, SELECTOR_CODE_KERNEL, 0, 0);
    idt[60] = idt_create_entry((u32) gate_60, SELECTOR_CODE_KERNEL, 0, 0);
    idt[61] = idt_create_entry((u32) gate_61, SELECTOR_CODE_KERNEL, 0, 0);
    idt[62] = idt_create_entry((u32) gate_62, SELECTOR_CODE_KERNEL, 0, 0);
    idt[63] = idt_create_entry((u32) gate_63, SELECTOR_CODE_KERNEL, 0, 0);
    idt[64] = idt_create_entry((u32) gate_64, SELECTOR_CODE_KERNEL, 0, 0);
    idt[65] = idt_create_entry((u32) gate_65, SELECTOR_CODE_KERNEL, 0, 0);
    idt[66] = idt_create_entry((u32) gate_66, SELECTOR_CODE_KERNEL, 0, 0);
    idt[67] = idt_create_entry((u32) gate_67, SELECTOR_CODE_KERNEL, 0, 0);
    idt[68] = idt_create_entry((u32) gate_68, SELECTOR_CODE_KERNEL, 0, 0);
    idt[69] = idt_create_entry((u32) gate_69, SELECTOR_CODE_KERNEL, 0, 0);
    idt[70] = idt_create_entry((u32) gate_70, SELECTOR_CODE_KERNEL, 0, 0);
    idt[71] = idt_create_entry((u32) gate_71, SELECTOR_CODE_KERNEL, 0, 0);
    idt[72] = idt_create_entry((u32) gate_72, SELECTOR_CODE_KERNEL, 0, 0);
    idt[73] = idt_create_entry((u32) gate_73, SELECTOR_CODE_KERNEL, 0, 0);
    idt[74] = idt_create_entry((u32) gate_74, SELECTOR_CODE_KERNEL, 0, 0);
    idt[75] = idt_create_entry((u32) gate_75, SELECTOR_CODE_KERNEL, 0, 0);
    idt[76] = idt_create_entry((u32) gate_76, SELECTOR_CODE_KERNEL, 0, 0);
    idt[77] = idt_create_entry((u32) gate_77, SELECTOR_CODE_KERNEL, 0, 0);
    idt[78] = idt_create_entry((u32) gate_78, SELECTOR_CODE_KERNEL, 0, 0);
    idt[79] = idt_create_entry((u32) gate_79, SELECTOR_CODE_KERNEL, 0, 0);
    idt[80] = idt_create_entry((u32) gate_80, SELECTOR_CODE_KERNEL, 0, 0);
    idt[81] = idt_create_entry((u32) gate_81, SELECTOR_CODE_KERNEL, 0, 0);
    idt[82] = idt_create_entry((u32) gate_82, SELECTOR_CODE_KERNEL, 0, 0);
    idt[83] = idt_create_entry((u32) gate_83, SELECTOR_CODE_KERNEL, 0, 0);
    idt[84] = idt_create_entry((u32) gate_84, SELECTOR_CODE_KERNEL, 0, 0);
    idt[85] = idt_create_entry((u32) gate_85, SELECTOR_CODE_KERNEL, 0, 0);
    idt[86] = idt_create_entry((u32) gate_86, SELECTOR_CODE_KERNEL, 0, 0);
    idt[87] = idt_create_entry((u32) gate_87, SELECTOR_CODE_KERNEL, 0, 0);
    idt[88] = idt_create_entry((u32) gate_88, SELECTOR_CODE_KERNEL, 0, 0);
    idt[89] = idt_create_entry((u32) gate_89, SELECTOR_CODE_KERNEL, 0, 0);
    idt[90] = idt_create_entry((u32) gate_90, SELECTOR_CODE_KERNEL, 0, 0);
    idt[91] = idt_create_entry((u32) gate_91, SELECTOR_CODE_KERNEL, 0, 0);
    idt[92] = idt_create_entry((u32) gate_92, SELECTOR_CODE_KERNEL, 0, 0);
    idt[93] = idt_create_entry((u32) gate_93, SELECTOR_CODE_KERNEL, 0, 0);
    idt[94] = idt_create_entry((u32) gate_94, SELECTOR_CODE_KERNEL, 0, 0);
    idt[95] = idt_create_entry((u32) gate_95, SELECTOR_CODE_KERNEL, 0, 0);
    idt[96] = idt_create_entry((u32) gate_96, SELECTOR_CODE_KERNEL, 0, 0);
    idt[97] = idt_create_entry((u32) gate_97, SELECTOR_CODE_KERNEL, 0, 0);
    idt[98] = idt_create_entry((u32) gate_98, SELECTOR_CODE_KERNEL, 0, 0);
    idt[99] = idt_create_entry((u32) gate_99, SELECTOR_CODE_KERNEL, 0, 0);
    idt[100] = idt_create_entry((u32) gate_100, SELECTOR_CODE_KERNEL, 0, 0);
    idt[101] = idt_create_entry((u32) gate_101, SELECTOR_CODE_KERNEL, 0, 0);
    idt[102] = idt_create_entry((u32) gate_102, SELECTOR_CODE_KERNEL, 0, 0);
    idt[103] = idt_create_entry((u32) gate_103, SELECTOR_CODE_KERNEL, 0, 0);
    idt[104] = idt_create_entry((u32) gate_104, SELECTOR_CODE_KERNEL, 0, 0);
    idt[105] = idt_create_entry((u32) gate_105, SELECTOR_CODE_KERNEL, 0, 0);
    idt[106] = idt_create_entry((u32) gate_106, SELECTOR_CODE_KERNEL, 0, 0);
    idt[107] = idt_create_entry((u32) gate_107, SELECTOR_CODE_KERNEL, 0, 0);
    idt[108] = idt_create_entry((u32) gate_108, SELECTOR_CODE_KERNEL, 0, 0);
    idt[109] = idt_create_entry((u32) gate_109, SELECTOR_CODE_KERNEL, 0, 0);
    idt[110] = idt_create_entry((u32) gate_110, SELECTOR_CODE_KERNEL, 0, 0);
    idt[111] = idt_create_entry((u32) gate_111, SELECTOR_CODE_KERNEL, 0, 0);
    idt[112] = idt_create_entry((u32) gate_112, SELECTOR_CODE_KERNEL, 0, 0);
    idt[113] = idt_create_entry((u32) gate_113, SELECTOR_CODE_KERNEL, 0, 0);
    idt[114] = idt_create_entry((u32) gate_114, SELECTOR_CODE_KERNEL, 0, 0);
    idt[115] = idt_create_entry((u32) gate_115, SELECTOR_CODE_KERNEL, 0, 0);
    idt[116] = idt_create_entry((u32) gate_116, SELECTOR_CODE_KERNEL, 0, 0);
    idt[117] = idt_create_entry((u32) gate_117, SELECTOR_CODE_KERNEL, 0, 0);
    idt[118] = idt_create_entry((u32) gate_118, SELECTOR_CODE_KERNEL, 0, 0);
    idt[119] = idt_create_entry((u32) gate_119, SELECTOR_CODE_KERNEL, 0, 0);
    idt[120] = idt_create_entry((u32) gate_120, SELECTOR_CODE_KERNEL, 0, 0);
    idt[121] = idt_create_entry((u32) gate_121, SELECTOR_CODE_KERNEL, 0, 0);
    idt[122] = idt_create_entry((u32) gate_122, SELECTOR_CODE_KERNEL, 0, 0);
    idt[123] = idt_create_entry((u32) gate_123, SELECTOR_CODE_KERNEL, 0, 0);
    idt[124] = idt_create_entry((u32) gate_124, SELECTOR_CODE_KERNEL, 0, 0);
    idt[125] = idt_create_entry((u32) gate_125, SELECTOR_CODE_KERNEL, 0, 0);
    idt[126] = idt_create_entry((u32) gate_126, SELECTOR_CODE_KERNEL, 0, 0);
    idt[127] = idt_create_entry((u32) gate_127, SELECTOR_CODE_KERNEL, 0, 0);
    idt[129] = idt_create_entry((u32) gate_129, SELECTOR_CODE_KERNEL, 0, 0);
    idt[130] = idt_create_entry((u32) gate_130, SELECTOR_CODE_KERNEL, 0, 0);
    idt[131] = idt_create_entry((u32) gate_131, SELECTOR_CODE_KERNEL, 0, 0);
    idt[132] = idt_create_entry((u32) gate_132, SELECTOR_CODE_KERNEL, 0, 0);
    idt[133] = idt_create_entry((u32) gate_133, SELECTOR_CODE_KERNEL, 0, 0);
    idt[134] = idt_create_entry((u32) gate_134, SELECTOR_CODE_KERNEL, 0, 0);
    idt[135] = idt_create_entry((u32) gate_135, SELECTOR_CODE_KERNEL, 0, 0);
    idt[136] = idt_create_entry((u32) gate_136, SELECTOR_CODE_KERNEL, 0, 0);
    idt[137] = idt_create_entry((u32) gate_137, SELECTOR_CODE_KERNEL, 0, 0);
    idt[138] = idt_create_entry((u32) gate_138, SELECTOR_CODE_KERNEL, 0, 0);
    idt[139] = idt_create_entry((u32) gate_139, SELECTOR_CODE_KERNEL, 0, 0);
    idt[140] = idt_create_entry((u32) gate_140, SELECTOR_CODE_KERNEL, 0, 0);
    idt[141] = idt_create_entry((u32) gate_141, SELECTOR_CODE_KERNEL, 0, 0);
    idt[142] = idt_create_entry((u32) gate_142, SELECTOR_CODE_KERNEL, 0, 0);
    idt[143] = idt_create_entry((u32) gate_143, SELECTOR_CODE_KERNEL, 0, 0);
    idt[144] = idt_create_entry((u32) gate_144, SELECTOR_CODE_KERNEL, 0, 0);
    idt[145] = idt_create_entry((u32) gate_145, SELECTOR_CODE_KERNEL, 0, 0);
    idt[146] = idt_create_entry((u32) gate_146, SELECTOR_CODE_KERNEL, 0, 0);
    idt[147] = idt_create_entry((u32) gate_147, SELECTOR_CODE_KERNEL, 0, 0);
    idt[148] = idt_create_entry((u32) gate_148, SELECTOR_CODE_KERNEL, 0, 0);
    idt[149] = idt_create_entry((u32) gate_149, SELECTOR_CODE_KERNEL, 0, 0);
    idt[150] = idt_create_entry((u32) gate_150, SELECTOR_CODE_KERNEL, 0, 0);
    idt[151] = idt_create_entry((u32) gate_151, SELECTOR_CODE_KERNEL, 0, 0);
    idt[152] = idt_create_entry((u32) gate_152, SELECTOR_CODE_KERNEL, 0, 0);
    idt[153] = idt_create_entry((u32) gate_153, SELECTOR_CODE_KERNEL, 0, 0);
    idt[154] = idt_create_entry((u32) gate_154, SELECTOR_CODE_KERNEL, 0, 0);
    idt[155] = idt_create_entry((u32) gate_155, SELECTOR_CODE_KERNEL, 0, 0);
    idt[156] = idt_create_entry((u32) gate_156, SELECTOR_CODE_KERNEL, 0, 0);
    idt[157] = idt_create_entry((u32) gate_157, SELECTOR_CODE_KERNEL, 0, 0);
    idt[158] = idt_create_entry((u32) gate_158, SELECTOR_CODE_KERNEL, 0, 0);
    idt[159] = idt_create_entry((u32) gate_159, SELECTOR_CODE_KERNEL, 0, 0);
    idt[160] = idt_create_entry((u32) gate_160, SELECTOR_CODE_KERNEL, 0, 0);
    idt[161] = idt_create_entry((u32) gate_161, SELECTOR_CODE_KERNEL, 0, 0);
    idt[162] = idt_create_entry((u32) gate_162, SELECTOR_CODE_KERNEL, 0, 0);
    idt[163] = idt_create_entry((u32) gate_163, SELECTOR_CODE_KERNEL, 0, 0);
    idt[164] = idt_create_entry((u32) gate_164, SELECTOR_CODE_KERNEL, 0, 0);
    idt[165] = idt_create_entry((u32) gate_165, SELECTOR_CODE_KERNEL, 0, 0);
    idt[166] = idt_create_entry((u32) gate_166, SELECTOR_CODE_KERNEL, 0, 0);
    idt[167] = idt_create_entry((u32) gate_167, SELECTOR_CODE_KERNEL, 0, 0);
    idt[168] = idt_create_entry((u32) gate_168, SELECTOR_CODE_KERNEL, 0, 0);
    idt[169] = idt_create_entry((u32) gate_169, SELECTOR_CODE_KERNEL, 0, 0);
    idt[170] = idt_create_entry((u32) gate_170, SELECTOR_CODE_KERNEL, 0, 0);
    idt[171] = idt_create_entry((u32) gate_171, SELECTOR_CODE_KERNEL, 0, 0);
    idt[172] = idt_create_entry((u32) gate_172, SELECTOR_CODE_KERNEL, 0, 0);
    idt[173] = idt_create_entry((u32) gate_173, SELECTOR_CODE_KERNEL, 0, 0);
    idt[174] = idt_create_entry((u32) gate_174, SELECTOR_CODE_KERNEL, 0, 0);
    idt[175] = idt_create_entry((u32) gate_175, SELECTOR_CODE_KERNEL, 0, 0);
    idt[176] = idt_create_entry((u32) gate_176, SELECTOR_CODE_KERNEL, 0, 0);
    idt[177] = idt_create_entry((u32) gate_177, SELECTOR_CODE_KERNEL, 0, 0);
    idt[178] = idt_create_entry((u32) gate_178, SELECTOR_CODE_KERNEL, 0, 0);
    idt[179] = idt_create_entry((u32) gate_179, SELECTOR_CODE_KERNEL, 0, 0);
    idt[180] = idt_create_entry((u32) gate_180, SELECTOR_CODE_KERNEL, 0, 0);
    idt[181] = idt_create_entry((u32) gate_181, SELECTOR_CODE_KERNEL, 0, 0);
    idt[182] = idt_create_entry((u32) gate_182, SELECTOR_CODE_KERNEL, 0, 0);
    idt[183] = idt_create_entry((u32) gate_183, SELECTOR_CODE_KERNEL, 0, 0);
    idt[184] = idt_create_entry((u32) gate_184, SELECTOR_CODE_KERNEL, 0, 0);
    idt[185] = idt_create_entry((u32) gate_185, SELECTOR_CODE_KERNEL, 0, 0);
    idt[186] = idt_create_entry((u32) gate_186, SELECTOR_CODE_KERNEL, 0, 0);
    idt[187] = idt_create_entry((u32) gate_187, SELECTOR_CODE_KERNEL, 0, 0);
    idt[188] = idt_create_entry((u32) gate_188, SELECTOR_CODE_KERNEL, 0, 0);
    idt[189] = idt_create_entry((u32) gate_189, SELECTOR_CODE_KERNEL, 0, 0);
    idt[190] = idt_create_entry((u32) gate_190, SELECTOR_CODE_KERNEL, 0, 0);
    idt[191] = idt_create_entry((u32) gate_191, SELECTOR_CODE_KERNEL, 0, 0);
    idt[192] = idt_create_entry((u32) gate_192, SELECTOR_CODE_KERNEL, 0, 0);
    idt[193] = idt_create_entry((u32) gate_193, SELECTOR_CODE_KERNEL, 0, 0);
    idt[194] = idt_create_entry((u32) gate_194, SELECTOR_CODE_KERNEL, 0, 0);
    idt[195] = idt_create_entry((u32) gate_195, SELECTOR_CODE_KERNEL, 0, 0);
    idt[196] = idt_create_entry((u32) gate_196, SELECTOR_CODE_KERNEL, 0, 0);
    idt[197] = idt_create_entry((u32) gate_197, SELECTOR_CODE_KERNEL, 0, 0);
    idt[198] = idt_create_entry((u32) gate_198, SELECTOR_CODE_KERNEL, 0, 0);
    idt[199] = idt_create_entry((u32) gate_199, SELECTOR_CODE_KERNEL, 0, 0);
    idt[200] = idt_create_entry((u32) gate_200, SELECTOR_CODE_KERNEL, 0, 0);
    idt[201] = idt_create_entry((u32) gate_201, SELECTOR_CODE_KERNEL, 0, 0);
    idt[202] = idt_create_entry((u32) gate_202, SELECTOR_CODE_KERNEL, 0, 0);
    idt[203] = idt_create_entry((u32) gate_203, SELECTOR_CODE_KERNEL, 0, 0);
    idt[204] = idt_create_entry((u32) gate_204, SELECTOR_CODE_KERNEL, 0, 0);
    idt[205] = idt_create_entry((u32) gate_205, SELECTOR_CODE_KERNEL, 0, 0);
    idt[206] = idt_create_entry((u32) gate_206, SELECTOR_CODE_KERNEL, 0, 0);
    idt[207] = idt_create_entry((u32) gate_207, SELECTOR_CODE_KERNEL, 0, 0);
    idt[208] = idt_create_entry((u32) gate_208, SELECTOR_CODE_KERNEL, 0, 0);
    idt[209] = idt_create_entry((u32) gate_209, SELECTOR_CODE_KERNEL, 0, 0);
    idt[210] = idt_create_entry((u32) gate_210, SELECTOR_CODE_KERNEL, 0, 0);
    idt[211] = idt_create_entry((u32) gate_211, SELECTOR_CODE_KERNEL, 0, 0);
    idt[212] = idt_create_entry((u32) gate_212, SELECTOR_CODE_KERNEL, 0, 0);
    idt[213] = idt_create_entry((u32) gate_213, SELECTOR_CODE_KERNEL, 0, 0);
    idt[214] = idt_create_entry((u32) gate_214, SELECTOR_CODE_KERNEL, 0, 0);
    idt[215] = idt_create_entry((u32) gate_215, SELECTOR_CODE_KERNEL, 0, 0);
    idt[216] = idt_create_entry((u32) gate_216, SELECTOR_CODE_KERNEL, 0, 0);
    idt[217] = idt_create_entry((u32) gate_217, SELECTOR_CODE_KERNEL, 0, 0);
    idt[218] = idt_create_entry((u32) gate_218, SELECTOR_CODE_KERNEL, 0, 0);
    idt[219] = idt_create_entry((u32) gate_219, SELECTOR_CODE_KERNEL, 0, 0);
    idt[220] = idt_create_entry((u32) gate_220, SELECTOR_CODE_KERNEL, 0, 0);
    idt[221] = idt_create_entry((u32) gate_221, SELECTOR_CODE_KERNEL, 0, 0);
    idt[222] = idt_create_entry((u32) gate_222, SELECTOR_CODE_KERNEL, 0, 0);
    idt[223] = idt_create_entry((u32) gate_223, SELECTOR_CODE_KERNEL, 0, 0);
    idt[224] = idt_create_entry((u32) gate_224, SELECTOR_CODE_KERNEL, 0, 0);
    idt[225] = idt_create_entry((u32) gate_225, SELECTOR_CODE_KERNEL, 0, 0);
    idt[226] = idt_create_entry((u32) gate_226, SELECTOR_CODE_KERNEL, 0, 0);
    idt[227] = idt_create_entry((u32) gate_227, SELECTOR_CODE_KERNEL, 0, 0);
    idt[228] = idt_create_entry((u32) gate_228, SELECTOR_CODE_KERNEL, 0, 0);
    idt[229] = idt_create_entry((u32) gate_229, SELECTOR_CODE_KERNEL, 0, 0);
    idt[230] = idt_create_entry((u32) gate_230, SELECTOR_CODE_KERNEL, 0, 0);
    idt[231] = idt_create_entry((u32) gate_231, SELECTOR_CODE_KERNEL, 0, 0);
    idt[232] = idt_create_entry((u32) gate_232, SELECTOR_CODE_KERNEL, 0, 0);
    idt[233] = idt_create_entry((u32) gate_233, SELECTOR_CODE_KERNEL, 0, 0);
    idt[234] = idt_create_entry((u32) gate_234, SELECTOR_CODE_KERNEL, 0, 0);
    idt[235] = idt_create_entry((u32) gate_235, SELECTOR_CODE_KERNEL, 0, 0);
    idt[236] = idt_create_entry((u32) gate_236, SELECTOR_CODE_KERNEL, 0, 0);
    idt[237] = idt_create_entry((u32) gate_237, SELECTOR_CODE_KERNEL, 0, 0);
    idt[238] = idt_create_entry((u32) gate_238, SELECTOR_CODE_KERNEL, 0, 0);
    idt[239] = idt_create_entry((u32) gate_239, SELECTOR_CODE_KERNEL, 0, 0);
    idt[240] = idt_create_entry((u32) gate_240, SELECTOR_CODE_KERNEL, 0, 0);
    idt[241] = idt_create_entry((u32) gate_241, SELECTOR_CODE_KERNEL, 0, 0);
    idt[242] = idt_create_entry((u32) gate_242, SELECTOR_CODE_KERNEL, 0, 0);
    idt[243] = idt_create_entry((u32) gate_243, SELECTOR_CODE_KERNEL, 0, 0);
    idt[244] = idt_create_entry((u32) gate_244, SELECTOR_CODE_KERNEL, 0, 0);
    idt[245] = idt_create_entry((u32) gate_245, SELECTOR_CODE_KERNEL, 0, 0);
    idt[246] = idt_create_entry((u32) gate_246, SELECTOR_CODE_KERNEL, 0, 0);
    idt[247] = idt_create_entry((u32) gate_247, SELECTOR_CODE_KERNEL, 0, 0);
    idt[248] = idt_create_entry((u32) gate_248, SELECTOR_CODE_KERNEL, 0, 0);
    idt[249] = idt_create_entry((u32) gate_249, SELECTOR_CODE_KERNEL, 0, 0);
    idt[250] = idt_create_entry((u32) gate_250, SELECTOR_CODE_KERNEL, 0, 0);
    idt[251] = idt_create_entry((u32) gate_251, SELECTOR_CODE_KERNEL, 0, 0);
    idt[252] = idt_create_entry((u32) gate_252, SELECTOR_CODE_KERNEL, 0, 0);
    idt[253] = idt_create_entry((u32) gate_253, SELECTOR_CODE_KERNEL, 0, 0);
    idt[254] = idt_create_entry((u32) gate_254, SELECTOR_CODE_KERNEL, 0, 0);
    idt[255] = idt_create_entry((u32) gate_255, SELECTOR_CODE_KERNEL, 0, 0);
    /* For systemcall interface IRQ, use dpl = 3  */
    idt[128] = idt_create_entry((u32) gate_128, SELECTOR_CODE_KERNEL, 0, 3);
    /* For the IDT, base + limit is the last byte of the table, so
     * the limit does not contain the size as for the GDT but
     * the size -1
     */
    idt_ptr.limit = sizeof(idt_entry_t) * 256 - 1;
    idt_ptr.base = (u32) (idt);
    return (u32) &idt_ptr;
}

/*
 * Get address of IDT pointer. Only call this if idt_create_table
 * has been called before.
 */
u32 idt_get_table() {
    return (u32) &idt_ptr;
}
