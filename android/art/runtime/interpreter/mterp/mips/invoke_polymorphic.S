%default { "helper":"UndefinedInvokeHandler" }
    /*
     * invoke-polymorphic handler wrapper.
     */
    /* op {vC, vD, vE, vF, vG}, meth@BBBB, proto@HHHH */
    /* op {vCCCC..v(CCCC+AA-1)}, meth@BBBB, proto@HHHH */
    .extern $helper
    EXPORT_PC()
    move    a0, rSELF
    addu    a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    move    a3, rINST
    JAL($helper)
    beqz    v0, MterpException
    FETCH_ADVANCE_INST(4)
    JAL(MterpShouldSwitchInterpreters)
    bnez    v0, MterpFallback
    GET_INST_OPCODE(t0)
    GOTO_OPCODE(t0)
