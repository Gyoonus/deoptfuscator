    /*
     * Generic one-operand compare-and-branch operation.  Provide a "condition"
     * fragment that specifies the comparison to perform, e.g. for
     * "if-lez" you would use "le".
     *
     * For: if-eqz, if-nez, if-ltz, if-gez, if-gtz, if-lez
     */
    /* if-cmp vAA, +BBBB */
    srl     a2, rINST, 8                # a2 <- AA
    lh      rINST, 2(rPC)               # rINST <- offset (sign-extended BBBB)
    GET_VREG a0, a2                     # a0 <- vAA
    b${condition}zc a0, MterpCommonTakenBranchNoFlags
    li      v0, JIT_CHECK_OSR           # possible OSR re-entry?
    beqc    rPROFILE, v0, .L_check_not_taken_osr
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction
