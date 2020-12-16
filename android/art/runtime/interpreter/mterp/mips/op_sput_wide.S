    /*
     * 64-bit SPUT handler.
     */
    /* sput-wide vAA, field@BBBB */
    .extern MterpSet64Static
    EXPORT_PC()
    FETCH(a0, 1)                           # a0 <- field ref CCCC
    GET_OPA(a1)                            # a1 <- AA
    EAS2(a1, rFP, a1)                      # a1 <- &fp[AA]
    lw    a2, OFF_FP_METHOD(rFP)           # a2 <- method
    move  a3, rSELF                        # a3 <- self
    PREFETCH_INST(2)                       # load rINST
    JAL(MterpSet64Static)
    bnez  v0, MterpException               # bail out
    ADVANCE(2)                             # advance rPC
    GET_INST_OPCODE(t0)                    # extract opcode from rINST
    GOTO_OPCODE(t0)                        # jump to next instruction
