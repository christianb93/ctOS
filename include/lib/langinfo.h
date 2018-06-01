#ifndef _LANGINFO_H
#define _LANGINFO_H

#include "nl_types.h"

#define CODESET     100

#define D_T_FMT     201
#define D_FMT       202
#define T_FMT       203
#define T_FMT_AMPM  204
#define AM_STR      205
#define PM_STR      206

/*
 * POSIX does not specify that these
 * constants form a consecutive range 
 * but I have seen code (for instance in GNU coretutils)
 * assuming that, so let us do it
 */
#define DAY_1       300
#define DAY_2       301
#define DAY_3       302
#define DAY_4       303
#define DAY_5       304
#define DAY_6       305
#define DAY_7       306

#define ABDAY_1     350
#define ABDAY_2     351
#define ABDAY_3     352
#define ABDAY_4     353
#define ABDAY_5     354
#define ABDAY_6     355
#define ABDAY_7     356

#define MON_1       400
#define MON_2       401
#define MON_3       402
#define MON_4       403
#define MON_5       404
#define MON_6       405
#define MON_7       406
#define MON_8       407
#define MON_9       408
#define MON_10      409
#define MON_11      410
#define MON_12      411

#define ABMON_1     450
#define ABMON_2     451
#define ABMON_3     452
#define ABMON_4     453
#define ABMON_5     454
#define ABMON_6     455
#define ABMON_7     456
#define ABMON_8     457
#define ABMON_9     458
#define ABMON_10    459
#define ABMON_11    460
#define ABMON_12    461

#define ERA         500
#define ERA_D_FMT   501
#define ERA_D_T_FMT 502
#define ERA_T_FMT   503

#define ALT_DIGITS  504
#define RADIXCHAR   505
#define THOUSEP     506
#define YESEXPR     507
#define NOEXPR      508
#define CRNCYSTR    509



char *nl_langinfo(nl_item);

#endif