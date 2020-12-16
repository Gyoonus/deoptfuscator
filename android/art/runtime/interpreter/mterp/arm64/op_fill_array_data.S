    /* fill-array-data vAA, +BBBBBBBB */
    EXPORT_PC
    FETCH   w0, 1                       // x0<- 000000000000bbbb (lo)
    FETCH_S x1, 2                       // x1<- ssssssssssssBBBB (hi)
    lsr     w3, wINST, #8               // w3<- AA
    orr     x1, x0, x1, lsl #16         // x1<- ssssssssBBBBbbbb
    GET_VREG w0, w3                     // w0<- vAA (array object)
    add     x1, xPC, x1, lsl #1         // x1<- PC + ssssssssBBBBbbbb*2 (array data off.)
    bl      MterpFillArrayData          // (obj, payload)
    cbz     w0, MterpPossibleException      // exception?
    FETCH_ADVANCE_INST 3                // advance rPC, load rINST
    GET_INST_OPCODE ip                  // extract opcode from rINST
    GOTO_OPCODE ip                      // jump to next instruction
