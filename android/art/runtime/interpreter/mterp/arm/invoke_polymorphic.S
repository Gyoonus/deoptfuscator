%default { "helper":"UndefinedInvokeHandler" }
    /*
     * invoke-polymorphic handler wrapper.
     */
    /* op {vC, vD, vE, vF, vG}, meth@BBBB, proto@HHHH */
    /* op {vCCCC..v(CCCC+AA-1)}, meth@BBBB, proto@HHHH */
    .extern $helper
    EXPORT_PC
    mov     r0, rSELF
    add     r1, rFP, #OFF_FP_SHADOWFRAME
    mov     r2, rPC
    mov     r3, rINST
    bl      $helper
    cmp     r0, #0
    beq     MterpException
    FETCH_ADVANCE_INST 4
    bl      MterpShouldSwitchInterpreters
    cmp     r0, #0
    bne     MterpFallback
    GET_INST_OPCODE ip
    GOTO_OPCODE ip
