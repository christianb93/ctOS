/*
 * tan.S
 *
 */

.global __ctOS_tan_kernel


/*******************************************
 * Return tan of argument                  *
 # Only call this if x is in [-2^63, 2^63] *
 *******************************************/
 __ctOS_tan_kernel:
    #
    # Get argument (IEEE double, i.e. stored in 
    # four bytes) from stack
    # and push it onto the FPU stack
    #
    fldl 4(%esp)
    #
    # Calculate the tan. Note that this operation will
    # also push 1.0, so that after the operation has
    # completed, st(1) is the tan, st(0) is 1.0
    #
    fptan 
    #
    # Multiply and pop so that st(1) becomes st(0)
    #
    fmulp
    ret