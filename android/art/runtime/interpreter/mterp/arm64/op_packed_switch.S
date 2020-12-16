%default { "func":"MterpDoPackedSwitch" }
    /*
     * Handle a packed-switch or sparse-switch instruction.  In both cases
     * we decode it and hand it off to a helper function.
     *
     * We don't really expect backward branches in a switch statement, but
     * they're perfectly legal, so we check for them here.
     *
     * for: packed-switch, sparse-switch
     */
    /* op vAA, +BBBB */
    FETCH   w0, 1                       // x0<- 000000000000bbbb (lo)
    FETCH_S x1, 2                       // x1<- ssssssssssssBBBB (hi)
    lsr     w3, wINST, #8               // w3<- AA
    orr     x0, x0, x1, lsl #16         // x0<- ssssssssBBBBbbbb
    GET_VREG w1, w3                     // w1<- vAA
    add     x0, xPC, x0, lsl #1         // x0<- PC + ssssssssBBBBbbbb*2
    bl      $func                       // w0<- code-unit branch offset
    sxtw    xINST, w0
    b       MterpCommonTakenBranchNoFlags
