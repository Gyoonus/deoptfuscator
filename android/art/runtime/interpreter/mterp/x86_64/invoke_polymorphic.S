%default { "helper":"UndefinedInvokeHandler" }
    /*
     * invoke-polymorphic handler wrapper.
     */
    /* op {vC, vD, vE, vF, vG}, meth@BBBB, proto@HHHH */
    /* op {vCCCC..v(CCCC+AA-1)}, meth@BBBB, proto@HHHH */
    .extern $helper
    EXPORT_PC
    movq    rSELF, OUT_ARG0
    leaq    OFF_FP_SHADOWFRAME(rFP), OUT_ARG1
    movq    rPC, OUT_ARG2
    REFRESH_INST ${opnum}
    movl    rINST, OUT_32_ARG3
    call    SYMBOL($helper)
    testb   %al, %al
    jz      MterpException
    ADVANCE_PC 4
    call    SYMBOL(MterpShouldSwitchInterpreters)
    testb   %al, %al
    jnz     MterpFallback
    FETCH_INST
    GOTO_NEXT
