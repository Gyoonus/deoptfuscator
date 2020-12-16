    /*
     * 64-bit SGET handler.
     */
    /* sget-wide vAA, field@BBBB */
    .extern MterpGet64Static
    EXPORT_PC()
    FETCH(a0, 1)                           # a0 <- field ref BBBB
    lw    a1, OFF_FP_METHOD(rFP)           # a1 <- method
    move  a2, rSELF                        # a2 <- self
    JAL(MterpGet64Static)
    lw    a3, THREAD_EXCEPTION_OFFSET(rSELF)
    bnez  a3, MterpException
    GET_OPA(a1)                            # a1 <- AA
    FETCH_ADVANCE_INST(2)                  # advance rPC, load rINST
    GET_INST_OPCODE(t0)                    # extract opcode from rINST
    SET_VREG64_GOTO(v0, v1, a1, t0)        # vAA/vAA+1 <- v0/v1
