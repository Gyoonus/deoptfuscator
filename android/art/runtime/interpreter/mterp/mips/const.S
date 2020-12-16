%default { "helper":"UndefinedConstHandler" }
    /* const/class vAA, type@BBBB */
    /* const/method-handle vAA, method_handle@BBBB */
    /* const/method-type vAA, proto@BBBB */
    /* const/string vAA, string@@BBBB */
    .extern $helper
    EXPORT_PC()
    FETCH(a0, 1)                        # a0 <- BBBB
    GET_OPA(a1)                         # a1 <- AA
    addu   a2, rFP, OFF_FP_SHADOWFRAME  # a2 <- shadow frame
    move   a3, rSELF
    JAL($helper)                        # v0 <- Mterp(index, tgt_reg, shadow_frame, self)
    PREFETCH_INST(2)                    # load rINST
    bnez   v0, MterpPossibleException
    ADVANCE(2)                          # advance rPC
    GET_INST_OPCODE(t0)                 # extract opcode from rINST
    GOTO_OPCODE(t0)                     # jump to next instruction
