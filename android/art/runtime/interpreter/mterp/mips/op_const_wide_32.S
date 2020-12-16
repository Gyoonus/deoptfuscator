    /* const-wide/32 vAA, +BBBBbbbb */
    FETCH(a0, 1)                           #  a0 <- 0000bbbb (low)
    GET_OPA(a3)                            #  a3 <- AA
    FETCH_S(a2, 2)                         #  a2 <- ssssBBBB (high)
    FETCH_ADVANCE_INST(3)                  #  advance rPC, load rINST
    INSERT_HIGH_HALF(a0, a2)               #  a0 <- BBBBbbbb
    sra       a1, a0, 31                   #  a1 <- ssssssss
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_GOTO(a0, a1, a3, t0)        #  vAA/vAA+1 <- a0/a1
