    /*
     * Generic 64-bit floating-point binary operation.  Provide an "instr"
     * line that specifies an instruction that performs "fv0 = fa0 op fa1".
     * This could be an MIPS instruction or a function call.
     *
     * for: add-double, sub-double, mul-double, div-double,
     *      rem-double
     *
     */
    /* binop vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    and       a2, a0, 255                  #  a2 <- BB
    srl       a3, a0, 8                    #  a3 <- CC
    EAS2(a2, rFP, a2)                      #  a2 <- &fp[BB]
    EAS2(t1, rFP, a3)                      #  a3 <- &fp[CC]
    LOAD64_F(fa0, fa0f, a2)
    LOAD64_F(fa1, fa1f, t1)

    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    $instr
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_F_GOTO(fv0, fv0f, rOBJ, t0)  #  vAA/vAA+1 <- fv0
