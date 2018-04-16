/*
 * This module holds ASM entry points
 * for all interrupts
 * Convention:
 * entry point for int x is called
 * gate_x
 */

#include "gdt_const.h"


.global gate_0
.global gate_1
.global gate_2
.global gate_3
.global gate_4
.global gate_5
.global gate_6
.global gate_7
.global gate_8
.global gate_9
.global gate_10
.global gate_11
.global gate_12
.global gate_13
.global gate_14
.global gate_15
.global gate_16
.global gate_17
.global gate_18
.global gate_19
.global gate_20
.global gate_21
.global gate_22
.global gate_23
.global gate_24
.global gate_25
.global gate_26
.global gate_27
.global gate_28
.global gate_29
.global gate_30
.global gate_31
.global gate_32
.global gate_33
.global gate_34
.global gate_35
.global gate_36
.global gate_37
.global gate_38
.global gate_39
.global gate_40
.global gate_41
.global gate_42
.global gate_43
.global gate_44
.global gate_45
.global gate_46
.global gate_47
.global gate_48
.global gate_49
.global gate_50
.global gate_51
.global gate_52
.global gate_53
.global gate_54
.global gate_55
.global gate_56
.global gate_57
.global gate_58
.global gate_59
.global gate_60
.global gate_61
.global gate_62
.global gate_63
.global gate_64
.global gate_65
.global gate_66
.global gate_67
.global gate_68
.global gate_69
.global gate_70
.global gate_71
.global gate_72
.global gate_73
.global gate_74
.global gate_75
.global gate_76
.global gate_77
.global gate_78
.global gate_79
.global gate_80
.global gate_81
.global gate_82
.global gate_83
.global gate_84
.global gate_85
.global gate_86
.global gate_87
.global gate_88
.global gate_89
.global gate_90
.global gate_91
.global gate_92
.global gate_93
.global gate_94
.global gate_95
.global gate_96
.global gate_97
.global gate_98
.global gate_99
.global gate_100
.global gate_101
.global gate_102
.global gate_103
.global gate_104
.global gate_105
.global gate_106
.global gate_107
.global gate_108
.global gate_109
.global gate_110
.global gate_111
.global gate_112
.global gate_113
.global gate_114
.global gate_115
.global gate_116
.global gate_117
.global gate_118
.global gate_119
.global gate_120
.global gate_121
.global gate_122
.global gate_123
.global gate_124
.global gate_125
.global gate_126
.global gate_127
.global gate_128
.global gate_129
.global gate_130
.global gate_131
.global gate_132
.global gate_133
.global gate_134
.global gate_135
.global gate_136
.global gate_137
.global gate_138
.global gate_139
.global gate_140
.global gate_141
.global gate_142
.global gate_143
.global gate_144
.global gate_145
.global gate_146
.global gate_147
.global gate_148
.global gate_149
.global gate_150
.global gate_151
.global gate_152
.global gate_153
.global gate_154
.global gate_155
.global gate_156
.global gate_157
.global gate_158
.global gate_159
.global gate_160
.global gate_161
.global gate_162
.global gate_163
.global gate_164
.global gate_165
.global gate_166
.global gate_167
.global gate_168
.global gate_169
.global gate_170
.global gate_171
.global gate_172
.global gate_173
.global gate_174
.global gate_175
.global gate_176
.global gate_177
.global gate_178
.global gate_179
.global gate_180
.global gate_181
.global gate_182
.global gate_183
.global gate_184
.global gate_185
.global gate_186
.global gate_187
.global gate_188
.global gate_189


.global gate_190
.global gate_191
.global gate_192
.global gate_193
.global gate_194
.global gate_195
.global gate_196
.global gate_197
.global gate_198
.global gate_199
.global gate_200
.global gate_201
.global gate_202
.global gate_203
.global gate_204
.global gate_205
.global gate_206
.global gate_207
.global gate_208
.global gate_209
.global gate_210
.global gate_211
.global gate_212
.global gate_213
.global gate_214
.global gate_215
.global gate_216
.global gate_217
.global gate_218
.global gate_219
.global gate_220
.global gate_221
.global gate_222
.global gate_223
.global gate_224
.global gate_225
.global gate_226
.global gate_227
.global gate_228
.global gate_229
.global gate_230
.global gate_231
.global gate_232
.global gate_233
.global gate_234
.global gate_235
.global gate_236
.global gate_237
.global gate_238
.global gate_239
.global gate_240
.global gate_241
.global gate_242
.global gate_243
.global gate_244
.global gate_245
.global gate_246
.global gate_247
.global gate_248
.global gate_249
.global gate_250
.global gate_251
.global gate_252
.global gate_253
.global gate_254
.global gate_255


gate_0:
	# push dummy error code
	push $DUMMY_ERROR_CODE
	# push interrupt number
	push $0
	# jump to common entry point
	jmp gate_common

gate_1:
	# push dummy error code
	push $DUMMY_ERROR_CODE
	# push interrupt number
	push $1
	# jump to common entry point
	jmp gate_common

gate_2:
	# push dummy error code
	push $DUMMY_ERROR_CODE
	# push interrupt number
	push $2
	# jump to common entry point
	jmp gate_common

gate_3:
	# push dummy error code
	push $DUMMY_ERROR_CODE
	# push interrupt number
	push $3
	# jump to common entry point
	jmp gate_common



gate_4:
	# push dummy error code
	push $DUMMY_ERROR_CODE
	# push interrupt number
	push $4
	# jump to common entry point
	jmp gate_common


gate_5:
	# push dummy error code
	push $DUMMY_ERROR_CODE
	# push interrupt number
	push $5
	# jump to common entry point
	jmp gate_common

gate_6:
	# push dummy error code
	push $DUMMY_ERROR_CODE
	# push interrupt number
	push $6
	# jump to common entry point
	jmp gate_common


gate_7:
	# push dummy error code
	push $DUMMY_ERROR_CODE
	# push interrupt number
	push $7
	# jump to common entry point
	jmp gate_common

gate_8:
	# no need to push error code,  CPU will do that
	# push interrupt number
	push $8
	# jump to common entry point
	jmp gate_common


gate_9:
	push $DUMMY_ERROR_CODE
	# push interrupt number
	push $9
	# jump to common entry point
	jmp gate_common


gate_10:
	# no need to push error code,  CPU will do that
	# push interrupt number
	push $10
	# jump to common entry point
	jmp gate_common

gate_11:
	# no need to push error code,  CPU will do that
	# push interrupt number
	push $11
	# jump to common entry point
	jmp gate_common

gate_12:
	# no need to push error code,  CPU will do that
	# push interrupt number
	push $12
	# jump to common entry point
	jmp gate_common

gate_13:
	# no need to push error code,  CPU will do that
	# push interrupt number
	push $13
	# jump to common entry point
	jmp gate_common

gate_14:
	# no need to push error code,  CPU will do that
	# push interrupt number
	push $14
	# jump to common entry point
	jmp gate_common

gate_15:
	push $DUMMY_ERROR_CODE
	# push interrupt number
	push $15
	# jump to common entry point
	jmp gate_common

gate_16:
	push $DUMMY_ERROR_CODE
	# push interrupt number
	push $16
	# jump to common entry point
	jmp gate_common

gate_17:
	# no need to push error code,  CPU will do that
	# push interrupt number
	push $17
	# jump to common entry point
	jmp gate_common

gate_32:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $32
    # jump to common entry point
    jmp gate_common

gate_33:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $33
    # jump to common entry point
    jmp gate_common

gate_34:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $34
    # jump to common entry point
    jmp gate_common

gate_35:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $35
    # jump to common entry point
    jmp gate_common

gate_36:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $36
    # jump to common entry point
    jmp gate_common

gate_37:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $37
    # jump to common entry point
    jmp gate_common

gate_38:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $38
    # jump to common entry point
    jmp gate_common

gate_39:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $39
    # jump to common entry point
    jmp gate_common

gate_40:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $40
    # jump to common entry point
    jmp gate_common

gate_41:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $41
    # jump to common entry point
    jmp gate_common

gate_42:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $42
    # jump to common entry point
    jmp gate_common

gate_43:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $43
    # jump to common entry point
    jmp gate_common

gate_44:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $44
    # jump to common entry point
    jmp gate_common

gate_45:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $45
    # jump to common entry point
    jmp gate_common

gate_46:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $46
    # jump to common entry point
    jmp gate_common

gate_47:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $47
    # jump to common entry point
    jmp gate_common

gate_48:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $48
    # jump to common entry point
    jmp gate_common

gate_49:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $49
    # jump to common entry point
    jmp gate_common

gate_50:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $50
    # jump to common entry point
    jmp gate_common

gate_51:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $51
    # jump to common entry point
    jmp gate_common

gate_52:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $52
    # jump to common entry point
    jmp gate_common

gate_53:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $53
    # jump to common entry point
    jmp gate_common

gate_54:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $54
    # jump to common entry point
    jmp gate_common

gate_55:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $55
    # jump to common entry point
    jmp gate_common

gate_56:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $56
    # jump to common entry point
    jmp gate_common

gate_57:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $57
    # jump to common entry point
    jmp gate_common

gate_58:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $58
    # jump to common entry point
    jmp gate_common

gate_59:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $59
    # jump to common entry point
    jmp gate_common

gate_60:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $60
    # jump to common entry point
    jmp gate_common

gate_61:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $61
    # jump to common entry point
    jmp gate_common

gate_62:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $62
    # jump to common entry point
    jmp gate_common

gate_63:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $63
    # jump to common entry point
    jmp gate_common

gate_64:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $64
    # jump to common entry point
    jmp gate_common

gate_65:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $65
    # jump to common entry point
    jmp gate_common

gate_66:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $66
    # jump to common entry point
    jmp gate_common

gate_67:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $67
    # jump to common entry point
    jmp gate_common

gate_68:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $68
    # jump to common entry point
    jmp gate_common

gate_69:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $69
    # jump to common entry point
    jmp gate_common

gate_70:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $70
    # jump to common entry point
    jmp gate_common

gate_71:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $71
    # jump to common entry point
    jmp gate_common

gate_72:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $72
    # jump to common entry point
    jmp gate_common

gate_73:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $73
    # jump to common entry point
    jmp gate_common

gate_74:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $74
    # jump to common entry point
    jmp gate_common

gate_75:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $75
    # jump to common entry point
    jmp gate_common

gate_76:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $76
    # jump to common entry point
    jmp gate_common

gate_77:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $77
    # jump to common entry point
    jmp gate_common

gate_78:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $78
    # jump to common entry point
    jmp gate_common

gate_79:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $79
    # jump to common entry point
    jmp gate_common

gate_80:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $80
    # jump to common entry point
    jmp gate_common

gate_81:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $81
    # jump to common entry point
    jmp gate_common

gate_82:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $82
    # jump to common entry point
    jmp gate_common

gate_83:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $83
    # jump to common entry point
    jmp gate_common

gate_84:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $84
    # jump to common entry point
    jmp gate_common

gate_85:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $85
    # jump to common entry point
    jmp gate_common

gate_86:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $86
    # jump to common entry point
    jmp gate_common

gate_87:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $87
    # jump to common entry point
    jmp gate_common

gate_88:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $88
    # jump to common entry point
    jmp gate_common

gate_89:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $89
    # jump to common entry point
    jmp gate_common

gate_90:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $90
    # jump to common entry point
    jmp gate_common

gate_91:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $91
    # jump to common entry point
    jmp gate_common

gate_92:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $92
    # jump to common entry point
    jmp gate_common

gate_93:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $93
    # jump to common entry point
    jmp gate_common

gate_94:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $94
    # jump to common entry point
    jmp gate_common

gate_95:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $95
    # jump to common entry point
    jmp gate_common

gate_96:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $96
    # jump to common entry point
    jmp gate_common

gate_97:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $97
    # jump to common entry point
    jmp gate_common

gate_98:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $98
    # jump to common entry point
    jmp gate_common

gate_99:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $99
    # jump to common entry point
    jmp gate_common

gate_100:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $100
    # jump to common entry point
    jmp gate_common

