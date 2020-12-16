%default { "helper":"UndefinedInvokeHandler" }
    /*
     * invoke-polymorphic handler wrapper.
     */
    /* op {vC, vD, vE, vF, vG}, meth@BBBB, proto@HHHH */
    /* op {vCCCC..v(CCCC+AA-1)}, meth@BBBB, proto@HHHH */
    .extern $helper
    EXPORT_PC
    mov     x0, xSELF
    add     x1, xFP, #OFF_FP_SHADOWFRAME
    mov     x2, xPC
    mov     x3, xINST
    bl      $helper
    cbz     w0, MterpException
    FETCH_ADVANCE_INST 4
    bl      MterpShouldSwitchInterpreters
    cbnz    w0, MterpFallback
    GET_INST_OPCODE ip
    GOTO_OPCODE ip
