    /*
     * Generic 32-bit floating-point unary operation.  Provide an "instr"
     * line that specifies an instruction that performs "fv0 = op fa0".
     * This could be a MIPS instruction or a function call.
     *
     * for: int-to-float
     */
    /* unop vA, vB */
    GET_OPB(a3)                            #  a3 <- B
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_VREG_F(fa0, a3)
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    $instr
    GET_INST_OPCODE(t1)                    #  extract opcode from rINST
    SET_VREG_F_GOTO(fv0, rOBJ, t1)         #  vA <- fv0
