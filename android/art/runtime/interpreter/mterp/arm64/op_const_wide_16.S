    /* const-wide/16 vAA, #+BBBB */
    FETCH_S x0, 1                       // x0<- ssssssssssssBBBB (sign-extended)
    lsr     w3, wINST, #8               // w3<- AA
    FETCH_ADVANCE_INST 2                // advance rPC, load rINST
    GET_INST_OPCODE ip                  // extract opcode from rINST
    SET_VREG_WIDE x0, w3
    GOTO_OPCODE ip                      // jump to next instruction
