/*
 * getopt.h
 *
 */

#ifndef _GETOPT_H_
#define _GETOPT_H_

extern int optind;
extern char* optarg;

int getopt(int argc, char * const argv[], const char *optstring);

#endif /* _GETOPT_H_ */
