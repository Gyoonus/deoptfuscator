    /* const-wide vAA, +HHHHhhhhBBBBbbbb */
    FETCH(a0, 1)                           #  a0 <- bbbb (low)
    FETCH(a1, 2)                           #  a1 <- BBBB (low middle)
    FETCH(a2, 3)                           #  a2 <- hhhh (high middle)
    INSERT_HIGH_HALF(a0, a1)               #  a0 <- BBBBbbbb (low word)
    FETCH(a3, 4)                           #  a3 <- HHHH (high)
    GET_OPA(t1)                            #  t1 <- AA
    INSERT_HIGH_HALF(a2, a3)               #  a2 <- HHHHhhhh (high word)
    FETCH_ADVANCE_INST(5)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_GOTO(a0, a2, t1, t0)        #  vAA/vAA+1 <- a0/a2
