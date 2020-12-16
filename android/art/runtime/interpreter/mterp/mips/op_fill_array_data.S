    /* fill-array-data vAA, +BBBBBBBB */
    EXPORT_PC()
    FETCH(a1, 1)                           #  a1 <- bbbb (lo)
    FETCH(a0, 2)                           #  a0 <- BBBB (hi)
    GET_OPA(a3)                            #  a3 <- AA
    INSERT_HIGH_HALF(a1, a0)               #  a1 <- BBBBbbbb
    GET_VREG(a0, a3)                       #  a0 <- vAA (array object)
    EAS1(a1, rPC, a1)                      #  a1 <- PC + BBBBbbbb*2 (array data off.)
    JAL(MterpFillArrayData)                #  v0 <- Mterp(obj, payload)
    beqz      v0,  MterpPossibleException  #  has exception
    FETCH_ADVANCE_INST(3)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction
