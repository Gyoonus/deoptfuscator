%default { "is_object":"0", "helper":"MterpGet32Static" }
    /*
     * General SGET handler.
     *
     * for: sget, sget-object, sget-boolean, sget-byte, sget-char, sget-short
     */
    /* op vAA, field@BBBB */
    .extern $helper
    EXPORT_PC()
    FETCH(a0, 1)                           # a0 <- field ref BBBB
    lw    a1, OFF_FP_METHOD(rFP)           # a1 <- method
    move  a2, rSELF                        # a2 <- self
    JAL($helper)
    lw    a3, THREAD_EXCEPTION_OFFSET(rSELF)
    GET_OPA(a2)                            # a2 <- AA
    PREFETCH_INST(2)
    bnez  a3, MterpException               # bail out
    ADVANCE(2)
    GET_INST_OPCODE(t0)                    # extract opcode from rINST
.if $is_object
    SET_VREG_OBJECT_GOTO(v0, a2, t0)       # fp[AA] <- v0
.else
    SET_VREG_GOTO(v0, a2, t0)              # fp[AA] <- v0
.endif
