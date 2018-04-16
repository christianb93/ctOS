/*
 * pthread.h
 *
 *  Created on: Aug 7, 2011
 *      Author: chr
 */

#ifndef _PTHREAD_H_
#define _PTHREAD_H_

typedef unsigned int pthread_t;
typedef struct {
    int cpuid;
    int priority;
} pthread_attr_t;
typedef void* (*pthread_start_routine)(void*);

void* pthread_create(pthread_t* thread, pthread_attr_t* attr,
        void* (*start_routine)(void*), void* arg);

#endif /* _PTHREAD_H_ */
