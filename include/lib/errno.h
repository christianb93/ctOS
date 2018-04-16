/*
 * errno.h
 *
 */

#ifndef __ERRNO_H_
#define __ERRNO_H_

#include "os/errors.h"

extern int* __errno_location();

#define errno (*__errno_location())

#endif /* __ERRNO_H_ */
