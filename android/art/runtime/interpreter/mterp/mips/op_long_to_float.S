    /*
     * long-to-float
     */
    /* unop vA, vB */
    GET_OPB(a3)                            #  a3 <- B
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    EAS2(a3, rFP, a3)                      #  a3 <- &fp[B]

#ifdef MIPS32REVGE6
    LOAD64_F(fv0, fv0f, a3)
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    cvt.s.l   fv0, fv0
#else
    LOAD64(rARG0, rARG1, a3)
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    JAL(__floatdisf)
#endif

    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_F_GOTO(fv0, rOBJ, t0)         #  vA <- fv0
