/*
 * getopt.c
 *
 * Implementation of the getopt command line parser
 *
 */


#include "lib/string.h"

int optind = 1;
char* optarg = 0;
int optopt = 0;

/*
 * Getopt
 *
 * A call to getopt will locate the next argument in the array argv. If no more arguments
 * are found, -1 is returned
 *
 * The argument optstring is a list of valid option characters. One argv entry can contain an arbitrary
 * number of options preceeded by a hyphen, for instance
 *
 * ./program -abc
 *
 * specifies the options a, b and c. Options can also be specified separated by whitespaces, like
 *
 * ./program -a -b -c
 *
 * If an option in the optstring parameter is followed by a colon, this implies that the option accepts an argument. If
 * the argument is present, the global variable optarg will be made to point to the argument
 *
 * The getopt function has some internal state which is preserved across function calls. To reset this state,
 * set optind to 0 and call getopt. This will set optind to 1, reset the internal state and then continue
 * as if getopt had been called after program initialization
 *
 * BASED ON: POSIX 2004
 *
 * LIMTATIONS:
 *
 * 1) the opterr flag is not supported
 */
int getopt(int argc, char * const argv[], const char *optstring) {
    char* c;
    int match;
    int rc = -1;
    int found_arg = 0;
    /*
     * Position within current argument
     */
    static int pos = 0;
    /*
     * glibc allows for a complete reset by setting optind to zero. For
     * the sake of compatibility, we also do this
     */
    if (0 == optind) {
        pos = 0;
        optind = 1;
    }
    /*
     * Locate next character in argv[optind] which matches a character
     * in optstring
     */
    match = 0;
    while ((optind < argc) && (argv[optind][pos]) && (!match)) {
        /*
         * First character is supposed to be a -
         */
        if (0 == pos) {
            if ((argv[optind][pos] != '-'))
                return -1;
            if (0 == strcmp("--", argv[optind])) {
                optind++;
                return -1;
            }
            pos++;
        }
        /*
         * Is this an option character?
         */
        if ((c = strchr(optstring, argv[optind][pos])) != 0) {
            if (*c) {
                rc = optopt = *c;
                match = 1;
                /*
                 * Do we take an argument?
                 */
                if (*++c == ':') {
                    found_arg = 0;
                    /*
                     * Case 1: argument is in next argv entry
                     */
                    if (0 == argv[optind][pos+1]) {
                        if (optind + 1 >= argc)
                            return (optstring[0]==':') ? ':' : '?';
                        optarg = argv[optind+1];
                        found_arg = 1;
                    }
                    /*
                     * Case 2: argument is in same argv entry
                     */
                    else {
                        optarg = argv[optind]+pos+1;
                        pos += strlen(optarg);
                    }
                }
            }
        }
        else {
            return '?';
        }
        /*
         * If we have reached the end of the argument, update optind
         * and reset position. Note that if we have parsed an argument
         * in a separate argv entry, we need to increase optind by two
         */
        if (0 == argv[optind][++pos]) {
            optind++;
            if (found_arg) {
                optind++;
                found_arg = 0;
            }
            pos=0;
        }
    }
    return rc;
}
