    /*
     * Generic 32bit-to-64bit floating-point unary operation.  Provide an "instr"
     * line that specifies an instruction that performs "fv0 = op fa0".
     *
     * For: int-to-double, float-to-double
     */
    /* unop vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a3)                            #  a3 <- B
    GET_VREG_F(fa0, a3)
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    $instr
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_F_GOTO(fv0, fv0f, rOBJ, t0) #  vA/vA+1 <- fv0