gate_101:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $101
    # jump to common entry point
    jmp gate_common

gate_102:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $102
    # jump to common entry point
    jmp gate_common

gate_103:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $103
    # jump to common entry point
    jmp gate_common

gate_104:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $104
    # jump to common entry point
    jmp gate_common

gate_105:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $105
    # jump to common entry point
    jmp gate_common

gate_106:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $106
    # jump to common entry point
    jmp gate_common

gate_107:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $107
    # jump to common entry point
    jmp gate_common

gate_108:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $108
    # jump to common entry point
    jmp gate_common

gate_109:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $109
    # jump to common entry point
    jmp gate_common

gate_110:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $110
    # jump to common entry point
    jmp gate_common

gate_111:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $111
    # jump to common entry point
    jmp gate_common

gate_112:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $112
    # jump to common entry point
    jmp gate_common

gate_113:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $113
    # jump to common entry point
    jmp gate_common

gate_114:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $114
    # jump to common entry point
    jmp gate_common

gate_115:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $115
    # jump to common entry point
    jmp gate_common

gate_116:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $116
    # jump to common entry point
    jmp gate_common

gate_117:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $117
    # jump to common entry point
    jmp gate_common

gate_118:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $118
    # jump to common entry point
    jmp gate_common

gate_119:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $119
    # jump to common entry point
    jmp gate_common

gate_120:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $120
    # jump to common entry point
    jmp gate_common

gate_121:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $121
    # jump to common entry point
    jmp gate_common

gate_122:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $122
    # jump to common entry point
    jmp gate_common

gate_123:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $123
    # jump to common entry point
    jmp gate_common

gate_124:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $124
    # jump to common entry point
    jmp gate_common

gate_125:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $125
    # jump to common entry point
    jmp gate_common

gate_126:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $126
    # jump to common entry point
    jmp gate_common

gate_127:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $127
    # jump to common entry point
    jmp gate_common

gate_128:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $128
    # jump to common entry point
    jmp gate_common

gate_129:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $129
    # jump to common entry point
    jmp gate_common

gate_130:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $130
    # jump to common entry point
    jmp gate_common

gate_131:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $131
    # jump to common entry point
    jmp gate_common

gate_132:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $132
    # jump to common entry point
    jmp gate_common

gate_133:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $133
    # jump to common entry point
    jmp gate_common

gate_134:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $134
    # jump to common entry point
    jmp gate_common

gate_135:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $135
    # jump to common entry point
    jmp gate_common

gate_136:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $136
    # jump to common entry point
    jmp gate_common

gate_137:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $137
    # jump to common entry point
    jmp gate_common

gate_138:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $138
    # jump to common entry point
    jmp gate_common

gate_139:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $139
    # jump to common entry point
    jmp gate_common

gate_140:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $140
    # jump to common entry point
    jmp gate_common

gate_141:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $141
    # jump to common entry point
    jmp gate_common

gate_142:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $142
    # jump to common entry point
    jmp gate_common

gate_143:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $143
    # jump to common entry point
    jmp gate_common

gate_144:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $144
    # jump to common entry point
    jmp gate_common

gate_145:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $145
    # jump to common entry point
    jmp gate_common

gate_146:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $146
    # jump to common entry point
    jmp gate_common

gate_147:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $147
    # jump to common entry point
    jmp gate_common

gate_148:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $148
    # jump to common entry point
    jmp gate_common

gate_149:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $149
    # jump to common entry point
    jmp gate_common

gate_150:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $150
    # jump to common entry point
    jmp gate_common

gate_151:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $151
    # jump to common entry point
    jmp gate_common

gate_152:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $152
    # jump to common entry point
    jmp gate_common

gate_153:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $153
    # jump to common entry point
    jmp gate_common

gate_154:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $154
    # jump to common entry point
    jmp gate_common

gate_155:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $155
    # jump to common entry point
    jmp gate_common

gate_156:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $156
    # jump to common entry point
    jmp gate_common

gate_157:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $157
    # jump to common entry point
    jmp gate_common

gate_158:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $158
    # jump to common entry point
    jmp gate_common

gate_159:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $159
    # jump to common entry point
    jmp gate_common

