    /* move-wide vA, vB */
    /* NOTE: regs can overlap, e.g. "move v6, v7" or "move v7, v6" */
    GET_OPA4(a2)                           #  a2 <- A(+)
    GET_OPB(a3)                            #  a3 <- B
    EAS2(a3, rFP, a3)                      #  a3 <- &fp[B]
    LOAD64(a0, a1, a3)                     #  a0/a1 <- fp[B]
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_GOTO(a0, a1, a2, t0)        #  fp[A] <- a0/a1
