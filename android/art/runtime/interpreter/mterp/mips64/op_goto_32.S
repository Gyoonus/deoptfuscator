    /*
     * Unconditional branch, 32-bit offset.
     *
     * The branch distance is a signed code-unit offset, which we need to
     * double to get a byte offset.
     *
     * Unlike most opcodes, this one is allowed to branch to itself, so
     * our "backward branch" test must be "<=0" instead of "<0".
     */
    /* goto/32 +AAAAAAAA */
    lh      rINST, 2(rPC)               # rINST <- aaaa (low)
    lh      a1, 4(rPC)                  # a1 <- AAAA (high)
    ins     rINST, a1, 16, 16           # rINST <- offset (sign-extended AAAAaaaa)
    b       MterpCommonTakenBranchNoFlags
