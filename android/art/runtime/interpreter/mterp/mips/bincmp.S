    /*
     * Generic two-operand compare-and-branch operation.  Provide a "condition"
     * fragment that specifies the comparison to perform.
     *
     * For: if-eq, if-ne, if-lt, if-ge, if-gt, if-le
     */
    /* if-cmp vA, vB, +CCCC */
    GET_OPA4(a0)                           #  a0 <- A+
    GET_OPB(a1)                            #  a1 <- B
    GET_VREG(a3, a1)                       #  a3 <- vB
    GET_VREG(a0, a0)                       #  a0 <- vA
    FETCH_S(rINST, 1)                      #  rINST<- branch offset, in code units
    b${condition} a0, a3, MterpCommonTakenBranchNoFlags  #  compare (vA, vB)
    li        t0, JIT_CHECK_OSR
    beq       rPROFILE, t0, .L_check_not_taken_osr
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction
