    /*
     * Unconditional branch, 8-bit offset.
     *
     * The branch distance is a signed code-unit offset, which we need to
     * double to get a byte offset.
     */
    /* goto +AA */
    sll       a0, rINST, 16                #  a0 <- AAxx0000
    sra       rINST, a0, 24                #  rINST <- ssssssAA (sign-extended)
    b       MterpCommonTakenBranchNoFlags
