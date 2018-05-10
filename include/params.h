/*
 * params.h
 */

#ifndef _PARAMS_H_
#define _PARAMS_H_

#include "ktypes.h"

typedef struct _kparm_t {
    char* name;
    char* value;
    int length;
    char* default_string;
    u32 int_value;
} kparm_t;

void params_parse();
char* params_get(char* name);
u32 params_get_int(char* name);

#endif /* _PARAMS_H_ */
