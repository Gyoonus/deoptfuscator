    /*
     * Unconditional branch, 8-bit offset.
     *
     * The branch distance is a signed code-unit offset, which we need to
     * double to get a byte offset.
     */
    /* goto +AA */
    srl     rINST, rINST, 8
    seb     rINST, rINST                # rINST <- offset (sign-extended AA)
    b       MterpCommonTakenBranchNoFlags
