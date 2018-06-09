#include "lib/os/mathlib.h"
#include "lib/math.h"


/*
 * We simply delegate all functions to the corresponding
 * __ctOS* functions
 */
double floor(double x) {
    return __ctOS_floor(x);
}

double ceil(double x) {
    return __ctOS_ceil(x);
}

double exp2(double x) {
    return __ctOS_exp2(x);
}

double exp(double x) {
    return __ctOS_exp(x);
}

double log2(double x) {
    return __ctOS_log2(x);
}

double cos(double x) {
    return __ctOS_cos(x);
}

/*
 * We simply use
 * log(x) = log2(x) * log(2)
 * ignoring that there are probably better ways to do this
 */
double log(double x) {
    return 0.69314718056 * __ctOS_log2(x);
}

/*
 * We simply use
 * log10(x) = log2(x) * log10(2)
 */
double log10(double x) {
    return 0.301029995664 * __ctOS_log2(x);
}