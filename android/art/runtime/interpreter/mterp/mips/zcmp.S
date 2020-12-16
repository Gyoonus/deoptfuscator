    /*
     * Generic one-operand compare-and-branch operation.  Provide a "condition"
     * fragment that specifies the comparison to perform.
     *
     * for: if-eqz, if-nez, if-ltz, if-gez, if-gtz, if-lez
     */
    /* if-cmp vAA, +BBBB */
    GET_OPA(a0)                            #  a0 <- AA
    GET_VREG(a0, a0)                       #  a0 <- vAA
    FETCH_S(rINST, 1)                      #  rINST <- branch offset, in code units
    b${condition} a0, zero, MterpCommonTakenBranchNoFlags
    li        t0, JIT_CHECK_OSR            # possible OSR re-entry?
    beq       rPROFILE, t0, .L_check_not_taken_osr
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction
