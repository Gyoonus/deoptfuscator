    /* move-exception vAA */
    GET_OPA(a2)                                 #  a2 <- AA
    lw    a3, THREAD_EXCEPTION_OFFSET(rSELF)    #  get exception obj
    FETCH_ADVANCE_INST(1)                       #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                         #  extract opcode from rINST
    GET_OPCODE_TARGET(t0)
    SET_VREG_OBJECT(a3, a2)                     #  fp[AA] <- exception obj
    sw    zero, THREAD_EXCEPTION_OFFSET(rSELF)  #  clear exception
    JR(t0)                                      #  jump to next instruction
