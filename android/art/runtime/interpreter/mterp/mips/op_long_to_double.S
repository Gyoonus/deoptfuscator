    /*
     * long-to-double
     */
    /* unop vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a3)                            #  a3 <- B
    EAS2(a3, rFP, a3)                      #  a3 <- &fp[B]

#ifdef MIPS32REVGE6
    LOAD64_F(fv0, fv0f, a3)
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    cvt.d.l   fv0, fv0
#else
    LOAD64(rARG0, rARG1, a3)
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    JAL(__floatdidf)                       #  a0/a1 <- op, a2-a3 changed
#endif

    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_F_GOTO(fv0, fv0f, rOBJ, t0) #  vA/vA+1 <- result