gate_160:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $160
    # jump to common entry point
    jmp gate_common

gate_161:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $161
    # jump to common entry point
    jmp gate_common

gate_162:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $162
    # jump to common entry point
    jmp gate_common

gate_163:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $163
    # jump to common entry point
    jmp gate_common

gate_164:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $164
    # jump to common entry point
    jmp gate_common

gate_165:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $165
    # jump to common entry point
    jmp gate_common

gate_166:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $166
    # jump to common entry point
    jmp gate_common

gate_167:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $167
    # jump to common entry point
    jmp gate_common

gate_168:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $168
    # jump to common entry point
    jmp gate_common

gate_169:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $169
    # jump to common entry point
    jmp gate_common

gate_170:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $170
    # jump to common entry point
    jmp gate_common

gate_171:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $171
    # jump to common entry point
    jmp gate_common

gate_172:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $172
    # jump to common entry point
    jmp gate_common

gate_173:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $173
    # jump to common entry point
    jmp gate_common

gate_174:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $174
    # jump to common entry point
    jmp gate_common

gate_175:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $175
    # jump to common entry point
    jmp gate_common

gate_176:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $176
    # jump to common entry point
    jmp gate_common

gate_177:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $177
    # jump to common entry point
    jmp gate_common

gate_178:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $178
    # jump to common entry point
    jmp gate_common

gate_179:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $179
    # jump to common entry point
    jmp gate_common

gate_180:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $180
    # jump to common entry point
    jmp gate_common

gate_181:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $181
    # jump to common entry point
    jmp gate_common

gate_182:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $182
    # jump to common entry point
    jmp gate_common

gate_183:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $183
    # jump to common entry point
    jmp gate_common

gate_184:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $184
    # jump to common entry point
    jmp gate_common

gate_185:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $185
    # jump to common entry point
    jmp gate_common

gate_186:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $186
    # jump to common entry point
    jmp gate_common

gate_187:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $187
    # jump to common entry point
    jmp gate_common

gate_188:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $188
    # jump to common entry point
    jmp gate_common

gate_189:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $189
    # jump to common entry point
    jmp gate_common

gate_190:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $190
    # jump to common entry point
    jmp gate_common

gate_191:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $191
    # jump to common entry point
    jmp gate_common

gate_192:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $192
    # jump to common entry point
    jmp gate_common

gate_193:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $193
    # jump to common entry point
    jmp gate_common

gate_194:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $194
    # jump to common entry point
    jmp gate_common

gate_195:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $195
    # jump to common entry point
    jmp gate_common

gate_196:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $196
    # jump to common entry point
    jmp gate_common

gate_197:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $197
    # jump to common entry point
    jmp gate_common

gate_198:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $198
    # jump to common entry point
    jmp gate_common

gate_199:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $199
    # jump to common entry point
    jmp gate_common

gate_200:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $200
    # jump to common entry point
    jmp gate_common

gate_201:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $201
    # jump to common entry point
    jmp gate_common

gate_202:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $202
    # jump to common entry point
    jmp gate_common

gate_203:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $203
    # jump to common entry point
    jmp gate_common

gate_204:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $204
    # jump to common entry point
    jmp gate_common

gate_205:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $205
    # jump to common entry point
    jmp gate_common

gate_206:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $206
    # jump to common entry point
    jmp gate_common

gate_207:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $207
    # jump to common entry point
    jmp gate_common

gate_208:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $208
    # jump to common entry point
    jmp gate_common

gate_209:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $209
    # jump to common entry point
    jmp gate_common

gate_210:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $210
    # jump to common entry point
    jmp gate_common

gate_211:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $211
    # jump to common entry point
    jmp gate_common

gate_212:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $212
    # jump to common entry point
    jmp gate_common

gate_213:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $213
    # jump to common entry point
    jmp gate_common

gate_214:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $214
    # jump to common entry point
    jmp gate_common

gate_215:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $215
    # jump to common entry point
    jmp gate_common

gate_216:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $216
    # jump to common entry point
    jmp gate_common

gate_217:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $217
    # jump to common entry point
    jmp gate_common

