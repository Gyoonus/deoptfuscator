    /* int-to-long vA, vB */
    lsr     w3, wINST, #12              // w3<- B
    ubfx    w4, wINST, #8, #4           // w4<- A
    GET_VREG_S x0, w3                   // x0<- sign_extend(fp[B])
    FETCH_ADVANCE_INST 1                // advance rPC, load wINST
    GET_INST_OPCODE ip                  // extract opcode from wINST
    SET_VREG_WIDE x0, w4                // fp[A]<- x0
    GOTO_OPCODE ip                      // jump to next instruction
