%default {"load":"LOAD64_F(fa0, fa0f, a3)"}
    /*
     * Generic 64bit-to-32bit floating-point unary operation.  Provide an "instr"
     * line that specifies an instruction that performs "fv0 = op fa0".
     *
     * For: double-to-float
     */
    /* unop vA, vB */
    GET_OPB(a3)                            #  a3 <- B
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    EAS2(a3, rFP, a3)                      #  a3 <- &fp[B]
    $load
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    $instr
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_F_GOTO(fv0, rOBJ, t0)         #  vA <- fv0
