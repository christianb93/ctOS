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


double sin(double x) {
    return __ctOS_sin(x);
}

double tan(double x) {
    return __ctOS_tan(x);
}

double cosh(double x) {
    return __ctOS_cosh(x);
}

double sinh(double x) {
    return __ctOS_sinh(x);
}

double tanh(double x) {
    return __ctOS_tanh(x);
}

double sqrt(double x) {
    return __ctOS_sqrt(x);
}

double atan2(double y, double x) {
    return __ctOS_atan2(y, x);
}

double atan(double x) {
    return __ctOS_atan(x);
}

double asin(double x) {
    return __ctOS_asin(x);
}

double acos(double x) {
    return __ctOS_acos(x);
}

double pow(double x, double y) {
    return __ctOS_pow(x,y);
}

double modf(double x, double* iptr) {
    return __ctOS_modf(x, iptr);
}

double fabs(double x) {
    return __ctOS_fabs(x);
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