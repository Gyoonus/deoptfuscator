%default {"preinstr":"", "result0":"a0", "result1":"a1", "chkzero":"0", "arg0":"a0", "arg1":"a1", "arg2":"a2", "arg3":"a3"}
    /*
     * Generic 64-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0-a1 op a2-a3".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register pair other than a0-a1, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vB (a2-a3).  Useful for integer division and modulus.
     *
     * For: add-long/2addr, sub-long/2addr, div-long/2addr, rem-long/2addr,
     *      and-long/2addr, or-long/2addr, xor-long/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a1)                            #  a1 <- B
    EAS2(a1, rFP, a1)                      #  a1 <- &fp[B]
    EAS2(t0, rFP, rOBJ)                    #  t0 <- &fp[A]
    LOAD64($arg2, $arg3, a1)               #  a2/a3 <- vB/vB+1
    LOAD64($arg0, $arg1, t0)               #  a0/a1 <- vA/vA+1
    .if $chkzero
    or        t0, $arg2, $arg3             #  second arg (a2-a3) is zero?
    beqz      t0, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST

    $preinstr                              #  optional op
    $instr                                 #  result <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_GOTO($result0, $result1, rOBJ, t0)   #  vA/vA+1 <- $result0/$result1
