/*
 * exp.S
 *
 */

.global __ctOS_exp2_kernel


/*******************************************
 * Return base 2 exponential of argument   *
 * prototype:                              *
 * double exp2(double x)                   *
 # Only call this if x is in [-1,1]        *
 *******************************************/
 __ctOS_exp2_kernel:
    #
    # Get argument (IEEE double, i.e. stored in 
    # four bytes) from stack
    # and push it onto the FPU stack
    #
    fldl 4(%esp)
    #
    # Calculate 2**x - 1. The result will
    # be stored in st0 again
    #
    f2xm1 
    #
    # Push 1 onto the stack - this will
    # move the result of the previous calculation
    # into st1
    fld1
    #
    # And add st1 and st2, store in st1 and pop
    # so that st0 contains the result
    #
    faddp
check:  
    ret