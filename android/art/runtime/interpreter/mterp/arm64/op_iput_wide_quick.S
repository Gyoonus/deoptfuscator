    /* iput-wide-quick vA, vB, offset//CCCC */
    lsr     w2, wINST, #12              // w2<- B
    FETCH w3, 1                         // w3<- field byte offset
    GET_VREG w2, w2                     // w2<- fp[B], the object pointer
    ubfx    w0, wINST, #8, #4           // w0<- A
    cbz     w2, common_errNullObject    // object was null
    GET_VREG_WIDE x0, w0                // x0<- fp[A]
    FETCH_ADVANCE_INST 2                // advance rPC, load wINST
    str     x0, [x2, x3]                // obj.field<- x0
    GET_INST_OPCODE ip                  // extract opcode from wINST
    GOTO_OPCODE ip                      // jump to next instruction
