    /*
     * double-to-long
     *
     * We have to clip values to long min/max per the specification.  The
     * expected common case is a "reasonable" value that converts directly
     * to modest integer.  The EABI convert function isn't doing this for us
     * for pre-R6.
     */
    /* unop vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a3)                            #  a3 <- B
    EAS2(a3, rFP, a3)                      #  a3 <- &fp[B]
    LOAD64_F(fa0, fa0f, a3)
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST

#ifdef MIPS32REVGE6
    GET_INST_OPCODE(t1)                    #  extract opcode from rINST
    trunc.l.d fa0, fa0
    SET_VREG64_F_GOTO(fa0, fa0f, rOBJ, t1) #  vA <- result
#else
    c.eq.d    fcc0, fa0, fa0
    li        rRESULT0, 0
    li        rRESULT1, 0
    bc1f      fcc0, .L${opcode}_get_opcode

    li        t0, LONG_MIN_AS_DOUBLE_HIGH
    mtc1      zero, fa1
    MOVE_TO_FPU_HIGH(t0, fa1, fa1f)
    c.ole.d   fcc0, fa0, fa1
    li        rRESULT1, LONG_MIN_HIGH
    bc1t      fcc0, .L${opcode}_get_opcode

    neg.d     fa1, fa1
    c.ole.d   fcc0, fa1, fa0
    nor       rRESULT0, rRESULT0, zero
    nor       rRESULT1, rRESULT1, zero
    bc1t      fcc0, .L${opcode}_get_opcode

    JAL(__fixdfdi)
    GET_INST_OPCODE(t1)                    #  extract opcode from rINST
    b         .L${opcode}_set_vreg
#endif
%break

#ifndef MIPS32REVGE6
.L${opcode}_get_opcode:
    GET_INST_OPCODE(t1)                    #  extract opcode from rINST
.L${opcode}_set_vreg:
    SET_VREG64_GOTO(rRESULT0, rRESULT1, rOBJ, t1)   #  vA/vA+1 <- v0/v1
#endif
