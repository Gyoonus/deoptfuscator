    /* const-wide/32 vAA, #+BBBBbbbb */
    FETCH   w0, 1                       // x0<- 000000000000bbbb (low)
    lsr     w3, wINST, #8               // w3<- AA
    FETCH_S x2, 2                       // x2<- ssssssssssssBBBB (high)
    FETCH_ADVANCE_INST 3                // advance rPC, load wINST
    GET_INST_OPCODE ip                  // extract opcode from wINST
    orr     x0, x0, x2, lsl #16         // x0<- ssssssssBBBBbbbb
    SET_VREG_WIDE x0, w3
    GOTO_OPCODE ip                      // jump to next instruction
