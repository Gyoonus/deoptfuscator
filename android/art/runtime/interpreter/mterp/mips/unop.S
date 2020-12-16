%default {"preinstr":"", "result0":"a0"}
    /*
     * Generic 32-bit unary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result0 = op a0".
     * This could be a MIPS instruction or a function call.
     *
     * for: int-to-byte, int-to-char, int-to-short,
     *      neg-int, not-int, neg-float
     */
    /* unop vA, vB */
    GET_OPB(a3)                            #  a3 <- B
    GET_OPA4(t0)                           #  t0 <- A+
    GET_VREG(a0, a3)                       #  a0 <- vB
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    $preinstr                              #  optional op
    $instr                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t1)                    #  extract opcode from rINST
    SET_VREG_GOTO($result0, t0, t1)        #  vA <- result0
