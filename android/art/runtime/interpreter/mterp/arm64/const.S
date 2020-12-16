%default { "helper":"UndefinedConstHandler" }
    /* const/class vAA, type@BBBB */
    /* const/method-handle vAA, method_handle@BBBB */
    /* const/method-type vAA, proto@BBBB */
    /* const/string vAA, string@@BBBB */
    .extern $helper
    EXPORT_PC
    FETCH w0, 1                         // w0<- BBBB
    lsr     w1, wINST, #8               // w1<- AA
    add     x2, xFP, #OFF_FP_SHADOWFRAME
    mov     x3, xSELF
    bl      $helper                     // (index, tgt_reg, shadow_frame, self)
    PREFETCH_INST 2                     // load rINST
    cbnz    w0, MterpPossibleException  // let reference interpreter deal with it.
    ADVANCE 2                           // advance rPC
    GET_INST_OPCODE ip                  // extract opcode from rINST
    GOTO_OPCODE ip                      // jump to next instruction