gate_218:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $218
    # jump to common entry point
    jmp gate_common

gate_219:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $219
    # jump to common entry point
    jmp gate_common

gate_220:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $220
    # jump to common entry point
    jmp gate_common

gate_221:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $221
    # jump to common entry point
    jmp gate_common

gate_222:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $222
    # jump to common entry point
    jmp gate_common

gate_223:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $223
    # jump to common entry point
    jmp gate_common

gate_224:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $224
    # jump to common entry point
    jmp gate_common

gate_225:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $225
    # jump to common entry point
    jmp gate_common

gate_226:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $226
    # jump to common entry point
    jmp gate_common

gate_227:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $227
    # jump to common entry point
    jmp gate_common

gate_228:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $228
    # jump to common entry point
    jmp gate_common

gate_229:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $229
    # jump to common entry point
    jmp gate_common

gate_230:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $230
    # jump to common entry point
    jmp gate_common

gate_231:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $231
    # jump to common entry point
    jmp gate_common

gate_232:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $232
    # jump to common entry point
    jmp gate_common

gate_233:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $233
    # jump to common entry point
    jmp gate_common

gate_234:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $234
    # jump to common entry point
    jmp gate_common

gate_235:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $235
    # jump to common entry point
    jmp gate_common

gate_236:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $236
    # jump to common entry point
    jmp gate_common

gate_237:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $237
    # jump to common entry point
    jmp gate_common

gate_238:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $238
    # jump to common entry point
    jmp gate_common

gate_239:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $239
    # jump to common entry point
    jmp gate_common

gate_240:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $240
    # jump to common entry point
    jmp gate_common

gate_241:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $241
    # jump to common entry point
    jmp gate_common

gate_242:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $242
    # jump to common entry point
    jmp gate_common

gate_243:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $243
    # jump to common entry point
    jmp gate_common

gate_244:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $244
    # jump to common entry point
    jmp gate_common

gate_245:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $245
    # jump to common entry point
    jmp gate_common

gate_246:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $246
    # jump to common entry point
    jmp gate_common

gate_247:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $247
    # jump to common entry point
    jmp gate_common

gate_248:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $248
    # jump to common entry point
    jmp gate_common

gate_249:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $249
    # jump to common entry point
    jmp gate_common

gate_250:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $250
    # jump to common entry point
    jmp gate_common

gate_251:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $251
    # jump to common entry point
    jmp gate_common

gate_252:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $252
    # jump to common entry point
    jmp gate_common

gate_253:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $253
    # jump to common entry point
    jmp gate_common

gate_254:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $254
    # jump to common entry point
    jmp gate_common

gate_255:
    push $DUMMY_ERROR_CODE
    # push interrupt number
    push $255
    # jump to common entry point
    jmp gate_common




gate_common:

    push %eax
    push %ebx
    push %ecx
    push %edx
    push %esi
    push %edi
	push %ebp
	mov %ds, %eax
	push %eax
    mov %cr2, %eax
    push %eax


	mov %esp, %eax
	mov %cr3, %ebx
	push %eax
	push %ebx


	# call common C interrupt handler
	call irq_handle_interrupt

    # Get CR3 and ESP from stack
    pop %edi
    pop %esi
    # Jump over code for post handler if eax is 0
    test %eax, %eax
    jz 0f
    # Switch over to stack within the common area
    mov %eax, %esp
    # and save old values of ESP and CR3 there
    push %edi
    push %esi
    # Call post handler
    call irq_post
    # Switch back to address space and stack of active task
    pop %esi
    pop %edi
0:
    # Only write to CR3 if value has changed
    # to avoid invalidating the TLB without need
    mov %edi, %eax
    mov %cr3, %ebx
    cmp %eax, %ebx
    je 0f
    mov %edi, %cr3
0:
    mov %esi, %esp

	pop %eax
	mov %eax, %cr2
	pop %eax
	mov %ax, %ds
	pop %ebp
	pop %edi
	pop %esi
	pop %edx
	pop %ecx
	pop %ebx
	pop %eax


	# unwind stack, i.e. remove vector and error code
	add $8, %esp

	# return
	iret

